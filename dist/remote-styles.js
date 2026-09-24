// GENERATED FILE — do not edit by hand.
//
// Lifted from components/espidf_ble_keyboard/web_page.html so the Home
// Assistant card draws remotes with exactly the code the device's own web page
// uses. Regenerate after any change to the styles, catalogue, renderer or CSS:
//
//     node tools/gen-remote-styles.mjs
//
// Built-ins in this snapshot: default, style1, style2, style3, style4, style5, style6
// Custom styles are not here — they live in the device's NVS. The card takes
// those as pasted JSON, or from /api/ble_keyboard/remote_templates when it can
// reach the device (a dashboard on https cannot).

const RI={
power:'<path d="M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z"/>',
search:'<path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/>',
info:'<path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/>',
mute:'<path d="M16.5 12c0-1.77-1.02-3.29-2.5-4.03v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51C20.63 14.91 21 13.5 21 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06c1.38-.31 2.63-.95 3.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/>',
home:'<path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>',
back:'<path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/>',
up:'<path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/>',
left:'<path d="M15.41 16.59L10.83 12l4.58-4.59L14 6l-6 6 6 6z"/>',
right:'<path d="M8.59 16.59L13.17 12 8.59 7.41 10 6l6 6-6 6z"/>',
down:'<path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/>',
plus:'<path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/>',
minus:'<path d="M19 13H5v-2h14v2z"/>',
prev:'<path d="M6 6h2v12H6zm3.5 6l8.5 6V6z"/>',
rew:'<path d="M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"/>',
play:'<path d="M8 5v14l11-7z"/>',
pause:'<path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/>',
stop:'<path d="M6 6h12v12H6z"/>',
ff:'<path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/>',
next:'<path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/>',
rec:'<circle cx="12" cy="12" r="7"/>',
menu:'<path d="M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z"/>',
exit:'<path d="M10.09 15.59L11.5 17l5-5-5-5-1.41 1.41L12.67 11H3v2h9.67l-2.58 2.59zM19 3H5c-1.11 0-2 .9-2 2v4h2V5h14v14H5v-4H3v4c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2z"/>',
guide:'<path d="M21 3H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14zM5 7h6v2H5V7zm0 4h6v2H5v-2zm0 4h6v2H5v-2zm8-8h6v2h-6V7zm0 4h6v2h-6v-2zm0 4h6v2h-6v-2z"/>',
mic:'<path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3zm5-3c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z"/>',
cc:'<path d="M19 4H5c-1.11 0-2 .9-2 2v12c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2zm0 14H5V6h14v12zM7 15h3v-1.5H8.5v-3H10V9H7v6zm7 0h3v-1.5h-1.5v-3H17V9h-3v6z"/>',
tv:'<path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h5v2h8v-2h5c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12z"/>',
bspace:'<path d="M22 3H7c-.69 0-1.23.35-1.59.88L0 12l5.41 8.11c.36.53.9.89 1.59.89h15c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-3 12.59L17.59 17 14 13.41 10.41 17 9 15.59 12.59 12 9 8.41 10.41 7 14 10.59 17.59 7 19 8.41 15.41 12 19 15.59z"/>',
bright_up:'<path d="M20 8.69V4h-4.69L12 .69 8.69 4H4v4.69L.69 12 4 15.31V20h4.69L12 23.31 15.31 20H20v-4.69L23.31 12 20 8.69zM12 18c-3.31 0-6-2.69-6-6s2.69-6 6-6 6 2.69 6 6-2.69 6-6 6zm0-10c-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4-1.79-4-4-4z"/>',
bright_dn:'<path d="M20 15.31L23.31 12 20 8.69V4h-4.69L12 .69 8.69 4H4v4.69L.69 12 4 15.31V20h4.69L12 23.31 15.31 20H20v-4.69zM12 18c-3.31 0-6-2.69-6-6s2.69-6 6-6 6 2.69 6 6-2.69 6-6 6z"/>',
host_prev:'<path d="M17.59 18L19 16.59 14.42 12 19 7.41 17.59 6l-6 6zM11 18l1.41-1.41L7.83 12l4.58-4.59L11 6l-6 6z"/>',
host_next:'<path d="M6.41 6L5 7.41 9.58 12 5 16.59 6.41 18l6-6zM13 6l-1.41 1.41L16.17 12l-4.58 4.59L13 18l6-6z"/>',
host_last:'<path d="M6.99 11L3 15l3.99 4v-3H14v-2H6.99v-3zM21 9l-3.99-4v3H10v2h7.01v3L21 9z"/>',
kb_next:'<path d="M20 5H4c-1.1 0-1.99.9-1.99 2L2 17c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm-9 3h2v2h-2V8zm0 3h2v2h-2v-2zM8 8h2v2H8V8zm0 3h2v2H8v-2zm-1 2H5v-2h2v2zm0-3H5V8h2v2zm9 7H8v-2h8v2zm0-4h-2v-2h2v2zm0-3h-2V8h2v2zm3 3h-2v-2h2v2zm0-3h-2V8h2v2z"/>',
all_prev:'<path d="M12.41 7.41L8.83 11H23v2H8.83l3.59 3.59L11 18l-6-6 6-6 1.41 1.41zM4 6v12H2V6h2z"/>',
all_next:'<path d="M11.59 7.41L15.17 11H1v2h14.17l-3.59 3.59L13 18l6-6-6-6-1.41 1.41zM20 6v12h2V6h-2z"/>'};

