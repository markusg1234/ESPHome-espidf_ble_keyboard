// Regenerates dist/remote-styles.js from the firmware's embedded web page.
//
//     node tools/gen-remote-styles.mjs
//
// The remote-style catalogue, renderer and validator live in web_page.html,
// because that is what the device serves. The Lovelace card needs the
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
const PAGE = new URL('../components/espidf_ble_keyboard/web_page.html', here);
const OUT = new URL('../dist/remote-styles.js', here);

// Normalised to LF the moment it is read. The working copy is CRLF on Windows
// (core.autocrlf), and everything lifted below is pasted into the output — so
// without this the generated file's line endings depend on whose checkout ran
// it, and the CSS goes into a template literal carrying \r into the string.
const page = readFileSync(PAGE, 'utf8').replace(/\r\n/g, '\n');
if (page.length < 1000) throw new Error('web_page.html is too short to be the control page');

/** From `marker` through the balanced bracket pair that follows it. */
function balanced(marker, open = '{') {
  const close = open === '{' ? '}' : ']';
  const at = page.indexOf(marker);
  if (at < 0) throw new Error(`not found in web_page.html: ${marker}`);
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
  if (at < 0) throw new Error(`not found in web_page.html: ${marker}`);
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
  ['const RMT_KEY_H=', () => oneLine('const RMT_KEY_H=')],
  ['const RMT_ICON_H=', () => oneLine('const RMT_ICON_H=')],
  ['const RMT_RING=', () => oneLine('const RMT_RING=')],
  ['const RMT_CROSS=', () => oneLine('const RMT_CROSS=')],
  ['const RMT_KNOB=', () => oneLine('const RMT_KNOB=')],
  ['const RMT_SLIDER=', () => oneLine('const RMT_SLIDER=')],
  ['const RMT_SIDE_GAP=', () => oneLine('const RMT_SIDE_GAP=')],
  ['const RMT_GROUP_NAME=', () => oneLine('const RMT_GROUP_NAME=')],
  ['const RMT_GROUP_MAX=', () => oneLine('const RMT_GROUP_MAX=')],
  ['const KNOB_SET=', () => oneLine('const KNOB_SET=')],
  ['const RMT_LCD_OPTS=', () => oneLine('const RMT_LCD_OPTS=')],
  ['const RMT_LCD_COLOURS=', () => oneLine('const RMT_LCD_COLOURS=')],
  ['const RMT_LCD_LABELLED=', () => oneLine('const RMT_LCD_LABELLED=')],
  ['const RMT_LCD_KEYS=', () => oneLine('const RMT_LCD_KEYS=')],
  ['const RMT_HEX=', () => oneLine('const RMT_HEX=')],
  ['const RMT_CLIP=', () => oneLine('const RMT_CLIP=')],
  ['const RMT_FETCH=', () => oneLine('const RMT_FETCH=')],
  ['const RMT_ICON_D=', () => oneLine('const RMT_ICON_D=')],
  ['const RMT_ICON_VB=', () => oneLine('const RMT_ICON_VB=')],
  ['const RMT_ICON_T=', () => oneLine('const RMT_ICON_T=')],
  ['const RMT_ICON_NAME=', () => oneLine('const RMT_ICON_NAME=')],
  ['const RMT_ICON_KEYS=', () => oneLine('const RMT_ICON_KEYS=')],
  ['const RMT_ICON_LEN=', () => oneLine('const RMT_ICON_LEN=')],
  ['const RMT_ICON_MAX=', () => oneLine('const RMT_ICON_MAX=')],
  ['const RMT_ICONS=', () => oneLine('const RMT_ICONS=')],
  ['function iconBad(', () => balanced('function iconBad(')],
  ['function useIcons(', () => balanced('function useIcons(')],
  ['function groupOf(', () => balanced('function groupOf(')],
  ['function tplGroups(', () => balanced('function tplGroups(')],
  ['function iconNames(', () => balanced('function iconNames(')],
  ['function icon(', () => balanced('function icon(')],
  // The knob's gestures. The card wires its own remote with it, so a knob turns
  // the same on both surfaces.
  ['function knobShow(', () => balanced('function knobShow(')],
  ['const KNOB_SETTLE_MS=', () => oneLine('const KNOB_SETTLE_MS=')],
  ['function knobHold(', () => balanced('function knobHold(')],
  ['function knobSettle(', () => balanced('function knobSettle(')],
  ['function knobLevel(', () => balanced('function knobLevel(')],
  ['function knobWire(', () => balanced('function knobWire(')],
  // A group's opening, shutting and sizing, which the card wires its remote with,
  // and what keeps a lone key beside a rocker laid across inside the remote.
  ['function rockerFit(', () => balanced('function rockerFit(')],
  ['function groupWire(', () => balanced('function groupWire(')],
  // The typing key's box, so it sends exactly the same on both surfaces.
  ['function typeWire(', () => balanced('function typeWire(')],
  // And the width that makes the keyboard key's keys square, for a pop-out.
  ['function keysSquareWidth(', () => balanced('function keysSquareWidth(')],
  ['function esc(', () => balanced('function esc(')],
  // Before validateTpl, which calls it — and the card's renderer calls it too,
  // which is the whole point of it being here rather than inline in the importer.
  ['function themeValueBad(', () => balanced('function themeValueBad(')],
  ['function btnHtml(', () => balanced('function btnHtml(')],
  ['function lcdLabel(', () => balanced('function lcdLabel(')],
  ['function sectionHtml(', () => balanced('function sectionHtml(')],
  ['function groupsHtml(', () => balanced('function groupsHtml(')],
  ['function validateTpl(', () => balanced('function validateTpl(')],
];
const js = PARTS.map(([, take]) => take()).join('\n\n');

