// Regenerates dist/remote-styles.js from the firmware's embedded web page.
//
//     node tools/gen-remote-styles.mjs
//
// The remote-style catalogue, renderer and validator live in web_control.cpp's
// PAGE_HTML, because that is what the device serves. The Lovelace card needs the
// same code, and a hand-kept second copy would drift the first time a button or
// a style changed. So it is lifted at build time and committed — users never run
// this; HACS ships the generated file beside the cards.
//
// The output is a snapshot of the firmware's *built-in* styles. User-authored
// ones live in device NVS and are never in here; the card takes those as pasted
// JSON, or fetches them from /remote_templates when it can reach the device.
//
// Re-run this after touching RMT_BUILTIN, RMT_BTNS, the icons, the renderer or
// the .rmt-* CSS, and commit the result.
import { readFileSync, writeFileSync } from 'node:fs';

const here = new URL('./', import.meta.url);
const CPP = new URL('../components/espidf_ble_keyboard/web_control.cpp', here);
const OUT = new URL('../dist/remote-styles.js', here);

const src = readFileSync(CPP, 'utf8');
const page = src.slice(src.indexOf('R"rawhtml(') + 10, src.indexOf(')rawhtml"'));
if (page.length < 1000) throw new Error('PAGE_HTML slice failed — did the raw-string delimiter change?');

/** From `marker` through the balanced bracket pair that follows it. */
function balanced(marker, open = '{') {
  const close = open === '{' ? '}' : ']';
  const at = page.indexOf(marker);
  if (at < 0) throw new Error(`not found in web_control.cpp: ${marker}`);
  const start = page.indexOf(open, at + marker.length - 1);
  let depth = 0;
  for (let i = start; i < page.length; i++) {
    if (page[i] === open) depth++;
    else if (page[i] === close) { if (--depth === 0) return page.slice(at, i + 1); }
  }
  throw new Error(`unbalanced ${open}${close} after ${marker}`);
}

/** A single-line `const NAME = …;` declaration. */
function oneLine(marker) {
  const at = page.indexOf(marker);
  if (at < 0) throw new Error(`not found in web_control.cpp: ${marker}`);
  const end = page.indexOf(';', at);
  if (end < 0) throw new Error(`unterminated declaration at ${marker}`);
  return page.slice(at, end + 1);
}

// ── the JS ────────────────────────────────────────────────────────
const PARTS = [
  ['const RI=', () => balanced('const RI=') + ';'],
  ['const RMT_BTNS=', () => balanced('const RMT_BTNS=') + ';'],
  ['const RMT_VARS=', () => balanced('const RMT_VARS=') + ';'],
  ['const RMT_BUILTIN=', () => balanced('const RMT_BUILTIN=', '[') + ';'],
  ['const RMT_KINDS=', () => oneLine('const RMT_KINDS=')],
  ['const RMT_OPTS=', () => oneLine('const RMT_OPTS=')],
  ['const RMT_HEX=', () => oneLine('const RMT_HEX=')],
  ['const RMT_CLIP=', () => oneLine('const RMT_CLIP=')],
  ['function icon(', () => balanced('function icon(')],
  ['function esc(', () => balanced('function esc(')],
  ['function btnHtml(', () => balanced('function btnHtml(')],
  ['function sectionHtml(', () => balanced('function sectionHtml(')],
  ['function validateTpl(', () => balanced('function validateTpl(')],
];
const js = PARTS.map(([, take]) => take()).join('\n\n');

// Everything the bundle uses must be something it also defines. Checking this
// by hand is what let RMT_OPTS slip out of the gallery's bundle once, and the
// symptom was every button rendering as nothing at all — silently.
const EXPORTS = ['RI', 'RMT_BTNS', 'RMT_VARS', 'RMT_BUILTIN', 'RMT_KINDS', 'RMT_OPTS',
  'RMT_HEX', 'RMT_CLIP', 'icon', 'esc', 'btnHtml', 'sectionHtml', 'validateTpl'];