const RMT_BTNS={
remote_power:{t:'Power',i:'power',c:'power',g:0},
search:{t:'Search',i:'search',g:0},
info:{t:'Info',i:'info',g:0},
mute:{t:'Mute',i:'mute',g:0},
home:{t:'Home',i:'home',g:0},
back:{t:'Back',i:'back',g:0},
up:{t:'Up',i:'up',r:1,g:1},
left:{t:'Left',i:'left',r:1,g:1},
ok:{t:'OK',x:'OK',c:'center',g:1},
right:{t:'Right',i:'right',r:1,g:1},
down:{t:'Down',i:'down',r:1,g:1},
volume_up:{t:'Volume Up',i:'plus',r:1,g:2},
volume_down:{t:'Volume Down',i:'minus',r:1,g:2},
channel_up:{t:'Channel Up',i:'up',r:1,g:2},
channel_down:{t:'Channel Down',i:'down',r:1,g:2},
brightness_up:{t:'Brightness Up',i:'bright_up',r:1,g:2},
brightness_down:{t:'Brightness Down',i:'bright_dn',r:1,g:2},
prev_track:{t:'Previous',i:'prev',c:'media',g:3},
rewind:{t:'Rewind',i:'rew',c:'media',r:1,g:3},
play_pause:{t:'Play/Pause',i:'play',c:'media',g:3},
play:{t:'Play',i:'play',c:'media',g:3},
pause:{t:'Pause',i:'pause',c:'media',g:3},
stop:{t:'Stop',i:'stop',c:'media',g:3},
fast_forward:{t:'Fast Forward',i:'ff',c:'media',r:1,g:3},
next_track:{t:'Next',i:'next',c:'media',g:3},
record:{t:'Record',i:'rec',c:'media rec',g:3},
color_red:{t:'Red (F1)',c:'col red',g:4},
color_green:{t:'Green (F2)',c:'col green',g:4},
color_yellow:{t:'Yellow (F3)',c:'col yellow',g:4},
color_blue:{t:'Blue (F4)',c:'col blue',g:4},
app_explorer:{t:'File Explorer',x:'Explorer',c:'app',g:5},
app_browser:{t:'Web Browser',x:'Browser',c:'app',g:5},
app_email:{t:'Email Client',x:'Email',c:'app',g:5},
app_calc:{t:'Calculator',x:'Calc',c:'app',g:5},
// Keys a TV remote has that a keyboard doesn't. Standard HID usages, but that
// page is patchily implemented — a host that ignores one is a case for an
// override, not a bug.
menu:{t:'Menu',i:'menu',g:6},
exit:{t:'Exit',i:'exit',g:6},
guide:{t:'Guide',i:'guide',g:6},
voice:{t:'Voice / Mic',i:'mic',g:6},
captions:{t:'Subtitles',i:'cc',g:6},
tv:{t:'TV',i:'tv',g:6},
// The keypad, as keyboard digits — direct channel entry on a TV, typing a
// number on a PC.
num1:{t:'1',x:'1',g:7},num2:{t:'2',x:'2',g:7},num3:{t:'3',x:'3',g:7},
num4:{t:'4',x:'4',g:7},num5:{t:'5',x:'5',g:7},num6:{t:'6',x:'6',g:7},
num7:{t:'7',x:'7',g:7},num8:{t:'8',x:'8',g:7},num9:{t:'9',x:'9',g:7},
num0:{t:'0',x:'0',g:7},
backspace:{t:'Backspace',i:'bspace',r:1,g:7},
// Spares send nothing until the host they are on gives them an override. A
// style normally relabels them, so the digit is only what they fall back to.
spare1:{t:'Spare 1',x:'1',g:8},
spare2:{t:'Spare 2',x:'2',g:8},
spare3:{t:'Spare 3',x:'3',g:8},
spare4:{t:'Spare 4',x:'4',g:8},
spare5:{t:'Spare 5',x:'5',g:8},
spare6:{t:'Spare 6',x:'6',g:8},
spare7:{t:'Spare 7',x:'7',g:8},
spare8:{t:'Spare 8',x:'8',g:8},
spare9:{t:'Spare 9',x:'9',g:8},
spare10:{t:'Spare 10',x:'10',g:8},
spare11:{t:'Spare 11',x:'11',g:8},
spare12:{t:'Spare 12',x:'12',g:8},
spare13:{t:'Spare 13',x:'13',g:8},
spare14:{t:'Spare 14',x:'14',g:8},
spare15:{t:'Spare 15',x:'15',g:8},
spare16:{t:'Spare 16',x:'16',g:8},
spare17:{t:'Spare 17',x:'17',g:8},
spare18:{t:'Spare 18',x:'18',g:8},
spare19:{t:'Spare 19',x:'19',g:8},
spare20:{t:'Spare 20',x:'20',g:8},
spare21:{t:'Spare 21',x:'21',g:8},
spare22:{t:'Spare 22',x:'22',g:8},
spare23:{t:'Spare 23',x:'23',g:8},
spare24:{t:'Spare 24',x:'24',g:8},
spare25:{t:'Spare 25',x:'25',g:8},
spare26:{t:'Spare 26',x:'26',g:8},
spare27:{t:'Spare 27',x:'27',g:8},
spare28:{t:'Spare 28',x:'28',g:8},
spare29:{t:'Spare 29',x:'29',g:8},
spare30:{t:'Spare 30',x:'30',g:8},
spare31:{t:'Spare 31',x:'31',g:8},
spare32:{t:'Spare 32',x:'32',g:8},
// Never repeats: held down, it would spin through every host.
prev_host:{t:'Prev Host',i:'host_prev',g:9},
next_host:{t:'Next Host',i:'host_next',g:9},
last_host:{t:'Last Host',i:'host_last',g:9},
// Keys for the web page's own remote: they move the tab across linked keyboards
// (peers: in the YAML) and are handled there, never sent. Anywhere else they do
// nothing.
next_keyboard:{t:'Next Keyboard',i:'kb_next',g:9},
prev_host_all:{t:'Prev Host (all)',i:'all_prev',g:9},
next_host_all:{t:'Next Host (all)',i:'all_next',g:9}};

const RMT_VARS={bg:'--rb-bg',border:'--rb-border',radius:'--rb-radius',pad:'--rb-pad',
maxw:'--rb-maxw',zoom:'--rb-zoom',btn_bg:'--rb-btn-bg',btn_fg:'--rb-btn-fg',btn_border:'--rb-btn-border',
btn_radius:'--rb-btn-radius',ok_bg:'--rb-ok-bg',ok_fg:'--rb-ok-fg',
ring_bg:'--rb-ring-bg',ring_fg:'--rb-ring-fg',light_bg:'--rb-light-bg',light_fg:'--rb-light-fg',shadow:'--rb-shadow',
label:'--rb-label',divider:'--rb-divider',clip:'--rb-clip',
lcd_bg:'--rb-lcd-bg',lcd_fg:'--rb-lcd-fg',lcd_label:'--rb-lcd-label',
lcd_border:'--rb-lcd-border',lcd_radius:'--rb-lcd-radius',
lit_bg:'--rb-lit-bg',lit_fg:'--rb-lit-fg'};

