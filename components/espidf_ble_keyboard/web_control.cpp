#include "esphome/core/defines.h"
#ifdef USE_BLE_KEYBOARD_WEB_CONTROL

#include "web_control.h"
#include "espidf_ble_keyboard.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <span>

namespace esphome {
namespace espidf_ble_keyboard {

static const char *const TAG = "ble_kb_web";

// ── Embedded HTML/JS page ──────────────────────────────────────────
// Minified single-page app with keyboard + mouse control
static const char PAGE_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>BLE Keyboard &amp; Mouse</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#1a1e28;--fg:#e2e8f0;--card:#13161e;--border:#252a38;--muted:#7c8aad;--accent:#00d4aa;--active:#03a9f4;--caps:#ff9800;--name:var(--fg)}
body.light{--bg:#f0f2f5;--fg:#1a1e28;--card:#ffffff;--border:#d0d5dd;--muted:#7c8aad;--accent:#00875a;--active:#0288d1;--caps:#e65100;--name:var(--fg)}
body{background:var(--bg);color:var(--fg);font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:12px;max-width:640px;margin:0 auto;user-select:none;-webkit-user-select:none;transition:background .2s,color .2s}
h2{font-size:15px;font-weight:600;margin:12px 0 8px;color:var(--accent);display:flex;align-items:center;gap:6px}
h2 svg{width:18px;height:18px;fill:var(--accent)}
.toolbar{display:flex;align-items:center;justify-content:space-between;padding:8px 10px;margin-bottom:10px;background:var(--card);border:1px solid var(--border);border-radius:10px;flex-wrap:wrap;gap:6px;overflow:hidden}
.toolbar-left{display:flex;align-items:center;gap:8px}
.status-dot{width:10px;height:10px;border-radius:50%;background:var(--muted);transition:background .3s}
.status-dot.connected{background:var(--accent)}
.status-dot.paired{background:var(--active)}
.status-text{font-size:13px;color:var(--muted)}
.status-text.on{color:var(--accent)}
.dev-name{font-size:13px;color:var(--name);margin-left:4px;font-weight:500}
/* flex-grow so this reaches the right-hand card padding instead of stopping at its
   content width — the leftover is what the toggles below share out. */
.toolbar-right{display:flex;align-items:center;gap:6px;flex-wrap:wrap;flex-grow:1}
.zoom-controls{display:flex;align-items:center;gap:4px}
.zoom-btn,.theme-btn{width:30px;height:30px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--fg);font-size:17px;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;touch-action:manipulation}
.zoom-btn:active,.theme-btn:active{background:var(--active);color:#fff}
.theme-btn{font-size:15px}
.zoom-label{font-size:13px;color:var(--name);min-width:36px;text-align:center}
.card{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:10px;margin-bottom:12px}
.kb-header{display:flex;align-items:center;justify-content:space-between;margin:0 0 8px;gap:8px}
.kb-title{font-size:15px;font-weight:600;color:var(--accent)}
.layout-sel{padding:4px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--fg);font-size:12px;font-family:inherit;cursor:pointer}
.scalable{transform-origin:top center;transition:transform .15s}
.row{display:flex;gap:3px;margin-bottom:3px}
.row:last-child{margin-bottom:0}
.k{flex:1;min-width:0;padding:9px 1px;border:1px solid var(--border);border-radius:5px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .08s;outline:none;-webkit-tap-highlight-color:transparent;position:relative}
.k[data-al]::after{content:attr(data-al);position:absolute;bottom:1px;right:3px;font-size:10px;opacity:.55;pointer-events:none}
.k:focus,.k:focus-visible{outline:none}
.k:active,.k.p{background:var(--active);color:#fff;border-color:var(--active)}
.k.active{background:var(--active);color:#fff;border-color:var(--active)}
.k.caps{background:var(--caps);color:#fff;border-color:var(--caps)}
.k.shift-lock{background:var(--caps);color:#fff;border-color:var(--caps)}
.k.fk{font-size:10px;padding:6px 1px}
.k.kb-l-top{border-bottom-left-radius:0;border-bottom-right-radius:0;border-bottom-color:transparent;position:relative;z-index:1}
.k.kb-l-top::after{content:'';position:absolute;top:100%;left:-1px;right:-1px;height:5px;background:var(--bg);border-left:1px solid var(--border);border-right:1px solid var(--border);pointer-events:none}
.k.kb-l-top:active::after,.k.kb-l-top.p::after{background:var(--active);border-color:var(--active)}
.k.kb-l-bot{border-top-left-radius:0}
.touchpad{width:100%;aspect-ratio:16/9;background:var(--bg);border-radius:10px;border:2px solid var(--border);cursor:crosshair;touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none;position:relative;overflow:hidden;transition:border-color .15s}
.touchpad.active{border-color:var(--active)}
.touchpad-hint{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:var(--muted);font-size:12px;pointer-events:none;opacity:.5}
.touchpad.active .touchpad-hint{opacity:0}
.mbtn-row{display:grid;grid-template-columns:1fr .7fr 1fr;gap:6px;margin-top:8px}
.mbtn{padding:12px 0;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .1s}
.mbtn:active,.mbtn.p{background:var(--active);color:#fff;border-color:var(--active)}
.mbtn.held{background:var(--accent);color:#fff;border-color:var(--accent)}
.scroll-row{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:6px}
.sbtn{padding:8px 0;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:16px;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .1s}
.sbtn:active{background:var(--active);color:#fff;border-color:var(--active)}
.prog-btns{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.prog-btn{padding:8px 14px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;touch-action:manipulation;transition:background .1s}
.prog-btn:active,.prog-btn.p{background:var(--active);color:#fff;border-color:var(--active)}
.prog-empty{font-size:12px;color:var(--muted);padding:4px 0}
.macro-wrap{display:flex;align-items:center;gap:4px}
.macro-act{width:24px;height:24px;border:1px solid var(--border);border-radius:4px;background:var(--bg);color:var(--name);font-size:14px;cursor:pointer;display:flex;align-items:center;justify-content:center;flex-shrink:0}
.macro-act:active{background:var(--active);color:#fff}
.macro-act.del{color:#c44}
.macro-act.del:hover{background:#c44;color:#fff}
.macro-form{display:flex;gap:4px;margin-top:8px;flex-wrap:wrap;align-items:center}
.macro-form input,.macro-form select,.macro-form textarea{flex:1;min-width:60px;padding:6px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--fg);font-size:12px;font-family:inherit;resize:vertical}
/* Cap the select: it sizes to its widest <option>, so a long preset label
   would otherwise stretch the row and squeeze the action box beside it. */
.macro-form select{flex:0 1 auto;min-width:100px;max-width:150px}
/* The action is the field that actually needs room, in both editors — three shares
   against one for the name beside it. */
#ov-act,#ma{flex:3}
/* Both preset dropdowns to one fixed width so the two editors line up. The shared
   .macro-form select rule only *caps* them, so each was really sized by its longest
   option and they came out different. Kept off that rule because it also covers the
   combo-row key picker and #ov-slot, which need to size themselves. */
#mp,#ov-preset{flex:0 0 150px}
/* The host picker is the name of what this whole card acts on, so it overrides
   the 150px select cap and takes every pixel Forget Host leaves on its row.
   Widen it further by shrinking #ov-forget's fixed width, not by touching this. */
#ov-slot{flex:1 1 auto;max-width:none}
/* Fixed width (flex:0 0 <w>) so the button neither grows with the row nor
   shrinks when its label swaps to "Confirm?" mid-interaction. Change the 110px
   to resize it — that single number is the whole knob. Styled by id because
   .macro-form button would otherwise win on specificity and paint it accent. */
#ov-forget{flex:0 0 110px;padding:6px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:#c44;font-size:12px;font-weight:600;cursor:pointer;touch-action:manipulation;transition:background .15s;white-space:nowrap}
#ov-forget:active{background:#c44;color:#fff}
.macro-form button{padding:6px 12px;border:none;border-radius:6px;background:var(--accent);color:#fff;font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap}
.macro-form button:active{opacity:.8}
.macro-form .cancel{background:var(--muted)}
.combo-row{display:flex;gap:4px;width:100%;align-items:center;flex-wrap:wrap}
.mod-btn{padding:4px 8px;border:1px solid var(--border);border-radius:4px;background:var(--bg);color:var(--fg);font-size:11px;font-weight:600;cursor:pointer;user-select:none}
.mod-btn.on{background:var(--active);color:#fff;border-color:var(--active)}
.macro-edit-btn{margin-left:auto;padding:4px 11px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--name);font-size:11px;cursor:pointer}
.macro-edit-btn.on{background:var(--active);color:#fff;border-color:var(--active)}
/* Second button in a heading sits next to the first, not spaced apart —
   .macro-edit-btn's margin-left:auto would otherwise split the free space. */
#bk-load{margin-left:0}
.macros-card:not(.editing) .macro-act,.macros-card:not(.editing) .macro-form{display:none}
.ovr-row{display:flex;align-items:center;gap:6px;padding:5px 0;border-bottom:1px solid var(--border);font-size:12px}
.ovr-row:last-child{border-bottom:none}
.ovr-name{font-weight:600;color:var(--name);flex-shrink:0}
.ovr-act{color:var(--fg);font-family:ui-monospace,monospace;word-break:break-all;flex:1}
.ovr-tag{font-size:9px;font-weight:700;letter-spacing:.5px;padding:2px 5px;border-radius:4px;background:var(--bg);border:1px solid var(--border);color:var(--muted);flex-shrink:0}
/* Dangling macro: reference — the override will do nothing until it resolves. */
.ovr-warn{font-size:11px;line-height:1;padding:2px 4px;border-radius:4px;background:#c62828;color:#fff;flex-shrink:0;cursor:help}
.hid-panel{margin-top:10px;border-top:1px solid var(--border);padding-top:8px}
.hid-toggle{width:100%;text-align:left;padding:6px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--name);font-size:12px;font-weight:600;cursor:pointer}
.hid-panel:not(.open) .hid-body{display:none}
.hid-group{margin:8px 0}
.hid-group-name{font-size:10px;font-weight:700;letter-spacing:.5px;text-transform:uppercase;color:var(--muted);margin-bottom:4px}
.hid-items{display:flex;flex-wrap:wrap;gap:4px 12px}
.hid-item{display:flex;align-items:center;gap:5px;font-size:12px;color:var(--fg);min-width:120px;cursor:pointer}
.hid-item input{margin:0;cursor:pointer}
.rpt-times{display:flex;flex-wrap:wrap;gap:6px 16px;margin:8px 0}
.rpt-times label{display:flex;align-items:center;gap:5px;font-size:12px;color:var(--fg)}
.rpt-times input{width:70px;padding:4px 6px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:12px}
/* Grid, not flex, and deliberately so. Five fixed 1fr columns: the buttons divide the
   bar's full width between them, so a full row starts and ends on the same 10px padding
   as the keyboard keys and the touchpad, with no leftover piling up at either end. The
   5 is the whole knob — change it to fit more or fewer per line.
   minmax(0,1fr), not a bare 1fr, because 1fr means minmax(auto,1fr) and refuses to
   shrink below the widest MAC address; on a narrow phone that would push the row wider
   than the bar. The 0 lets tracks shrink and .host-btn's overflow:hidden clips instead.
   A short final row leaves its buttons one track wide, left-aligned under the row
   above — grid places them in the first columns rather than stretching them. */
.host-bar{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:6px;padding:8px 10px;margin-bottom:10px;background:var(--card);border:1px solid var(--border);border-radius:10px;overflow:hidden}
/* Width comes from the grid track above, so no flex/max-width here — a lone host on
   the last row is one track wide, it cannot stretch to fill the bar. */
.host-btn{padding:8px 4px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--fg);font-size:11px;font-weight:500;cursor:pointer;text-align:center;touch-action:manipulation;transition:background .15s;overflow:hidden}
.host-btn.active{background:var(--active);color:#fff;border-color:var(--active)}
.host-btn.occupied{border-color:var(--accent)}
.host-btn .slot-label{font-size:11px;color:var(--fg);display:block}
.host-btn.active .slot-label{color:rgba(255,255,255,.7)}
/* Deliberately NOT the host bar's 5-column grid: these are content-width labels, and
   forcing them into equal columns stretches short ones like Finder into slabs.
   flex-grow passes the toolbar's spare width down to the buttons instead. */
.section-toggles{display:flex;gap:4px;align-items:center;flex-wrap:wrap;touch-action:none;flex-grow:1}
/* flex-grow (not flex:1) keeps each label's natural width and only shares out what is
   left over, so they widen by a pixel or two each rather than becoming equal slabs.
   max-width stops the two or three on a wrapped last row from ballooning to fill it. */
.toggle-btn{flex-grow:1;max-width:150px;padding:4px 8px;border:1px solid var(--border);border-radius:6px;background:var(--bg);color:var(--name);font-size:11px;font-weight:500;cursor:pointer;touch-action:manipulation;transition:background .15s,color .15s;user-select:none;-webkit-user-select:none}
.toggle-btn.on{background:var(--active);color:#fff;border-color:var(--active)}
.toggle-btn.dragging{opacity:.5}
.toggle-btn.drag-over{box-shadow:inset 0 0 0 2px var(--accent)}
.rmt-section{margin-bottom:10px}
.rmt-section:last-child{margin-bottom:0}
.rmt-row{display:flex;justify-content:center;align-items:center;gap:8px;margin-bottom:8px}
.rmt-row:last-child{margin-bottom:0}
.rmt-btn{width:48px;height:48px;border:1px solid var(--border);border-radius:50%;background:var(--bg);color:var(--fg);font-size:12px;font-weight:500;cursor:pointer;touch-action:manipulation;display:flex;align-items:center;justify-content:center;transition:background .1s,transform .1s;user-select:none;-webkit-user-select:none}
.rmt-btn:active,.rmt-btn.p{background:var(--active);color:#fff;border-color:var(--active);transform:scale(.93)}
.rmt-btn svg{width:20px;height:20px;fill:currentColor;pointer-events:none}
.rmt-btn.power{background:#c62828;color:#fff;border-color:#c62828}
.rmt-btn.power:active,.rmt-btn.power.p{background:#e53935}
/* Currently held down on the host (press and hold). Icons use currentColor. */
.rmt-btn.held{background:var(--accent);color:#fff;border-color:var(--accent)}
.rmt-dpad{display:grid;grid-template-columns:48px 48px 48px;grid-template-rows:48px 48px 48px;gap:4px;justify-content:center;margin:8px 0}
.rmt-dpad .rmt-btn{border-radius:12px}
.rmt-dpad .center{background:var(--active);color:#fff;border-color:var(--active);font-size:11px;font-weight:700;border-radius:50%}
.rmt-dpad .center:active{background:var(--accent)}
.rmt-dpad .empty{visibility:hidden}
.rmt-strip{display:flex;align-items:center;justify-content:center;gap:16px}
.rmt-strip-group{display:flex;flex-direction:column;align-items:center;gap:4px}
.rmt-strip-label{font-size:10px;color:var(--muted);font-weight:600;text-transform:uppercase}
.rmt-divider{height:1px;background:var(--border);margin:10px 0}
.rmt-media-row{display:flex;justify-content:center;gap:8px}
.rmt-btn.media{width:42px;height:42px}
.rmt-btn.rec{background:#c62828;color:#fff;border-color:#c62828}
.rmt-btn.rec:active,.rmt-btn.rec.p{background:#e53935}
.rmt-btn.col{width:44px;height:44px;border:none}
.rmt-btn.col:active,.rmt-btn.col.p{transform:scale(.93);filter:brightness(1.25)}
.rmt-btn.red{background:#e53935}
.rmt-btn.green{background:#43a047}
.rmt-btn.yellow{background:#fdd835}
.rmt-btn.blue{background:#1e88e5}
.rmt-app-row{display:flex;justify-content:center;gap:8px;flex-wrap:wrap}
.rmt-btn.app{width:auto;height:38px;border-radius:19px;padding:0 14px;font-size:11px}
</style></head><body>

<div id="scalable" class="scalable">
<div class="toolbar">
<div class="toolbar-left">
<div class="status-dot" id="sdot"></div>
<span class="status-text" id="stxt">Disconnected</span>
<span class="dev-name" id="dname"></span>
<!-- Carries the release tag, and `-dev` while main is ahead of the last one. Drop
     the suffix when tagging; append a letter (v1.7.0-dev-b) to tell two dev builds
     apart when chasing a "my edit didn't reach the device" problem. -->
<span id="webver" style="font-size:11px;color:var(--muted);margin-left:6px;letter-spacing:.3px">v1.7.0-dev</span>
</div>
<div class="toolbar-right">
<div class="section-toggles" id="toggle-bar">
<button class="toggle-btn on" data-section="keyboard">Keyboard</button>
<button class="toggle-btn on" data-section="mouse-card">Mouse</button>
<button class="toggle-btn on" data-section="finder-card">Finder</button>
<button class="toggle-btn on" data-section="media-card">Remote</button>
<button class="toggle-btn on" data-section="btns-card">Buttons</button>
<button class="toggle-btn on" data-section="macros-card">Macros</button>
<button class="toggle-btn on" data-section="hostact-card">Host Actions</button>
</div>
<div class="zoom-controls">
<button class="zoom-btn" id="zout">-</button>
<span class="zoom-label" id="zlbl">100%</span>
<button class="zoom-btn" id="zin">+</button>
</div>
<button class="theme-btn" id="thm" title="Toggle light/dark">&#9788;</button>
</div>
</div>

<div class="host-bar" id="host-bar" style="display:none"></div>
<div class="card" id="keyboard">
<div class="kb-header"><span class="kb-title">Keyboard</span><select id="layoutSel" class="layout-sel"></select></div>
<div id="kb-rows"></div>
</div>

<div class="card" id="mouse-card">
<h2><svg viewBox="0 0 24 24"><path d="M12 2C8.14 2 5 5.14 5 9v6c0 3.86 3.14 7 7 7s7-3.14 7-7V9c0-3.86-3.14-7-7-7zm0 2c2.76 0 5 2.24 5 5v2h-4V5h-2v6H7V9c0-2.76 2.24-5 5-5z"/></svg>Mouse</h2>
<div class="touchpad" id="touchpad"><span class="touchpad-hint">Drag to move</span></div>
<div class="mbtn-row">
<button class="mbtn" id="ml">Left</button>
<button class="mbtn" id="mm">Middle</button>
<button class="mbtn" id="mr">Right</button>
</div>
<div style="font-size:11px;color:var(--muted);margin-top:6px;text-align:center">Long-press a button to hold (drag) &middot; tap it again to release</div>
<div class="scroll-row">
<button class="sbtn" id="su">&#9650; Up</button>
<button class="sbtn" id="sd">&#9660; Down</button>
</div>
</div>

<div class="card" id="finder-card">
<h2><svg viewBox="0 0 24 24"><path d="M11 2h2v6h-2zM2 11h6v2H2zm14 0h6v2h-6zm-5 5h2v6h-2zM7 7l3 3-1.4 1.4L5.6 8.4zm10 0l1.4 1.4-3 3L14 11zm0 10l-3-3 1.4-1.4 3 3zM7 17l-1.4-1.4 3-3L10 14z"/></svg>Position Finder<button class="macro-edit-btn" id="finder-edit-toggle">Edit</button></h2>
<div style="font-size:12px;color:var(--muted);margin-bottom:8px">Locked by default so a stray tap can't move the cursor &mdash; click <strong>Edit</strong> to aim &amp; calibrate. Then press &amp; drag the map to move the cursor; copy the <code>mouse_goto</code> value into a button/macro.</div>
<div id="finder-map" style="position:relative;width:100%;background:var(--card);border:1px solid var(--border);border-radius:8px;overflow:hidden;cursor:crosshair;touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none"></div>
<div style="display:flex;gap:8px;align-items:center;margin-top:10px;flex-wrap:wrap">
<code id="finder-val" style="font-size:14px;padding:5px 10px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg)">mouse_goto:0:0</code>
<button class="mbtn" id="finder-copy" style="flex:0 0 auto">Copy</button>
<span id="finder-info" style="font-size:12px;color:var(--muted)"></span>
</div>
<div id="finder-edit-section">
<div style="display:flex;gap:6px;align-items:center;margin-top:8px;flex-wrap:wrap">
<label style="font-size:12px;color:var(--muted)">nudge&nbsp;target</label>
<label style="font-size:12px;color:var(--muted)">X</label>
<button id="gx-dn" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">&minus;</button>
<button id="gx-up" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">+</button>
<label style="font-size:12px;color:var(--muted)">Y</label>
<button id="gy-dn" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">&minus;</button>
<button id="gy-up" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">+</button>
<span style="font-size:11px;color:var(--muted)">moves the cursor &amp; updates the value (1&nbsp;px steps)</span>
</div>
<div style="display:flex;gap:8px;align-items:center;margin-top:10px;flex-wrap:wrap">
<label style="font-size:12px;color:var(--muted)">goto scale (live)</label>
<label style="font-size:12px;color:var(--muted)">X</label>
<button id="sx-dn" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">&minus;</button>
<input id="finder-scale-x" type="number" step="0.0001" min="0.05" max="20" style="width:80px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:14px">
<button id="sx-up" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">+</button>
<label style="font-size:12px;color:var(--muted)">Y</label>
<button id="sy-dn" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">&minus;</button>
<input id="finder-scale-y" type="number" step="0.0001" min="0.05" max="20" style="width:80px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:14px">
<button id="sy-up" style="flex:0 0 auto;min-width:32px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:16px;font-weight:700;cursor:pointer">+</button>
<button id="sc-reset" style="flex:0 0 auto;padding:5px 10px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:13px;cursor:pointer">Reset</button>
<span style="font-size:11px;color:var(--muted)">&plusmn; nudges 0.0001 (4 dp) · Reset = YAML default · saved per host (also YAML <code>mouse_goto_scale_x</code> / <code>_y</code>)</span>
</div>
<div style="display:flex;gap:8px;align-items:center;margin-top:10px;flex-wrap:wrap">
<label style="font-size:12px;color:var(--muted)">landed&nbsp;at</label>
<input id="finder-act-x" type="number" placeholder="actual X" style="width:90px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:14px">
<input id="finder-act-y" type="number" placeholder="actual Y" style="width:90px;padding:5px 8px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--fg);font-size:14px">
<button class="mbtn" id="finder-cal" style="flex:0 0 auto">Auto‑calibrate</button>
<span id="finder-cal-info" style="font-size:11px;color:var(--muted)">send to a target, then enter where it REALLY landed (read with cursorpos.bat) → computes X/Y scale</span>
</div>
</div>
</div>

<div class="card" id="media-card">
<h2><svg viewBox="0 0 24 24"><path d="M21 3H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14zM5 15h2v2H5v-2zm4 0h2v2H9v-2zm4 0h2v2h-2v-2z"/></svg>Remote</h2>
<div class="rmt-section"><div class="rmt-row">
<button class="rmt-btn power" data-action="remote_power" title="Power"><svg viewBox="0 0 24 24"><path d="M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z"/></svg></button>
<div style="flex:1"></div>
<button class="rmt-btn" data-action="search" title="Search"><svg viewBox="0 0 24 24"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg></button>
<button class="rmt-btn" data-action="info" title="Info"><svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg></button>
<button class="rmt-btn" data-action="mute" title="Mute"><svg viewBox="0 0 24 24"><path d="M16.5 12c0-1.77-1.02-3.29-2.5-4.03v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51C20.63 14.91 21 13.5 21 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06c1.38-.31 2.63-.95 3.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></svg></button>
<button class="rmt-btn" data-action="home" title="Home"><svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg></button>
<button class="rmt-btn" data-action="back" title="Back"><svg viewBox="0 0 24 24"><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button>
</div></div>
<div class="rmt-section"><div class="rmt-dpad">
<div class="empty"></div>
<button class="rmt-btn" data-action="up" data-repeat="1" title="Up"><svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg></button>
<div class="empty"></div>
<button class="rmt-btn" data-action="left" data-repeat="1" title="Left"><svg viewBox="0 0 24 24"><path d="M15.41 16.59L10.83 12l4.58-4.59L14 6l-6 6 6 6z"/></svg></button>
<button class="rmt-btn center" data-action="ok" title="OK">OK</button>
<button class="rmt-btn" data-action="right" data-repeat="1" title="Right"><svg viewBox="0 0 24 24"><path d="M8.59 16.59L13.17 12 8.59 7.41 10 6l6 6-6 6z"/></svg></button>
<div class="empty"></div>
<button class="rmt-btn" data-action="down" data-repeat="1" title="Down"><svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg></button>
<div class="empty"></div>
</div></div>
<div class="rmt-section"><div class="rmt-strip">
<div class="rmt-strip-group">
<span class="rmt-strip-label">Vol</span>
<button class="rmt-btn" data-action="volume_up" data-repeat="1" title="Volume Up"><svg viewBox="0 0 24 24"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></svg></button>
<button class="rmt-btn" data-action="volume_down" data-repeat="1" title="Volume Down"><svg viewBox="0 0 24 24"><path d="M19 13H5v-2h14v2z"/></svg></button>
</div>
<div style="width:40px"></div>
<div class="rmt-strip-group">
<span class="rmt-strip-label">Ch</span>
<button class="rmt-btn" data-action="channel_up" data-repeat="1" title="Channel Up"><svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg></button>
<button class="rmt-btn" data-action="channel_down" data-repeat="1" title="Channel Down"><svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg></button>
</div>
</div></div>
<div class="rmt-divider"></div>
<div class="rmt-section"><div class="rmt-media-row">
<button class="rmt-btn media" data-action="prev_track" title="Previous"><svg viewBox="0 0 24 24"><path d="M6 6h2v12H6zm3.5 6l8.5 6V6z"/></svg></button>
<button class="rmt-btn media" data-action="rewind" data-repeat="1" title="Rewind"><svg viewBox="0 0 24 24"><path d="M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"/></svg></button>
<button class="rmt-btn media" data-action="play_pause" title="Play/Pause"><svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg></button>
<button class="rmt-btn media" data-action="stop" title="Stop"><svg viewBox="0 0 24 24"><path d="M6 6h12v12H6z"/></svg></button>
<button class="rmt-btn media" data-action="fast_forward" data-repeat="1" title="Fast Forward"><svg viewBox="0 0 24 24"><path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/></svg></button>
<button class="rmt-btn media" data-action="next_track" title="Next"><svg viewBox="0 0 24 24"><path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/></svg></button>
<button class="rmt-btn media rec" data-action="record" title="Record"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="7"/></svg></button>
</div></div>
<div class="rmt-divider"></div>
<div class="rmt-section"><div class="rmt-media-row">
<button class="rmt-btn col red" data-action="color_red" title="Red (F1)"></button>
<button class="rmt-btn col green" data-action="color_green" title="Green (F2)"></button>
<button class="rmt-btn col yellow" data-action="color_yellow" title="Yellow (F3)"></button>
<button class="rmt-btn col blue" data-action="color_blue" title="Blue (F4)"></button>
</div></div>
<div class="rmt-divider"></div>
<div class="rmt-section"><div class="rmt-app-row">
<button class="rmt-btn app" data-action="app_explorer" title="File Explorer">Explorer</button>
<button class="rmt-btn app" data-action="app_browser" title="Web Browser">Browser</button>
<button class="rmt-btn app" data-action="app_email" title="Email Client">Email</button>
<button class="rmt-btn app" data-action="app_calc" title="Calculator">Calc</button>
<button class="rmt-btn app" data-action="search" title="Search (same key as the magnifier above)">Search</button>
</div></div>
</div>

<div class="card" id="btns-card">
<h2><svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H5V5h14v14zM7 12h2v2H7v-2zm0-4h2v2H7V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8z"/></svg>Buttons</h2>
<div class="prog-btns" id="prog-btns"><span class="prog-empty">Loading...</span></div>
</div>

<div class="card macros-card" id="macros-card">
<h2><svg viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H5V5h14v14zM7 12h2v2H7v-2zm0-4h2v2H7V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8zm4 4h2v2h-2v-2zm0-4h2v2h-2V8z"/></svg>Macros<button class="macro-edit-btn" id="macro-edit-toggle">Edit</button></h2>
<div class="prog-btns" id="macro-btns"><span class="prog-empty">Loading...</span></div>
<div class="macro-form" id="macro-form">
<input id="mn" placeholder="Name" maxlength="31" title="Must be unique and cannot contain '|'.&#10;Host actions reference a macro by name (macro:&lt;name&gt;), so renaming one breaks any override pointing at it.">
<textarea id="ma" placeholder="Action (use | to chain steps)" maxlength="255" rows="3"></textarea>
<select id="mp"><option value="">Preset...</option>
<optgroup label="Media">
<option value="play_pause">Play/Pause</option>
<option value="next_track">Next Track</option>
<option value="prev_track">Prev Track</option>
<option value="stop">Stop</option>
<option value="record">Record</option>
<option value="rewind">Rewind</option>
<option value="fast_forward">Fast Forward</option>
<option value="volume_up">Volume Up</option>
<option value="volume_down">Volume Down</option>
<option value="mute">Mute</option>
</optgroup>
<optgroup label="Remote">
<option value="remote_power">Power</option>
<option value="up">Up</option>
<option value="down">Down</option>
<option value="left">Left</option>
<option value="right">Right</option>
<option value="ok">OK</option>
<option value="home">Home</option>
<option value="back">Back</option>
<option value="search">Search</option>
<option value="info">Info</option>
<option value="channel_up">Channel Up</option>
<option value="channel_down">Channel Down</option>
<option value="color_red">Colour Red (F1)</option>
<option value="color_green">Colour Green (F2)</option>
<option value="color_yellow">Colour Yellow (F3)</option>
<option value="color_blue">Colour Blue (F4)</option>
<option value="app_explorer">App: Explorer</option>
<option value="app_browser">App: Browser</option>
<option value="app_email">App: Email</option>
<option value="app_calc">App: Calculator</option>
</optgroup>
<optgroup label="System">
<option value="ctrl_alt_del">Ctrl+Alt+Del</option>
<option value="power">Power</option>
<option value="sleep">Sleep</option>
<option value="shutdown">Shutdown</option>
<option value="hibernate">Hibernate</option>
</optgroup>
<optgroup label="Clipboard">
<option value="combo:2:4">Ctrl+A</option>
<option value="combo:2:6">Ctrl+C</option>
<option value="combo:2:25">Ctrl+V</option>
<option value="combo:2:29">Ctrl+Z</option>
</optgroup>
<optgroup label="Consumer HID">
<option value="consumer:0x0030">HID Power</option>
<option value="consumer:0x0223">HID Home</option>
<option value="consumer:0x0224">HID Back</option>
<option value="consumer:0x0221">HID Search</option>
</optgroup>
<optgroup label="Mouse">
<option value="left_click">Left Click</option>
<option value="right_click">Right Click</option>
<option value="middle_click">Middle Click</option>
<option value="left_click_hold">Hold Left Button (drag)</option>
<option value="right_click_hold">Hold Right Button</option>
<option value="middle_click_hold">Hold Middle Button</option>
<option value="mouse_release">Release Mouse Buttons</option>
<option value="mouse_move:0:-10">Move Cursor (edit x:y)</option>
<option value="mouse_scroll:1">Scroll Up</option>
<option value="mouse_scroll:-1">Scroll Down</option>
</optgroup>
<optgroup label="Cursor Position">
<option value="mouse_abs:0:0">Cursor Top-Left</option>
<option value="mouse_abs:50:50">Cursor Center</option>
<option value="mouse_abs:100:100">Cursor Bottom-Right</option>
<option value="mouse_abs_px:960:540">Cursor to px (edit x:y)</option>
<option value="mouse_abs_mon:0:50:50">Cursor in Monitor (edit)</option>
<option value="mouse_goto:0:0">Goto desktop px — all monitors (edit x:y)</option>
<option value="mouse_abs_save">Save Cursor Pos</option>
<option value="mouse_abs_restore">Restore Cursor Pos</option>
</optgroup>
<optgroup label="Other">
<option value="send_custom_text">Send Custom Text</option>
<option value="string:">Type Text</option>
<option value="delay:100">Delay 100ms</option>
<option value="delay:500">Delay 500ms</option>
<option value="delay:1000">Delay 1000ms</option>
<option value="repeat:5:">Repeat 5&times; (edit count)</option>
<option value="__br__">&mdash; New branch (||) &mdash;</option>
<option value="__alt__">Alternate (one per press)</option>
</optgroup>
</select>
<button id="macro-save">Save</button>
<div class="combo-row">
<button class="mod-btn" data-mod="1">Ctrl</button>
<button class="mod-btn" data-mod="2">Shift</button>
<button class="mod-btn" data-mod="4">Alt</button>
<button class="mod-btn" data-mod="8">Win</button>
<select id="mk"><option value="">+ Key...</option>
<optgroup label="F Keys">
<option value="58">F1</option><option value="59">F2</option><option value="60">F3</option><option value="61">F4</option>
<option value="62">F5</option><option value="63">F6</option><option value="64">F7</option><option value="65">F8</option>
<option value="66">F9</option><option value="67">F10</option><option value="68">F11</option><option value="69">F12</option>
</optgroup>
<optgroup label="Navigation">
<option value="74">Home</option><option value="75">PgUp</option><option value="77">End</option><option value="78">PgDn</option>
<option value="80">Left</option><option value="81">Down</option><option value="82">Up</option><option value="79">Right</option>
</optgroup>
<optgroup label="Editing">
<option value="40">Enter</option><option value="41">Esc</option><option value="42">Backspace</option><option value="43">Tab</option>
<option value="44">Space</option><option value="73">Insert</option><option value="76">Delete</option><option value="70">PrtSc</option>
<option value="71">ScrLk</option><option value="72">Pause</option>
</optgroup>
<optgroup label="Letters">
<option value="4">A</option><option value="5">B</option><option value="6">C</option><option value="7">D</option>
<option value="8">E</option><option value="9">F</option><option value="10">G</option><option value="11">H</option>
<option value="12">I</option><option value="13">J</option><option value="14">K</option><option value="15">L</option>
<option value="16">M</option><option value="17">N</option><option value="18">O</option><option value="19">P</option>
<option value="20">Q</option><option value="21">R</option><option value="22">S</option><option value="23">T</option>
<option value="24">U</option><option value="25">V</option><option value="26">W</option><option value="27">X</option>
<option value="28">Y</option><option value="29">Z</option>
</optgroup>
<optgroup label="Numbers">
<option value="30">1</option><option value="31">2</option><option value="32">3</option><option value="33">4</option><option value="34">5</option>
<option value="35">6</option><option value="36">7</option><option value="37">8</option><option value="38">9</option><option value="39">0</option>
</optgroup>
</select>
</div>
</div>
</div>

<div class="card" id="hostact-card">
<h2><svg viewBox="0 0 24 24"><path d="M20 18c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2H4c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2H0v2h24v-2h-4zM4 6h16v10H4V6z"/></svg>Host Actions<button class="macro-edit-btn" id="bk-save" title="Download all settings as a JSON file">Backup</button><button class="macro-edit-btn" id="bk-load" title="Restore settings from a backup file">Restore</button></h2>
<input type="file" id="bk-file" accept="application/json,.json" style="display:none">
<div id="bk-status" style="font-size:11px;color:var(--muted);margin-bottom:8px"></div>
<div style="font-size:12px;color:var(--muted);margin-bottom:8px">Remap a <strong>named</strong> action for one host, so the same button suits each machine &mdash; e.g. <code>record</code> &rarr; <code>combo:0x0C:0x15</code> (Game Bar) on a Windows host, while a TV host keeps HID Record. Saved per host; YAML rows come from your config and are restored when you delete the override on top.</div>
<div class="macro-form" style="margin-top:0;margin-bottom:8px">
<select id="ov-slot"></select>
<button id="ov-forget" title="Remove the BLE bond for the selected host slot and clear its stored address.&#10;Disconnects it if it is the active host.&#10;&#10;Tap once to arm, tap again to confirm.">Forget Host</button>
</div>
<div id="ov-list"><span class="prog-empty">Loading...</span></div>
<div class="macro-form">
<input id="ov-name" list="ov-names" placeholder="Action name (e.g. record)" maxlength="31" title="The named action to remap on this host &mdash; start typing to pick from the list.&#10;Only named actions can be overridden; parametric forms like combo: and consumer: always mean exactly what they say.">
<datalist id="ov-names">
<option value="record"><option value="play_pause"><option value="stop"><option value="next_track"><option value="prev_track">
<option value="rewind"><option value="fast_forward"><option value="volume_up"><option value="volume_down"><option value="mute">
<option value="remote_power"><option value="up"><option value="down"><option value="left"><option value="right"><option value="ok">
<option value="home"><option value="back"><option value="search"><option value="info"><option value="channel_up"><option value="channel_down">
<option value="color_red"><option value="color_green"><option value="color_yellow"><option value="color_blue">
<option value="app_explorer"><option value="app_browser"><option value="app_email"><option value="app_calc">
<option value="ctrl_alt_del"><option value="power"><option value="sleep"><option value="shutdown"><option value="hibernate">
<option value="left_click"><option value="right_click"><option value="middle_click"><option value="mouse_release">
</datalist>
<textarea id="ov-act" placeholder="Replacement action" maxlength="255" rows="3" title="What this action should do on the selected host.&#10;&#10;Examples:&#10;combo:0x0C:0x15 = Win+Alt+R (Game Bar record)&#10;combo:0x01:0x15 = Ctrl+R (Sound Recorder, needs focus)&#10;consumer:0x00B2 = raw HID usage&#10;play_pause = another named action&#10;string:hello = type text&#10;&#10;Chain steps with a | between them:&#10;combo:0x0C:0x15 | delay:500 | string:rec&#10;&#10;Modifier bits add up: 1=Ctrl, 2=Shift, 4=Alt, 8=Win&#10;so Win+Alt = 12 = 0x0C. Decimal or 0x hex both work."></textarea>
<select id="ov-preset"><option value="">Preset...</option></select>
<button id="ov-save">Save</button>
</div>
<div class="hid-panel" id="hid-panel">
<button class="hid-toggle" id="hid-toggle">Remote buttons &#9662;</button>
<div class="hid-body" id="hid-body">
<div style="font-size:12px;color:var(--muted);margin:6px 0">Untick a button to remove it from the remote on this host. The action still works from macros, buttons and Home Assistant &mdash; only the button goes away.</div>
<div id="hid-list"></div>
<div class="macro-form"><button id="hid-save">Save buttons</button><button id="hid-all" class="cancel">All</button><button id="hid-none" class="cancel">None</button></div>
</div>
</div>
<div class="hid-panel" id="rpt-panel">
<button class="hid-toggle" id="rpt-toggle">Hold to repeat &#9662;</button>
<div class="hid-body" id="rpt-body">
<div style="font-size:12px;color:var(--muted);margin:6px 0">Tick a button to make it fire again and again while you hold it on this host &mdash; handy for volume and menu scrolling. A quick tap still sends exactly one press. Saved per host; <strong>Reset</strong> returns to the defaults (D-pad, volume, channel, rewind and fast forward).</div>
<div class="rpt-times">
<label>Start after <input id="rpt-delay" type="number" min="100" max="2000" step="50" value="400" title="How long a button must be held before it starts repeating."> ms</label>
<label>Repeat every <input id="rpt-rate" type="number" min="50" max="2000" step="10" value="180" title="Time between repeats while held. Values below 50ms are raised to 50 &mdash; identical presses closer together than 30ms are dropped by the device's duplicate guard."> ms</label>
</div>
<div id="rpt-list"></div>
<div class="macro-form"><button id="rpt-save">Save repeat</button><button id="rpt-all" class="cancel">All</button><button id="rpt-none" class="cancel">None</button><button id="rpt-reset" class="cancel">Reset</button></div>
</div>
</div>
<div class="hid-panel" id="hld-panel">
<button class="hid-toggle" id="hld-toggle">Press and hold &#9662;</button>
<div class="hid-body" id="hld-body">
<div style="font-size:12px;color:var(--muted);margin:6px 0">Tick a button to keep it <strong>held down</strong> on this host for as long as you hold it here, instead of sending a quick tap &mdash; what push-to-talk needs. Saved per host. A button already set to repeat is greyed out: repeating and holding are the same gesture, so a button can only do one of them.</div>
<div id="hld-list"></div>
<div class="macro-form"><button id="hld-save">Save hold</button><button id="hld-none" class="cancel">None</button></div>
</div>
</div>
</div>
</div>

<script>
// API helper
function api(endpoint,params){
  const url='/api/ble_keyboard/'+endpoint+'?'+new URLSearchParams(params);
  fetch(url,{method:'POST'}).catch(()=>{});
}

// Tap helper: visual feedback on press, fires action on release only if finger didn't scroll
function onTap(el,fn,cls){
  cls=cls||'p';
  let sx,sy,ok;
  el.addEventListener('pointerdown',e=>{sx=e.clientX;sy=e.clientY;ok=true;el.classList.add(cls)});
  el.addEventListener('pointermove',e=>{if(ok&&(Math.abs(e.clientX-sx)+Math.abs(e.clientY-sy))>10){ok=false;el.classList.remove(cls)}});
  el.addEventListener('pointerup',e=>{el.classList.remove(cls);if(ok){ok=false;fn(e)}});
  el.addEventListener('pointercancel',()=>{ok=false;el.classList.remove(cls)});
}

// ── Theme ──
const thmBtn=document.getElementById('thm');
if(localStorage.getItem('blekb_theme')==='light')document.body.classList.add('light');
function toggleTheme(){
  document.body.classList.toggle('light');
  localStorage.setItem('blekb_theme',document.body.classList.contains('light')?'light':'dark');
}
thmBtn.addEventListener('click',toggleTheme);

// ── Zoom ──
let zoom=100;
const scalable=document.getElementById('scalable');
const zlbl=document.getElementById('zlbl');
function setZoom(v){zoom=Math.max(50,Math.min(200,v));scalable.style.transform='scale('+(zoom/100)+')';zlbl.textContent=zoom+'%';localStorage.setItem('blekb_zoom',zoom)}
document.getElementById('zin').addEventListener('click',()=>setZoom(zoom+5));
document.getElementById('zout').addEventListener('click',()=>setZoom(zoom-5));
setZoom(parseInt(localStorage.getItem('blekb_zoom'))||100);

// ── Status polling ──
const sdot=document.getElementById('sdot');
const stxt=document.getElementById('stxt');
const dname=document.getElementById('dname');
function pollStatus(){
  fetch('/api/ble_keyboard/status').then(r=>r.json()).then(d=>{
    const c=d.connected,p=d.paired;
    sdot.className='status-dot'+(p?' paired':c?' connected':'');
    stxt.className='status-text'+((c||p)?' on':'');
    stxt.textContent=p?'Paired':c?'Connected':'Disconnected';
    if(d.device_name){dname.textContent='('+d.device_name+')';document.title=d.device_name+' — BLE Control'}
  }).catch(()=>{
    sdot.className='status-dot';
    stxt.className='status-text';
    stxt.textContent='Offline';
  });
}
pollStatus();
setInterval(pollStatus,3000);

// ── Host Switcher ──
(function(){
  const bar=document.getElementById('host-bar');
  let lastHostsRaw=null;
  function loadHosts(){
    fetch('/api/ble_keyboard/hosts').then(r=>r.text()).then(txt=>{
      if(txt===lastHostsRaw)return;
      lastHostsRaw=txt;
      const d=JSON.parse(txt);
      // Before the single-slot early return below: a one-host device still has
      // a hidden set and a repeat set to apply, and the active slot may have
      // changed.
      if(window.applyHidden)window.applyHidden(false);
      if(window.applyRepeat)window.applyRepeat(false);
      if(window.applyHold)window.applyHold(false);
      if(!d.slots||d.slots.length<=1){bar.style.display='none';return}
      bar.style.display='';
      bar.innerHTML='';
      d.slots.forEach(s=>{
        const b=document.createElement('button');
        b.className='host-btn'+(s.slot===d.active?' active':'')+(s.occupied?' occupied':'');
        // Prefer the stable identity so this matches the host MAC sensor and the
        // address the phone shows for itself; s.addr rotates on Android.
        b.innerHTML='<span class="slot-label">'+(s.name||('Host '+(s.slot+1)))+'</span>'+(s.occupied?(s.identity||s.addr):'Empty');
        onTap(b,()=>{
          api('switch_host',{slot:s.slot});
          bar.querySelectorAll('.host-btn').forEach(x=>x.classList.remove('active'));
          b.classList.add('active');
        },'active');
        bar.appendChild(b);
      });
    }).catch(()=>{bar.style.display='none'});
  }
  loadHosts();
  setInterval(loadHosts,5000);
})();

// `alternate:` owns the whole chain (it splits the '|' itself), so its preset
// wraps what's already in the field instead of being appended like every other
// one. Build the steps normally, then pick it to turn them into a toggle.
// Shared: both the Macros form and Host Actions use it.
function wrapAlternate(el){
  const cur=el.value.trim();
  if(cur.indexOf('alternate:')===0)return;   // already one
  el.value=cur?('alternate:'+cur):'alternate:';
}

// Start the next alternate branch. No-op on an empty field or one already
// ending in a separator, so picking it twice can't produce '|| ||'.
function newBranch(el){
  const cur=el.value.trim();
  if(!cur||cur.charAt(cur.length-1)==='|')return;
  el.value=cur+' || ';
}

// Macro names, for spotting host overrides that point at a macro that no longer
// exists. Null until the first /buttons load lands — flagging before then would
// mark every reference as missing.
let knownMacroNames=null;
// Set by the Host Actions card so a macro rename/delete can re-flag its rows
// without a page reload.
let reflagOverrides=null;

// Pull every macro reference out of an action string, mirroring the firmware's
// own parse order (repeat: and alternate: are read before the '|' split, and a
// step is only a reference if it *starts* with macro:). Matching the parser
// rather than pattern-matching the text is what keeps this honest: it finds
// nested refs like 'repeat:3:macro:X' and ignores literal text that merely
// contains 'macro:', such as 'string:run macro:later'.
function macroRefs(action,out){
  out=out||[];
  const a=String(action||'').trim();
  if(!a)return out;
  if(a.indexOf('repeat:')===0){
    const sep=a.indexOf(':',7);
    if(sep!==-1)macroRefs(a.substring(sep+1),out);
    return out;
  }
  if(a.indexOf('alternate:')===0){
    a.substring(10).split('||').forEach(b=>macroRefs(b,out));
    return out;
  }
  if(a.indexOf('|')!==-1){
    a.split('|').forEach(s=>macroRefs(s,out));
    return out;
  }
  if(a.indexOf('macro:')===0){
    const n=a.substring(6).trim();
    if(n&&out.indexOf(n)===-1)out.push(n);
  }
  return out;
}

// Presets join with ' | ' (next step), with two exceptions driven by the last
// character already in the box:
//   '|'  a separator is already there — '… || foo', not '… || | foo'
//   ':'  the value is a prefix awaiting its body ('repeat:3:', 'string:'), and
//        those delimit with ':'. Joining with '|' would give 'repeat:3: | foo',
//        which parses as repeating 'foo' minus its own prefix — silently wrong.
function appendStep(el,val){
  const cur=el.value.trim();
  if(!cur){el.value=val;return}
  const last=cur.charAt(cur.length-1);
  el.value=cur+(last==='|'?' ':last===':'?'':' | ')+val;
}

// ── Buttons & Macros ──
(function(){
  const containerBtns=document.getElementById('prog-btns');
  const containerMacros=document.getElementById('macro-btns');
  const nameIn=document.getElementById('mn');
  const actIn=document.getElementById('ma');
  const presetSel=document.getElementById('mp');
  const saveBtn=document.getElementById('macro-save');
  const macrosCard=document.getElementById('macros-card');
  const editToggle=document.getElementById('macro-edit-toggle');
  let editIdx=-1;

  editToggle.addEventListener('click',()=>{
    macrosCard.classList.toggle('editing');
    editToggle.classList.toggle('on');
    const editing=macrosCard.classList.contains('editing');
    editToggle.textContent=editing?'Done':'Edit';
    if(!editing){editIdx=-1;nameIn.value='';actIn.value=''}
    loadButtons();
  });

  presetSel.addEventListener('change',()=>{
    if(presetSel.value==='__alt__'){
      wrapAlternate(actIn);
    }else if(presetSel.value==='__br__'){
      newBranch(actIn);
    }else if(presetSel.value){
      appendStep(actIn,presetSel.value);
      if(!nameIn.value)nameIn.value=presetSel.options[presetSel.selectedIndex].text;
    }
    presetSel.value='';
  });

  // Combo builder: modifier toggles + key dropdown
  const modBtns=document.querySelectorAll('.mod-btn');
  const keySel=document.getElementById('mk');
  modBtns.forEach(b=>b.addEventListener('click',()=>b.classList.toggle('on')));
  keySel.addEventListener('change',()=>{
    if(!keySel.value)return;
    let mod=0;
    modBtns.forEach(b=>{if(b.classList.contains('on'))mod|=parseInt(b.dataset.mod)});
    const combo='combo:'+mod+':'+keySel.value;
    const label=(mod?[...modBtns].filter(b=>b.classList.contains('on')).map(b=>b.textContent).join('+')+'+':'')+keySel.options[keySel.selectedIndex].text;
    if(actIn.value.trim()){actIn.value+=' | '+combo}else{actIn.value=combo}
    if(!nameIn.value)nameIn.value=label;
    keySel.value='';
  });

  function loadButtons(){
    fetch('/api/ble_keyboard/buttons').then(r=>r.json()).then(btns=>{
      // Macros are the editable entries. Refresh the shared name list first so
      // a rename or delete immediately re-flags any override referencing it.
      knownMacroNames=btns.filter(b=>b.editable).map(b=>b.name);
      if(reflagOverrides)reflagOverrides();
      containerBtns.innerHTML='';
      containerMacros.innerHTML='';
      let hasBtns=false, hasMacros=false;
      btns.forEach(b=>{
        if(!b.editable){
          hasBtns=true;
          const el=document.createElement('button');
          el.className='prog-btn';
          el.textContent=b.name;
          onTap(el,()=>api('press',{action:b.action}));
          containerBtns.appendChild(el);
        }else{
          hasMacros=true;
          const wrap=document.createElement('div');
          wrap.className='macro-wrap';
          const el=document.createElement('button');
          el.className='prog-btn';
          const editing=macrosCard.classList.contains('editing');
          el.textContent=editing?'['+b.index+'] '+b.name:b.name;
          el.title=editing?'Macro #'+b.index+': '+b.action:b.name;
          onTap(el,()=>api('press',{action:b.action}));
          const eb=document.createElement('button');
          eb.className='macro-act';
          eb.textContent='\u270E';
          eb.title='Edit';
          eb.addEventListener('click',()=>{
            nameIn.value=b.name;actIn.value=b.action;editIdx=b.index;
          });
          const db=document.createElement('button');
          db.className='macro-act del';
          db.textContent='\u2715';
          db.title='Delete';
          db.addEventListener('click',()=>{
            if(confirm('Delete macro "'+b.name+'"?')){
              fetch('/api/ble_keyboard/macro_delete?'+new URLSearchParams({index:b.index}),{method:'POST'}).then(()=>{
                if(editIdx===b.index){editIdx=-1;nameIn.value='';actIn.value=''}
                loadButtons();
              });
            }
          });
          wrap.appendChild(el);wrap.appendChild(eb);wrap.appendChild(db);
          containerMacros.appendChild(wrap);
        }
      });
      if(!hasBtns) containerBtns.innerHTML='<span class="prog-empty">No buttons</span>';
      if(!hasMacros) containerMacros.innerHTML='<span class="prog-empty">No macros</span>';
      // Surface the YAML (non-editable) buttons as presets in both dropdowns. The
      // Host Actions preset mirrors mp, so populate mp then re-sync ov-preset.
      const og=document.createElement('optgroup');
      og.label='Buttons'; og.className='yaml-preset';
      btns.forEach(b=>{
        if(!b.editable){
          const o=document.createElement('option');
          o.value=b.action;      // action string dropped into the action field
          o.textContent=b.name;  // button name shown in the dropdown
          og.appendChild(o);
        }
      });
      const old=presetSel.querySelector('optgroup.yaml-preset'); if(old)old.remove();
      if(og.children.length)presetSel.insertBefore(og, presetSel.children[1]||null);
      // Host Actions preset mirrors mp, then also gets a Macros group (macros are
      // only offered there, not in the Macros editor's own preset).
      const ov=document.getElementById('ov-preset');
      if(ov){
        ov.innerHTML=presetSel.innerHTML;
        const mog=document.createElement('optgroup');
        mog.label='Macros'; mog.className='macro-preset';
        btns.forEach(b=>{
          if(b.editable){
            const o=document.createElement('option');
            // A reference, not a copy of the text — so editing the macro later
            // updates every override that points at it.
            o.value='macro:'+b.name;
            o.textContent=b.name;  // macro name shown in the dropdown
            mog.appendChild(o);
          }
        });
        if(mog.children.length){
          const ref=ov.querySelector('optgroup.yaml-preset');
          if(ref)ref.after(mog); else ov.insertBefore(mog, ov.children[1]||null);
        }
      }
    }).catch(()=>{
      containerBtns.innerHTML='<span class="prog-empty">Error loading</span>';
      containerMacros.innerHTML='<span class="prog-empty">Error loading</span>';
    });
  }

  saveBtn.addEventListener('click',()=>{
    const n=nameIn.value.trim(),a=actIn.value.trim();
    if(!n||!a){alert('Name and action are required');return}
    const ep=editIdx>=0?'macro_update':'macro_add';
    const p=editIdx>=0?{index:editIdx,name:n,action:a}:{name:n,action:a};
    fetch('/api/ble_keyboard/'+ep+'?'+new URLSearchParams(p),{method:'POST'}).then(r=>{
      if(!r.ok)return r.text().then(t=>{alert(t)});
      nameIn.value='';actIn.value='';presetSel.value='';editIdx=-1;
      loadButtons();
    });
  });

  loadButtons();
})();

// ── Host Actions (per-host action overrides) ──
(function(){
  const slotSel=document.getElementById('ov-slot');
  const list=document.getElementById('ov-list');
  const nameIn=document.getElementById('ov-name');
  const actIn=document.getElementById('ov-act');
  const saveBtn=document.getElementById('ov-save');
  const preset=document.getElementById('ov-preset');
  const forgetBtn=document.getElementById('ov-forget');
  if(!slotSel)return;
  let slot=null;  // null until the first load picks the active slot

  // Reuse the macro form's preset list rather than shipping a second copy of
  // ~60 <option> rows; it stays in sync automatically.
  const macroPreset=document.getElementById('mp');
  if(preset&&macroPreset)preset.innerHTML=macroPreset.innerHTML;
  if(preset)preset.addEventListener('change',()=>{
    if(preset.value==='__alt__'){
      wrapAlternate(actIn);
    }else if(preset.value==='__br__'){
      newBranch(actIn);
    }else if(preset.value){
      appendStep(actIn,preset.value);
    }
    preset.value='';
  });

  function loadSlots(){
    return fetch('/api/ble_keyboard/hosts').then(r=>r.json()).then(d=>{
      if(slot===null)slot=d.active;
      slotSel.innerHTML='';
      d.slots.forEach(s=>{
        const o=document.createElement('option');
        o.value=s.slot;
        // Name it the same way the host bar does (1-based, or the switch_host
        // button's name), but also state the raw slot number — this card maps
        // straight onto YAML `slot:` and the API's 0-based slot argument.
        const label=s.name||('Host '+(s.slot+1));
        o.textContent=label+' — slot '+s.slot+(s.slot===d.active?' (active)':'');
        slotSel.appendChild(o);
      });
      slotSel.value=slot;
    });
  }

  // Mark overrides whose macro: reference no longer resolves. Runs after the
  // list renders and again whenever the macro list changes, so renaming or
  // deleting a macro lights up the rows that pointed at it.
  function applyMacroFlags(){
    list.querySelectorAll('.ovr-row').forEach(row=>{
      const old=row.querySelector('.ovr-warn');
      if(old)old.remove();
      if(!knownMacroNames)return;   // names not loaded yet — flagging now would be noise
      const missing=macroRefs(row.dataset.act).filter(n=>knownMacroNames.indexOf(n)===-1);
      if(!missing.length)return;
      const w=document.createElement('span');
      w.className='ovr-warn';
      w.textContent='⚠';
      w.title='No macro named '+missing.join(', ')+'.\nThis override does nothing until that macro exists again — recreate it under that name, or edit this override.';
      row.insertBefore(w,row.querySelector('.ovr-tag'));
    });
  }
  reflagOverrides=applyMacroFlags;

  function load(){
    fetch('/api/ble_keyboard/overrides?'+new URLSearchParams({slot:slot})).then(r=>r.json()).then(d=>{
      list.innerHTML='';
      if(!d.items.length){list.innerHTML='<span class="prog-empty">No overrides — this host uses the built-in actions</span>';return}
      d.items.forEach(it=>{
        const row=document.createElement('div');
        row.className='ovr-row';
        // Kept on the element so flags can be re-evaluated after a macro
        // changes without refetching the override list.
        row.dataset.act=it.action;
        const n=document.createElement('span');
        n.className='ovr-name';n.textContent=it.name;
        const a=document.createElement('span');
        a.className='ovr-act';a.textContent=it.action;
        const tag=document.createElement('span');
        tag.className='ovr-tag';
        tag.textContent=it.src==='yaml'?'YAML':'SAVED';
        tag.title=it.src==='yaml'?'From your YAML config — set one here to override it':'Saved on the device; delete to fall back to YAML';
        row.appendChild(n);row.appendChild(a);row.appendChild(tag);
        const eb=document.createElement('button');
        eb.className='macro-act';eb.textContent='✎';eb.title='Edit';
        eb.addEventListener('click',()=>{nameIn.value=it.name;actIn.value=it.action});
        row.appendChild(eb);
        if(it.src!=='yaml'){
          const db=document.createElement('button');
          db.className='macro-act del';db.textContent='✕';db.title='Delete (revert to YAML/built-in)';
          db.addEventListener('click',()=>{
            if(confirm('Delete override "'+it.name+'" on slot '+slot+'?')){
              fetch('/api/ble_keyboard/override_clear?'+new URLSearchParams({slot:slot,name:it.name}),{method:'POST'}).then(load);
            }
          });
          row.appendChild(db);
        }
        list.appendChild(row);
      });
      applyMacroFlags();
    }).catch(()=>{list.innerHTML='<span class="prog-empty">Error loading</span>'});
  }

  // ── Forget host ──
  // Sits beside the slot picker and acts on whatever it shows, so a host that
  // isn't the active one can be forgotten too. Two taps rather than confirm():
  // an accidental brush can't drop a bond, and the second tap stays on the page.
  let forgetArmed=false,forgetTimer=null;
  function resetForget(){
    clearTimeout(forgetTimer);forgetArmed=false;
    forgetBtn.textContent='Forget Host';forgetBtn.style.background='';forgetBtn.style.color='';
  }
  if(forgetBtn)forgetBtn.addEventListener('click',()=>{
    if(!forgetArmed){
      forgetArmed=true;
      forgetBtn.textContent='Confirm?';
      forgetBtn.style.background='#c44';forgetBtn.style.color='#fff';
      forgetTimer=setTimeout(resetForget,3000);
    }else{
      resetForget();
      api('forget_host',{slot:slot});
      // Give the bond removal a moment, then repaint the picker — the slot's
      // name and (active) marker both move when the active host is forgotten.
      setTimeout(()=>loadSlots().then(load),1000);
    }
  });

  slotSel.addEventListener('change',()=>{
    slot=parseInt(slotSel.value);
    // An armed confirm belongs to the slot it was armed on, never the new one.
    if(forgetBtn)resetForget();
    load();
    if(hidPanel&&hidPanel.classList.contains('open'))loadHidden();
    if(rptPanel&&rptPanel.classList.contains('open'))loadRepeat();
    if(hldPanel&&hldPanel.classList.contains('open'))loadHold();
  });

  saveBtn.addEventListener('click',()=>{
    const n=nameIn.value.trim(),a=actIn.value.trim();
    if(!n||!a){alert('Action name and replacement are required');return}
    fetch('/api/ble_keyboard/override_set?'+new URLSearchParams({slot:slot,name:n,action:a}),{method:'POST'}).then(r=>{
      if(!r.ok)return r.text().then(t=>{alert(t)});
      nameIn.value='';actIn.value='';
      load();
    });
  });

  loadSlots().then(load).catch(()=>{list.innerHTML='<span class="prog-empty">Error loading</span>'});

  // ── Hidden buttons (per host) ──
  // The button list is built from the remote's own DOM rather than a hardcoded
  // table, so it can never drift as buttons are added or removed.
  const hidPanel=document.getElementById('hid-panel');
  const hidToggle=document.getElementById('hid-toggle');
  const hidList=document.getElementById('hid-list');
  const hidSave=document.getElementById('hid-save');

  function remoteButtons(){
    const out=[],seen={};
    document.querySelectorAll('#media-card [data-action]').forEach(b=>{
      const a=b.dataset.action;
      if(seen[a])return;              // e.g. search is on two buttons
      seen[a]=true;
      const sec=b.closest('.rmt-section');
      const secs=[...document.querySelectorAll('#media-card .rmt-section')];
      out.push({action:a,label:b.title||b.textContent.trim()||a,group:secs.indexOf(sec),
                rpt:b.hasAttribute('data-repeat')});
    });
    return out;
  }
  const GROUP_NAMES=['Power &amp; navigation','D-pad','Volume &amp; channel','Media','Colours','Apps'];

  // Shared by all three per-host panels: same button list, same grouping — only
  // what a tick means differs, so the caller supplies that as a predicate.
  // `blocked` optionally returns a reason string for a button that must not be
  // ticked here (repeat and hold claim the same gesture), which greys it out.
  function buildCheckGrid(host,isChecked,blocked){
    const btns=remoteButtons();
    host.innerHTML='';
    const groups={};
    btns.forEach(b=>{(groups[b.group]=groups[b.group]||[]).push(b)});
    Object.keys(groups).sort((a,b)=>a-b).forEach(g=>{
      const wrap=document.createElement('div');
      wrap.className='hid-group';
      const h=document.createElement('div');
      h.className='hid-group-name';
      h.innerHTML=GROUP_NAMES[g]||('Group '+(parseInt(g)+1));
      wrap.appendChild(h);
      const items=document.createElement('div');
      items.className='hid-items';
      groups[g].forEach(b=>{
        const lab=document.createElement('label');
        lab.className='hid-item';
        const cb=document.createElement('input');
        cb.type='checkbox';
        cb.dataset.action=b.action;
        cb.checked=isChecked(b);
        const why=blocked?blocked(b):'';
        if(why){cb.disabled=true;cb.checked=false;lab.title=why;lab.style.opacity='.45'}
        lab.appendChild(cb);
        lab.appendChild(document.createTextNode(b.label));
        items.appendChild(lab);
      });
      wrap.appendChild(items);
      host.appendChild(wrap);
    });
  }

  function loadHidden(){
    if(!hidList)return Promise.resolve();
    return fetch('/api/ble_keyboard/hidden?'+new URLSearchParams({slot:slot}))
      .then(r=>r.json())
      .then(d=>{const hidden=d.hidden||[];
                buildCheckGrid(hidList,b=>hidden.indexOf(b.action)<0)})  // ticked = shown
      .catch(()=>{hidList.innerHTML='<span class="prog-empty">Error loading</span>'});
  }

  if(hidToggle)hidToggle.addEventListener('click',()=>{
    hidPanel.classList.toggle('open');
    hidToggle.innerHTML='Remote buttons '+(hidPanel.classList.contains('open')?'▴':'▾');
    if(hidPanel.classList.contains('open'))loadHidden();
  });
  const hidAll=document.getElementById('hid-all');
  const hidNone=document.getElementById('hid-none');
  if(hidAll)hidAll.addEventListener('click',()=>hidList.querySelectorAll('input').forEach(c=>c.checked=true));
  if(hidNone)hidNone.addEventListener('click',()=>hidList.querySelectorAll('input').forEach(c=>c.checked=false));

  if(hidSave)hidSave.addEventListener('click',()=>{
    const off=[...hidList.querySelectorAll('input')].filter(c=>!c.checked).map(c=>c.dataset.action);
    fetch('/api/ble_keyboard/hidden_set?'+new URLSearchParams({slot:slot,names:off.join(',')}),{method:'POST'})
      .then(r=>{
        if(!r.ok)return r.text().then(t=>{alert(t)});
        if(window.applyHidden)window.applyHidden(true);
      });
  });

  // ── Hold to repeat (per host) ──
  // Same grid as above; a tick here means "repeats while held". An untouched
  // host reports set:false and the grid shows the page's own data-repeat
  // defaults, so saving is what turns those defaults into a stored choice.
  const rptPanel=document.getElementById('rpt-panel');
  const rptToggle=document.getElementById('rpt-toggle');
  const rptList=document.getElementById('rpt-list');
  const rptDelayIn=document.getElementById('rpt-delay');
  const rptRateIn=document.getElementById('rpt-rate');

  // Both fetches: /hold carries this slot's hold set, which is what the repeat
  // grid greys out (and the reverse in loadHold below).
  function loadRepeat(){
    if(!rptList)return Promise.resolve();
    return Promise.all([
        fetch('/api/ble_keyboard/repeat?'+new URLSearchParams({slot:slot})).then(r=>r.json()),
        fetch('/api/ble_keyboard/hold?'+new URLSearchParams({slot:slot})).then(r=>r.json())])
      .then(([d,h])=>{
        rptDelayIn.value=d.delay;rptRateIn.value=d.rate;
        const on=d.buttons||[],held=h.buttons||[];
        buildCheckGrid(rptList,b=>d.set?on.indexOf(b.action)>=0:b.rpt,
                       b=>held.indexOf(b.action)>=0?'Set to hold on this host — untick it under "Press and hold" first':'');
      })
      .catch(()=>{rptList.innerHTML='<span class="prog-empty">Error loading</span>'});
  }

  if(rptToggle)rptToggle.addEventListener('click',()=>{
    rptPanel.classList.toggle('open');
    rptToggle.innerHTML='Hold to repeat '+(rptPanel.classList.contains('open')?'▴':'▾');
    if(rptPanel.classList.contains('open'))loadRepeat();
  });
  const rptAll=document.getElementById('rpt-all');
  const rptNone=document.getElementById('rpt-none');
  // :not(:disabled) throughout — a greyed-out button belongs to the other panel,
  // and ticking it here would only earn a 400 from the device.
  if(rptAll)rptAll.addEventListener('click',()=>rptList.querySelectorAll('input:not(:disabled)').forEach(c=>c.checked=true));
  if(rptNone)rptNone.addEventListener('click',()=>rptList.querySelectorAll('input').forEach(c=>c.checked=false));

  const rptSave=document.getElementById('rpt-save');
  if(rptSave)rptSave.addEventListener('click',()=>{
    const on=[...rptList.querySelectorAll('input:not(:disabled)')].filter(c=>c.checked).map(c=>c.dataset.action);
    fetch('/api/ble_keyboard/repeat_set?'+new URLSearchParams({
        slot:slot,delay:rptDelayIn.value,rate:rptRateIn.value,names:on.join(',')}),{method:'POST'})
      .then(r=>{
        if(!r.ok)return r.text().then(t=>{alert(t)});
        // Re-read rather than trust the inputs: the device clamps the timings,
        // so the boxes should show what was actually stored.
        if(window.applyRepeat)window.applyRepeat(true);
        if(hldPanel&&hldPanel.classList.contains('open'))loadHold();
        return loadRepeat();
      });
  });

  const rptReset=document.getElementById('rpt-reset');
  if(rptReset)rptReset.addEventListener('click',()=>{
    fetch('/api/ble_keyboard/repeat_set?'+new URLSearchParams({slot:slot,reset:'1'}),{method:'POST'})
      .then(r=>{
        if(!r.ok)return r.text().then(t=>{alert(t)});
        if(window.applyRepeat)window.applyRepeat(true);
        return loadRepeat();
      });
  });

  // ── Press and hold (per host) ──
  // The only one of the three panels that changes what a press *does* on the
  // wire: a ticked button is held down for as long as it is held here, instead
  // of tapped. There is no "All" — holding everything is never what you want,
  // and each extra hold is one more key that can be left down.
  const hldPanel=document.getElementById('hld-panel');
  const hldToggle=document.getElementById('hld-toggle');
  const hldList=document.getElementById('hld-list');

  function loadHold(){
    if(!hldList)return Promise.resolve();
    return fetch('/api/ble_keyboard/hold?'+new URLSearchParams({slot:slot}))
      .then(r=>r.json()).then(d=>{
        const on=d.buttons||[],rpt=d.repeat||[];
        buildCheckGrid(hldList,b=>on.indexOf(b.action)>=0,
                       b=>rpt.indexOf(b.action)>=0?'Set to repeat on this host — untick it under "Hold to repeat" first':'');
      })
      .catch(()=>{hldList.innerHTML='<span class="prog-empty">Error loading</span>'});
  }

  if(hldToggle)hldToggle.addEventListener('click',()=>{
    hldPanel.classList.toggle('open');
    hldToggle.innerHTML='Press and hold '+(hldPanel.classList.contains('open')?'▴':'▾');
    if(hldPanel.classList.contains('open'))loadHold();
  });
  const hldNone=document.getElementById('hld-none');
  if(hldNone)hldNone.addEventListener('click',()=>hldList.querySelectorAll('input').forEach(c=>c.checked=false));

  const hldSave=document.getElementById('hld-save');
  if(hldSave)hldSave.addEventListener('click',()=>{
    const on=[...hldList.querySelectorAll('input:not(:disabled)')].filter(c=>c.checked).map(c=>c.dataset.action);
    fetch('/api/ble_keyboard/hold_set?'+new URLSearchParams({slot:slot,names:on.join(',')}),{method:'POST'})
      .then(r=>{
        if(!r.ok)return r.text().then(t=>{alert(t)});
        if(window.applyHold)window.applyHold(true);
        // The repeat grid's greyed-out set just changed, so re-read it if it is
        // open — otherwise it would offer a button this panel has taken.
        if(rptPanel&&rptPanel.classList.contains('open'))loadRepeat();
        return loadHold();
      });
  });

  // ── Backup / Restore ──
  // Restore replays the file through the same endpoints the UI already uses,
  // rather than uploading it: the handler parses query args only, and reusing
  // those endpoints keeps their validation (name/action limits, slot range).
  // The trade-off is that a restore is NOT atomic — it stops at the first
  // failure and reports how far it got.
  const UI_KEYS=['blekb_theme','blekb_zoom','blekb_sections','blekb_order','blekb_finder_unlocked'];
  const bkSave=document.getElementById('bk-save');
  const bkLoad=document.getElementById('bk-load');
  const bkFile=document.getElementById('bk-file');
  const bkStatus=document.getElementById('bk-status');
  function status(msg,err){if(bkStatus){bkStatus.textContent=msg;bkStatus.style.color=err?'#c44':'var(--muted)'}}

  if(bkSave)bkSave.addEventListener('click',()=>{
    status('Reading settings...');
    fetch('/api/ble_keyboard/backup').then(r=>{
      if(!r.ok)throw new Error('HTTP '+r.status);
      return r.json();
    }).then(d=>{
      const ui={};
      UI_KEYS.forEach(k=>{const v=localStorage.getItem(k);if(v!==null)ui[k]=v});
      d.ui=ui;
      const name='ble-kb-backup-'+(d.device||'device').replace(/[^A-Za-z0-9_-]/g,'_')+'.json';
      const url=URL.createObjectURL(new Blob([JSON.stringify(d,null,2)],{type:'application/json'}));
      const a=document.createElement('a');
      a.href=url;a.download=name;a.click();
      setTimeout(()=>URL.revokeObjectURL(url),1000);
      const nm=(d.macros||[]).length, no=Object.keys(d.overrides||{}).length, nh=(d.hosts||[]).length;
      status('Saved '+name+' — '+nm+' macro(s), '+no+' slot(s) with overrides, '+nh+' host(s).');
    }).catch(e=>status('Backup failed: '+e.message,true));
  });

  if(bkLoad)bkLoad.addEventListener('click',()=>bkFile.click());

  // POST a restore step; reject on a non-OK so the run stops with a clear cause.
  function post(ep,params){
    return fetch('/api/ble_keyboard/'+ep+'?'+new URLSearchParams(params),{method:'POST'})
      .then(r=>r.text().then(t=>{
        if(!r.ok)throw new Error(ep+': '+t);
        return t;
      }));
  }

  if(bkFile)bkFile.addEventListener('change',()=>{
    const f=bkFile.files&&bkFile.files[0];
    if(!f)return;
    const reader=new FileReader();
    reader.onload=()=>{
      let d;
      try{d=JSON.parse(reader.result)}catch(e){status('Not a valid backup file: '+e.message,true);bkFile.value='';return}
      if(!d||typeof d!=='object'||!d.schema){status('Not a backup file (missing "schema").',true);bkFile.value='';return}
      const macros=Array.isArray(d.macros)?d.macros:[];
      const overrides=d.overrides&&typeof d.overrides==='object'?d.overrides:{};
      const scales=d.goto_scale&&typeof d.goto_scale==='object'?d.goto_scale:{};
      const hosts=Array.isArray(d.hosts)?d.hosts:[];
      let msg='Restore from "'+(d.device||'unknown')+'"?\n\n'
        +'This REPLACES current settings:\n'
        +'  • '+macros.length+' macro(s) — existing macros are deleted first\n'
        +'  • overrides on '+Object.keys(overrides).length+' host slot(s) — existing saved overrides are cleared\n'
        +'  • keyboard layout: '+(d.layout||'unchanged')+'\n'
        +'  • calibration for '+Object.keys(scales).length+' slot(s)\n'
        +'  • UI preferences (theme, zoom, section layout)';
      if(!confirm(msg)){bkFile.value='';return}
      let doHosts=false;
      if(hosts.length){
        doHosts=confirm('Also restore '+hosts.length+' learned host slot(s)?\n\n'
          +'This restores addresses ONLY. Bluetooth pairing keys cannot be backed up, '
          +'so any host whose bond is no longer on this device will need to be paired again — '
          +'the slot will look occupied but will not connect until you do.\n\n'
          +'OK = restore addresses, Cancel = leave host slots alone.');
      }
      restore(d,macros,overrides,scales,hosts,doHosts);
      bkFile.value='';
    };
    reader.onerror=()=>{status('Could not read the file.',true);bkFile.value=''};
    reader.readAsText(f);
  });

  function restore(d,macros,overrides,scales,hosts,doHosts){
    const notes=[];
    status('Restoring: clearing macros...');
    // Fetch current state first so we know what to clear.
    fetch('/api/ble_keyboard/buttons').then(r=>r.json()).then(btns=>{
      // Delete highest index first so earlier indices don't shift under us.
      const idx=btns.filter(b=>b.editable).map(b=>b.index).sort((a,b)=>b-a);
      let chain=Promise.resolve();
      idx.forEach(i=>{chain=chain.then(()=>post('macro_delete',{index:i}))});
      return chain;
    }).then(()=>{
      status('Restoring: macros...');
      let chain=Promise.resolve();
      macros.forEach(m=>{
        if(!m||!m.name||!m.action)return;
        chain=chain.then(()=>post('macro_add',{name:m.name,action:m.action}));
      });
      return chain;
    }).then(()=>{
      status('Restoring: host actions...');
      // Clear every saved override on every slot, then apply the file's.
      let chain=Promise.resolve();
      for(let s=0;s<slotSel.options.length;s++){
        const sl=parseInt(slotSel.options[s].value);
        chain=chain.then(()=>fetch('/api/ble_keyboard/overrides?'+new URLSearchParams({slot:sl}))
          .then(r=>r.json()).then(cur=>{
            let c=Promise.resolve();
            cur.items.filter(it=>it.src==='nvs').forEach(it=>{
              c=c.then(()=>post('override_clear',{slot:sl,name:it.name}));
            });
            return c;
          }));
      }
      Object.keys(overrides).forEach(sl=>{
        const map=overrides[sl]||{};
        Object.keys(map).forEach(name=>{
          chain=chain.then(()=>post('override_set',{slot:sl,name:name,action:map[name]}));
        });
      });
      return chain;
    }).then(()=>{
      status('Restoring: hidden buttons...');
      // Post every slot, including ones absent from the file, so Replace really
      // clears a slot the backup doesn't mention.
      const hid=d.hidden&&typeof d.hidden==='object'?d.hidden:{};
      let chain=Promise.resolve();
      for(let s=0;s<slotSel.options.length;s++){
        const sl=parseInt(slotSel.options[s].value);
        const names=Array.isArray(hid[sl])?hid[sl]:(Array.isArray(hid[String(sl)])?hid[String(sl)]:[]);
        chain=chain.then(()=>post('hidden_set',{slot:sl,names:names.join(',')}));
      }
      return chain;
    }).then(()=>{
      // Holds are cleared before the repeat pass and written after it: the two
      // lists reject each other's buttons, so restoring a file that moves a
      // button from one to the other would otherwise fail on whichever went
      // first. The file itself can never hold a conflicting pair.
      let chain=Promise.resolve();
      for(let s=0;s<slotSel.options.length;s++){
        const sl=parseInt(slotSel.options[s].value);
        chain=chain.then(()=>post('hold_set',{slot:sl,names:''}));
      }
      return chain;
    }).then(()=>{
      status('Restoring: hold to repeat...');
      // Same every-slot rule as above, except a slot the file doesn't mention
      // is reset rather than emptied — an empty list is itself a setting here.
      const rpt=d.repeat&&typeof d.repeat==='object'?d.repeat:{};
      let chain=Promise.resolve();
      for(let s=0;s<slotSel.options.length;s++){
        const sl=parseInt(slotSel.options[s].value);
        const v=rpt[sl]||rpt[String(sl)];
        if(v&&Array.isArray(v.buttons)){
          chain=chain.then(()=>post('repeat_set',
            {slot:sl,delay:v.delay,rate:v.rate,names:v.buttons.join(',')}));
        }else{
          chain=chain.then(()=>post('repeat_set',{slot:sl,reset:'1'}));
        }
      }
      return chain;
    }).then(()=>{
      status('Restoring: press and hold...');
      const hld=d.hold&&typeof d.hold==='object'?d.hold:{};
      let chain=Promise.resolve();
      for(let s=0;s<slotSel.options.length;s++){
        const sl=parseInt(slotSel.options[s].value);
        const names=Array.isArray(hld[sl])?hld[sl]:(Array.isArray(hld[String(sl)])?hld[String(sl)]:[]);
        if(names.length)chain=chain.then(()=>post('hold_set',{slot:sl,names:names.join(',')}));
      }
      return chain;
    }).then(()=>{
      if(!d.layout)return;
      status('Restoring: layout...');
      return post('set_layout',{id:d.layout});
    }).then(()=>{
      status('Restoring: calibration...');
      let chain=Promise.resolve();
      Object.keys(scales).forEach(sl=>{
        const v=scales[sl]||{};
        if(typeof v.x!=='number'||typeof v.y!=='number')return;
        chain=chain.then(()=>post('goto_scale_slot',{slot:sl,x:v.x,y:v.y}));
      });
      return chain;
    }).then(()=>{
      if(!doHosts)return;
      status('Restoring: host slots...');
      let chain=Promise.resolve();
      hosts.forEach(h=>{
        if(!h||typeof h.slot!=='number'||!h.addr)return;
        chain=chain.then(()=>post('set_host_slot',{slot:h.slot,addr:h.addr,type:h.type||0})
          .then(t=>{if(t==='OK-NOBOND')notes.push('host '+h.slot+' needs re-pairing')}));
      });
      return chain;
    }).then(()=>{
      // UI prefs last — writing them means a reload to take effect.
      let uiRestored=false;
      if(d.ui&&typeof d.ui==='object'){
        UI_KEYS.forEach(k=>{if(typeof d.ui[k]==='string'){localStorage.setItem(k,d.ui[k]);uiRestored=true}});
      }
      let msg='Restore complete.';
      if(notes.length)msg+=' Note: '+notes.join(', ')+'.';
      status(msg);
      if(uiRestored){
        alert(msg+'\n\nReloading to apply the interface preferences.');
        location.reload();
      }else{
        alert(msg);
        load();
        if(window.applyHidden)window.applyHidden(true);
        if(window.applyRepeat)window.applyRepeat(true);
        if(window.applyHold)window.applyHold(true);
      }
    }).catch(e=>status('Restore stopped at "'+e.message+'" — the device is partly restored.',true));
  }
})();

// ── Keyboard ──
// CK = character -> raw HID keycode (used for Ctrl+letter combos). Physical
// positions of letters/digits are identical across US/UK/DE/FR HID, so this
// table is layout-independent.
const CK={a:0x04,b:0x05,c:0x06,d:0x07,e:0x08,f:0x09,g:0x0A,h:0x0B,i:0x0C,j:0x0D,k:0x0E,l:0x0F,m:0x10,n:0x11,o:0x12,p:0x13,q:0x14,r:0x15,s:0x16,t:0x17,u:0x18,v:0x19,w:0x1A,x:0x1B,y:0x1C,z:0x1D,'1':0x1E,'2':0x1F,'3':0x20,'4':0x21,'5':0x22,'6':0x23,'7':0x24,'8':0x25,'9':0x26,'0':0x27,'`':0x35,'-':0x2D,'=':0x2E,'[':0x2F,']':0x30,'\\':0x31,';':0x33,"'":0x34,',':0x36,'.':0x37,'/':0x38,' ':0x2C};
// Per-layout row definitions. To add a layout: append an entry here and a
// matching ASCII/Unicode table on the C++ side (keyboard_layouts.cpp).
const LAYOUTS={us:{ROWS:[
[{l:'Esc',t:'s',kc:0x29,f:1.2},{l:'F1',t:'s',kc:0x3A},{l:'F2',t:'s',kc:0x3B},{l:'F3',t:'s',kc:0x3C},{l:'F4',t:'s',kc:0x3D},{l:'F5',t:'s',kc:0x3E},{l:'F6',t:'s',kc:0x3F},{l:'F7',t:'s',kc:0x40},{l:'F8',t:'s',kc:0x41},{l:'F9',t:'s',kc:0x42},{l:'F10',t:'s',kc:0x43},{l:'F11',t:'s',kc:0x44},{l:'F12',t:'s',kc:0x45}],
[{l:'`',sl:'~',t:'c',c:'`',sc:'~'},{l:'1',sl:'!',t:'c',c:'1',sc:'!'},{l:'2',sl:'@',t:'c',c:'2',sc:'@'},{l:'3',sl:'#',t:'c',c:'3',sc:'#'},{l:'4',sl:'$',t:'c',c:'4',sc:'$'},{l:'5',sl:'%',t:'c',c:'5',sc:'%'},{l:'6',sl:'^',t:'c',c:'6',sc:'^'},{l:'7',sl:'&',t:'c',c:'7',sc:'&'},{l:'8',sl:'*',t:'c',c:'8',sc:'*'},{l:'9',sl:'(',t:'c',c:'9',sc:'('},{l:'0',sl:')',t:'c',c:'0',sc:')'},{l:'-',sl:'_',t:'c',c:'-',sc:'_'},{l:'=',sl:'+',t:'c',c:'=',sc:'+'},{l:'Bksp',t:'s',kc:0x2A,f:1.5}],
[{l:'Tab',t:'s',kc:0x2B,f:1.3},{l:'q',sl:'Q',t:'c',c:'q',sc:'Q'},{l:'w',sl:'W',t:'c',c:'w',sc:'W'},{l:'e',sl:'E',t:'c',c:'e',sc:'E'},{l:'r',sl:'R',t:'c',c:'r',sc:'R'},{l:'t',sl:'T',t:'c',c:'t',sc:'T'},{l:'y',sl:'Y',t:'c',c:'y',sc:'Y'},{l:'u',sl:'U',t:'c',c:'u',sc:'U'},{l:'i',sl:'I',t:'c',c:'i',sc:'I'},{l:'o',sl:'O',t:'c',c:'o',sc:'O'},{l:'p',sl:'P',t:'c',c:'p',sc:'P'},{l:'[',sl:'{',t:'c',c:'[',sc:'{'},{l:']',sl:'}',t:'c',c:']',sc:'}'},{l:'\\',sl:'|',t:'c',c:'\\',sc:'|'}],
[{l:'Caps',t:'caps',kc:0x39,f:1.5},{l:'a',sl:'A',t:'c',c:'a',sc:'A'},{l:'s',sl:'S',t:'c',c:'s',sc:'S'},{l:'d',sl:'D',t:'c',c:'d',sc:'D'},{l:'f',sl:'F',t:'c',c:'f',sc:'F'},{l:'g',sl:'G',t:'c',c:'g',sc:'G'},{l:'h',sl:'H',t:'c',c:'h',sc:'H'},{l:'j',sl:'J',t:'c',c:'j',sc:'J'},{l:'k',sl:'K',t:'c',c:'k',sc:'K'},{l:'l',sl:'L',t:'c',c:'l',sc:'L'},{l:';',sl:':',t:'c',c:';',sc:':'},{l:"'",sl:'"',t:'c',c:"'",sc:'"'},{l:'Enter',t:'s',kc:0x28,f:1.8}],
[{l:'Shift',t:'m',mod:'shift',bit:0x02,f:2},{l:'z',sl:'Z',t:'c',c:'z',sc:'Z'},{l:'x',sl:'X',t:'c',c:'x',sc:'X'},{l:'c',sl:'C',t:'c',c:'c',sc:'C'},{l:'v',sl:'V',t:'c',c:'v',sc:'V'},{l:'b',sl:'B',t:'c',c:'b',sc:'B'},{l:'n',sl:'N',t:'c',c:'n',sc:'N'},{l:'m',sl:'M',t:'c',c:'m',sc:'M'},{l:',',sl:'<',t:'c',c:',',sc:'<'},{l:'.',sl:'>',t:'c',c:'.',sc:'>'},{l:'/',sl:'?',t:'c',c:'/',sc:'?'},{l:'Shift R',t:'m',mod:'rshift',bit:0x20,f:2}],
[{l:'Ctrl',t:'m',mod:'ctrl',bit:0x01,f:1.2},{l:'Win',t:'m',mod:'win',bit:0x08,f:1.2},{l:'Alt',t:'m',mod:'alt',bit:0x04,f:1.2},{l:'',t:'c',c:' ',sc:' ',f:6},{l:'Alt R',t:'m',mod:'altgr',bit:0x40,f:1.2},{l:'Del',t:'s',kc:0x4C,f:1.2},{l:'\u2190',t:'s',kc:0x50},{l:'\u2191',t:'s',kc:0x52},{l:'\u2193',t:'s',kc:0x51},{l:'\u2192',t:'s',kc:0x4F}]
]},
uk:{ROWS:[
[{l:'Esc',t:'s',kc:0x29,f:1.2},{l:'F1',t:'s',kc:0x3A},{l:'F2',t:'s',kc:0x3B},{l:'F3',t:'s',kc:0x3C},{l:'F4',t:'s',kc:0x3D},{l:'F5',t:'s',kc:0x3E},{l:'F6',t:'s',kc:0x3F},{l:'F7',t:'s',kc:0x40},{l:'F8',t:'s',kc:0x41},{l:'F9',t:'s',kc:0x42},{l:'F10',t:'s',kc:0x43},{l:'F11',t:'s',kc:0x44},{l:'F12',t:'s',kc:0x45}],
[{l:'`',sl:'\u00ac',t:'c',c:'`',sc:'\u00ac'},{l:'1',sl:'!',t:'c',c:'1',sc:'!'},{l:'2',sl:'"',t:'c',c:'2',sc:'"'},{l:'3',sl:'\u00a3',t:'c',c:'3',sc:'\u00a3'},{l:'4',sl:'$',t:'c',c:'4',sc:'$'},{l:'5',sl:'%',t:'c',c:'5',sc:'%'},{l:'6',sl:'^',t:'c',c:'6',sc:'^'},{l:'7',sl:'&',t:'c',c:'7',sc:'&'},{l:'8',sl:'*',t:'c',c:'8',sc:'*'},{l:'9',sl:'(',t:'c',c:'9',sc:'('},{l:'0',sl:')',t:'c',c:'0',sc:')'},{l:'-',sl:'_',t:'c',c:'-',sc:'_'},{l:'=',sl:'+',t:'c',c:'=',sc:'+'},{l:'Bksp',t:'s',kc:0x2A,f:1.5}],
[{l:'Tab',t:'s',kc:0x2B,f:1.5},{l:'q',sl:'Q',t:'c',c:'q',sc:'Q'},{l:'w',sl:'W',t:'c',c:'w',sc:'W'},{l:'e',sl:'E',t:'c',c:'e',sc:'E'},{l:'r',sl:'R',t:'c',c:'r',sc:'R'},{l:'t',sl:'T',t:'c',c:'t',sc:'T'},{l:'y',sl:'Y',t:'c',c:'y',sc:'Y'},{l:'u',sl:'U',t:'c',c:'u',sc:'U'},{l:'i',sl:'I',t:'c',c:'i',sc:'I'},{l:'o',sl:'O',t:'c',c:'o',sc:'O'},{l:'p',sl:'P',t:'c',c:'p',sc:'P'},{l:'[',sl:'{',t:'c',c:'[',sc:'{'},{l:']',sl:'}',t:'c',c:']',sc:'}'},{l:'Enter',t:'s',kc:0x28,f:1.25,cls:'kb-l-top'}],
[{l:'Caps',t:'caps',kc:0x39,f:1.5},{l:'a',sl:'A',t:'c',c:'a',sc:'A'},{l:'s',sl:'S',t:'c',c:'s',sc:'S'},{l:'d',sl:'D',t:'c',c:'d',sc:'D'},{l:'f',sl:'F',t:'c',c:'f',sc:'F'},{l:'g',sl:'G',t:'c',c:'g',sc:'G'},{l:'h',sl:'H',t:'c',c:'h',sc:'H'},{l:'j',sl:'J',t:'c',c:'j',sc:'J'},{l:'k',sl:'K',t:'c',c:'k',sc:'K'},{l:'l',sl:'L',t:'c',c:'l',sc:'L'},{l:';',sl:':',t:'c',c:';',sc:':'},{l:"'",sl:'@',t:'c',c:"'",sc:'@'},{l:'#',sl:'~',t:'c',c:'#',sc:'~'},{l:'Enter',t:'s',kc:0x28,f:1.75,cls:'kb-l-bot'}],
[{l:'Shift',t:'m',mod:'shift',bit:0x02,f:1.25},{l:'\\',sl:'|',t:'c',c:'\\',sc:'|'},{l:'z',sl:'Z',t:'c',c:'z',sc:'Z'},{l:'x',sl:'X',t:'c',c:'x',sc:'X'},{l:'c',sl:'C',t:'c',c:'c',sc:'C'},{l:'v',sl:'V',t:'c',c:'v',sc:'V'},{l:'b',sl:'B',t:'c',c:'b',sc:'B'},{l:'n',sl:'N',t:'c',c:'n',sc:'N'},{l:'m',sl:'M',t:'c',c:'m',sc:'M'},{l:',',sl:'<',t:'c',c:',',sc:'<'},{l:'.',sl:'>',t:'c',c:'.',sc:'>'},{l:'/',sl:'?',t:'c',c:'/',sc:'?'},{l:'Shift R',t:'m',mod:'rshift',bit:0x20,f:2.75}],
[{l:'Ctrl',t:'m',mod:'ctrl',bit:0x01,f:1.2},{l:'Win',t:'m',mod:'win',bit:0x08,f:1.2},{l:'Alt',t:'m',mod:'alt',bit:0x04,f:1.2},{l:'',t:'c',c:' ',sc:' ',f:6},{l:'Alt R',t:'m',mod:'altgr',bit:0x40,f:1.2},{l:'Del',t:'s',kc:0x4C,f:1.2},{l:'\u2190',t:'s',kc:0x50},{l:'\u2191',t:'s',kc:0x52},{l:'\u2193',t:'s',kc:0x51},{l:'\u2192',t:'s',kc:0x4F}]
]},
de:{ROWS:[
[{l:'Esc',t:'s',kc:0x29,f:1.2},{l:'F1',t:'s',kc:0x3A},{l:'F2',t:'s',kc:0x3B},{l:'F3',t:'s',kc:0x3C},{l:'F4',t:'s',kc:0x3D},{l:'F5',t:'s',kc:0x3E},{l:'F6',t:'s',kc:0x3F},{l:'F7',t:'s',kc:0x40},{l:'F8',t:'s',kc:0x41},{l:'F9',t:'s',kc:0x42},{l:'F10',t:'s',kc:0x43},{l:'F11',t:'s',kc:0x44},{l:'F12',t:'s',kc:0x45}],
[{l:'^',sl:'\u00b0',t:'c',c:'^',sc:'\u00b0'},{l:'1',sl:'!',t:'c',c:'1',sc:'!'},{l:'2',sl:'"',t:'c',c:'2',sc:'"',al:'\u00b2'},{l:'3',sl:'\u00a7',t:'c',c:'3',sc:'\u00a7',al:'\u00b3'},{l:'4',sl:'$',t:'c',c:'4',sc:'$'},{l:'5',sl:'%',t:'c',c:'5',sc:'%'},{l:'6',sl:'&',t:'c',c:'6',sc:'&'},{l:'7',sl:'/',t:'c',c:'7',sc:'/',al:'{'},{l:'8',sl:'(',t:'c',c:'8',sc:'(',al:'['},{l:'9',sl:')',t:'c',c:'9',sc:')',al:']'},{l:'0',sl:'=',t:'c',c:'0',sc:'=',al:'}'},{l:'\u00df',sl:'?',t:'c',c:'\u00df',sc:'?',al:'\\'},{l:'\u00b4',sl:'`',t:'c',c:'\u00b4',sc:'`'},{l:'Bksp',t:'s',kc:0x2A,f:1.5}],
[{l:'Tab',t:'s',kc:0x2B,f:1.5},{l:'q',sl:'Q',t:'c',c:'q',sc:'Q',al:'@'},{l:'w',sl:'W',t:'c',c:'w',sc:'W'},{l:'e',sl:'E',t:'c',c:'e',sc:'E',al:'\u20ac'},{l:'r',sl:'R',t:'c',c:'r',sc:'R'},{l:'t',sl:'T',t:'c',c:'t',sc:'T'},{l:'z',sl:'Z',t:'c',c:'z',sc:'Z'},{l:'u',sl:'U',t:'c',c:'u',sc:'U'},{l:'i',sl:'I',t:'c',c:'i',sc:'I'},{l:'o',sl:'O',t:'c',c:'o',sc:'O'},{l:'p',sl:'P',t:'c',c:'p',sc:'P'},{l:'\u00fc',sl:'\u00dc',t:'c',c:'\u00fc',sc:'\u00dc'},{l:'+',sl:'*',t:'c',c:'+',sc:'*',al:'~'},{l:'Enter',t:'s',kc:0x28,f:1.25,cls:'kb-l-top'}],
[{l:'Caps',t:'caps',kc:0x39,f:1.5},{l:'a',sl:'A',t:'c',c:'a',sc:'A'},{l:'s',sl:'S',t:'c',c:'s',sc:'S'},{l:'d',sl:'D',t:'c',c:'d',sc:'D'},{l:'f',sl:'F',t:'c',c:'f',sc:'F'},{l:'g',sl:'G',t:'c',c:'g',sc:'G'},{l:'h',sl:'H',t:'c',c:'h',sc:'H'},{l:'j',sl:'J',t:'c',c:'j',sc:'J'},{l:'k',sl:'K',t:'c',c:'k',sc:'K'},{l:'l',sl:'L',t:'c',c:'l',sc:'L'},{l:'\u00f6',sl:'\u00d6',t:'c',c:'\u00f6',sc:'\u00d6'},{l:'\u00e4',sl:'\u00c4',t:'c',c:'\u00e4',sc:'\u00c4'},{l:'#',sl:"'",t:'c',c:'#',sc:"'"},{l:'Enter',t:'s',kc:0x28,f:1.75,cls:'kb-l-bot'}],
[{l:'Shift',t:'m',mod:'shift',bit:0x02,f:1.25},{l:'<',sl:'>',t:'c',c:'<',sc:'>',al:'|'},{l:'y',sl:'Y',t:'c',c:'y',sc:'Y'},{l:'x',sl:'X',t:'c',c:'x',sc:'X'},{l:'c',sl:'C',t:'c',c:'c',sc:'C'},{l:'v',sl:'V',t:'c',c:'v',sc:'V'},{l:'b',sl:'B',t:'c',c:'b',sc:'B'},{l:'n',sl:'N',t:'c',c:'n',sc:'N'},{l:'m',sl:'M',t:'c',c:'m',sc:'M',al:'µ'},{l:',',sl:';',t:'c',c:',',sc:';'},{l:'.',sl:':',t:'c',c:'.',sc:':'},{l:'-',sl:'_',t:'c',c:'-',sc:'_'},{l:'Shift R',t:'m',mod:'rshift',bit:0x20,f:2.75}],
[{l:'Strg',t:'m',mod:'ctrl',bit:0x01,f:1.2},{l:'Win',t:'m',mod:'win',bit:0x08,f:1.2},{l:'Alt',t:'m',mod:'alt',bit:0x04,f:1.2},{l:'',t:'c',c:' ',sc:' ',f:6},{l:'AltGr',t:'m',mod:'altgr',bit:0x40,f:1.2},{l:'Entf',t:'s',kc:0x4C,f:1.2},{l:'\u2190',t:'s',kc:0x50},{l:'\u2191',t:'s',kc:0x52},{l:'\u2193',t:'s',kc:0x51},{l:'\u2192',t:'s',kc:0x4F}]
]},
be:{ROWS:[
[{l:'Esc',t:'s',kc:0x29,f:1.2},{l:'F1',t:'s',kc:0x3A},{l:'F2',t:'s',kc:0x3B},{l:'F3',t:'s',kc:0x3C},{l:'F4',t:'s',kc:0x3D},{l:'F5',t:'s',kc:0x3E},{l:'F6',t:'s',kc:0x3F},{l:'F7',t:'s',kc:0x40},{l:'F8',t:'s',kc:0x41},{l:'F9',t:'s',kc:0x42},{l:'F10',t:'s',kc:0x43},{l:'F11',t:'s',kc:0x44},{l:'F12',t:'s',kc:0x45}],
[{l:'\u00b2',sl:'\u00b3',t:'c',c:'\u00b2',sc:'\u00b3'},{l:'&',sl:'1',t:'c',c:'&',sc:'1',al:'|'},{l:'\u00e9',sl:'2',t:'c',c:'\u00e9',sc:'2',al:'@'},{l:'"',sl:'3',t:'c',c:'"',sc:'3',al:'#'},{l:"'",sl:'4',t:'c',c:"'",sc:'4',al:'{'},{l:'(',sl:'5',t:'c',c:'(',sc:'5',al:'['},{l:'\u00a7',sl:'6',t:'c',c:'\u00a7',sc:'6',al:'^'},{l:'\u00e8',sl:'7',t:'c',c:'\u00e8',sc:'7',al:'`'},{l:'!',sl:'8',t:'c',c:'!',sc:'8'},{l:'\u00e7',sl:'9',t:'c',c:'\u00e7',sc:'9'},{l:'\u00e0',sl:'0',t:'c',c:'\u00e0',sc:'0',al:'}'},{l:')',sl:'\u00b0',t:'c',c:')',sc:'\u00b0'},{l:'-',sl:'_',t:'c',c:'-',sc:'_'},{l:'Bksp',t:'s',kc:0x2A,f:1.5}],
[{l:'Tab',t:'s',kc:0x2B,f:1.5},{l:'a',sl:'A',t:'c',c:'a',sc:'A'},{l:'z',sl:'Z',t:'c',c:'z',sc:'Z'},{l:'e',sl:'E',t:'c',c:'e',sc:'E',al:'\u20ac'},{l:'r',sl:'R',t:'c',c:'r',sc:'R'},{l:'t',sl:'T',t:'c',c:'t',sc:'T'},{l:'y',sl:'Y',t:'c',c:'y',sc:'Y'},{l:'u',sl:'U',t:'c',c:'u',sc:'U'},{l:'i',sl:'I',t:'c',c:'i',sc:'I'},{l:'o',sl:'O',t:'c',c:'o',sc:'O'},{l:'p',sl:'P',t:'c',c:'p',sc:'P'},{l:'^',sl:'\u00a8',t:'c',c:'^',sc:'\u00a8',al:'['},{l:'$',sl:'*',t:'c',c:'$',sc:'*',al:']'},{l:'Enter',t:'s',kc:0x28,f:1.25,cls:'kb-l-top'}],
[{l:'Caps',t:'caps',kc:0x39,f:1.5},{l:'q',sl:'Q',t:'c',c:'q',sc:'Q'},{l:'s',sl:'S',t:'c',c:'s',sc:'S'},{l:'d',sl:'D',t:'c',c:'d',sc:'D'},{l:'f',sl:'F',t:'c',c:'f',sc:'F'},{l:'g',sl:'G',t:'c',c:'g',sc:'G'},{l:'h',sl:'H',t:'c',c:'h',sc:'H'},{l:'j',sl:'J',t:'c',c:'j',sc:'J'},{l:'k',sl:'K',t:'c',c:'k',sc:'K'},{l:'l',sl:'L',t:'c',c:'l',sc:'L'},{l:'m',sl:'M',t:'c',c:'m',sc:'M'},{l:'\u00f9',sl:'%',t:'c',c:'\u00f9',sc:'%'},{l:'\u00b5',sl:'\u00a3',t:'c',c:'\u00b5',sc:'\u00a3'},{l:'Enter',t:'s',kc:0x28,f:1.75,cls:'kb-l-bot'}],
[{l:'Shift',t:'m',mod:'shift',bit:0x02,f:1.25},{l:'<',sl:'>',t:'c',c:'<',sc:'>',al:'\\'},{l:'w',sl:'W',t:'c',c:'w',sc:'W'},{l:'x',sl:'X',t:'c',c:'x',sc:'X'},{l:'c',sl:'C',t:'c',c:'c',sc:'C'},{l:'v',sl:'V',t:'c',c:'v',sc:'V'},{l:'b',sl:'B',t:'c',c:'b',sc:'B'},{l:'n',sl:'N',t:'c',c:'n',sc:'N'},{l:',',sl:'?',t:'c',c:',',sc:'?'},{l:';',sl:'.',t:'c',c:';',sc:'.'},{l:':',sl:'/',t:'c',c:':',sc:'/'},{l:'=',sl:'+',t:'c',c:'=',sc:'+'},{l:'Shift R',t:'m',mod:'rshift',bit:0x20,f:2.75}],
[{l:'Ctrl',t:'m',mod:'ctrl',bit:0x01,f:1.2},{l:'Win',t:'m',mod:'win',bit:0x08,f:1.2},{l:'Alt',t:'m',mod:'alt',bit:0x04,f:1.2},{l:'',t:'c',c:' ',sc:' ',f:6},{l:'AltGr',t:'m',mod:'altgr',bit:0x40,f:1.2},{l:'Suppr',t:'s',kc:0x4C,f:1.2},{l:'\u2190',t:'s',kc:0x50},{l:'\u2191',t:'s',kc:0x52},{l:'\u2193',t:'s',kc:0x51},{l:'\u2192',t:'s',kc:0x4F}]
]}};
let currentLayout='us';

let shift=false,capsLock=false,ctrl=false,alt=false,win=false,rshift=false,altgr=false,shiftLock=false;
const modBtns={shift:[],ctrl:[],alt:[],win:[],rshift:[],altgr:[]};
const charKeys=[];
let capsBtn=null;

let kbListenersBound=false;
function buildKeyboard(){
  const kb=document.getElementById('kb-rows');
  kb.innerHTML='';
  charKeys.length=0;
  Object.keys(modBtns).forEach(m=>modBtns[m].length=0);
  capsBtn=null;
  const rows=(LAYOUTS[currentLayout]||LAYOUTS.us).ROWS;
  rows.forEach((row,ri)=>{
    const rd=document.createElement('div');
    rd.className='row';
    row.forEach((k,ki)=>{
      const b=document.createElement('button');
      b.className='k';
      if(ri===0)b.classList.add('fk');
      if(k.cls)b.classList.add(k.cls);
      if(k.f)b.style.flex=k.f;
      b.textContent=k.c===' '?'Space':k.l;
      b.dataset.r=ri;b.dataset.k=ki;
      if(k.al)b.dataset.al=k.al;
      if(k.t==='c'&&k.sl)charKeys.push({btn:b,def:k});
      if(k.t==='m'&&modBtns[k.mod])modBtns[k.mod].push(b);
      if(k.t==='caps'){capsBtn=b;if(capsLock)b.classList.add('caps')}
      rd.appendChild(b);
    });
    kb.appendChild(rd);
  });
  Object.keys(modBtns).forEach(m=>{
    const on=({shift,ctrl,alt,win,rshift,altgr})[m];
    if(on)modBtns[m].forEach(b=>b.classList.add('active'));
  });
  if(shiftLock){
    modBtns.shift.forEach(b=>b.classList.add('shift-lock'));
    modBtns.rshift.forEach(b=>b.classList.add('shift-lock'));
  }
  updateLabels();
  if(kbListenersBound)return;
  kbListenersBound=true;
  // L-Enter visual sync: pressing either half highlights both halves + bridge.
  function setKeyP(el,on){
    if(!el)return;
    el.classList.toggle('p',on);
    let partner=null;
    if(el.classList.contains('kb-l-top'))partner=document.querySelector('#kb-rows .k.kb-l-bot');
    else if(el.classList.contains('kb-l-bot'))partner=document.querySelector('#kb-rows .k.kb-l-top');
    if(partner)partner.classList.toggle('p',on);
  }
  let sx,sy,ok,ab,sLockT=null,sLockFired=false;
  function clrSLockT(){if(sLockT){clearTimeout(sLockT);sLockT=null}}
  kb.addEventListener('pointerdown',e=>{
    ab=e.target.closest('.k');if(!ab)return;
    sx=e.clientX;sy=e.clientY;ok=true;setKeyP(ab,true);
    sLockFired=false;
    const rows=(LAYOUTS[currentLayout]||LAYOUTS.us).ROWS;
    const k=rows[+ab.dataset.r][+ab.dataset.k];
    // Long-press (500 ms) on either Shift locks it sticky until tapped again.
    if(k&&k.t==='m'&&(k.mod==='shift'||k.mod==='rshift')){
      sLockT=setTimeout(()=>{
        sLockT=null;sLockFired=true;shiftLock=true;
        if(k.mod==='shift'){if(!shift)toggleMod('shift')}else{if(!rshift)toggleMod('rshift')}
        modBtns.shift.forEach(b=>b.classList.add('shift-lock'));
        modBtns.rshift.forEach(b=>b.classList.add('shift-lock'));
      },500);
    }
  });
  kb.addEventListener('pointermove',e=>{if(ok&&(Math.abs(e.clientX-sx)+Math.abs(e.clientY-sy))>10){ok=false;setKeyP(ab,false);clrSLockT()}});
  kb.addEventListener('pointerup',e=>{
    setKeyP(ab,false);clrSLockT();
    if(ok&&ab&&!sLockFired){const rows=(LAYOUTS[currentLayout]||LAYOUTS.us).ROWS;const k=rows[+ab.dataset.r][+ab.dataset.k];onKey(k)}
    ok=false;ab=null;sLockFired=false;
  });
  kb.addEventListener('pointercancel',()=>{setKeyP(ab,false);clrSLockT();ok=false;ab=null;sLockFired=false});
}

function toggleMod(mod){
  if(mod==='shift'){shift=!shift}else if(mod==='ctrl'){ctrl=!ctrl}else if(mod==='alt'){alt=!alt}else if(mod==='win'){win=!win}else if(mod==='rshift'){rshift=!rshift}else if(mod==='altgr'){altgr=!altgr}
  const active=mod==='shift'?shift:mod==='ctrl'?ctrl:mod==='alt'?alt:mod==='win'?win:mod==='rshift'?rshift:altgr;
  modBtns[mod].forEach(b=>b.classList.toggle('active',active));
  if(mod==='shift'||mod==='rshift')updateLabels();
}

function updateLabels(){
  const sh=(shift||rshift)!==capsLock;
  charKeys.forEach(({btn,def})=>{
    const isLetter=def.c>='a'&&def.c<='z';
    btn.textContent=isLetter?(sh?def.sl:def.l):((shift||rshift)?def.sl:def.l);
  });
}

function onKey(k){
  if(k.t==='m'){
    if((k.mod==='shift'||k.mod==='rshift')&&shiftLock){
      shiftLock=false;
      modBtns.shift.forEach(b=>b.classList.remove('shift-lock'));
      modBtns.rshift.forEach(b=>b.classList.remove('shift-lock'));
      if(shift)toggleMod('shift');
      if(rshift)toggleMod('rshift');
      return;
    }
    toggleMod(k.mod);return;
  }
  if(k.t==='caps'){
    capsLock=!capsLock;
    if(capsBtn)capsBtn.classList.toggle('caps',capsLock);
    api('key',{modifier:0,keycode:0x39});
    updateLabels();return;
  }
  let mb=0;
  if(ctrl)mb|=0x01;if(alt)mb|=0x04;if(win)mb|=0x08;if(rshift)mb|=0x20;if(altgr)mb|=0x40;
  if(k.t==='c'){
    if(mb!==0){
      if(shift)mb|=0x02;
      const code=CK[k.c];
      if(code!==undefined)api('key',{modifier:mb,keycode:code});
    }else{
      const isL=k.c>='a'&&k.c<='z';
      const sh=isL?((shift||rshift)!==capsLock):(shift||rshift);
      const ch=sh?k.sc:k.c;
      api('string',{keys:ch});
    }
  }else if(k.t==='s'){
    if(shift)mb|=0x02;
    api('key',{modifier:mb,keycode:k.kc});
  }
  if(shift&&!shiftLock)toggleMod('shift');
  if(ctrl)toggleMod('ctrl');
  if(alt)toggleMod('alt');
  if(win)toggleMod('win');
  if(rshift&&!shiftLock)toggleMod('rshift');
  if(altgr)toggleMod('altgr');
}

buildKeyboard();

// ── Layout selector ──
(function(){
  const sel=document.getElementById('layoutSel');
  function apply(id){
    if(!LAYOUTS[id]){console.warn('Unknown layout',id);return}
    if(id===currentLayout)return;
    currentLayout=id;
    buildKeyboard();
  }
  sel.addEventListener('change',()=>{
    const id=sel.value;
    fetch('/api/ble_keyboard/set_layout?'+new URLSearchParams({id}),{method:'POST'}).then(r=>{
      if(r.ok)apply(id);else alert('Layout switch failed');
    }).catch(()=>{});
  });
  // One-time fetch to populate the dropdown and pick up the active layout.
  fetch('/api/ble_keyboard/status').then(r=>r.json()).then(d=>{
    if(d.layouts){
      sel.innerHTML='';
      d.layouts.forEach(L=>{
        const o=document.createElement('option');
        o.value=L.id;o.textContent=L.name;
        if(L.id===(d.layout||'us'))o.selected=true;
        sel.appendChild(o);
      });
    }
    if(d.layout&&LAYOUTS[d.layout])apply(d.layout);
  }).catch(()=>{});
})();

// ── Mouse ──
(function(){
  const pad=document.getElementById('touchpad');
  let tracking=false,lastX=0,lastY=0,lastTime=0,startTime=0,moved=false,startSX=0,startSY=0;
  let accumX=0,accumY=0,moveAC=null,heldBtn=0;
  let baseSens=1.0,accelFactor=0.15,maxSens=4.0,scrollSens=2;
  const tapDeadZone=5;
  fetch('/api/ble_keyboard/mouse_config').then(r=>r.json()).then(c=>{
    baseSens=c.sensitivity;accelFactor=c.acceleration;maxSens=c.max_speed;scrollSens=c.scroll_sensitivity;
  }).catch(()=>{});

  function onStart(x,y){tracking=true;lastX=startSX=x;lastY=startSY=y;lastTime=startTime=Date.now();moved=false;accumX=0;accumY=0;moveAC=new AbortController();pad.classList.add('active')}
  // Single request in flight; deltas keep merging into accumX/accumY while it
  // runs, so a direction reversal cancels in the accumulator instead of
  // queueing stale moves (a queued backlog was why reversals only registered
  // after lifting the finger — lift aborts the queue).
  let sendBusy=false;
  function flushMove(){
    if(sendBusy||!moveAC)return;
    const dx=Math.trunc(accumX),dy=Math.trunc(accumY);
    if(dx===0&&dy===0)return;
    const cx=Math.max(-127,Math.min(127,dx)),cy=Math.max(-127,Math.min(127,dy));
    accumX-=dx;accumY-=dy;moved=true;sendBusy=true;
    fetch('/api/ble_keyboard/mouse_move?'+new URLSearchParams({x:cx,y:cy}),{method:'POST',signal:moveAC.signal})
      .catch(()=>{}).finally(()=>{sendBusy=false;flushMove()});
  }
  function onMove(x,y){
    if(!tracking)return;
    if(!moved){const td=Math.abs(x-startSX)+Math.abs(y-startSY);if(td<tapDeadZone)return}
    const now=Date.now(),dt=Math.max(now-lastTime,1);
    const rawDx=x-lastX,rawDy=y-lastY;
    const dist=Math.sqrt(rawDx*rawDx+rawDy*rawDy);
    const speed=dist/dt;
    const sens=Math.min(baseSens+speed*accelFactor,maxSens);
    accumX+=rawDx*sens;accumY+=rawDy*sens;
    lastX=x;lastY=y;lastTime=now;
    flushMove();
  }
  function onEnd(){
    if(!tracking)return;tracking=false;pad.classList.remove('active');
    if(moveAC){moveAC.abort();moveAC=null}
    accumX=0;accumY=0;
    if(!moved&&!heldBtn&&Date.now()-startTime<250)api('mouse_click',{btn:1});
  }

  pad.addEventListener('mousedown',e=>{e.preventDefault();onStart(e.clientX,e.clientY)});
  window.addEventListener('mousemove',e=>onMove(e.clientX,e.clientY));
  window.addEventListener('mouseup',()=>onEnd());

  // The pad owns every touch that starts on it (touch-action:none). pan-y +
  // direction-claiming was tried before, but the browser can start a vertical
  // pan inside the claim slack — after that, touchmoves are non-cancelable and
  // preventDefault can't stop the page scrolling mid-drag.
  function tCleanup(){window.removeEventListener('touchmove',winTouchMove);window.removeEventListener('touchend',winTouchEnd);window.removeEventListener('touchcancel',winTouchCancel)}
  function winTouchMove(e){e.preventDefault();const t=e.touches[0];onMove(t.clientX,t.clientY)}
  function winTouchEnd(e){if(e.touches.length===0){onEnd();tCleanup()}}
  function winTouchCancel(){moved=true;onEnd();tCleanup()}  // moved=true: a canceled tap must not click
  pad.addEventListener('contextmenu',e=>e.preventDefault());  // no long-press menu mid-drag
  pad.addEventListener('touchstart',e=>{
    const t=e.touches[0];
    onStart(t.clientX,t.clientY);
    window.addEventListener('touchmove',winTouchMove,{passive:false});
    window.addEventListener('touchend',winTouchEnd,{passive:false});
    window.addEventListener('touchcancel',winTouchCancel,{passive:false});
  },{passive:true});

  let wheelAccum=0,wheelBusy=false;
  function flushWheel(){
    if(wheelBusy)return;
    const s=Math.trunc(wheelAccum);
    if(s===0)return;
    wheelAccum-=s;wheelBusy=true;
    fetch('/api/ble_keyboard/mouse_scroll?'+new URLSearchParams({amount:Math.max(-127,Math.min(127,s))}),{method:'POST'})
      .catch(()=>{}).finally(()=>{wheelBusy=false;flushWheel()});
  }
  pad.addEventListener('wheel',e=>{
    e.preventDefault();
    wheelAccum+=-e.deltaY*scrollSens*0.02;
    flushWheel();
  },{passive:false});

  // Mouse buttons: tap = click; long-press (400ms) = hold (for dragging via the
  // touchpad or mouse_goto); tap the held button to release. Held state is
  // client-side only — a normal tap on any button releases everything
  // server-side too, so it also clears the mark.
  const btnMap={ml:1,mm:4,mr:2};
  function setHeld(b){
    heldBtn=b;
    for(const[id,btn]of Object.entries(btnMap))document.getElementById(id).classList.toggle('held',b===btn);
  }
  for(const[id,btn]of Object.entries(btnMap)){
    const el=document.getElementById(id);
    let sx,sy,ok,ht=null;
    const clearHt=()=>{if(ht){clearTimeout(ht);ht=null}};
    el.addEventListener('contextmenu',e=>e.preventDefault());  // stop mobile long-press menu
    el.addEventListener('pointerdown',e=>{
      sx=e.clientX;sy=e.clientY;ok=true;el.classList.add('p');
      clearHt();
      ht=setTimeout(()=>{ht=null;if(ok){ok=false;el.classList.remove('p');api('mouse_hold',{btn:btn});setHeld(btn)}},400);
    });
    el.addEventListener('pointermove',e=>{if(ok&&(Math.abs(e.clientX-sx)+Math.abs(e.clientY-sy))>10){ok=false;el.classList.remove('p');clearHt()}});
    el.addEventListener('pointerup',()=>{
      el.classList.remove('p');clearHt();
      if(!ok)return;
      ok=false;
      if(heldBtn===btn){api('mouse_release');setHeld(0)}
      else{api('mouse_click',{btn:btn});setHeld(0)}
    });
    el.addEventListener('pointercancel',()=>{ok=false;el.classList.remove('p');clearHt()});
  }

  // Scroll buttons
  let si=null;
  function startScroll(a){api('mouse_scroll',{amount:a});si=setInterval(()=>api('mouse_scroll',{amount:a}),150)}
  function stopScroll(){if(si){clearInterval(si);si=null}}
  for(const[id,a]of[['su',3],['sd',-3]]){
    const el=document.getElementById(id);
    el.addEventListener('pointerdown',()=>startScroll(a));
    el.addEventListener('pointerup',stopScroll);
    el.addEventListener('pointercancel',stopScroll);
  }
})();

// ── Remote Controls ──
(function(){
  const card=document.getElementById('media-card');
  if(!card)return;
  // Hold-to-repeat set for the active host. Populated by applyRepeat() below;
  // until that first fetch lands the page falls back to its own data-repeat
  // markup, so holding works from the moment the page is usable.
  let rptSet=null,rptDelay=400,rptRate=180;
  const repeats=(el,a)=>rptSet?rptSet.indexOf(a)>=0:el.hasAttribute('data-repeat');
  // Hold-to-send set for the active host. Empty until applyHold() lands — there
  // is no markup default for this one, because holding a key is never something
  // to start doing on a host that didn't ask for it.
  let hldSet=[];
  const holds=a=>hldSet.indexOf(a)>=0;

  let ri=null,rt=null;
  function stopRepeat(){if(ri){clearInterval(ri);ri=null}if(rt){clearTimeout(rt);rt=null}}
  // Tracks the button currently held down on the host, so the release is sent
  // exactly once however the press ends (up, cancel, or a flick off the card).
  let holdEl=null;
  function endHold(){
    if(!holdEl)return;
    holdEl.classList.remove('held');holdEl=null;
    fetch('/api/ble_keyboard/release',{method:'POST'}).catch(()=>{});
  }
  // `fired` is what keeps hold and tap from both counting: once the hold timer
  // has sent anything, pointerup must NOT send its usual press on release.
  {let sx,sy,ok,ab,fired=false;
  card.addEventListener('pointerdown',e=>{
    ab=e.target.closest('.rmt-btn');if(!ab)return;
    sx=e.clientX;sy=e.clientY;ok=true;fired=false;ab.classList.add('p');
    const act=ab.dataset.action;
    // Hold wins outright: it goes down now, not after a threshold, because a
    // push-to-talk key that only engages after 400ms clips the first word.
    // fired=true keeps pointerup from also sending the ordinary press.
    if(holds(act)){
      endHold();  // a second finger on another button: only one hold at a time
      fired=true;holdEl=ab;ab.classList.add('held');
      const drop=()=>{ab.classList.remove('held');if(holdEl===ab)holdEl=null};
      fetch('/api/ble_keyboard/hold_action?'+new URLSearchParams({action:act}),{method:'POST'})
        // 400 means this host remapped it to something unholdable (a macro, a
        // sequence). Send the ordinary press rather than leaving a dead button.
        .then(r=>{if(!r.ok){drop();api('press',{action:act})}})
        .catch(drop);
      return;
    }
    if(!repeats(ab,act))return;
    // Nothing is sent until the hold threshold passes, so a tap still fires on
    // release and a drag can still cancel it — both were the point of the
    // fire-on-release rework this repeat was reinstated into.
    rt=setTimeout(()=>{
      rt=null;if(!ok)return;
      fired=true;api('press',{action:act});
      ri=setInterval(()=>api('press',{action:act}),rptRate);
    },rptDelay);
  });
  card.addEventListener('pointermove',e=>{if(ok&&(Math.abs(e.clientX-sx)+Math.abs(e.clientY-sy))>10){ok=false;if(ab)ab.classList.remove('p');stopRepeat()}});
  card.addEventListener('pointerup',()=>{
    if(ab)ab.classList.remove('p');
    if(ok&&ab&&!fired){api('press',{action:ab.dataset.action})}
    ok=false;ab=null;fired=false;stopRepeat();endHold();
  });
  const abort=()=>{if(ab)ab.classList.remove('p');ok=false;ab=null;fired=false;stopRepeat();endHold()};
  card.addEventListener('pointercancel',abort);
  // Releasing outside the card never fires pointerup on it, which would strand a
  // running interval hammering the device. The 10px drag cancel usually catches
  // that first; this is the backstop for a fast flick off the edge.
  card.addEventListener('pointerleave',abort);
  // A tab switch or a locked screen never delivers pointerup, and a key left
  // down on the host is worse than a press cut short. max_key_hold_ms is the
  // device-side backstop for the cases even this misses (the tab being killed).
  window.addEventListener('blur',endHold);
  document.addEventListener('visibilitychange',()=>{if(document.hidden)endHold()});
  }

  // ── Per-host hold-to-repeat ──
  // Timing lives on the device so it follows the host rather than the browser
  // that set it; the repeat itself is driven here, one ordinary press per tick.
  let lastRptKey=null;
  window.applyRepeat=function(force){
    fetch('/api/ble_keyboard/repeat').then(r=>r.json()).then(d=>{
      const key=d.slot+':'+(d.set?1:0)+':'+d.delay+':'+d.rate+':'+(d.buttons||[]).join(',');
      if(!force&&key===lastRptKey)return;
      lastRptKey=key;
      // set:false means this host was never configured — back to data-repeat.
      rptSet=d.set?(d.buttons||[]):null;
      rptDelay=d.delay||400;rptRate=d.rate||180;
      stopRepeat();  // a rate change mid-hold would otherwise keep the old one
    }).catch(()=>{});
  };
  window.applyRepeat(true);

  // ── Per-host press and hold ──
  // Which buttons stay down while held, for this host. Unlike the repeat set
  // this one has no markup fallback: an unconfigured host holds nothing.
  let lastHldKey=null;
  window.applyHold=function(force){
    fetch('/api/ble_keyboard/hold').then(r=>r.json()).then(d=>{
      const key=d.slot+':'+(d.buttons||[]).join(',');
      if(!force&&key===lastHldKey)return;
      lastHldKey=key;
      hldSet=d.buttons||[];
      endHold();  // whatever was held belonged to the old set
    }).catch(()=>{});
  };
  window.applyHold(true);

  // ── Per-host hidden buttons ──
  // Hides the buttons this host has no use for, then collapses any container
  // left empty so no stray label or divider is stranded behind them.
  let lastHiddenKey=null;
  window.applyHidden=function(force){
    fetch('/api/ble_keyboard/hidden').then(r=>r.json()).then(d=>{
      const key=d.slot+':'+(d.hidden||[]).join(',');
      if(!force&&key===lastHiddenKey)return;
      lastHiddenKey=key;
      const hide=d.hidden||[];
      card.querySelectorAll('[data-action]').forEach(b=>{
        b.style.display=hide.indexOf(b.dataset.action)>=0?'none':'';
      });
      // Collapse a group/section once every button inside it is gone.
      const empty=el=>![...el.querySelectorAll('[data-action]')].some(b=>b.style.display!=='none');
      card.querySelectorAll('.rmt-strip-group').forEach(g=>{g.style.display=empty(g)?'none':''});
      card.querySelectorAll('.rmt-section').forEach(s=>{s.style.display=empty(s)?'none':''});
      // A divider only earns its place between two visible sections.
      const kids=[...card.children];
      kids.forEach((el,i)=>{
        if(!el.classList.contains('rmt-divider'))return;
        const vis=j=>j.classList&&j.classList.contains('rmt-section')&&j.style.display!=='none';
        let before=false,after=false;
        for(let j=i-1;j>=0;j--){if(vis(kids[j])){before=true;break}}
        for(let j=i+1;j<kids.length;j++){if(vis(kids[j])){after=true;break}}
        el.style.display=(before&&after)?'':'none';
      });
    }).catch(()=>{});
  };
  window.applyHidden(true);
})();

// ── Position Finder ──
(function(){
  const map=document.getElementById('finder-map');
  if(!map)return;
  const val=document.getElementById('finder-val');
  const info=document.getElementById('finder-info');
  const copyBtn=document.getElementById('finder-copy');
  const scaleInX=document.getElementById('finder-scale-x');
  const scaleInY=document.getElementById('finder-scale-y');
  const actXIn=document.getElementById('finder-act-x');
  const actYIn=document.getElementById('finder-act-y');
  const calBtn=document.getElementById('finder-cal');
  const calInfo=document.getElementById('finder-cal-info');
  let SW=1920,SH=1080,OX=0,OY=0,mons=[],lastW=-1,wx=0,wy=0,dragging=false,lastTx=null,lastTy=null;
  // Lock: keep the map non-interactive and the calibration controls hidden until
  // "Edit" is clicked, so a stray tap can't move the cursor or change the scale.
  const editToggle=document.getElementById('finder-edit-toggle');
  const editSection=document.getElementById('finder-edit-section');
  let locked=localStorage.getItem('blekb_finder_unlocked')!=='1';
  function applyLock(){
    if(editSection)editSection.style.display=locked?'none':'';
    if(editToggle)editToggle.textContent=locked?'Edit':'Lock';
    map.style.cursor=locked?'default':'crosshair';
    map.style.opacity=locked?'0.9':'1';
  }
  if(editToggle)editToggle.addEventListener('click',()=>{locked=!locked;localStorage.setItem('blekb_finder_unlocked',locked?'0':'1');applyLock()});
  applyLock();
  function draw(){
    const w=map.clientWidth||300;lastW=w;
    map.style.height=Math.max(60,Math.round(w*SH/SW))+'px';
    map.innerHTML='';
    mons.forEach((m,i)=>{
      const d=document.createElement('div');
      d.style.cssText='position:absolute;box-sizing:border-box;border:1px solid var(--accent);display:flex;align-items:center;justify-content:center;font-size:14px;font-weight:700;color:var(--accent);opacity:.85';
      d.style.left=(m.x/SW*100)+'%';d.style.top=(m.y/SH*100)+'%';
      d.style.width=(m.w/SW*100)+'%';d.style.height=(m.h/SH*100)+'%';
      d.textContent=i+(m.p?' (primary 0,0)':'');if(m.p)d.style.borderWidth='2px';map.appendChild(d);
    });
    const mk=document.createElement('div');
    mk.id='finder-marker';
    mk.style.cssText='position:absolute;width:12px;height:12px;margin:-6px 0 0 -6px;border-radius:50%;background:#e44;box-shadow:0 0 0 2px #fff;pointer-events:none;display:none';
    map.appendChild(mk);
    const sm=document.createElement('div');
    sm.id='finder-sent';
    sm.style.cssText='position:absolute;width:14px;height:14px;margin:-7px 0 0 -7px;border-radius:50%;border:2px solid #2ec27e;background:rgba(46,194,126,.4);box-shadow:0 0 0 1px #fff;pointer-events:none;display:none';
    map.appendChild(sm);
    if(lastTx!=null)placeSent(lastTx,lastTy);
  }
  function placeSent(tx,ty){
    lastTx=tx;lastTy=ty;
    const dx=(tx+OX)/SW,dy=(ty+OY)/SH;
    const sm=document.getElementById('finder-sent');
    if(sm){
      if(dx<0||dx>1||dy<0||dy>1)sm.style.display='none';
      else{sm.style.display='';sm.style.left=(dx*100)+'%';sm.style.top=(dy*100)+'%'}
    }
    // Reflect the last sent target (e.g. a mouse_goto from a button or macro)
    // in the value + main marker too — unless the user is actively aiming a tap.
    if(!dragging){
      wx=tx;wy=ty;
      val.textContent='mouse_goto:'+tx+':'+ty;
      const mk=document.getElementById('finder-marker');
      if(mk){
        if(dx<0||dx>1||dy<0||dy>1)mk.style.display='none';
        else{mk.style.display='';mk.style.left=(dx*100)+'%';mk.style.top=(dy*100)+'%'}
      }
    }
  }
  function load(){
    fetch('/api/ble_keyboard/screen').then(r=>r.json()).then(d=>{
      SW=d.w||1920;SH=d.h||1080;OX=d.ox||0;OY=d.oy||0;mons=d.mon||[];
      if(scaleInX&&d.gsx!=null&&document.activeElement!==scaleInX)scaleInX.value=(+d.gsx).toFixed(4);
      if(scaleInY&&d.gsy!=null&&document.activeElement!==scaleInY)scaleInY.value=(+d.gsy).toFixed(4);
      draw();
    }).catch(()=>draw());
  }
  // input = live preview; change (commit) = also persist to the active host
  if(scaleInX){scaleInX.addEventListener('input',()=>{const v=parseFloat(scaleInX.value);if(v>=0.05&&v<=20)api('goto_scale',{vx:v})});scaleInX.addEventListener('change',()=>{const v=parseFloat(scaleInX.value);if(v>=0.05&&v<=20)api('goto_scale',{vx:v,save:1})})}
  if(scaleInY){scaleInY.addEventListener('input',()=>{const v=parseFloat(scaleInY.value);if(v>=0.05&&v<=20)api('goto_scale',{vy:v})});scaleInY.addEventListener('change',()=>{const v=parseFloat(scaleInY.value);if(v>=0.05&&v<=20)api('goto_scale',{vy:v,save:1})})}
  // +/- nudge buttons: step 0.001, applied live and saved to the active host
  function nudge(inp,ax,d){if(!inp)return;let v=(parseFloat(inp.value)||1)+d;v=Math.round(Math.max(0.05,Math.min(20,v))*10000)/10000;inp.value=v.toFixed(4);api('goto_scale',ax==='x'?{vx:v,save:1}:{vy:v,save:1})}
  [['sx-dn',scaleInX,'x',-0.0001],['sx-up',scaleInX,'x',0.0001],['sy-dn',scaleInY,'y',-0.0001],['sy-up',scaleInY,'y',0.0001]].forEach(a=>{const b=document.getElementById(a[0]);if(b)b.addEventListener('click',()=>nudge(a[1],a[2],a[3]))});
  // Goto-target nudge (5px): move the cursor and update the value + markers
  function nudgeGoto(ax,d){
    if(ax==='x')wx+=d;else wy+=d;
    val.textContent='mouse_goto:'+wx+':'+wy;
    const mk=document.getElementById('finder-marker');
    if(mk){mk.style.display='';mk.style.left=((wx+OX)/SW*100)+'%';mk.style.top=((wy+OY)/SH*100)+'%'}
    placeSent(wx,wy);
    api('press',{action:'mouse_goto:'+wx+':'+wy});
  }
  [['gx-dn','x',-1],['gx-up','x',1],['gy-dn','y',-1],['gy-up','y',1]].forEach(a=>{const b=document.getElementById(a[0]);if(b)b.addEventListener('click',()=>nudgeGoto(a[1],a[2]))});
  // Reset scale to YAML defaults (per host), then refresh the inputs
  {const rb=document.getElementById('sc-reset');if(rb)rb.addEventListener('click',()=>{api('goto_scale',{reset:1});setTimeout(load,200)})}
  function aim(e){
    const r=map.getBoundingClientRect();
    let px=(e.clientX-r.left)/r.width,py=(e.clientY-r.top)/r.height;
    px=Math.max(0,Math.min(1,px));py=Math.max(0,Math.min(1,py));
    const gx=px*SW,gy=py*SH;
    wx=Math.round(gx-OX);wy=Math.round(gy-OY);
    val.textContent='mouse_goto:'+wx+':'+wy;
    const mk=document.getElementById('finder-marker');
    if(mk){mk.style.display='';mk.style.left=(px*100)+'%';mk.style.top=(py*100)+'%'}
    let mon='';
    for(let i=0;i<mons.length;i++){const m=mons[i];if(gx>=m.x&&gx<m.x+m.w&&gy>=m.y&&gy<m.y+m.h){mon=' · mon '+i;break}}
    info.textContent='Windows '+wx+','+wy+mon+' — release to move cursor';
  }
  map.addEventListener('contextmenu',e=>e.preventDefault());  // stop long-press / right-click menu
  map.addEventListener('pointerdown',e=>{if(locked)return;e.preventDefault();dragging=true;try{map.setPointerCapture(e.pointerId)}catch(_){}aim(e)});
  map.addEventListener('pointermove',e=>{if(dragging){e.preventDefault();aim(e)}});
  map.addEventListener('pointerup',e=>{if(!dragging)return;dragging=false;aim(e);placeSent(wx,wy);api('press',{action:'mouse_goto:'+wx+':'+wy})});
  map.addEventListener('pointercancel',()=>{dragging=false});
  copyBtn.addEventListener('click',()=>{
    if(navigator.clipboard)navigator.clipboard.writeText(val.textContent).catch(()=>{});
    copyBtn.textContent='Copied';setTimeout(()=>copyBtn.textContent='Copy',1200);
  });
  // Auto-calibrate: new_scale = current_scale * target / actual, per axis.
  if(calBtn){calBtn.addEventListener('click',()=>{
    if(lastTx==null){calInfo.textContent='send to a target first (tap the map)';return}
    const ax=parseFloat(actXIn.value),ay=parseFloat(actYIn.value);let out=[],warn=[];
    // Desktop bounds in Windows coords; a reading at the edge is clamped/unreliable.
    const E=3,minWX=-OX,maxWX=SW-OX,minWY=-OY,maxWY=SH-OY;
    const xClamp=isFinite(ax)&&(ax<=minWX+E||ax>=maxWX-E);
    const yClamp=isFinite(ay)&&(ay<=minWY+E||ay>=maxWY-E);
    if(isFinite(ax)&&ax!==0&&lastTx!==0&&!xClamp){const cur=parseFloat(scaleInX.value)||1;const ns=Math.round(Math.max(0.05,Math.min(20,cur*lastTx/ax))*10000)/10000;scaleInX.value=ns.toFixed(4);api('goto_scale',{vx:ns,save:1});out.push('X '+ns.toFixed(4))}
    else if(xClamp)warn.push('X hit the screen edge');
    if(isFinite(ay)&&ay!==0&&lastTy!==0&&!yClamp){const cur=parseFloat(scaleInY.value)||1;const ns=Math.round(Math.max(0.05,Math.min(20,cur*lastTy/ay))*10000)/10000;scaleInY.value=ns.toFixed(4);api('goto_scale',{vy:ns,save:1});out.push('Y '+ns.toFixed(4))}
    else if(yClamp)warn.push('Y hit the screen edge');
    let msg=out.length?('new scale → '+out.join('  ')):'';
    if(warn.length)msg+=(msg?' · ':'')+warn.join('; ')+' (clamped — aim nearer the middle)';
    calInfo.textContent=msg||'enter the actual X and/or Y where it landed';
  })}
  // Poll the last sent target so the green marker tracks goto's from any source.
  function pollSent(){fetch('/api/ble_keyboard/goto_last').then(r=>r.json()).then(d=>{if(d&&d.x!=null)placeSent(d.x,d.y)}).catch(()=>{})}
  setInterval(pollSent,1200);pollSent();
  if(window.ResizeObserver){new ResizeObserver(()=>{if(map.clientWidth!==lastW)draw()}).observe(map)}
  else{window.addEventListener('resize',draw)}
  load();
})();

// ── Section Toggles + Drag Reorder ──
(function(){
  const bar=document.getElementById('toggle-bar');
  const scalable=document.getElementById('scalable');
  const KEY='blekb_sections';
  const OKEY='blekb_order';
  let state={};
  try{state=JSON.parse(localStorage.getItem(KEY))||{}}catch(e){}
  let order=null;
  try{order=JSON.parse(localStorage.getItem(OKEY))}catch(e){}

  // Apply saved order to buttons and cards
  function applyOrder(ids){
    ids.forEach(id=>{
      const btn=[...bar.children].find(b=>b.dataset&&b.dataset.section===id);
      if(btn)bar.appendChild(btn);
      const card=document.getElementById(id);
      if(card)scalable.appendChild(card);
    });
  }
  if(order&&order.length)applyOrder(order);

  function saveOrder(){
    const ids=[...bar.querySelectorAll('.toggle-btn')].map(b=>b.dataset.section);
    localStorage.setItem(OKEY,JSON.stringify(ids));
  }

  // Apply saved visibility
  bar.querySelectorAll('.toggle-btn').forEach(btn=>{
    const id=btn.dataset.section;
    if(state[id]===false){btn.classList.remove('on');const el=document.getElementById(id);if(el)el.style.display='none'}
  });

  function toggleSection(btn){
    const id=btn.dataset.section;
    const on=btn.classList.toggle('on');
    const el=document.getElementById(id);
    if(el)el.style.display=on?'':'none';
    state[id]=on;
    localStorage.setItem(KEY,JSON.stringify(state));
  }

  // Drag reorder: long-press (300ms) to enter drag mode, then slide to target
  // Short tap toggles section visibility. All via pointer events (no click).
  let dragBtn=null,tapBtn=null,dragPid=null,didDrag=false,holdTimer=null,startX=0,startY=0;
  const MOVE_THRESHOLD=15;

  function hitBtn(x,y){
    const btns=bar.querySelectorAll('.toggle-btn');
    for(const b of btns){
      const r=b.getBoundingClientRect();
      if(x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom)return b;
    }
    return null;
  }

  function cancelHold(){if(holdTimer){clearTimeout(holdTimer);holdTimer=null}}

  bar.addEventListener('pointerdown',e=>{
    const btn=e.target.closest('.toggle-btn');
    if(!btn)return;
    e.preventDefault();
    tapBtn=btn;dragPid=e.pointerId;didDrag=false;
    startX=e.clientX;startY=e.clientY;
    cancelHold();
    holdTimer=setTimeout(()=>{
      holdTimer=null;didDrag=true;
      dragBtn=btn;tapBtn=null;
      btn.classList.add('dragging');
    },300);
  });

  bar.addEventListener('pointermove',e=>{
    if(e.pointerId!==dragPid)return;
    e.preventDefault();
    const moved=Math.abs(e.clientX-startX)+Math.abs(e.clientY-startY)>MOVE_THRESHOLD;
    if(!didDrag){
      if(moved){cancelHold();tapBtn=null}
      return;
    }
    const els=bar.querySelectorAll('.toggle-btn');
    els.forEach(b=>b.classList.remove('drag-over'));
    const target=hitBtn(e.clientX,e.clientY);
    if(target&&target!==dragBtn)target.classList.add('drag-over');
  });

  function endDrag(e){
    cancelHold();
    // Short tap — toggle section
    if(tapBtn&&!didDrag){toggleSection(tapBtn);tapBtn=null;dragPid=null;return}
    tapBtn=null;
    if(!dragBtn){dragPid=null;return}
    const els=bar.querySelectorAll('.toggle-btn');
    els.forEach(b=>b.classList.remove('drag-over'));
    dragBtn.classList.remove('dragging');
    if(didDrag){
      const target=hitBtn(e.clientX,e.clientY);
      if(target&&target!==dragBtn){
        const ids=[...els].map(b=>b.dataset.section);
        const fromIdx=ids.indexOf(dragBtn.dataset.section);
        const toIdx=ids.indexOf(target.dataset.section);
        ids.splice(fromIdx,1);
        ids.splice(toIdx,0,dragBtn.dataset.section);
        applyOrder(ids);
        saveOrder();
      }
    }
    dragBtn=null;dragPid=null;didDrag=false;
  }
  bar.addEventListener('pointerup',endDrag);
  bar.addEventListener('pointercancel',endDrag);
})();
</script></body></html>)rawhtml";


// JSON-escape a string (handles \n, \r, \t, \, ")
static std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += c; break;
    }
  }
  return out;
}

// ── Internal handler class ─────────────────────────────────────────
// Inherits from the platform-specific AsyncWebHandler via web_server_base

class BleKbWebHandler : public AsyncWebHandler {
 public:
  BleKbWebHandler(EspidfBleKeyboard *kb) : kb_(kb) {}

  static std::string get_url(AsyncWebServerRequest *request) {
    char buf[513];
    std::span<char, 513> span(buf);
    auto ref = request->url_to(span);
    return std::string(ref.begin(), ref.end());
  }

  bool canHandle(AsyncWebServerRequest *request) const override {
    std::string url = get_url(request);
    return url == "/ble_keyboard" ||
           url.rfind("/api/ble_keyboard/", 0) == 0;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
      std::string url = get_url(request);

      // Do NOT add Access-Control-Allow-Origin here. ESPHome's web_server (which
      // web_control requires) already installs one globally via DefaultHeaders,
      // so setting it again emits the header twice — and a browser rejects
      // "*, *" outright, which is worse than having no CORS at all. That broke
      // cross-origin reads of /hosts from the Home Assistant cards.
      auto send_response = [request](int code, const char* type, const char* content) {
        AsyncWebServerResponse* response = request->beginResponse(code, type, content);
        response->addHeader("Connection", "close");
        request->send(response);
      };

    // Serve the page — use Progmem response to avoid heap-copying the large HTML
    if (url == "/ble_keyboard") {
      AsyncWebServerResponse* response = request->beginResponse(
          200, "text/html",
          reinterpret_cast<const uint8_t *>(PAGE_HTML), sizeof(PAGE_HTML) - 1);
      response->addHeader("Connection", "close");
      // Force the browser to revalidate PAGE_HTML on every load so a firmware
      // flash always brings UI changes (new layouts, fixes, etc.) without
      // needing a manual hard-reload.
      response->addHeader("Cache-Control", "no-cache");
      request->send(response);
      return;
    }

    std::string path = url.substr(strlen("/api/ble_keyboard/"));

    // GET-only endpoints (read state)
    if (path == "status") {
      std::string json = "{\"connected\":";
      json += kb_->is_connected() ? "true" : "false";
      json += ",\"paired\":";
      json += kb_->is_paired() ? "true" : "false";
      json += ",\"device_name\":\"";
      json += kb_->device_name();
      json += "\",\"layout\":\"";
      json += kb_->active_layout_id();
      json += "\",\"layouts\":[";
      for (size_t i = 0; i < layout_count(); i++) {
        const KeyboardLayout *lay = layout_at(i);
        if (lay == nullptr) continue;
        if (i > 0) json += ",";
        json += "{\"id\":\"";
        json += lay->id;
        json += "\",\"name\":\"";
        json += lay->display_name;
        json += "\"}";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "buttons") {
      std::string json = "[";
      bool first = true;
      // YAML-defined buttons (read-only)
      const auto &btns = kb_->get_buttons();
      for (size_t i = 0; i < btns.size(); i++) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(btns[i].name);
        json += "\",\"action\":\"";
        json += json_escape(btns[i].action);
        json += "\",\"editable\":false}";
      }
      // Buttons from other ESPHome platforms (wake_on_lan, template, …).
      // Read-only like the YAML ones — the page can press them but not edit.
      for (const auto &ext : kb_->get_external_buttons()) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(ext.name);
        json += "\",\"action\":\"";
        json += json_escape(ext.action);
        json += "\",\"editable\":false}";
      }
      // User-defined macros (editable)
      const auto &macros = kb_->get_macros();
      for (size_t i = 0; i < macros.size(); i++) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"";
        json += json_escape(macros[i].name);
        json += "\",\"action\":\"";
        json += json_escape(macros[i].action);
        json += "\",\"editable\":true,\"index\":";
        json += std::to_string(i);
        json += "}";
      }
      json += "]";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "mouse_config") {
      char buf[128];
      snprintf(buf, sizeof(buf),
               "{\"sensitivity\":%.2f,\"acceleration\":%.2f,\"max_speed\":%.2f,\"scroll_sensitivity\":%.2f}",
               kb_->mouse_sensitivity(), kb_->mouse_accel(),
               kb_->mouse_max_speed(), kb_->scroll_sensitivity());
      send_response(200, "application/json", buf);
      return;
    }

    if (path == "screen") {
      // Absolute-pointer geometry for the web Position Finder.
      char gsxbuf[16], gsybuf[16];
      snprintf(gsxbuf, sizeof(gsxbuf), "%.4f", kb_->mouse_goto_scale_x());
      snprintf(gsybuf, sizeof(gsybuf), "%.4f", kb_->mouse_goto_scale_y());
      std::string json = "{\"w\":" + std::to_string(kb_->screen_width()) +
                         ",\"h\":" + std::to_string(kb_->screen_height()) +
                         ",\"ox\":" + std::to_string(kb_->primary_origin_x()) +
                         ",\"oy\":" + std::to_string(kb_->primary_origin_y()) +
                         ",\"gsx\":" + gsxbuf + ",\"gsy\":" + gsybuf + ",\"mon\":[";
      const auto &mons = kb_->get_monitors();
      for (size_t i = 0; i < mons.size(); i++) {
        if (i) json += ",";
        json += "{\"x\":" + std::to_string(mons[i].x) +
                ",\"y\":" + std::to_string(mons[i].y) +
                ",\"w\":" + std::to_string(mons[i].width) +
                ",\"h\":" + std::to_string(mons[i].height) +
                ",\"p\":" + (mons[i].primary ? "1" : "0") + "}";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "goto_last") {
      // Last mouse_goto target (Windows coords) so the Finder can mark where the
      // cursor was last sent, from any source. Lightweight — safe to poll.
      std::string json = "{\"x\":" + std::to_string(kb_->last_goto_x()) +
                         ",\"y\":" + std::to_string(kb_->last_goto_y()) + "}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "hosts") {
      // Build slot-to-name map from registered switch_host buttons
      std::map<uint8_t, std::string> slot_names;
      for (const auto &btn : kb_->get_buttons()) {
        if (btn.action.find("switch_host:") == 0) {
          int slot = -1;
          if (sscanf(btn.action.c_str(), "switch_host:%i", &slot) == 1 && slot >= 0) {
            slot_names[(uint8_t) slot] = btn.name;
          }
        }
      }
      std::string json = "{\"active\":";
      json += std::to_string(kb_->active_host_slot());
      json += ",\"slots\":[";
      for (uint8_t i = 0; i < kb_->host_slots(); i++) {
        if (i > 0) json += ",";
        const auto &h = kb_->get_host_slot(i);
        json += "{\"slot\":";
        json += std::to_string(i);
        json += ",\"occupied\":";
        json += h.occupied ? "true" : "false";
        if (h.occupied) {
          char addr_str[18];
          format_bd_addr(h.addr, addr_str);
          json += ",\"addr\":\"";
          json += addr_str;
          json += "\"";
          // The stable identity, which the UI shows in preference to `addr` —
          // that is only what the host connected with, and a phone rotates it.
          // Remembered on the slot once seen; the live lookup is the fallback
          // for a host that has not connected since this was added.
          esp_bd_addr_t identity;
          bool have_identity = h.has_identity;
          if (have_identity) {
            memcpy(identity, h.identity, sizeof(esp_bd_addr_t));
          } else {
            have_identity = kb_->peer_identity_addr(h.addr, identity);
          }
          if (have_identity) {
            char id_str[18];
            format_bd_addr(identity, id_str);
            json += ",\"identity\":\"";
            json += id_str;
            json += "\"";
          }
        }
        auto it = slot_names.find(i);
        if (it != slot_names.end()) {
          json += ",\"name\":\"";
          json += json_escape(it->second);  // button names are user-supplied
          json += "\"";
        }
        json += "}";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "overrides") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      // NVS overrides first; a YAML entry of the same name is shadowed and omitted,
      // so the list shows exactly what would run.
      const auto &nvs = kb_->get_nvs_overrides((uint8_t) slot);
      const auto &yaml = kb_->get_yaml_overrides((uint8_t) slot);
      std::string json = "{\"slot\":";
      json += std::to_string(slot);
      json += ",\"active\":";
      json += std::to_string(kb_->active_host_slot());
      json += ",\"items\":[";
      bool first = true;
      for (const auto &o : nvs) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + json_escape(o.name) + "\",\"action\":\"" + json_escape(o.action) +
                "\",\"src\":\"nvs\"}";
      }
      for (const auto &o : yaml) {
        bool shadowed = false;
        for (const auto &n : nvs)
          if (n.name == o.name) { shadowed = true; break; }
        if (shadowed) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + json_escape(o.name) + "\",\"action\":\"" + json_escape(o.action) +
                "\",\"src\":\"yaml\"}";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "hidden") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &h = kb_->get_hidden((uint8_t) slot);
      std::string json = "{\"slot\":" + std::to_string(slot) + ",\"hidden\":[";
      for (size_t i = 0; i < h.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(h[i]) + "\"";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "repeat") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &r = kb_->get_repeat((uint8_t) slot);
      // "set" is what tells the page whether to use these values or fall back to
      // its own data-repeat defaults — an empty "buttons" with set:true means
      // the user deliberately turned every repeat off for this host.
      std::string json = "{\"slot\":" + std::to_string(slot) +
                         ",\"set\":" + (r.set ? "true" : "false") +
                         ",\"delay\":" + std::to_string(r.delay) +
                         ",\"rate\":" + std::to_string(r.rate) + ",\"buttons\":[";
      for (size_t i = 0; i < r.names.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(r.names[i]) + "\"";
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "hold") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : kb_->active_host_slot();
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
        return;
      }
      const auto &h = kb_->get_hold((uint8_t) slot);
      const auto &r = kb_->get_repeat((uint8_t) slot);
      // The repeat set rides along so the editor can grey out the buttons that
      // are already spoken for without a second round trip.
      std::string json = "{\"slot\":" + std::to_string(slot) + ",\"buttons\":[";
      for (size_t i = 0; i < h.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + json_escape(h[i]) + "\"";
      }
      json += "],\"repeat\":[";
      if (r.set) {
        for (size_t i = 0; i < r.names.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(r.names[i]) + "\"";
        }
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    if (path == "backup") {
      // Everything the user can edit at runtime, in one document. Deliberately
      // excludes the passkey and the generated per-slot addresses (device
      // identity, not settings) and YAML-defined overrides (restoring those as
      // NVS entries would shadow later YAML edits). The browser adds its own
      // "ui" section before saving the file.
      std::string json = "{\"schema\":1,\"device\":\"";
      json += json_escape(kb_->device_name());
      json += "\",\"layout\":\"";
      json += json_escape(kb_->active_layout_id());
      json += "\",\"macros\":[";
      const auto &macros = kb_->get_macros();
      for (size_t i = 0; i < macros.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + json_escape(macros[i].name) +
                "\",\"action\":\"" + json_escape(macros[i].action) + "\"}";
      }
      json += "],\"overrides\":{";
      bool first_slot = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &ovr = kb_->get_nvs_overrides(s);
        if (ovr.empty()) continue;
        if (!first_slot) json += ",";
        first_slot = false;
        json += "\"" + std::to_string(s) + "\":{";
        for (size_t i = 0; i < ovr.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(ovr[i].name) + "\":\"" + json_escape(ovr[i].action) + "\"";
        }
        json += "}";
      }
      json += "},\"hidden\":{";
      bool first_hidden = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_hidden(s);
        if (h.empty()) continue;
        if (!first_hidden) json += ",";
        first_hidden = false;
        json += "\"" + std::to_string(s) + "\":[";
        for (size_t i = 0; i < h.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(h[i]) + "\"";
        }
        json += "]";
      }
      json += "},\"repeat\":{";
      bool first_repeat = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &r = kb_->get_repeat(s);
        if (!r.set) continue;  // unset slots restore as "reset to defaults"
        if (!first_repeat) json += ",";
        first_repeat = false;
        json += "\"" + std::to_string(s) + "\":{\"delay\":" + std::to_string(r.delay) +
                ",\"rate\":" + std::to_string(r.rate) + ",\"buttons\":[";
        for (size_t i = 0; i < r.names.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(r.names[i]) + "\"";
        }
        json += "]}";
      }
      json += "},\"hold\":{";
      bool first_hold = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_hold(s);
        if (h.empty()) continue;
        if (!first_hold) json += ",";
        first_hold = false;
        json += "\"" + std::to_string(s) + "\":[";
        for (size_t i = 0; i < h.size(); i++) {
          if (i > 0) json += ",";
          json += "\"" + json_escape(h[i]) + "\"";
        }
        json += "]";
      }
      json += "},\"goto_scale\":{";
      bool first_scale = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        float gx = 0, gy = 0;
        if (!kb_->get_saved_goto_scale(s, gx, gy)) continue;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s\"%u\":{\"x\":%.4f,\"y\":%.4f}",
                 first_scale ? "" : ",", (unsigned) s, gx, gy);
        first_scale = false;
        json += buf;
      }
      json += "},\"hosts\":[";
      bool first_host = true;
      for (uint8_t s = 0; s < kb_->host_slots(); s++) {
        const auto &h = kb_->get_host_slot(s);
        if (!h.occupied) continue;
        // This is the backup export: it records the address the host connected
        // with, because that is what restoring a slot needs. Deliberately not the
        // identity — restore feeds this straight back into the slot, and
        // advertising keys its behaviour off the connection address.
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "%s{\"slot\":%u,\"addr\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"type\":%u,\"bonded\":%s}",
                 first_host ? "" : ",", (unsigned) s,
                 h.addr[0], h.addr[1], h.addr[2], h.addr[3], h.addr[4], h.addr[5],
                 (unsigned) h.addr_type, kb_->host_slot_bonded(s) ? "true" : "false");
        first_host = false;
        json += buf;
      }
      json += "]}";
      send_response(200, "application/json", json.c_str());
      return;
    }

    // Remaining endpoints — POST only
    if (request->method() != HTTP_POST) {
      send_response(405, "text/plain", "POST only");
      return;
    }

    if (path == "mouse_move") {
      int x = request->hasArg("x") ? atoi(request->arg("x").c_str()) : 0;
      int y = request->hasArg("y") ? atoi(request->arg("y").c_str()) : 0;
      kb_->send_mouse_move((int8_t) x, (int8_t) y);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_click") {
      int btn = request->hasArg("btn") ? atoi(request->arg("btn").c_str()) : 1;
      kb_->send_mouse_click((uint8_t) btn);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_hold") {
      int btn = request->hasArg("btn") ? atoi(request->arg("btn").c_str()) : 1;
      kb_->send_mouse_click_start((uint8_t) btn);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_release") {
      kb_->send_mouse_click_release();
      send_response(200, "text/plain", "OK");

    } else if (path == "hold_action") {
      // Press and hold, for as long as the remote's button is held. The paired
      // "release" below is sent on pointerup; max_key_hold_ms is the backstop
      // if the browser goes away before it arrives.
      //
      // Not "hold": the GET block above claims that name for the per-host set,
      // and it is matched before the POST-only check, so a POST there would
      // read back JSON instead of holding anything.
      if (!request->hasArg("action")) {
        send_response(400, "text/plain", "Missing action");
        return;
      }
      std::string action = request->arg("action").c_str();
      if (!kb_->hold_action(action)) {
        send_response(400, "text/plain", "Action cannot be held");
        return;
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "release") {
      kb_->release_held();
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_scroll") {
      int amount = request->hasArg("amount") ? atoi(request->arg("amount").c_str()) : 0;
      kb_->send_mouse_scroll((int8_t) amount);
      send_response(200, "text/plain", "OK");

    } else if (path == "mouse_abs") {
      // Move the pointer to an absolute position. x/y are percent by default,
      // pixels when unit=px; monitor=<idx> selects a declared monitor region.
      std::string sx = request->hasArg("x") ? request->arg("x").c_str() : "0";
      std::string sy = request->hasArg("y") ? request->arg("y").c_str() : "0";
      std::string action;
      if (request->hasArg("monitor")) {
        action = "mouse_abs_mon:" + std::string(request->arg("monitor").c_str()) + ":" + sx + ":" + sy;
      } else if (request->hasArg("unit") && std::string(request->arg("unit").c_str()) == "px") {
        action = "mouse_abs_px:" + sx + ":" + sy;
      } else {
        action = "mouse_abs:" + sx + ":" + sy;
      }
      kb_->execute_action(action);
      if (request->hasArg("btn")) {
        int btn = atoi(request->arg("btn").c_str());
        if (btn != 0) kb_->send_mouse_click((uint8_t) btn);  // clicks at the new position
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "goto_scale") {
      // Live mouse_goto calibration. v sets both axes; vx/vy set each; reset
      // restores the YAML defaults; save persists to the active host (NVS).
      if (request->hasArg("reset")) {
        kb_->reset_goto_scale_for_host();
      } else {
        if (request->hasArg("v")) {
          float v = atof(request->arg("v").c_str());
          if (v >= 0.05f && v <= 20.0f) kb_->set_mouse_goto_scale(v);
        }
        if (request->hasArg("vx")) {
          float v = atof(request->arg("vx").c_str());
          if (v >= 0.05f && v <= 20.0f) kb_->set_mouse_goto_scale_x(v);
        }
        if (request->hasArg("vy")) {
          float v = atof(request->arg("vy").c_str());
          if (v >= 0.05f && v <= 20.0f) kb_->set_mouse_goto_scale_y(v);
        }
        if (request->hasArg("save")) kb_->save_goto_scale_for_host();  // persist to active host
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "string") {
      if (request->hasArg("keys")) {
        std::string keys = request->arg("keys").c_str();
        ESP_LOGD(TAG, "WEB /string keys=\"%s\"", keys.c_str());
        kb_->send_string(keys);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "key") {
      int modifier = request->hasArg("modifier") ? atoi(request->arg("modifier").c_str()) : 0;
      int keycode = request->hasArg("keycode") ? atoi(request->arg("keycode").c_str()) : 0;
      ESP_LOGD(TAG, "WEB /key mod=%d keycode=%d", modifier, keycode);
      kb_->send_key_combo((uint8_t) modifier, (uint8_t) keycode);
      send_response(200, "text/plain", "OK");

    } else if (path == "press") {
      // Press a button by action string
      if (request->hasArg("action")) {
        std::string action = request->arg("action").c_str();
        kb_->execute_action(action);
      }
      send_response(200, "text/plain", "OK");

    } else if (path == "switch_host") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : 0;
      kb_->switch_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else if (path == "forget_host") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : 0;
      kb_->forget_host((uint8_t) slot);
      send_response(200, "text/plain", "OK");

    } else if (path == "macro_add") {
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (name.empty() || action.empty()) {
        send_response(400, "text/plain", "name and action required");
      } else if (name.size() > 31 || action.size() > 255) {
        send_response(400, "text/plain", "name max 31, action max 255 chars");
      } else if (name.find('|') != std::string::npos) {
        send_response(400, "text/plain", "Name cannot contain '|'");
      } else if (!kb_->macro_name_available(name, -1)) {
        send_response(400, "text/plain", "A macro with that name already exists");
      } else if (!kb_->add_macro(name, action)) {
        send_response(400, "text/plain", "Max macros reached");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "macro_update") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (index < 0 || name.empty() || action.empty()) {
        send_response(400, "text/plain", "index, name, and action required");
      } else if (name.size() > 31 || action.size() > 255) {
        send_response(400, "text/plain", "name max 31, action max 255 chars");
      } else if (name.find('|') != std::string::npos) {
        send_response(400, "text/plain", "Name cannot contain '|'");
      } else if (!kb_->macro_name_available(name, index)) {
        send_response(400, "text/plain", "A macro with that name already exists");
      } else if (!kb_->update_macro((uint8_t) index, name, action)) {
        send_response(404, "text/plain", "Invalid index");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "set_layout") {
      std::string id = request->hasArg("id") ? request->arg("id").c_str() : "";
      if (id.empty()) {
        send_response(400, "text/plain", "id required");
      } else {
        const KeyboardLayout *prev = kb_->active_layout();
        kb_->set_runtime_layout(id);
        if (kb_->active_layout() == prev && (prev == nullptr || id != prev->id)) {
          send_response(400, "text/plain", "Unknown layout");
        } else {
          send_response(200, "text/plain", "OK");
        }
      }

    } else if (path == "override_set") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      std::string action = request->hasArg("action") ? request->arg("action").c_str() : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (!EspidfBleKeyboard::valid_override_name(name)) {
        send_response(400, "text/plain",
                      "Invalid action name: max 31 chars, no '=', '|' or spaces. Only named "
                      "actions can be overridden (e.g. record, play_pause, stop).");
      } else if (action.empty() || action.size() > 255) {
        send_response(400, "text/plain", "Action required, max 255 chars");
      } else if (!kb_->set_override((uint8_t) slot, name, action)) {
        send_response(400, "text/plain", "Max overrides reached for this host (8)");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "hidden_set") {
      // Replaces the whole set for one slot; an empty "names" clears it.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (list.size() > EspidfBleKeyboard::MAX_HIDDEN) {
        send_response(400, "text/plain", "Too many hidden buttons (max 40)");
      } else if (!kb_->set_hidden((uint8_t) slot, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "repeat_set") {
      // Replaces the whole set for one slot. An empty "names" is a real setting
      // ("nothing repeats here"); "reset=1" is what returns the slot to the
      // page's built-in defaults.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      // Via c_str() rather than comparing arg() directly: the return type differs
      // between the Async and esp-idf web server backends, .c_str() does not.
      bool reset = request->hasArg("reset") &&
                   std::string(request->arg("reset").c_str()) == "1";
      int delay = request->hasArg("delay") ? atoi(request->arg("delay").c_str()) : 400;
      int rate = request->hasArg("rate") ? atoi(request->arg("rate").c_str()) : 180;
      // Ceiling applied here, before the narrowing cast below: set_repeat() would
      // clamp too, but only after a value past 65535 had already wrapped into
      // something plausible-looking. The floor is left to set_repeat().
      if (delay > EspidfBleKeyboard::REPEAT_DELAY_MAX) delay = EspidfBleKeyboard::REPEAT_DELAY_MAX;
      if (rate > EspidfBleKeyboard::REPEAT_RATE_MAX) rate = EspidfBleKeyboard::REPEAT_RATE_MAX;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      // Resolved before the chain so the message can name the button; the setter
      // makes the same check but only reports pass/fail.
      std::string clash = (slot >= 0 && slot < kb_->host_slots())
                              ? kb_->hold_repeat_conflict((uint8_t) slot, list, false) : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (reset) {
        kb_->clear_repeat((uint8_t) slot);
        send_response(200, "text/plain", "OK");
      } else if (list.size() > EspidfBleKeyboard::MAX_REPEAT_BUTTONS) {
        send_response(400, "text/plain", "Too many repeat buttons (max 40)");
      } else if (delay < 0 || rate < 0) {
        send_response(400, "text/plain", "Invalid timing");
      } else if (!clash.empty()) {
        std::string msg = "\"" + clash + "\" is set to hold on this host — untick it there first";
        send_response(400, "text/plain", msg.c_str());
      } else if (!kb_->set_repeat((uint8_t) slot, (uint16_t) delay, (uint16_t) rate, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "hold_set") {
      // Replaces the whole press-and-hold set for one slot. Empty clears it.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string names = request->hasArg("names") ? request->arg("names").c_str() : "";
      std::vector<std::string> list;
      size_t start = 0;
      while (start < names.size()) {
        size_t end = names.find(',', start);
        if (end == std::string::npos) end = names.size();
        std::string n = names.substr(start, end - start);
        start = end + 1;
        if (!n.empty()) list.push_back(n);
      }
      std::string clash = (slot >= 0 && slot < kb_->host_slots())
                              ? kb_->hold_repeat_conflict((uint8_t) slot, list, true) : "";
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (list.size() > EspidfBleKeyboard::MAX_HOLD) {
        send_response(400, "text/plain", "Too many hold buttons (max 40)");
      } else if (!clash.empty()) {
        std::string msg = "\"" + clash + "\" is set to repeat on this host — untick it there first";
        send_response(400, "text/plain", msg.c_str());
      } else if (!kb_->set_hold((uint8_t) slot, list)) {
        send_response(400, "text/plain", "Invalid button name in list");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "goto_scale_slot") {
      // Restore calibration for any slot, not just the active one.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      float x = request->hasArg("x") ? atof(request->arg("x").c_str()) : 0.0f;
      float y = request->hasArg("y") ? atof(request->arg("y").c_str()) : 0.0f;
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (x < 0.05f || x > 20.0f || y < 0.05f || y > 20.0f) {
        send_response(400, "text/plain", "Scale must be 0.05-20.0");
      } else {
        kb_->set_saved_goto_scale((uint8_t) slot, x, y);
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "set_host_slot") {
      // Restores the slot -> address mapping only. BLE bonding keys live in the
      // stack's own NVS and cannot be exported, so a slot restored without a
      // surviving bond will advertise at a host that refuses encryption until
      // it is re-paired. The UI warns about this before calling here.
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string addr_str = request->hasArg("addr") ? request->arg("addr").c_str() : "";
      int type = request->hasArg("type") ? atoi(request->arg("type").c_str()) : 0;
      unsigned int b[6];
      if (slot < 0 || slot >= kb_->host_slots()) {
        send_response(400, "text/plain", "Invalid slot");
      } else if (addr_str.size() != 17 ||
                 sscanf(addr_str.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
                        &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
        send_response(400, "text/plain", "addr must be AA:BB:CC:DD:EE:FF");
      } else if (type < 0 || type > 3) {
        send_response(400, "text/plain", "Invalid address type");
      } else {
        esp_bd_addr_t addr;
        for (int i = 0; i < 6; i++) addr[i] = (uint8_t) b[i];
        kb_->assign_host_slot_((uint8_t) slot, addr, (esp_ble_addr_type_t) type);
        kb_->save_host_slots_();
        send_response(200, "text/plain", kb_->host_slot_bonded((uint8_t) slot) ? "OK" : "OK-NOBOND");
      }

    } else if (path == "override_clear") {
      int slot = request->hasArg("slot") ? atoi(request->arg("slot").c_str()) : -1;
      std::string name = request->hasArg("name") ? request->arg("name").c_str() : "";
      if (slot < 0 || slot >= kb_->host_slots() || name.empty()) {
        send_response(400, "text/plain", "slot and name required");
      } else if (!kb_->clear_override((uint8_t) slot, name)) {
        send_response(404, "text/plain", "No saved override with that name");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else if (path == "macro_delete") {
      int index = request->hasArg("index") ? atoi(request->arg("index").c_str()) : -1;
      if (index < 0) {
        send_response(400, "text/plain", "index required");
      } else if (!kb_->delete_macro((uint8_t) index)) {
        send_response(404, "text/plain", "Invalid index");
      } else {
        send_response(200, "text/plain", "OK");
      }

    } else {
      send_response(404, "text/plain", "Unknown endpoint");
    }
  }

 protected:
  EspidfBleKeyboard *kb_;
};

// ── Setup ──────────────────────────────────────────────────────────

void BleKeyboardWebControl::setup() {
  auto *handler = new BleKbWebHandler(this->keyboard_);
  this->base_->add_handler(handler);
  ESP_LOGI(TAG, "Web control registered at /ble_keyboard");
}

}  // namespace espidf_ble_keyboard
}  // namespace esphome

#endif  // USE_BLE_KEYBOARD_WEB_CONTROL