const defined = new Set([...js.matchAll(/(?:^|\n)\s*(?:const|function)\s+([A-Za-z_$][\w$]*)/g)]
  .map(m => m[1]));
for (const name of EXPORTS) {
  if (!defined.has(name)) throw new Error(`bundle is missing ${name} — extraction markers moved`);
}

// ── the CSS ───────────────────────────────────────────────────────
// Whole rules, not matching lines. A line filter truncated every rule written
// across two lines, which left `.rmt-ring{` unterminated and silently swallowed
// the rules after it.
const styleAt = page.indexOf('<style>'), styleEnd = page.indexOf('</style>', styleAt);
if (styleAt < 0 || styleEnd < 0) throw new Error('could not find the page <style> block');
const allCss = page.slice(styleAt + 7, styleEnd).replace(/\/\*[\s\S]*?\*\//g, '');

const rules = [];
let depth = 0, start = 0;
for (let i = 0; i < allCss.length; i++) {
  if (allCss[i] === '{') { if (depth++ === 0) { /* selector ran from `start` */ } }
  else if (allCss[i] === '}' && --depth === 0) {
    rules.push(allCss.slice(start, i + 1).trim());
    start = i + 1;
  }
}
// Only the remote's own rules: the card has its own card chrome, and the page's
// body/keyboard rules would be dead weight (or worse) inside a shadow root.
const css = rules.filter(r => /(^|[,\s])\.rmt-/.test(r.split('{')[0])).join('\n');
for (const need of ['.rmt-btn{', '.rmt-ring{', '.rmt-rocker-col{', '.rmt-body{']) {
  if (!css.includes(need)) throw new Error(`CSS is missing ${need}`);
}
// The cards build their styles inside a JS template literal, so a stray
// backtick silently terminates the string and the file stops parsing. That has
// bitten this repo three times; refuse it here rather than ship it.
if (css.includes('`') || css.includes('${')) {
  throw new Error('the remote CSS contains a backtick or ${ — it cannot go in a template literal');
}

// ── prove it works before writing ─────────────────────────────────
// Name checks catch a missing symbol; only running it catches a broken one.
const probe = new Function(`${js}\nreturn sectionHtml(['row','mute',['spare1','X','light sm']]);`)();
if (!/data-action="mute"/.test(probe) || !/data-action="spare1"/.test(probe) ||
    !/class="[^"]*\blight\b[^"]*\bsm\b/.test(probe)) {
  throw new Error(`bundle renders incorrectly:\n${probe}`);
}
const builtins = new Function(`${js}\nreturn RMT_BUILTIN.map(t=>t.id);`)();

const out = `// GENERATED FILE — do not edit by hand.
//
// Lifted from components/espidf_ble_keyboard/web_control.cpp so the Home
// Assistant card draws remotes with exactly the code the device's own web page
// uses. Regenerate after any change to the styles, catalogue, renderer or CSS:
//
//     node tools/gen-remote-styles.mjs
//
// Built-ins in this snapshot: ${builtins.join(', ')}
// Custom styles are not here — they live in the device's NVS. The card takes
// those as pasted JSON, or from /api/ble_keyboard/remote_templates when it can
// reach the device (a dashboard on https cannot).

${js}

// The remote's stylesheet. It falls back to the page palette (--bg, --fg,
// --border, --muted, --active, --accent), so whatever hosts this must map those
// onto its own theme — inside a shadow root there is no :root to inherit from.
export const RMT_CSS = \`
${css}
\`;

export { ${EXPORTS.join(', ')} };
`;

writeFileSync(OUT, out);
console.log(`wrote dist/remote-styles.js — ${(out.length / 1024).toFixed(1)} KB`);
console.log(`  built-ins: ${builtins.join(', ')}`);
console.log(`  catalogue: ${new Function(`${js}\nreturn Object.keys(RMT_BTNS).length;`)()} actions`);
console.log(`  css rules: ${css.split('}').length - 1}`);