const RMT_BUILTIN=[
{id:'default',name:'Full remote',theme:{},sections:[
 ['row','remote_power','|','search','info','mute','home','back'],
 ['dpad'],
 ['strip',['Vol','volume_up','volume_down'],['Ch','channel_up','channel_down']],
 ['-'],
 ['media','prev_track','rewind','play_pause','stop','fast_forward','next_track','record'],
 ['-'],
 ['media','color_red','color_green','color_yellow','color_blue'],
 ['-'],
 ['apps','app_explorer','app_browser','app_email','app_calc','search']]},
// `divider` is set, not left to fall back: an unset one resolves to the page's
// --border, which flips with the light/dark toggle and drew a pale line across
// this style's dark body in light mode. A style that fixes its own colours has
// to fix all of them.
{id:'style1',name:'Style 1',theme:{bg:'#17181d',border:'#2a2c33',radius:'34px',pad:'22px 12px',
 maxw:'250px',btn_bg:'#232630',btn_fg:'#e8e8ec',btn_border:'#303341',ok_bg:'#454a5c',
 divider:'#303341'},sections:[
 ['row','remote_power','|','search','mute'],
 ['dpad'],
 ['row','back','home','info'],
 ['media','rewind','play_pause','fast_forward'],
 ['strip',['Vol','volume_up','volume_down'],['Ch','channel_up','channel_down']],
 ['-'],
 ['apps','app_explorer','app_browser','app_email','app_calc']]},
{id:'style2',name:'Style 2',theme:{bg:'#0d0d10',border:'#26262b',radius:'30px',pad:'20px 14px',
 maxw:'250px',btn_bg:'#1a1a1f',btn_fg:'#ededed',btn_border:'#2a2a30',ok_bg:'#333338'},sections:[
 ['row','remote_power','|','info','search'],
 ['dpad'],
 ['row','back','home','play_pause'],
 ['strip',['Vol','volume_up','volume_down'],['Ch','channel_up','channel_down']],
 ['row','mute']]},
{id:'style3',name:'Style 3',theme:{},sections:[
 ['row','remote_power','|','mute','volume_down','volume_up'],
 ['media','prev_track','rewind','play_pause','stop','fast_forward','next_track']]},
// The keypad pair — the only built-ins with numbers, colour keys and a nav
// ring, and 5 is the only pale one. Their spares carry the keys that have no
// standard usage (Input, Mark, Set) and the four app pills, so give them
// per-host overrides to make them do anything.
{id:'style4',name:'Style 4',theme:{bg:'#252525',border:'#2f2f2f',radius:'34px',pad:'18px 12px',
 maxw:'236px',btn_bg:'#2f2f2f',btn_fg:'#e6e6e6',btn_border:'#0f0f0f',ring_bg:'#d8d8d8',
 ring_fg:'#1c1c1c',ok_bg:'#2a2a2a',ok_fg:'#ededed',light_bg:'#ededed',light_fg:'#1c1c1c',
 label:'#8f8f8f',divider:'#343434',shadow:'0 2px 10px rgba(0,0,0,.4)'},sections:[
 ['row','remote_power',['spare1','Input'],'num1'],
 ['row','num2','num3','num4'],
 ['row','num5','num6','num7'],
 ['row','num8','num9','captions'],
 ['row','num0','info',['spare2','Mark']],
 ['row',['spare3','Kbd','light'],['spare4','Set']],
 ['row',['color_red','','sm'],['color_green','','sm'],['color_yellow','','sm'],['color_blue','','sm']],
 ['ring'],
 ['row','back',['home','','light'],'tv'],
 ['rocker',['Vol','volume_up','volume_down'],['','mute'],['Ch','channel_up','channel_down']],
 ['apps',['spare5','App 1','light wide'],['spare6','App 2','light wide'],['spare7','App 3','light wide']],
 ['apps',['spare8','App 4','light wide']]]},
{id:'style5',name:'Style 5',theme:{bg:'#f4f4f4',border:'#dcdcdc',radius:'34px',pad:'18px 12px',
 maxw:'236px',btn_bg:'#ffffff',btn_fg:'#6a6a6a',btn_border:'#bababa',ring_bg:'#cfcfcf',
 ring_fg:'#4a4a4a',ok_bg:'#f4f4f4',ok_fg:'#4a4a4a',light_bg:'#ffffff',light_fg:'#3a3a3a',
 label:'#7a7a7a',divider:'#e2e2e2',shadow:'0 2px 8px rgba(0,0,0,.14)'},sections:[
 ['row','remote_power',['spare1','Input'],'num1'],
 ['row','num2','num3','num4'],
 ['row','num5','num6','num7'],
 ['row','num8','num9','captions'],
 ['row','num0','info',['spare2','Mark']],
 ['row',['spare3','Kbd','light'],['spare4','Set']],
 ['row',['color_red','','sm'],['color_green','','sm'],['color_yellow','','sm'],['color_blue','','sm']],
 ['ring'],
 ['row','back',['home','','light'],'tv'],
 ['rocker',['Vol','volume_up','volume_down'],['','mute'],['Ch','channel_up','channel_down']],
 ['apps',['spare5','App 1','light wide'],['spare6','App 2','light wide'],['spare7','App 3','light wide']],
 ['apps',['spare8','App 4','light wide']]]},
// The built-in with a screen and logo keys. The screen reads @host and @state,
// which the firmware always knows, so it says something useful the moment the
// style is picked — no sources, no sensor, nothing to configure. 16 characters
// wide so it sits in the body as a display rather than a banner, and two
// reserved rows so it holds its height as a host name changes length.
// Every spare names an imported icon with icon:<name>. Until one is imported
// under that name the key shows its label, so each logo appears as it is added;
// the last key uses one of the remote's own icons the same way. Pick it and
// Export to see the syntax written out.
{id:'style6',name:'Style 6',theme:{bg:'#17181d',border:'#2a2c33',radius:'34px',pad:'22px 12px',
 maxw:'280px',btn_bg:'#232630',btn_fg:'#e8e8ec',btn_border:'#303341',ring_bg:'#232630',
 ring_fg:'#e8e8ec',ok_bg:'#454a5c',ok_fg:'#ffffff',label:'#8a8d99',divider:'#303341'},sections:[
 ['row','remote_power','|','search','mute'],
 ['ring'],
 ['row','back','home','menu'],
 ['media','rewind','play_pause','fast_forward'],
 ['rocker',['Vol','volume_up','volume_down'],['Ch','channel_up','channel_down']],
 ['-'],
 ['lcd',{cols:16,rows:2,fg:'#7fd4ff',bg:'#0e1014',label:'#5a6b78',border:'#303341'},
  ['','@host','centre'],['','@state','sm centre']],
 ['apps',['spare1','Netflix','icon:netflix lg squircle'],['spare2','YouTube','icon:youtube lg squircle'],
         ['spare3','Prime Video','icon:prime lg squircle'],['spare4','Disney+','icon:disney lg squircle']],
 ['apps',['spare5','Spotify','icon:spotify lg squircle'],['spare6','Plex','icon:plex lg squircle'],
         ['spare7','Kodi','icon:kodi lg squircle'],['spare8','TV','icon:tv lg squircle']]]}];

const RMT_KINDS=['row','dpad','ring','strip','rocker','media','apps','grid','lcd','-'];

const RMT_OPTS=['light','sm','md','lg','xl','wide','sq','squircle','fill'];

const RMT_KEY_H=[24,160];

const RMT_ICON_H=[8,160];

const RMT_RING=[120,320],RMT_RING_C=[40,200],RMT_RING_ROOM=40;

const RMT_LCD_OPTS=['sm','lg','xl','left','center','centre','right'];

const RMT_LCD_COLOURS=['fg','bg','label','border'];

const RMT_LCD_LABELLED=['@last','@station'];

const RMT_LCD_KEYS=['@host','@slot','@mac','@state','@rssi','@battery','@layout',
'@last','@station','@msg'];

const RMT_HEX=/^#[0-9a-f]{3,8}$/i;

const RMT_CLIP=/^polygon\(\s*[-0-9%.,\s]+\)$/i;

const RMT_FETCH=/(url|image-set)\s*\(/i;

const RMT_ICON_D=/^[MmZzLlHhVvCcSsQqTtAa0-9eE .,+-]+$/;

const RMT_ICON_VB=/^-?[\d.]+(?:e[-+]?\d+)?(?:[ ,]+-?[\d.]+(?:e[-+]?\d+)?){3}$/i;

const RMT_ICON_T=/^(?:(?:matrix|translate|scale|rotate|skewX|skewY)\([-\d.eE ,]+\) ?)+$/;

const RMT_ICON_NAME=/^[a-z0-9_]{1,15}$/;

const RMT_ICON_KEYS=['d','f','r','t','s','w','c','j','o'];

const RMT_ICON_LEN=4200;

const RMT_ICON_MAX=16;

const RMT_ICONS={};

function iconBad(rec){
  if(!rec||typeof rec!=='object'||Array.isArray(rec))return 'an icon is an object with a vb and a list of paths';
  if(typeof rec.vb!=='string'||!RMT_ICON_VB.test(rec.vb.trim()))return 'its viewBox must be four numbers';
  if(!Array.isArray(rec.p)||!rec.p.length||rec.p.length>256)return 'it needs between 1 and 256 paths';
  const paint=v=>v===undefined||v==='none'||v==='currentColor'||(typeof v==='string'&&RMT_HEX.test(v));
  const num=(v,hi)=>v===undefined||(typeof v==='number'&&v>=0&&v<=hi);
  for(const p of rec.p){
    if(!p||typeof p!=='object'||Array.isArray(p))return 'each path is an object';
    for(const k of Object.keys(p))if(RMT_ICON_KEYS.indexOf(k)<0)return 'unknown path field "'+k+'"';
    if(typeof p.d!=='string'||!RMT_ICON_D.test(p.d))return 'path data may hold only path commands and numbers';
    if(!paint(p.f)||!paint(p.s))return 'a path colour must be a #hex value, none or currentColor';
    if(p.r!==undefined&&p.r!==1)return 'r is 1 for evenodd, or left out';
    if(p.t!==undefined&&(typeof p.t!=='string'||!RMT_ICON_T.test(p.t)))
      return 'a transform may only be matrix, translate, scale, rotate or skew, with numbers';
    if(!num(p.w,10000)||!num(p.o,1))return 'stroke width and opacity must be numbers in range';
    if((p.c!==undefined&&p.c!=='round'&&p.c!=='square')||(p.j!==undefined&&p.j!=='round'&&p.j!=='bevel'))
      return 'line caps are round or square, and joins round or bevel';
  }
  return '';
}

function useIcons(map){
  for(const k of Object.keys(RMT_ICONS))delete RMT_ICONS[k];
  if(map&&typeof map==='object')for(const k of Object.keys(map))
    if(RMT_ICON_NAME.test(k))RMT_ICONS[k]=map[k];
}

function iconNames(t){
  const out=[];
  if(!t||!Array.isArray(t.sections))return out;
  const look=it=>{
    if(!Array.isArray(it)||typeof it[2]!=='string')return;
    for(const tok of it[2].split(/\s+/))
      if(tok.indexOf('icon:')===0&&out.indexOf(tok.slice(5))<0)out.push(tok.slice(5));
  };
  for(const s of t.sections){
    if(!Array.isArray(s)||s[0]==='lcd')continue;
    // A grid's shared options reach every key in it, an icon included.
    if(s[0]==='grid'&&s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))look(['','',s[1].opts]);
    for(const it of s.slice(1)){
      if(s[0]==='strip'||s[0]==='rocker'){if(Array.isArray(it))it.slice(1).forEach(look)}
      else look(it);
    }
  }
  return out;
}