// Everything the bundle uses must be something it also defines. Checking this
// by hand is what let RMT_OPTS slip out of the gallery's bundle once, and the
// symptom was every button rendering as nothing at all — silently.
const EXPORTS = ['RI', 'RMT_BTNS', 'RMT_VARS', 'RMT_BUILTIN', 'RMT_KINDS', 'RMT_OPTS', 'RMT_KEY_H',
  'RMT_LCD_OPTS', 'RMT_LCD_COLOURS', 'RMT_LCD_KEYS', 'RMT_LCD_LABELLED', 'lcdLabel', 'RMT_HEX', 'RMT_CLIP', 'RMT_FETCH', 'icon', 'esc',
  'RMT_ICON_NAME', 'RMT_ICON_LEN', 'RMT_ICON_MAX', 'RMT_ICONS', 'iconBad', 'useIcons', 'iconNames',
  'themeValueBad', 'btnHtml', 'sectionHtml', 'validateTpl', 'knobWire', 'knobLevel', 'typeWire', 'keysSquareWidth',
  'RMT_GROUP_NAME', 'groupOf', 'tplGroups', 'groupsHtml', 'groupWire', 'rockerFit'];
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
// body/keyboard rules would be dead weight (or worse) inside a shadow root. An
// @supports block comes along when what it wraps is a remote rule — it is how a
// newer CSS feature gets its fallback, and dropping it would leave the card on
// the fallback for good.
const css = rules.filter(r => {
  const sel = r.split('{')[0];
  return /(^|[,\s])\.rmt-/.test(sel) || (/^@supports\b/.test(sel.trim()) && /\.rmt-/.test(r));
}).join('\n');
for (const need of ['.rmt-btn{', '.rmt-ring{', '.rmt-cross{', '.rmt-knob-ring{', '.rmt-rocker-col{', '.rmt-rocker-line{',
                    '.rmt-body{', '.rmt-lcd{']) {
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
// The lcd branch draws no buttons, so the check above would pass with it
// missing entirely — it needs its own, and the card fills panels by data-lcd.
const lcdProbe = new Function(`${js}\nreturn sectionHtml(['lcd',['Room','temp'],['Host','@host','lg']]);`)();
if (!/data-lcd="temp"/.test(lcdProbe) || !/data-lcd="@host"/.test(lcdProbe) ||
    !/class="rmt-lcd-val lg"/.test(lcdProbe)) {
  throw new Error(`lcd section renders incorrectly:\n${lcdProbe}`);
}
// An imported icon drawn from its record, a key whose icon is missing falling
// back to its label, and a record carrying markup refused outright — three
// branches of btnHtml that none of the probes above reach.
const iconProbe = new Function(`${js}
useIcons({logo:{vb:'0 0 10 10',p:[{d:'M0 0h10v10z',f:'#e50914',r:1}]},
          evil:{vb:'0 0 1 1',p:[{d:'M0 0"/><script>x</script>'}]}});
return [sectionHtml(['row',['spare1','Logo','icon:logo wide']]),
        sectionHtml(['row',['spare2','Gone','icon:absent']]),
        sectionHtml(['row',['spare3','Bad','icon:evil']])];`)();
if (!iconProbe[0].includes('<svg class="rmt-ico" viewBox="0 0 10 10"><path d="M0 0h10v10z" fill="#e50914" fill-rule="evenodd"/></svg>') ||
    !iconProbe[1].includes('>Gone</button>') ||
    !iconProbe[2].includes('>Bad</button>') || iconProbe[2].includes('script')) {
  throw new Error(`icons render incorrectly:\n${iconProbe.join('\n')}`);
}
// h:<px> is the one button option that writes a number into a style attribute:
// in range it must land, out of range or malformed it must not.
const hProbe = new Function(`${js}
return [sectionHtml(['row',['spare1','A','fill h:80 sq']]),
        sectionHtml(['row',['spare2','B','h:10'],['spare3','C','h:999'],['spare4','D','h:8x']])];`)();
if (!/class="rmt-btn fill sq"/.test(hProbe[0]) || !hProbe[0].includes('style="height:80px"') ||
    hProbe[1].includes('height:')) {
  throw new Error(`h:/fill render incorrectly:\n${hProbe.join('\n')}`);
}
// ih:<px> writes a number into the style too, under the same rule.
const ihProbe = new Function(`${js}
return [sectionHtml(['row',['spare1','A','icon:home ih:36 #336699']]),
        sectionHtml(['row',['spare2','B','ih:7'],['spare3','C','ih:999'],['spare4','D','ih:3x']])];`)();
if (!/class="rmt-btn rmt-ih"/.test(ihProbe[0]) ||
    !ihProbe[0].includes('style="background:#336699;border-color:#336699;--rb-ih:36px"') ||
    ihProbe[1].includes('rb-ih') || ihProbe[1].includes('rmt-ih')) {
  throw new Error(`ih: renders incorrectly:\n${ihProbe.join('\n')}`);
}
// A ring's size and centre reach an inline style too: in range they land, and
// the actions after the settings object are the ones drawn.
const ringProbe = new Function(`${js}
return [sectionHtml(['ring',{size:200,center:96},'up','left','ok','right','down']),
        sectionHtml(['ring',{size:9999,center:'96px'}]), sectionHtml(['ring'])];`)();
if (!ringProbe[0].includes('<div class="rmt-ring" style="width:200px;height:200px;--rb-ring-c:96px">') ||
    !ringProbe[0].includes('data-action="ok"') || ringProbe[1].includes('style=') ||
    !ringProbe[1].includes('data-action="up"') || !ringProbe[2].includes('<div class="rmt-ring">')) {
  throw new Error(`ring renders incorrectly:\n${ringProbe.join('\n')}`);
}
// A ring's corners: a radius in range reaches the style, 0 (a square ring)
// included, and one out of range or not a whole number is dropped or refused.
const radiusProbe = new Function(`${js}
return [sectionHtml(['ring',{size:200,radius:28}]), sectionHtml(['ring',{radius:0}]),
        sectionHtml(['ring',{radius:999}]), sectionHtml(['ring',{radius:'28px'}]),
        validateTpl({id:'r',name:'R',sections:[['ring',{radius:28}]]}),
        validateTpl({id:'r',name:'R',sections:[['ring',{radius:161}]]}),
        validateTpl({id:'r',name:'R',sections:[['ring',{radius:'28px'}]]}),
        validateTpl({id:'r',name:'R',sections:[['ring',{corners:4}]]})];`)();
if (!radiusProbe[0].includes('<div class="rmt-ring" style="width:200px;height:200px;--rb-ring-r:28px">') ||
    !radiusProbe[1].includes('<div class="rmt-ring" style="--rb-ring-r:0px">') ||
    !radiusProbe[2].includes('<div class="rmt-ring">') || !radiusProbe[3].includes('<div class="rmt-ring">') ||
    radiusProbe[4] !== '' || !/ring "radius" is 0-160 pixels/.test(radiusProbe[5]) ||
    !/ring "radius" must be a whole number/.test(radiusProbe[6]) || !/A ring takes size, center and radius/.test(radiusProbe[7]) ||
    !css.includes('border-radius:var(--rb-ring-r,50%)')) {
  throw new Error(`ring radius renders or validates incorrectly:\n${radiusProbe.join('\n')}`);
}
// A grid: the column count reaches an inline style, the shared options reach
// every key, a key's own come after them, and "|" is an empty cell.
const gridProbe = new Function(`${js}
return sectionHtml(['grid',{cols:3,h:60,opts:'sq'},['spare1','Copy'],'|',['spare2','Paste','#336699']]);`)();
if (!gridProbe.includes('class="rmt-grid" style="grid-template-columns:repeat(3,minmax(0,1fr))"') ||
    (gridProbe.match(/height:60px/g) || []).length !== 2 || !gridProbe.includes('<div></div>') ||
    !/data-action="spare2" style="background:#336699;border-color:#336699;height:60px"/.test(gridProbe) ||
    !/class="rmt-btn sq" data-action="spare1"/.test(gridProbe)) {
  throw new Error(`grid renders incorrectly:\n${gridProbe}`);
}
// A knob: its defaults, its settings reaching the inline style in range and
// dropped outside it, a reading as the middle key's face, no key at all when it
// lists only the two it turns by, a key-sized one with its marks hidden, and
// a caption over the knob unless it is asked to go under.
const knobProbe = new Function(`${js}
return [sectionHtml(['pot']),
        sectionHtml(['pot',{size:200,center:90,step:15,show:'temp',label:'Heat'},['spare1','Warmer'],'spare2','spare3']),
        sectionHtml(['pot',{size:9999,center:5,step:1},'volume_up','volume_down']),
        sectionHtml(['pot',{size:42,marks:false},'volume_up','volume_down']),
        sectionHtml(['pot',{label:'Vol'}]), sectionHtml(['pot',{label:'Vol',label_at:'bottom'}])];`)();
const KNOB_DEFAULT = 'style="--rb-knob:150px;--rb-knob-c:75px;';
if (!knobProbe[0].includes('data-up="volume_up" data-down="volume_down" data-step="30" ' + KNOB_DEFAULT) ||
    !knobProbe[0].includes('stroke-dasharray="0 30"') || !/class="rmt-btn" data-action="mute"/.test(knobProbe[0]) ||
    !knobProbe[1].includes('style="--rb-knob:200px;--rb-knob-c:90px;') || !knobProbe[1].includes('stroke-dasharray="0 15"') ||
    !/data-action="spare3"[^>]*><span class="rmt-knob-val" data-lcd="temp">--<\/span><\/button>/.test(knobProbe[1]) ||
    !knobProbe[1].includes('<span class="rmt-knob-label">Heat</span>') || !knobProbe[1].includes('Warmer') ||
    !knobProbe[2].includes(KNOB_DEFAULT) || knobProbe[2].includes('data-action=') ||
    !knobProbe[2].includes('<div class="rmt-knob-cap"></div>') || !knobProbe[0].includes('rmt-knob-mk dn') ||
    !knobProbe[3].includes('style="--rb-knob:42px;--rb-knob-c:21px;') || knobProbe[3].includes('rmt-knob-mk') ||
    !knobProbe[3].includes('stroke-width="2.1"') ||
    !/px"><span class="rmt-knob-label">Vol<\/span><div class="rmt-knob-dial">/.test(knobProbe[4]) ||
    !/<\/div><\/div><span class="rmt-knob-label">Vol<\/span><\/div>/.test(knobProbe[5])) {
  throw new Error(`knob renders incorrectly:\n${knobProbe.join('\n')}`);
}
// A level knob: its scale in data attributes, a gauge in place of the dots, and
// drawn without a reading until one arrives; a scale that runs backwards is no
// level at all.
const levelProbe = new Function(`${js}
return [sectionHtml(['pot',{level:'vol',max:1,inc:0.02}]), sectionHtml(['pot',{level:'vol',min:5,max:1}])];`)();
if (!levelProbe[0].includes('class="rmt-knob nolevel"') ||
    !levelProbe[0].includes('data-level="vol" data-min="0" data-max="1" data-sweep="270" data-inc="0.02"') ||
    !levelProbe[0].includes('class="rmt-knob-gauge"') || levelProbe[0].includes('class="tk"') ||
    levelProbe[1].includes('data-level') || !levelProbe[1].includes('class="tk"')) {
  throw new Error(`level knob renders incorrectly:\n${levelProbe.join('\n')}`);
}
// A slider: upright unless told across, its length from length or a knob's
// size (held to the slider's bounds), a third action pressed by its thumb, and
// a value to send ("set") or a reading to step to ("level").
const sliderProbe = new Function(`${js}
return [sectionHtml(['slider',{set:'spare12',label:'Lamp'}]),
        sectionHtml(['slider',{level:'vol',horizontal:true,length:200},'volume_up','volume_down','mute']),
        sectionHtml(['slider',{size:42,center:20,step:30,analogue:true,level:'vol'}]),
        sectionHtml(['slider',{set:'spare12',width:40}]), sectionHtml(['slider',{set:'spare12',width:999}])];`)();
if (!/class="rmt-slider nolevel vert" data-up="" data-down="" data-set="spare12"/.test(sliderProbe[0]) ||
    !sliderProbe[0].includes('--rb-sl-len:160px') || !sliderProbe[0].includes('<span class="rmt-knob-label">Lamp</span>') ||
    !/class="rmt-slider nolevel" data-up="volume_up" data-down="volume_down" data-level="vol"/.test(sliderProbe[1]) ||
    !sliderProbe[1].includes('--rb-sl-len:200px') || !sliderProbe[1].includes('data-press="mute"') ||
    !/<div class="rmt-slider-thumb" title="Tap: Mute"><svg/.test(sliderProbe[1]) || sliderProbe[1].includes('data-action=') ||
    !sliderProbe[2].includes('--rb-sl-len:42px') || !sliderProbe[2].includes('rmt-slider nolevel vert') ||
    !sliderProbe[3].includes('style="--rb-sl-len:160px;--rb-sl-w:40px;--rb-sl-rail:9px;--rb-sl-th:29px"') ||
    sliderProbe[4].includes('--rb-sl-w')) {
  throw new Error(`slider renders incorrectly:\n${sliderProbe.join('\n')}`);
}
// A rocker's settings: width and h reach the style in range and are dropped
// outside it, and the groups after them are the ones drawn.
const rockerProbe = new Function(`${js}
return [sectionHtml(['rocker',{width:60,h:48},['Vol','volume_up','volume_down'],['','mute']]),
        sectionHtml(['rocker',{width:999},['Vol','volume_up','volume_down']]),
        sectionHtml(['rocker',['Vol','volume_up','volume_down']]),
        sectionHtml(['rocker',{h:30},['Vol','volume_up','volume_down']])];`)();
if (!rockerProbe[0].includes('<div class="rmt-rocker rk-w rk-h" style="--rb-rk-w:60px;--rb-rk-h:48px">') ||
    !rockerProbe[0].includes('data-action="mute"') || !rockerProbe[1].includes('<div class="rmt-rocker"><div') ||
    !rockerProbe[2].includes('<div class="rmt-rocker"><div') || !rockerProbe[2].includes('data-action="volume_down"') ||
    !rockerProbe[3].includes('<div class="rmt-rocker rk-h" style="--rb-rk-h:30px">')) {
  throw new Error(`rocker renders incorrectly:\n${rockerProbe.join('\n')}`);
}
// A rocker laid across: a bar to a line, a lone key beside the bar before it (or
// left of the first), the keys still in the order listed, channel arrows turned
// to run with the bar unless the key has a face of its own, and "horizontal"
// accepted only as true or false.
const acrossProbe = new Function(`${js}
return [sectionHtml(['rocker',{horizontal:true},['','mute'],['Vol','volume_up','volume_down'],['','home'],['Ch','channel_up','channel_down']]),
        sectionHtml(['rocker',{horizontal:true},['','mute']]),
        sectionHtml(['rocker',{horizontal:true},['Ch',['channel_up','Up'],['channel_down','','icon:down']]]),
        sectionHtml(['rocker',['Ch','channel_up','channel_down']]),
        validateTpl({id:'r',name:'R',sections:[['rocker',{horizontal:'yes'},['Vol','volume_up','volume_down']]]}),
        validateTpl({id:'r',name:'R',sections:[['rocker',{horizontal:true,width:50},['Vol','volume_up','volume_down']]]})];`)();
const lines = acrossProbe[0].split('class="rmt-rocker-line"').length - 1;
const order = [...acrossProbe[0].matchAll(/data-action="([a-z_]+)"/g)].map(m => m[1]).join();
if (!acrossProbe[0].startsWith('<div class="rmt-section"><div class="rmt-rocker across">') || lines !== 2 ||
    order !== 'mute,volume_up,volume_down,home,channel_up,channel_down' ||
    !/<div class="rmt-rocker-side l"><div class="rmt-rocker-col rmt-rocker-solo"><button[^>]*data-action="mute"/.test(acrossProbe[0]) ||
    !/<div class="rmt-rocker-side r"><div class="rmt-rocker-col rmt-rocker-solo"><button[^>]*data-action="home"/.test(acrossProbe[0]) ||
    !/data-action="channel_up"[^>]*><svg viewBox="0 0 24 24"><path d="M8\.59 16\.59/.test(acrossProbe[0]) ||
    !acrossProbe[1].includes('<div class="rmt-rocker-side c"><div class="rmt-rocker-col rmt-rocker-solo">') ||
    !acrossProbe[2].includes('>Up</button>') || !/data-action="channel_down"[^>]*><svg viewBox="0 0 24 24"><path d="M7\.41 8\.59/.test(acrossProbe[2]) ||
    !/data-action="channel_up"[^>]*><svg viewBox="0 0 24 24"><path d="M7\.41 15\.41/.test(acrossProbe[3]) ||
    acrossProbe[3].includes('rmt-rocker-line') || !/horizontal" is true or false/.test(acrossProbe[4]) || acrossProbe[5] !== '') {
  throw new Error(`rocker across renders or validates incorrectly:\n${acrossProbe.join('\n')}`);
}
// A cross: its size and centre reach the style in range and are dropped outside
// it, the plate is drawn behind five keys in the d-pad's order, and the centre
// may not crowd out the arrows.
const crossProbe = new Function(`${js}
return [sectionHtml(['cross']), sectionHtml(['cross',{size:120,center:40},'up','left','play_pause','right','down']),
        sectionHtml(['cross',{size:9999,center:'40px'}]),
        validateTpl({id:'c',name:'C',sections:[['cross',{size:150,center:80}]]}),
        validateTpl({id:'c',name:'C',sections:[['cross',{size:150,center:81}]]}),
        validateTpl({id:'c',name:'C',sections:[['cross',{size:50}]]}),
        validateTpl({id:'c',name:'C',sections:[['cross',{radius:4}]]}),
        validateTpl({id:'c',name:'C',sections:[['cross','up','down']]})];`)();
const crossOrder = [...crossProbe[0].matchAll(/data-action="([a-z_]+)"/g)].map(m => m[1]).join();
if (!crossProbe[0].includes('<div class="rmt-cross"><svg class="rmt-cross-pl" viewBox="0 0 3 3"') ||
    crossOrder !== 'up,left,ok,right,down' || !crossProbe[0].includes('<span class="c"><button class="rmt-btn center"') ||
    !crossProbe[1].includes('<div class="rmt-cross" style="--rb-cross:120px;--rb-cross-c:40px">') ||
    !crossProbe[1].includes('<span class="c"><button class="rmt-btn media" data-action="play_pause"') ||
    !crossProbe[2].includes('<div class="rmt-cross"><svg') || !crossProbe[2].includes('data-action="up"') ||
    crossProbe[3] !== '' || !/leaves the arrows no room in a 150px cross — at most 80/.test(crossProbe[4]) ||
    !/cross "size" is 72-320/.test(crossProbe[5]) || !/A cross takes size and center/.test(crossProbe[6]) ||
    !/A cross section lists exactly 5 actions/.test(crossProbe[7])) {
  throw new Error(`cross renders or validates incorrectly:\n${crossProbe.join('\n')}`);
}
// A side section: its parts drawn in a row, one level deep only (a side inside a
// side and a divider draw nothing), and the walkers that read a style through
// it — icon names and the labels an @last line shows — reaching its parts.
const sideProbe = new Function(`${js}
return [sectionHtml(['side',['strip',['Vol','volume_up','volume_down']],['pot',{size:100}],['strip',['Ch','channel_up','channel_down']]]),
        sectionHtml(['side',['side',['row','mute']],['-'],['row','home']]),
        iconNames({sections:[['side',['row',['spare1','A','icon:logo']]]]}).join(),
        lcdLabel({sections:[['side',['strip',['Vol',['volume_up','Louder']]]]]},'volume_up'),
        validateTpl({id:'s',name:'S',sections:[['side',['row','home'],['side',['row','mute']]]]}),
        validateTpl({id:'s',name:'S',sections:[['side',['row','home'],['pot']]]}),
        sectionHtml(['side',{gap:30},['row','home'],['row','mute']]),
        sectionHtml(['side',{gap:999},['row','home'],['row','mute']]),
        validateTpl({id:'s',name:'S',sections:[['side',{gap:30},['row','home']]]}),
        validateTpl({id:'s',name:'S',sections:[['side',{gap:30},['row','home'],['pot']]]})];`)();
if (!sideProbe[0].startsWith('<div class="rmt-section"><div class="rmt-side"><div class="rmt-section"><div class="rmt-strip">') ||
    (sideProbe[0].match(/class="rmt-section"/g) || []).length !== 4 || !sideProbe[0].includes('data-up="volume_up"') ||
    !sideProbe[1].includes('data-action="home"') || sideProbe[1].includes('data-action="mute"') ||
    sideProbe[1].includes('rmt-divider') || sideProbe[2] !== 'logo' || sideProbe[3] !== 'Louder' ||
    !/another side/.test(sideProbe[4]) || sideProbe[5] !== '' ||
    !sideProbe[6].includes('<div class="rmt-side" style="gap:30px"><div class="rmt-section"><div class="rmt-row">') ||
    !sideProbe[7].includes('<div class="rmt-side"><div') || !/two or more/.test(sideProbe[8]) || sideProbe[9] !== '') {
  throw new Error(`side renders incorrectly:\n${sideProbe.join('\n')}`);
}
// Groups: drawn hidden, a stay one marked, a ✕ only on one that stays or covers
// the whole remote (full), a group key with its tooltip and the default icon, the walkers reaching into groups, and the refusals — an unknown
// group, a group's key inside another, a group key turned by a pot, a bad name
// and a stay that is not a boolean — beside a pot's middle, which may be one.
const groupProbe = new Function(`${js}
const t={id:'g',name:'G',sections:[['row',['group:apps','Apps'],'group:num']],
  groups:{apps:{sections:[['row',['spare1','Netflix','icon:logo']]]},num:{stay:true,sections:[['grid',{cols:3},'num1','num2']]},
    page:{full:true,sections:[['row','mute']]}}};
const one=s=>({id:'g',name:'G',sections:s,groups:{a:{sections:[['row','mute']]}}});
return [groupsHtml(t), sectionHtml(t.sections[0]), validateTpl(t),
  validateTpl({id:'g',name:'G',sections:[['row','group:nope']]}),
  validateTpl({id:'g',name:'G',sections:[['row','group:a']],groups:{a:{sections:[['row','group:a']]}}}),
  validateTpl(one([['pot','group:a','volume_down']])),
  validateTpl({id:'g',name:'G',sections:[['row','mute']],groups:{_x:{sections:[['row','mute']]}}}),
  validateTpl({id:'g',name:'G',sections:[['row','mute']],groups:{a:{stay:1,sections:[['row','mute']]}}}),
  iconNames(t).join(), lcdLabel(t,'spare1'), groupOf('group:apps'), groupOf('group:__proto__'),
  validateTpl(one([['pot','volume_up','volume_down','group:a']])), validateTpl(one([['slider',{set:'spare1'},'group:a']])),
  validateTpl({id:'g',name:'G',sections:[['row','mute']],groups:{a:{full:'yes',sections:[['row','mute']]}}})];`)();
if (!groupProbe[0].startsWith('<div class="rmt-pop" data-group="apps" role="dialog" aria-label="apps" hidden><div class="rmt-pop-in">') ||
    !groupProbe[0].includes('<div class="rmt-pop" data-group="num" data-stay="1" role="dialog" aria-label="num" hidden><button type="button" class="rmt-pop-x"') ||
    !groupProbe[0].includes('<div class="rmt-pop full" data-group="page" role="dialog" aria-label="page" hidden><button type="button" class="rmt-pop-x"') ||
    !groupProbe[0].includes('data-action="spare1"') || !groupProbe[0].includes('data-action="num2"') ||
    !groupProbe[1].includes('data-action="group:apps" title="Apps — opens group apps">Apps</button>') ||
    !/data-action="group:num" title="Opens group num"><svg viewBox="0 0 24 24"><path d="M4 8h4V4H4v4z/.test(groupProbe[1]) ||
    groupProbe[2] !== '' || !/No group "nope"/.test(groupProbe[3]) || !/cannot open another group/.test(groupProbe[4]) ||
    !/cannot turn by a group key/.test(groupProbe[5]) || !/Group names/.test(groupProbe[6]) ||
    !/true or false/.test(groupProbe[7]) || groupProbe[8] !== 'logo' || groupProbe[9] !== 'Netflix' ||
    groupProbe[10] !== 'apps' || groupProbe[11] !== '' || groupProbe[12] !== '' || !/cannot carry a group key/.test(groupProbe[13]) ||
    !/group "full" is true or false/.test(groupProbe[14])) {
  throw new Error(`groups render or validate incorrectly:\n${groupProbe.join('\n')}`);
}
const builtins = new Function(`${js}\nreturn RMT_BUILTIN.map(t=>t.id);`)();

const out = `// GENERATED FILE — do not edit by hand.
//
// Lifted from components/espidf_ble_keyboard/web_page.html so the Home
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

// Which copy of this file the browser actually loaded, taken from the ?v= the
// importer wrote rather than from a constant someone has to remember to bump.
// The card reports it beside its own: those two disagreeing is precisely the
// shape of a half-updated install — a new card still holding a cached catalogue
// — which otherwise surfaces as the card rejecting a style the device just
// exported. Hand-installed without a query string, it reads "unversioned".
export const RMT_VER =
  new URL(import.meta.url).searchParams.get('v') || 'unversioned';

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