function icon(i){
  if(typeof i==='string')return '<svg viewBox="0 0 24 24">'+RI[i]+'</svg>';
  if(iconBad(i))return '';
  return '<svg class="rmt-ico" viewBox="'+i.vb.trim()+'">'+i.p.map(p=>'<path d="'+p.d+'"'+
    (p.f!==undefined?' fill="'+p.f+'"':'')+(p.r?' fill-rule="evenodd"':'')+
    (p.t!==undefined?' transform="'+p.t+'"':'')+(p.s!==undefined?' stroke="'+p.s+'"':'')+
    (p.w!==undefined?' stroke-width="'+p.w+'"':'')+(p.c!==undefined?' stroke-linecap="'+p.c+'"':'')+
    (p.j!==undefined?' stroke-linejoin="'+p.j+'"':'')+(p.o!==undefined?' opacity="'+p.o+'"':'')+
    '/>').join('')+'</svg>';
}

function esc(s){
    return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
  }

function themeValueBad(k,v){
  // Backslashes go first, because CSS resolves escapes while tokenising: \75 rl(
  // becomes url( long after a text search for "url" has already said no. No
  // theme value here has any reason to carry one, so refusing them outright
  // costs nothing and closes every spelling at once.
  if(v.indexOf('\\')>=0)return 'theme values cannot contain a backslash — "'+k+'" could hide a url()';
  if(RMT_FETCH.test(v))return 'theme values cannot load from elsewhere — "'+k+'" would fetch from another host';
  if(k==='clip'&&!RMT_CLIP.test(v.trim()))return 'clip must be a polygon(), e.g. polygon(0% 0%, 100% 0%, 82% 100%, 18% 100%)';
  return '';
}

function btnHtml(item){
    const arr=Array.isArray(item);
    const a=arr?item[0]:item, lab=arr?item[1]:null, opt=arr?item[2]:null;
    // Own properties only, for the reason validateTpl gives. Repeated inline
    // rather than shared: that helper lives in the other IIFE, and hoisting one
    // across is its own job (see the note about esc()).
    const b=Object.prototype.hasOwnProperty.call(RMT_BTNS,a)?RMT_BTNS[a]:null;
    if(!b)return '';   // a style naming a button this firmware doesn't have
    const tip=(lab!=null&&lab!=='')?esc(lab)+' — runs '+a:b.t;
    let cls='',css='',lit='',ic='',hgt=0,ih=0;
    if(typeof opt==='string'){
      for(const tok of opt.split(/\s+/)){
        if(!tok)continue;
        // h:<px> — a whole number inside RMT_KEY_H, tested again here for the
        // same reason the hex is: this is the other value that reaches the
        // inline style attribute, and a stored style never saw the importer.
        if(/^h:\d{2,3}$/.test(tok)){
          const n=Number(tok.slice(2));
          if(n>=RMT_KEY_H[0]&&n<=RMT_KEY_H[1])hgt=n;
          continue;
        }
        // ih:<px> — the same care, for the icon's height.
        if(/^ih:\d{1,3}$/.test(tok)){
          const n=Number(tok.slice(3));
          if(n>=RMT_ICON_H[0]&&n<=RMT_ICON_H[1])ih=n;
          continue;
        }
        // The only value that reaches an inline style attribute, so it is
        // matched against a strict hex pattern and nothing else — a token that
        // got this far already passed the same test at import.
        if(RMT_HEX.test(tok))css='background:'+tok+';border-color:'+tok;
        // lit:<source> — the key goes to its lit colour while that source reads
        // "on". An optional third part colours this one button; without it the
        // style's lit_bg/lit_fg apply. It rides the same map the panels use, so
        // both surfaces already receive the value.
        else if(tok.indexOf('lit:')===0){
          const bits=tok.split(':');
          lit=bits[1]||'';
          // A scoped custom property rather than a second rule: the per-button
          // colour and the theme default then meet in one place, the CSS.
          if(bits[2]&&RMT_HEX.test(bits[2]))css+=(css?';':'')+'--rb-lit-bg:'+bits[2];
        }
        else if(tok.indexOf('icon:')===0)ic=tok.slice(5);
        else if(RMT_OPTS.indexOf(tok)>=0)cls+=' '+tok;
      }
    }
    // Last, so a colour token written after it cannot replace it.
    if(hgt)css+=(css?';':'')+'height:'+hgt+'px';
    // A custom property the .rmt-ih rules read, so every key size and shape
    // takes the one height without a rule per size here.
    if(ih){cls+=' rmt-ih';css+=(css?';':'')+'--rb-ih:'+ih+'px'}
    // icon:<name> puts a picture on the face: one of ours first, then one imported
    // onto the device. The label stays as the tooltip, and is the face again when
    // the icon cannot be found — a key whose logo is missing still says what it is.
    const own=o=>Object.prototype.hasOwnProperty.call(o,ic);
    const pic=!ic?'':own(RI)?icon(ic):own(RMT_ICONS)?icon(RMT_ICONS[ic]):'';
    const face=pic||((lab!=null&&lab!=='')?esc(lab):(b.i?icon(b.i):(b.x||'')));
    return '<button class="rmt-btn'+(b.c?' '+b.c:'')+cls+'" data-action="'+a+'"'+
           (b.r?' data-repeat="1"':'')+(lit?' data-lit="'+esc(lit)+'"':'')+
           (css?' style="'+css+'"':'')+
           ' title="'+tip+'">'+face+'</button>';
  }

function lcdLabel(t,action){
    if(!action)return '';
    // A long press: the key's own label, marked, since the style names only the key.
    if(/.@long$/.test(action))return lcdLabel(t,action.slice(0,-5))+' (long)';
    if(t&&Array.isArray(t.sections))for(const s of t.sections){
      // An lcd line is [label, key], the opposite order, and its key is not an
      // action at all — scanning it would match the wrong half.
      if(s[0]==='lcd')continue;
      for(const it of s.slice(1)){
        if(!Array.isArray(it))continue;
        if(it[0]===action&&typeof it[1]==='string'&&it[1])return it[1];
        // strip and rocker hold their buttons one level down.
        for(const g of it)
          if(Array.isArray(g)&&g[0]===action&&typeof g[1]==='string'&&g[1])return g[1];
      }
    }
    const b=Object.prototype.hasOwnProperty.call(RMT_BTNS,action)?RMT_BTNS[action]:null;
    return (b&&b.t)?b.t:action;
  }

function sectionHtml(s){
    const k=s[0];
    if(k==='-')return '<div class="rmt-divider"></div>';
    let inner='';
    if(k==='row'){
      inner='<div class="rmt-row">'+s.slice(1).map(a=>a==='|'?'<div style="flex:1"></div>':btnHtml(a)).join('')+'</div>';
    }else if(k==='dpad'){
      // The four arrows and a centre, in reading order. Listing none is the
      // usual case and means the standard five.
      const d=s.length>1?s.slice(1):['up','left','ok','right','down'];
      inner='<div class="rmt-dpad"><div class="empty"></div>'+btnHtml(d[0])+'<div class="empty"></div>'+
            btnHtml(d[1])+btnHtml(d[2])+btnHtml(d[3])+
            '<div class="empty"></div>'+btnHtml(d[4])+'<div class="empty"></div></div>';
    }else if(k==='ring'){
      // Same five actions as a dpad, drawn as the nav ring instead. Wrappers do
      // the positioning so each button keeps its own press transform.
      // An optional {"size","center"} first. Whole numbers inside their bounds
      // only, tested here as well as at import: they reach an inline style.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:null;
      const keys=s.slice(cfg?2:1);
      const d=keys.length?keys:['up','left','ok','right','down'];
      const sz=cfg?cfg.size|0:0,cz=cfg?cfg.center|0:0;
      let st='';
      if(sz>=RMT_RING[0]&&sz<=RMT_RING[1])st='width:'+sz+'px;height:'+sz+'px';
      if(cz>=RMT_RING_C[0]&&cz<=RMT_RING_C[1])st+=(st?';':'')+'--rb-ring-c:'+cz+'px';
      const at=(p,i)=>'<span class="'+p+'">'+btnHtml(d[i])+'</span>';
      inner='<div class="rmt-ring"'+(st?' style="'+st+'"':'')+'>'+at('n',0)+at('w',1)+at('c',2)+at('e',3)+at('s',4)+'</div>';
    }else if(k==='rocker'){
      inner='<div class="rmt-rocker">'+s.slice(1).map(g=>{
        // Three entries = a two-way rocker with its label between the halves.
        // Two = a lone key at the same height, which is how mute sits between
        // a volume and a channel rocker.
        if(g.length>=3)
          return '<div class="rmt-rocker-col">'+btnHtml(g[1])+
                 (g[0]?'<span class="rmt-rocker-label">'+esc(g[0])+'</span>':'')+
                 btnHtml(g[2])+'</div>';
        return '<div class="rmt-rocker-col rmt-rocker-solo">'+btnHtml(g[1])+
               (g[0]?'<span class="rmt-rocker-label">'+esc(g[0])+'</span>':'')+'</div>';
      }).join('')+'</div>';
    }else if(k==='strip'){
      inner='<div class="rmt-strip">'+s.slice(1).map((g,i)=>
        (i?'<div style="width:40px"></div>':'')+
        '<div class="rmt-strip-group">'+(g[0]?'<span class="rmt-strip-label">'+esc(g[0])+'</span>':'')+
        g.slice(1).map(a=>btnHtml(a)).join('')+'</div>').join('')+'</div>';
    }else if(k==='media'||k==='apps'){
      inner='<div class="rmt-'+(k==='media'?'media-row':'app-row')+'">'+
            s.slice(1).map(a=>btnHtml(a)).join('')+'</div>';
    }else if(k==='grid'){
      // Equal rectangles in columns. The layout is said once, in an optional
      // settings object — columns, a height, options every key takes — so each
      // key carries only its action and label, and a page of named keys fits a
      // stored style with room to spare. "|" leaves a cell empty.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:null;
      // Whole numbers only, bounded here as well as at import: the column count
      // is the one value this builds into an inline style.
      const cols=cfg&&(cfg.cols|0)>=1&&(cfg.cols|0)<=8?(cfg.cols|0):2;
      const h=cfg?cfg.h|0:0;
      const shared=((cfg&&typeof cfg.opts==='string')?cfg.opts:'')+
                   (h>=RMT_KEY_H[0]&&h<=RMT_KEY_H[1]?' h:'+h:'');
      inner='<div class="rmt-grid" style="grid-template-columns:repeat('+cols+',minmax(0,1fr))">'+
            s.slice(cfg?2:1).map(it=>{
              if(it==='|')return '<div></div>';
              const arr=Array.isArray(it);
              // The key's own options come after the grid's, so its colour or
              // height is the one that lands.
              const own=arr&&typeof it[2]==='string'?it[2]:'';
              return btnHtml([arr?it[0]:it,arr?it[1]:'',(shared+' '+own).trim()]);
            }).join('')+'</div>';
    }else if(k==='lcd'){
      // The panel is drawn empty and filled in afterwards: the device page puts
      // numbers in it from its status poll, the Home Assistant card from entity
      // state. `data-lcd` is the only hook either of them needs, which is what
      // lets one renderer serve two very different sources of truth.
      // An optional settings object in front of the lines sizes the panel the
      // way a character display is specified — 16 across by 2 down. Without it
      // the panel fills the remote's width and grows with whatever it is given.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:null;
      let pstyle='';
      if(cfg){
        // Only ever whole numbers the validator has already bounded, so this is
        // the one place besides a #hex that builds an inline style here.
        // 22px = the panel's own padding and border, counted by box-sizing.
        if(cfg.cols)pstyle+='width:calc('+(cfg.cols|0)+'ch + 22px);';
        // Reserve the height of N lines, so a panel declared taller than its
        // content keeps that size and one whose values change length does not
        // jump. In the stylesheet's own terms rather than pixels: a line box is
        // 1.25em of the panel's pinned font, 4px separates two, and 18px is the
        // padding and border a border-box min-height has to include.
        if(cfg.rows)pstyle+='min-height:calc('+(cfg.rows|0)+' * 1.25em + '+
                            (4*((cfg.rows|0)-1))+'px + 18px);';
        // Straight onto the custom properties the stylesheet already reads, so
        // one panel can be a different colour from another in the same style
        // without a rule of its own. Hex-validated above.
        for(const c of RMT_LCD_COLOURS)
          if(typeof cfg[c]==='string')pstyle+='--rb-lcd-'+c+':'+cfg[c]+';';
      }
      inner='<div class="rmt-lcd'+(cfg&&cfg.cols?' fixed':'')+'"'+
            (pstyle?' style="'+pstyle+'"':'')+'>'+s.slice(cfg?2:1).map(g=>{
        const lab=g[0]||'',key=g[1]||'';
        let sz='',align='',css='';
        if(typeof g[2]==='string')for(const tok of g[2].split(/\s+/)){
          if(!tok)continue;
          // Same strict hex test the buttons use, and for the same reason: it is
          // the only value here that reaches an inline style attribute.
          if(RMT_HEX.test(tok))css='color:'+tok;
          // Both spellings of the middle one, mapped to the CSS spelling so
          // there is still only one class to style.
          else if(tok==='centre'||tok==='center')align='center';
          else if(tok==='left'||tok==='right')align=tok;
          else if(RMT_LCD_OPTS.indexOf(tok)>=0)sz+=' '+tok;
        }
        // A title has no value to sit opposite, so it centres unless the style
        // said otherwise. Everything else defaults to no alignment class at
        // all, which leaves the label and value spread to opposite edges.
        if(!key)return '<div class="rmt-lcd-line title '+(align||'center')+'">'+
               '<span class="rmt-lcd-label">'+esc(lab)+'</span></div>';
        return '<div class="rmt-lcd-line'+(align?' '+align:'')+'">'+
               (lab?'<span class="rmt-lcd-label">'+esc(lab)+'</span>':'')+
               '<span class="rmt-lcd-val'+sz+'" data-lcd="'+esc(key)+'"'+
               (css?' style="'+css+'"':'')+'>--</span></div>';
      }).join('')+'</div>';
    }
    return '<div class="rmt-section">'+inner+'</div>';
  }

function validateTpl(t){
    // A button's option string, which a grid's settings carry as well: every
    // token checked, '' when they all pass. Refused rather than ignored — a
    // silently dropped typo looks like the renderer is broken. The hex test is
    // the same one btnHtml applies, and is what keeps arbitrary CSS out of the
    // inline style attribute it builds.
    const optsBad=str=>{
      for(const tok of str.split(/\s+/)){
        if(!tok)continue;
        if(tok.indexOf('lit:')===0){
          // lit:<source>[:#hex] — lights the key while that source reads on.
          const bits=tok.split(':');
          if(bits.length>3)return 'lit: takes a source and an optional #hex colour — "'+tok+'"';
          if(!/^[a-z0-9_]{1,16}$/.test(bits[1]||''))
            return 'lit: needs a source name of 1-16 characters, a-z, 0-9 or _ — "'+tok+'"';
          if(bits[2]!==undefined&&!RMT_HEX.test(bits[2]))
            return 'lit: colour must be a #hex value — "'+tok+'"';
          continue;
        }
        // icon:<name> — whether an icon by that name exists is not checked: it
        // may be imported after the style, and a missing one shows the label.
        if(tok.indexOf('icon:')===0){
          if(!RMT_ICON_NAME.test(tok.slice(5)))
            return 'icon: needs a name of 1-15 characters, a-z, 0-9 or _ — "'+tok+'"';
          continue;
        }
        // h:<px> — the key's height, a whole number within RMT_KEY_H.
        if(tok.indexOf('h:')===0){
          const n=/^h:\d{2,3}$/.test(tok)?Number(tok.slice(2)):NaN;
          if(!(n>=RMT_KEY_H[0]&&n<=RMT_KEY_H[1]))
            return 'h: sets a height of '+RMT_KEY_H[0]+'-'+RMT_KEY_H[1]+' pixels, e.g. h:80 — "'+tok+'"';
          continue;
        }
        // ih:<px> — the icon's height, a whole number within RMT_ICON_H.
        if(tok.indexOf('ih:')===0){
          const n=/^ih:\d{1,3}$/.test(tok)?Number(tok.slice(3)):NaN;
          if(!(n>=RMT_ICON_H[0]&&n<=RMT_ICON_H[1]))
            return 'ih: sets an icon height of '+RMT_ICON_H[0]+'-'+RMT_ICON_H[1]+' pixels, e.g. ih:36 — "'+tok+'"';
          continue;
        }
        if(!RMT_HEX.test(tok)&&RMT_OPTS.indexOf(tok)<0)
          return 'Unknown button option "'+tok+'" — use a #hex colour, lit:<source>, icon:<name>, h:<px>, ih:<px> or '+RMT_OPTS.join(', ');
      }
      return '';
    };
    if(!t||typeof t!=='object'||Array.isArray(t))return 'Top level must be a JSON object';
    if(typeof t.id!=='string'||!/^[a-z0-9_]{1,15}$/.test(t.id))
      return 'id must be 1-15 characters of a-z, 0-9 or _';
    if(RMT_BUILTIN.some(b=>b.id===t.id))return '"'+t.id+'" is a built-in style — give yours another id';
    if(typeof t.name!=='string'||!t.name.trim()||t.name.length>24)return 'name is required (max 24 characters)';
    if(!Array.isArray(t.sections)||!t.sections.length)return 'sections must be a non-empty array';
    if(t.theme!==undefined&&(typeof t.theme!=='object'||t.theme===null||Array.isArray(t.theme)))
      return 'theme must be an object';
    // Theme values are handed to setProperty, which rejects malformed CSS on its
    // own — but a fetch is well-formed and would have the page load from
    // somewhere else the moment a style is applied. Nothing here needs one.
    // Same test the renderer applies, so what imports is what draws.
    if(t.theme)for(const k in t.theme){
      if(typeof t.theme[k]!=='string')continue;
      const bad=themeValueBad(k,t.theme[k]);
      if(bad)return bad;
    }
    // The icons an exported style carries along with it. Optional; a style kept on
    // the device never has them, since its icons are stored on their own.
    if(t.icons!==undefined){
      if(!t.icons||typeof t.icons!=='object'||Array.isArray(t.icons))
        return 'icons must be an object of name: icon';
      const names=Object.keys(t.icons);
      if(names.length>RMT_ICON_MAX)return 'A style can carry up to '+RMT_ICON_MAX+' icons';
      for(const n of names){
        if(!RMT_ICON_NAME.test(n))return 'Icon names are 1-15 characters of a-z, 0-9 or _ — "'+n+'"';
        if(Object.prototype.hasOwnProperty.call(RI,n))return 'Icon "'+n+'" has the name of a built-in icon — rename it';
        const bad=iconBad(t.icons[n]);
        if(bad)return 'Icon "'+n+'": '+bad;
        if(JSON.stringify(t.icons[n]).length>RMT_ICON_LEN)return 'Icon "'+n+'" is over '+RMT_ICON_LEN+' characters';
      }
    }
    for(const s of t.sections){
      if(!Array.isArray(s)||typeof s[0]!=='string')return 'Each section is an array starting with its kind';
      if(RMT_KINDS.indexOf(s[0])<0)return 'Unknown section kind "'+s[0]+'" — use '+RMT_KINDS.join(', ');
      let items=[];
      if(s[0]==='strip'||s[0]==='rocker'){
        for(const g of s.slice(1)){
          if(!Array.isArray(g))return 'Each '+s[0]+' group is an array: ["Label","action",…]';
          if(typeof g[0]!=='string')return 'A '+s[0]+' group starts with its label — "" for none';
          if(g[0].length>8)return 'Group labels are up to 8 characters — "'+g[0]+'" is too wide';
          if(s[0]==='rocker'&&(g.length<2||g.length>3))
            return 'A rocker group is ["Label","up","down"], or ["Label","action"] for a single key';
          items=items.concat(g.slice(1));
        }
      }else if(s[0]==='dpad'||s[0]==='ring'){
        let keys=s.slice(1);
        // A ring may open with {"size","center"}, the way a grid opens with its
        // layout. A dpad keeps its fixed cluster.
        if(s[0]==='ring'&&keys.length&&keys[0]&&typeof keys[0]==='object'&&!Array.isArray(keys[0])){
          const cfg=keys[0];
          keys=keys.slice(1);
          for(const k in cfg){
            if(k!=='size'&&k!=='center')return 'A ring takes size and center — "'+k+'" is neither';
            const n=cfg[k];
            if(typeof n!=='number'||!isFinite(n)||n!==Math.floor(n))
              return 'ring "'+k+'" must be a whole number of pixels, e.g. {"size":200,"center":96}';
          }
          if(cfg.size!==undefined&&(cfg.size<RMT_RING[0]||cfg.size>RMT_RING[1]))
            return 'ring "size" is '+RMT_RING[0]+'-'+RMT_RING[1]+' pixels — "'+cfg.size+'" is outside that';
          if(cfg.center!==undefined&&(cfg.center<RMT_RING_C[0]||cfg.center>RMT_RING_C[1]))
            return 'ring "center" is '+RMT_RING_C[0]+'-'+RMT_RING_C[1]+' pixels — "'+cfg.center+'" is outside that';
          const size=cfg.size!==undefined?cfg.size:168,center=cfg.center!==undefined?cfg.center:84;
          if(center>size-2*RMT_RING_ROOM)
            return 'ring "center" '+center+' leaves the arrows no room in a '+size+'px ring — at most '+
                   (size-2*RMT_RING_ROOM)+', or a larger "size"';
        }
        if(keys.length&&keys.length!==5)
          return 'A '+s[0]+' section lists exactly 5 actions (up, left, centre, right, down) or none';
        items=keys;
      }else if(s[0]==='lcd'){
        // Lines are ["Label","key"], label first — the way a strip or rocker
        // group carries its label. Nothing in here is a button, so `items` is
        // left empty and the button loop below never sees them.
        // An optional settings object in front of the lines gives the panel a
        // size in characters and lines, the way a display is specified.
        let lines=s.slice(1),cfg=null;
        if(lines.length&&lines[0]&&typeof lines[0]==='object'&&!Array.isArray(lines[0])){
          cfg=lines[0];
          lines=lines.slice(1);
          for(const k in cfg){
            if(RMT_LCD_COLOURS.indexOf(k)>=0){
              // Hex and nothing else: these land in an inline style, the same
              // rule a button's colour token follows.
              if(typeof cfg[k]!=='string'||!RMT_HEX.test(cfg[k]))
                return 'lcd "'+k+'" must be a #hex colour, e.g. {"'+k+'":"#6ee7a0"}';
              continue;
            }
            if(k!=='cols'&&k!=='rows')
              return 'An lcd panel takes cols, rows, '+RMT_LCD_COLOURS.join(', ')+
                     ' — "'+k+'" is none of them';
            const n=cfg[k];
            if(typeof n!=='number'||!isFinite(n)||n!==Math.floor(n))
              return 'lcd "'+k+'" must be a whole number, e.g. {"cols":16,"rows":2}';
          }
          if(cfg.cols!==undefined&&(cfg.cols<4||cfg.cols>40))
            return 'lcd "cols" is 4-40 characters — "'+cfg.cols+'" is outside that';
          if(cfg.rows!==undefined&&(cfg.rows<1||cfg.rows>8))
            return 'lcd "rows" is 1-8 lines — "'+cfg.rows+'" is outside that';
        }
        if(!lines.length)return 'An lcd section needs at least one line: ["lcd",["Room","temp"]]';
        // Declaring rows is also declaring how many lines fit: a panel with
        // more lines than rows would draw outside the height it asked for.
        const maxLines=(cfg&&cfg.rows)?cfg.rows:8;
        if(lines.length>maxLines)
          return 'This lcd panel holds '+maxLines+' line'+(maxLines===1?'':'s')+
                 ' — it has '+lines.length+(cfg&&cfg.rows?'. Raise "rows", or add a second ["lcd",…]':
                 '. Add a second ["lcd",…] for more');
        for(const g of lines){
          if(!Array.isArray(g)||g.length<2||g.length>3)
            return 'An lcd line is ["Label","key"] or ["Label","key","opts"]';
          if(typeof g[0]!=='string')return 'An lcd line starts with its label — "" for none';
          // Same 16 a button label gets: one number to remember, and a title
          // line has the whole panel to itself. A long label beside a value
          // squeezes it, which is visible and the author's to judge.
          if(g[0].length>16)return 'lcd labels are up to 16 characters — "'+g[0]+'" is too wide';
          if(typeof g[1]!=='string')
            return 'An lcd line names the value to show, or "" for a label-only title';
          if(g[1].charAt(0)==='@'){
            if(RMT_LCD_KEYS.indexOf(g[1])<0)
              return 'Unknown built-in value "'+g[1]+'" — use '+RMT_LCD_KEYS.join(', ');
          }else if(g[1]&&!/^[a-z0-9_]{1,16}$/.test(g[1])){
            // Only the spelling of a declared key can be checked: whether the
            // node actually has a sources entry by that name is something
            // the browser cannot know. One that names nothing draws as "--",
            // which is what a real display does with a missing reading.
            return 'An lcd key is 1-16 characters of a-z, 0-9 or _, or a built-in such as @host';
          }
          if(g.length===3){
            if(typeof g[2]!=='string')return 'lcd options must be a string, e.g. "lg right"';
            for(const tok of g[2].split(/\s+/)){
              if(!tok)continue;
              if(!RMT_HEX.test(tok)&&RMT_LCD_OPTS.indexOf(tok)<0)
                return 'Unknown lcd option "'+tok+'" — use a #hex colour or '+RMT_LCD_OPTS.join(', ');
            }
          }
        }
      }else if(s[0]==='grid'){
        // An optional settings object first — cols, h, and the options every key
        // takes — then the keys, each written as it would be in a row.
        let keys=s.slice(1);
        if(keys.length&&keys[0]&&typeof keys[0]==='object'&&!Array.isArray(keys[0])){
          const cfg=keys[0];
          keys=keys.slice(1);
          for(const k in cfg){
            if(k==='opts'){
              if(typeof cfg.opts!=='string')
                return 'grid "opts" is a string of button options, e.g. {"opts":"sq light"}';
              const bad=optsBad(cfg.opts);
              if(bad)return bad;
              continue;
            }
            if(k!=='cols'&&k!=='h')return 'A grid takes cols, h and opts — "'+k+'" is none of them';
            const n=cfg[k];
            if(typeof n!=='number'||!isFinite(n)||n!==Math.floor(n))
              return 'grid "'+k+'" must be a whole number, e.g. {"cols":2,"h":56}';
          }
          if(cfg.cols!==undefined&&(cfg.cols<1||cfg.cols>8))
            return 'grid "cols" is 1-8 — "'+cfg.cols+'" is outside that';
          if(cfg.h!==undefined&&(cfg.h<RMT_KEY_H[0]||cfg.h>RMT_KEY_H[1]))
            return 'grid "h" is '+RMT_KEY_H[0]+'-'+RMT_KEY_H[1]+' pixels — "'+cfg.h+'" is outside that';
        }
        if(!keys.length)return 'A grid needs at least one key: ["grid",{"cols":2},["spare1","Copy"]]';
        items=keys;
      }else{
        items=s.slice(1);
      }
      for(const it of items){
        if(it==='|'&&(s[0]==='row'||s[0]==='grid'))continue;
        // "action", ["action","Label"], or ["action","Label","opts"].
        const arr=Array.isArray(it);
        if(arr&&(it.length<2||it.length>3))
          return 'A button is ["action","Label"] or ["action","Label","opts"]';
        if(arr&&typeof it[1]!=='string')return 'A button label must be text ("" to keep the icon)';
        // 16 is what the widest button (an app pill) carries; a round key fits
        // about four, which is the caller's problem rather than an error.
        if(arr&&it[1].length>16)
          return 'Button labels are up to 16 characters — "'+it[1]+'" will not fit a key';
        if(arr&&it.length===3){
          if(typeof it[2]!=='string')return 'Button options must be a string, e.g. "light sm"';
          const bad=optsBad(it[2]);
          if(bad)return bad;
        }
        const a=arr?it[0]:it;
        // hasOwnProperty rather than a truthiness test: RMT_BTNS is an object
        // literal, so RMT_BTNS['constructor'] and ['__proto__'] are inherited and
        // truthy. ["row",["constructor","Hi"]] used to pass a check that claims
        // to name every unknown button, and btnHtml then drew a dead key for it.
        if(typeof a!=='string'||!Object.prototype.hasOwnProperty.call(RMT_BTNS,a))
          return 'Unknown button "'+a+'"';
      }
    }
    return '';
  }

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
export const RMT_CSS = `
.ico-face .rmt-btn{cursor:default}
.rmt-body,.rmt-body *{box-sizing:border-box}
.rmt-body{zoom:var(--rb-zoom,1);background:var(--rb-bg,transparent);border:1px solid var(--rb-border,transparent);border-radius:var(--rb-radius,0);padding:var(--rb-pad,0);max-width:var(--rb-maxw,none);margin:0 auto;box-shadow:var(--rb-shadow,none);clip-path:var(--rb-clip,none)}
.rmt-section{margin-bottom:10px}
.rmt-section:last-child{margin-bottom:0}
.rmt-row{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:8px;margin-bottom:8px}
.rmt-row:last-child{margin-bottom:0}
.rmt-btn{width:48px;height:48px;padding:0;margin:0;border:1px solid var(--rb-btn-border,var(--border));border-radius:var(--rb-btn-radius,50%);background:var(--rb-btn-bg,var(--bg));color:var(--rb-btn-fg,var(--fg));font-size:12px;font-weight:500;cursor:pointer;touch-action:manipulation;display:flex;align-items:center;justify-content:center;transition:background .1s,transform .1s;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
.rmt-btn:active,.rmt-btn.p{background:var(--active);color:#fff;border-color:var(--active);transform:scale(.93)}
.rmt-btn svg{width:20px;height:20px;fill:currentColor;pointer-events:none}
.rmt-btn.power{background:#c62828;color:#fff;border-color:#c62828}
.rmt-btn.power:active,.rmt-btn.power.p{background:#e53935}
.rmt-btn.held{background:var(--accent);color:#fff;border-color:var(--accent)}
.rmt-btn.has-long{background-image:radial-gradient(circle at 50% calc(100% - 6px),var(--accent) 0 2px,transparent 2.5px)!important}
.rmt-btn.long{box-shadow:0 0 0 3px var(--accent)}
.rmt-dpad{display:grid;grid-template-columns:48px 48px 48px;grid-template-rows:48px 48px 48px;gap:4px;justify-content:center;margin:8px 0}
.rmt-dpad .rmt-btn{border-radius:12px}
.rmt-dpad .center{background:var(--rb-ok-bg,var(--active));color:var(--rb-ok-fg,#fff);border-color:var(--rb-ok-bg,var(--active));font-size:11px;font-weight:700;border-radius:50%}
.rmt-dpad .center:active,.rmt-dpad .center.p{background:var(--accent)}
.rmt-dpad .empty{visibility:hidden}
.rmt-strip{display:flex;align-items:flex-start;justify-content:center;gap:16px}
.rmt-strip-group{display:flex;flex-direction:column;align-items:center;gap:4px}
.rmt-strip-label{font-size:10px;color:var(--rb-label,var(--muted));font-weight:600;text-transform:uppercase}
.rmt-divider{height:1px;background:var(--rb-divider,var(--border));margin:10px 0}
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
.rmt-grid{display:grid;gap:8px}
.rmt-grid .rmt-btn{border-radius:var(--rb-btn-radius,10px)}
.rmt-btn.app{width:auto;height:38px;border-radius:19px;padding:0 14px;font-size:11px}
.rmt-ring{position:relative;width:168px;height:168px;margin:10px auto;border-radius:50%;
  background:var(--rb-ring-bg,var(--rb-btn-bg,var(--bg)));border:1px solid var(--rb-btn-border,var(--border))}
.rmt-ring>span{position:absolute;display:flex}
.rmt-ring>span.n{top:6px;left:50%;transform:translateX(-50%)}
.rmt-ring>span.s{bottom:6px;left:50%;transform:translateX(-50%)}
.rmt-ring>span.w{left:6px;top:50%;transform:translateY(-50%)}
.rmt-ring>span.e{right:6px;top:50%;transform:translateY(-50%)}
.rmt-ring>span.c{left:50%;top:50%;transform:translate(-50%,-50%)}
.rmt-ring .rmt-btn{background:none;border-color:transparent;color:var(--rb-ring-fg,var(--rb-btn-fg,var(--fg)))}
.rmt-ring .rmt-btn:active,.rmt-ring .rmt-btn.p{background:rgba(255,255,255,.18);border-color:transparent}
.rmt-ring .center{width:var(--rb-ring-c,84px);height:var(--rb-ring-c,84px)}
.rmt-rocker{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:14px}
.rmt-rocker-col{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;
  padding:4px 0;width:50px;border-radius:25px;background:var(--rb-btn-bg,var(--bg));
  border:1px solid var(--rb-btn-border,var(--border))}
.rmt-rocker-col .rmt-btn{background:none;border-color:transparent;height:40px}
.rmt-rocker-col .rmt-btn:active,.rmt-rocker-col .rmt-btn.p{background:rgba(255,255,255,.18);border-color:transparent}
.rmt-rocker-label{font-size:9px;color:var(--rb-label,var(--muted));font-weight:700;text-transform:uppercase;letter-spacing:.4px}
.rmt-rocker-solo{background:none;border:none;padding:0;width:auto}
.rmt-lcd{background:var(--rb-lcd-bg,#11161c);border:1px solid var(--rb-lcd-border,rgba(0,0,0,.55));
  border-radius:var(--rb-lcd-radius,8px);padding:8px 10px;box-shadow:inset 0 1px 3px rgba(0,0,0,.55);
  font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;
  font-variant-numeric:tabular-nums;overflow:hidden;
  
  font-size:14px;box-sizing:border-box;margin:0 auto}
.rmt-lcd.fixed .rmt-lcd-line{overflow:hidden}
.rmt-lcd.fixed .rmt-lcd-val{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0}
.rmt-lcd-line{display:flex;align-items:baseline;justify-content:space-between;gap:10px;min-height:16px}
.rmt-lcd-line+.rmt-lcd-line{margin-top:4px}
.rmt-lcd-line.left{justify-content:flex-start}
.rmt-lcd-line.center{justify-content:center}
.rmt-lcd-line.right{justify-content:flex-end}
.rmt-lcd-line.left .rmt-lcd-val{text-align:left}
.rmt-lcd-line.center .rmt-lcd-val{text-align:center}
.rmt-lcd-line.title .rmt-lcd-label{white-space:normal}
.rmt-lcd-label{font-size:9px;font-weight:700;letter-spacing:.5px;text-transform:uppercase;
  color:var(--rb-lcd-label,#7a8a80);white-space:nowrap}
.rmt-lcd-line.title .rmt-lcd-label{font-size:10px}
.rmt-lcd-val{font-size:14px;font-weight:600;color:var(--rb-lcd-fg,#6ee7a0);
  text-align:right;overflow-wrap:anywhere;line-height:1.25}
.rmt-lcd-val.sm{font-size:11px}
.rmt-lcd-val.lg{font-size:18px}
.rmt-lcd-val.xl{font-size:24px}
.rmt-btn.sm{width:36px;height:36px;font-size:11px}
.rmt-btn.sm svg{width:16px;height:16px}
.rmt-btn.md{width:42px;height:42px;font-size:12px}
.rmt-btn.md svg{width:18px;height:18px}
.rmt-btn.lg{width:56px;height:56px}
.rmt-btn.xl{width:64px;height:64px;font-size:13px}
.rmt-btn.xl svg{width:26px;height:26px}
.rmt-btn.wide{width:auto;min-width:56px;padding:0 14px;border-radius:22px}
.rmt-btn.sq{border-radius:10px}
.rmt-btn.fill{flex:1 1 0;width:auto;min-width:0;padding:0 8px;overflow:hidden;text-align:center;line-height:1.2}
.rmt-grid .rmt-btn{width:auto;min-width:0;padding:0 6px;overflow:hidden;text-align:center;line-height:1.2}
.rmt-btn.squircle{border-radius:42%}
@supports (corner-shape:superellipse(1.4)){.rmt-btn.squircle{border-radius:50%;corner-shape:superellipse(1.4)}}
.rmt-btn svg.rmt-ico{width:auto;max-width:78%;height:20px}
.rmt-btn.sm svg.rmt-ico{height:16px}
.rmt-btn.md svg.rmt-ico{height:18px}
.rmt-btn.xl svg.rmt-ico{height:26px}
.rmt-btn.wide svg.rmt-ico,.rmt-btn.app svg.rmt-ico{max-width:120px;height:18px}
.rmt-btn.fill svg.rmt-ico{max-width:80%}
.rmt-btn.rmt-ih svg{width:var(--rb-ih);height:var(--rb-ih);max-height:100%}
.rmt-btn.rmt-ih svg.rmt-ico{width:auto;height:var(--rb-ih)}
.rmt-btn.rmt-ih.wide svg.rmt-ico,.rmt-btn.rmt-ih.app svg.rmt-ico{max-width:none}
.rmt-btn.lit{background:var(--rb-lit-bg,var(--rb-ok-bg,var(--active)));
  color:var(--rb-lit-fg,#fff);border-color:var(--rb-lit-bg,var(--rb-ok-bg,var(--active)))}
.rmt-btn.light{background:var(--rb-light-bg,#e9e9ee);color:var(--rb-light-fg,#16161a);border-color:var(--rb-light-bg,#e9e9ee)}
.rmt-btn.light:active,.rmt-btn.light.p{background:#fff;color:#000}
.popout .rmt-body{box-shadow:var(--rb-shadow,0 0 #0000),0 4px 0 var(--rb-bg,transparent)}
.rmt-head{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin-left:auto}
.rmt-head .macro-edit-btn{margin-left:0}
`;

export { RI, RMT_BTNS, RMT_VARS, RMT_BUILTIN, RMT_KINDS, RMT_OPTS, RMT_KEY_H, RMT_LCD_OPTS, RMT_LCD_COLOURS, RMT_LCD_KEYS, RMT_LCD_LABELLED, lcdLabel, RMT_HEX, RMT_CLIP, RMT_FETCH, icon, esc, RMT_ICON_NAME, RMT_ICON_LEN, RMT_ICON_MAX, RMT_ICONS, iconBad, useIcons, iconNames, themeValueBad, btnHtml, sectionHtml, validateTpl };
