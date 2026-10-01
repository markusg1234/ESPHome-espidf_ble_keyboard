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

const RMT_KINDS=['row','dpad','ring','pot','slider','strip','rocker','media','apps','grid','lcd','side','-'];

const RMT_OPTS=['light','sm','md','lg','xl','wide','sq','squircle','fill'];

const RMT_KEY_H=[24,160];

const RMT_ICON_H=[8,160];

const RMT_RING=[120,320],RMT_RING_C=[40,200],RMT_RING_ROOM=40;

const RMT_KNOB=[32,320],RMT_KNOB_C=[16,280],RMT_KNOB_ROOM=8,RMT_KNOB_STEP=[10,90],RMT_KNOB_SWEEP=[90,360];

const RMT_SLIDER=[30,480],RMT_SLIDER_W=[10,120],RMT_SLIDER_T=[8,120];

const RMT_SIDE_GAP=[0,200];

const KNOB_SET={};

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
  const sec=(s,inSide)=>{
    if(!Array.isArray(s)||s[0]==='lcd')return;
    // A side section's parts are sections of their own, one level deep.
    if(s[0]==='side'){if(!inSide)s.slice(1).forEach(c=>sec(c,true));return}
    // A grid's shared options reach every key in it, an icon included.
    if(s[0]==='grid'&&s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))look(['','',s[1].opts]);
    for(const it of s.slice(1)){
      if(s[0]==='strip'||s[0]==='rocker'){if(Array.isArray(it))it.slice(1).forEach(look)}
      else look(it);
    }
  };
  for(const s of t.sections)sec(s,false);
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

function knobShow(k,v){
  const min=+k.dataset.min,max=+k.dataset.max,sw=+k.dataset.sweep;
  k._lv=v;
  k.classList.toggle('nolevel',v===null);
  const f=v===null?0:Math.min(1,Math.max(0,(v-min)/(max-min)));
  // A slider: its fill and thumb that share of the way along.
  if(k.classList.contains('rmt-slider')){k.style.setProperty('--rb-sl',String(f));return}
  const face=k.querySelector('.rmt-knob-face'),arc=k.querySelector('.rmt-knob-gauge .lv');
  if(face)face.style.transform='rotate('+(-sw/2+sw*f)+'deg)';
  if(arc)arc.style.strokeDasharray=(sw*f)+' 360';
}

const KNOB_SETTLE_MS=5000,KNOB_HOLD_MS=10000;

function knobHold(k,v,ms){
  k._exp={v:v,until:Date.now()+ms,base:k._rd===undefined?null:k._rd};
  knobShow(k,v);
  knobSettle(k);
}

function knobSettle(k){
  clearTimeout(k._lt);
  const e=k._exp;
  if(!e)return;
  const lv=k._rd===undefined?null:k._rd,now=Date.now();
  if(lv!==null&&Math.abs(lv-e.v)<=(+k.dataset.inc||0)/2){k._exp=null;knobShow(k,lv);return}
  let wait=e.until-now;
  if(wait<=0){
    const cap=e.until+KNOB_HOLD_MS-now;
    wait=lv===e.base?cap:Math.min(cap,(k._rdAt||0)+KNOB_SETTLE_MS-now);
  }
  if(wait>0){k._lt=setTimeout(()=>knobSettle(k),wait);return}
  k._exp=null;
  knobShow(k,lv);
}

function knobLevel(root,vals){
  root.querySelectorAll('.rmt-knob[data-level],.rmt-slider[data-level]').forEach(k=>{
    const v=parseFloat(vals?vals[k.dataset.level]:NaN),lv=isFinite(v)?v:null;
    // When it last changed: a reading that has stopped short is believed only
    // once it has stayed put a while.
    if(k._rdAt===undefined||lv!==k._rd)k._rdAt=Date.now();
    k._rd=lv;
    if(k._exp){knobSettle(k);return}
    knobShow(k,lv);
  });
  root.querySelectorAll('.rmt-knob[data-set]:not([data-level]),.rmt-slider[data-set]:not([data-level])').forEach(k=>{
    const v=KNOB_SET[k.dataset.set];
    knobShow(k,typeof v==='number'?v:null);
  });
}

function knobWire(root,send){
  const KS=new WeakMap();
  const state=k=>{
    let s=KS.get(k);
    if(!s){s={a:0,q:[],busy:false,next:0,wheel:0,wt:0};KS.set(k,s)}
    return s;
  };
  // One request at a time, and what piles up behind it goes as one run. The
  // device answers before an action has run and queues only a few, so a quick
  // spin sent step by step would outrun it and lose steps; and the next run
  // waits out the last one's steps, at about what one takes there.
  const pump=s=>{
    if(s.busy||!s.q.length)return;
    s.busy=true;
    const wait=s.next-Date.now();
    if(wait>0){setTimeout(()=>{s.busy=false;pump(s)},wait);return}
    const run=s.q.shift();
    s.next=Date.now()+Math.max(100,60*run[1]);
    let done=false,guard=0,p;
    const fin=()=>{if(done)return;done=true;clearTimeout(guard);s.busy=false;pump(s)};
    // A send that never settles must not stop the knob for good.
    guard=setTimeout(fin,2000);
    try{p=send(run[0],run[1])}catch(_){}
    Promise.resolve(p).then(fin,fin);
  };
  // n presses of `a` onto the knob's queue, in one run with any just before.
  const queue=(k,a,n)=>{
    const s=state(k),last=s.q[s.q.length-1];
    if(last&&last[0]===a)last[1]+=n;else s.q.push([a,n]);
    pump(s);
  };
  // A value to set, onto the knob's queue. Only the latest matters, so one not
  // yet sent is replaced rather than followed.
  const queueSet=(k,key,v)=>{
    const s=state(k),last=s.q[s.q.length-1],run=key+'='+v;
    if(last&&last[2]===key)last[0]=run;else s.q.push([run,1,key]);
    pump(s);
  };
  // What a level knob takes the level to be: where what it sent is taking it
  // while that is on its way, else the last reading — or, for one that sets a
  // value with no reading to follow, what it last set.
  const believed=k=>{
    if(k._exp)return k._exp.v;
    if(k._rd!==undefined&&k._rd!==null)return k._rd;
    const v=k.dataset.set!==undefined?KNOB_SET[k.dataset.set]:undefined;
    return typeof v==='number'?v:null;
  };
  const within=(k,v)=>Math.min(+k.dataset.max,Math.max(+k.dataset.min,v));
  const flash=(k,dir)=>{
    k.classList.remove('fu','fd');void k.offsetWidth;k.classList.add(dir>0?'fu':'fd');
    clearTimeout(k._ft);k._ft=setTimeout(()=>k.classList.remove('fu','fd'),180);
  };
  // A knob that sets a value sends it, once: "spare12=40", which runs that
  // key's Host Action with {value} in it replaced by 40 — a Home Assistant
  // brightness or volume in one call. Kept on inc's grid, from min.
  const sendSet=(k,v)=>{
    if(k.classList.contains('off'))return;
    const mn=+k.dataset.min,inc=+k.dataset.inc||1;
    v=Math.round(within(k,mn+Math.round((v-mn)/inc)*inc)*1e4)/1e4;
    KNOB_SET[k.dataset.set]=v;
    // One with a reading to follow holds the value until the reading has it;
    // one without has only what it set, which knobLevel shows from KNOB_SET.
    if(k.dataset.level!==undefined)knobHold(k,v,3000);else knobShow(k,v);
    if(navigator.vibrate)try{navigator.vibrate(5)}catch(_){}
    queueSet(k,k.dataset.set,String(v));
  };
  const turn=(k,dir,buzz)=>{
    if(k.classList.contains('off'))return false;
    // One that sets a value steps the value, by inc, and sets that.
    if(k.dataset.set!==undefined){
      const b=believed(k);
      sendSet(k,(b===null?+k.dataset.min:b)+dir*(+k.dataset.inc||1));
      flash(k,dir);
      return true;
    }
    const a=dir>0?k.dataset.up:k.dataset.down;
    if(!a)return false;
    if(k.dataset.level!==undefined){
      // A knob showing a level points at the reading, not at the turn. Each step
      // flashes the side it went; with an inc the pointer also moves that much
      // ahead of the reading, which knobLevel then lets catch up.
      const inc=+k.dataset.inc,b=believed(k);
      if(inc&&b!==null)knobHold(k,within(k,b+dir*inc),1500);
      flash(k,dir);
    }else{
      // The face clicks round a step at a time, so it shows what was sent.
      const s=state(k);
      s.a+=dir*(+k.dataset.step||30);
      const f=k.querySelector('.rmt-knob-face');
      if(f)f.style.transform='rotate('+s.a+'deg)';
    }
    if(buzz&&navigator.vibrate)try{navigator.vibrate(5)}catch(_){}
    queue(k,a,1);
    return true;
  };
  // A pot sets a level rather than stepping it: the knob is dragged or tapped
  // to where it should be, and the difference from what it believes the level
  // is goes as that many presses, inc a press — a keyboard can only go up and
  // down. The pointer waits there for the reading to arrive.
  const potSet=(k,from,to)=>{
    const n=Math.round((to-from)/(+k.dataset.inc||1));
    const a=n>0?k.dataset.up:k.dataset.down;
    if(!n||!a||k.classList.contains('off')){knobShow(k,believed(k));return}
    knobHold(k,within(k,from+n*(+k.dataset.inc||1)),1500+Math.abs(n)*80);
    if(navigator.vibrate)try{navigator.vibrate(5)}catch(_){}
    queue(k,a,Math.abs(n));
  };
  // Where along a pot's sweep a bearing lies, 0 at min and 1 at max, or null in
  // the gap at its foot, between the stops.
  const potAt=(k,t)=>{
    const sw=+k.dataset.sweep,th=((t+90+540)%360)-180;   // from twelve o'clock
    return Math.abs(th)<=sw/2?(th+sw/2)/sw:null;
  };
  const potVal=(k,f)=>+k.dataset.min+f*(+k.dataset.max-+k.dataset.min);
  let drag=null;
  // The pointer's bearing from the knob's middle, clockwise from three o'clock.
  // Close to the middle it swings wildly for the smallest movement, so nothing
  // there counts, and the next point outside starts afresh.
  const bearing=(d,x,y)=>{
    const dx=x-d.cx,dy=y-d.cy;
    return Math.hypot(dx,dy)<d.r*0.2?null:Math.atan2(dy,dx)*180/Math.PI;
  };
  // A − or + held down, or a pot held still where a tap would step it: after a
  // moment it goes on stepping that way until let go — a lost release, a tab
  // switched away, or two hundred steps end it too, so nothing runs on unseen.
  let rep=null;
  const stopRep=()=>{if(rep){clearTimeout(rep.t);clearInterval(rep.i);rep=null}};
  const holdStep=(id,k,dir)=>{
    stopRep();
    const r={id:id,n:0};
    r.t=setTimeout(()=>{r.i=setInterval(()=>{
      if(rep!==r||++r.n>200||!k.isConnected){if(rep===r)stopRep();return}
      turn(k,dir,true);
    },150)},400);
    rep=r;
  };
  // A pot's own release handler runs after this one, so tell it the hold
  // stepped by itself, or it would add the tap's step on top.
  const endRep=e=>{if(rep&&e.pointerId===rep.id){if(rep.pot&&rep.i)rep.pot.held=true;stopRep()}};
  root.addEventListener('pointerup',endRep);
  root.addEventListener('pointercancel',endRep);
  root.addEventListener('lostpointercapture',endRep);
  document.addEventListener('visibilitychange',()=>{if(document.hidden)stopRep()});
  // A slider's own key, pressed by a tap on its thumb (or Enter or Space on the
  // slider): one ordinary press, which the thumb shows.
  const pressThumb=k=>{
    const a=k.dataset.press;
    if(!a||k.classList.contains('off')||k.classList.contains('nopress'))return;
    const t=k.querySelector('.rmt-slider-thumb');
    if(t){t.classList.add('p');setTimeout(()=>t.classList.remove('p'),150)}
    if(navigator.vibrate)try{navigator.vibrate(5)}catch(_){}
    queue(k,a,1);
  };
  // Where along a slider's track a point lies, 0 at its min end and 1 at max:
  // left to right across, bottom to top upright. The thumb stops half its size
  // short of each end, so the ends are taken off as it is drawn.
  const sliderAt=(d,x,y)=>{
    const r=d.rect,len=d.vert?r.height:r.width,p=d.vert?r.bottom-y:x-r.left;
    return Math.min(1,Math.max(0,(p-d.ins)/Math.max(1,len-2*d.ins)));
  };
  root.addEventListener('pointerdown',e=>{
    if(!e.target.closest||(e.pointerType==='mouse'&&e.button!==0))return;
    // A slider's − and +: a step each, as on a knob, and held they go on
    // stepping until let go.
    const mk=e.target.closest('.rmt-slider-mk');
    if(mk){
      const k=mk.closest('.rmt-slider');
      if(k&&!k.classList.contains('off')){
        e.preventDefault();
        const dir=mk.classList.contains('up')?1:-1;
        turn(k,dir,true);
        try{mk.setPointerCapture(e.pointerId)}catch(_){}
        holdStep(e.pointerId,k,dir);
      }
      return;
    }
    // A slider goes to where it is pressed and follows the finger along, held
    // at its two ends. Let go and it sends what it was put at — as the value,
    // or as the presses from the reading — as a knob's pot does.
    const tr=e.target.closest('.rmt-slider-track');
    if(tr){
      if(drag&&drag.ring.isConnected&&drag.ring.hasPointerCapture(drag.id))return;
      const k=tr.closest('.rmt-slider');
      if(!k||k.classList.contains('off'))return;
      const sets=k.dataset.set!==undefined,from=believed(k);
      // On the thumb of a slider with a key of its own, it may be a tap: it
      // moves only once dragged, and let go where it was it presses the key.
      const thumb=!!(k.dataset.press&&e.target.closest('.rmt-slider-thumb'));
      const can=sets||from!==null;   // without either there is nothing to work presses out from
      if(!can&&!thumb)return;
      e.preventDefault();
      tr.classList.add('pf');
      try{tr.focus({preventScroll:true})}catch(_){}
      try{tr.setPointerCapture(e.pointerId)}catch(_){}
      const vt=k.classList.contains('vert'),rc=tr.getBoundingClientRect(),
            tb=k.querySelector('.rmt-slider-thumb').getBoundingClientRect();
      drag={k:k,ring:tr,id:e.pointerId,slider:true,vert:vt,rect:rc,
            ins:Math.min((vt?tb.height:tb.width)/2,(vt?rc.height:rc.width)/2),
            from:from,sets:sets,x0:e.clientX,y0:e.clientY,moved:false,thumb:thumb,can:can,to:null};
      k.classList.add('drag');
      if(!thumb){drag.to=potVal(k,sliderAt(drag,e.clientX,e.clientY));knobShow(k,drag.to)}
      return;
    }
    const ring=e.target.closest('.rmt-knob-ring');
    if(!ring)return;
    // One turn at a time. One whose capture has gone without a release reaching
    // here — its knob redrawn, say — is over, and must not block the next.
    if(drag&&drag.ring.isConnected&&drag.ring.hasPointerCapture(drag.id))return;
    const k=ring.closest('.rmt-knob');
    if(!k||k.classList.contains('off'))return;
    e.preventDefault();
    // Focused, so the wheel and the arrow keys turn it from here on — without
    // the ring that arriving by Tab shows (.pf).
    ring.classList.add('pf');
    try{ring.focus({preventScroll:true})}catch(_){}
    // Captured, so the turn carries on wherever the finger goes.
    try{ring.setPointerCapture(e.pointerId)}catch(_){}
    const r=ring.getBoundingClientRect();
    drag={k:k,ring:ring,id:e.pointerId,cx:r.left+r.width/2,cy:r.top+r.height/2,r:r.width/2,
          x0:e.clientX,y0:e.clientY,acc:0,moved:false,turned:false};
    drag.prev=bearing(drag,e.clientX,e.clientY);
    // A pot with something to start from goes to where it is pressed, and from
    // there follows the finger round, stopping at its two ends as a real one
    // does. Pressed in the gap between the stops it stays put until dragged,
    // and a tap there is one step, as on any knob. With no reading there is
    // no level to set it against, and it turns like any knob.
    // One that sets a value is always a pot: it needs no reading to start from,
    // since what it sends is where it is put, not how far it moved.
    const sets=k.dataset.set!==undefined;
    const from=k.dataset.pot!==undefined||sets?believed(k):null;
    if(from!==null||sets){
      drag.pot=true;drag.from=from;drag.sets=sets;
      const f=drag.prev===null?null:potAt(k,drag.prev);
      drag.gap=f===null;
      const mn=+k.dataset.min,mx=+k.dataset.max;
      drag.f=f!==null?f:from===null?0:Math.min(1,Math.max(0,(from-mn)/(mx-mn)));
      drag.to=drag.at=f!==null?potVal(k,f):null;
      if(f!==null)knobShow(k,drag.to);
    }
    // Held still where a tap steps it — anywhere on a pot that steps, the gap at
    // the foot of one that sets a level — it goes on stepping that way, as a
    // slider's − or + does. A drag that follows takes over from it.
    if(!drag.pot||drag.gap){
      drag.held=false;
      holdStep(e.pointerId,k,drag.x0>=drag.cx?1:-1);
      rep.pot=drag;
    }
  });
  root.addEventListener('pointermove',e=>{
    const d=drag;
    if(!d||e.pointerId!==d.id)return;
    // A knob redrawn mid-turn takes its capture with it, and the release can
    // then land anywhere — so a mouse or pen moving with nothing pressed, or a
    // ring no longer on the page, means the turn is over, not still going.
    if(!d.ring.isConnected||(e.pointerType!=='touch'&&!(e.buttons&1))){
      if(d.slider)d.k.classList.remove('drag');
      drag=null;return;
    }
    if(Math.hypot(e.clientX-d.x0,e.clientY-d.y0)>8)d.moved=true;
    // Moved off the spot: no longer a hold.
    if(d.moved&&rep&&rep.pot===d){if(rep.i)d.held=true;stopRep()}
    if(d.slider){
      if(!d.can||(d.thumb&&!d.moved))return;
      d.to=potVal(d.k,sliderAt(d,e.clientX,e.clientY));knobShow(d.k,d.to);return;
    }
    const t=bearing(d,e.clientX,e.clientY);
    if(t===null){d.prev=null;return}
    if(d.pot){
      // The angle the finger has travelled, as a share of the sweep, held
      // between the stops — so going on past one does nothing, and coming back
      // moves it again straight away.
      if(d.prev!==null){
        d.f=Math.min(1,Math.max(0,d.f+(((t-d.prev+540)%360)-180)/(+d.k.dataset.sweep)));
        d.to=potVal(d.k,d.f);
        knobShow(d.k,d.to);
      }
      d.prev=t;
      return;
    }
    if(d.prev!==null){
      // The shorter way round, so crossing nine o'clock is not a whole turn.
      d.acc+=((t-d.prev+540)%360)-180;
      const step=+d.k.dataset.step||30;
      while(d.acc>=step){d.acc-=step;if(turn(d.k,1,true))d.turned=true}
      while(d.acc<=-step){d.acc+=step;if(turn(d.k,-1,true))d.turned=true}
    }
    d.prev=t;
  });
  const end=e=>{
    const d=drag;
    if(!d||e.pointerId!==d.id)return;
    drag=null;
    try{d.ring.releasePointerCapture(d.id)}catch(_){}
    if(d.slider){
      d.k.classList.remove('drag');
      if(e.type!=='pointerup'){knobShow(d.k,believed(d.k));return}
      if(d.thumb&&!d.moved){pressThumb(d.k);return}
      if(d.to===null){knobShow(d.k,believed(d.k));return}
      if(d.sets)sendSet(d.k,d.to);else potSet(d.k,d.from,d.to);
      return;
    }
    if(d.pot){
      // Let go: set it. A tap on the sweep sets where it landed; one in the gap
      // is a step, below; a gesture cut short puts the pointer back.
      if(e.type!=='pointerup'){knobShow(d.k,believed(d.k));return}
      const put=v=>d.sets?sendSet(d.k,v):potSet(d.k,d.from,v);
      if(!d.moved&&!d.gap){put(d.at);return}
      if(d.moved){if(d.to!==null)put(d.to);return}
    }
    // A tap — let go where it went down, nothing turned — is one step toward
    // the side it landed on: the right half up, the left down.
    // ...unless it was held long enough to step by itself already.
    const held=!!(rep&&rep.pot===d&&rep.i)||d.held;
    if(rep&&rep.pot===d)stopRep();
    if(e.type==='pointerup'&&!d.moved&&!d.turned&&!held)turn(d.k,d.x0>=d.cx?1:-1,true);
  };
  root.addEventListener('pointerup',end);
  root.addEventListener('pointercancel',end);
  root.addEventListener('lostpointercapture',end);
  // The wheel and the arrow keys only turn a knob that was clicked or tabbed to:
  // scrolling the page past one must not change the volume on the way.
  root.addEventListener('wheel',e=>{
    const k=e.target.closest&&e.target.closest('.rmt-knob,.rmt-slider');
    if(!k||k.classList.contains('off')||!k.matches(':focus-within'))return;
    e.preventDefault();
    const s=state(k),now=Date.now();
    // A pause starts the count afresh, so each notch of a mouse wheel is one
    // step whatever size the browser reports it as; a touchpad's small deltas
    // arrive close together and add up.
    if(now-s.wt>300)s.wheel=0;
    s.wt=now;
    s.wheel-=e.deltaY*(e.deltaMode===1?40:e.deltaMode===2?400:1);
    while(s.wheel>=100){s.wheel-=100;turn(k,1)}
    while(s.wheel<=-100){s.wheel+=100;turn(k,-1)}
  },{passive:false});
  root.addEventListener('keydown',e=>{
    const k=e.target.closest&&e.target.closest('.rmt-knob,.rmt-slider');
    if(!k||e.altKey||e.ctrlKey||e.metaKey)return;
    if((e.key==='Enter'||e.key===' ')&&k.dataset.press&&e.target.closest('.rmt-slider-track')){
      e.preventDefault();
      if(!e.repeat)pressThumb(k);
      return;
    }
    const dir=/^(ArrowUp|ArrowRight|PageUp)$/.test(e.key)?1:/^(ArrowDown|ArrowLeft|PageDown)$/.test(e.key)?-1:0;
    if(!dir||k.classList.contains('off'))return;
    e.preventDefault();
    turn(k,dir);
  });
  // A phone's own long-press menu would take the gesture away from the knob.
  root.addEventListener('focusout',e=>{
    const t=e.target;
    if(t&&t.classList&&t.classList.contains('pf'))t.classList.remove('pf');
  });
  root.addEventListener('contextmenu',e=>{
    if(e.target.closest&&e.target.closest('.rmt-knob-ring,.rmt-slider-track,.rmt-slider-mk'))e.preventDefault();
  });
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

function btnHtml(item,inner){
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
    const face=inner!=null?inner:pic||((lab!=null&&lab!=='')?esc(lab):(b.i?icon(b.i):(b.x||'')));
    return '<button class="rmt-btn'+(b.c?' '+b.c:'')+cls+'" data-action="'+a+'"'+
           (b.r?' data-repeat="1"':'')+(lit?' data-lit="'+esc(lit)+'"':'')+
           (css?' style="'+css+'"':'')+
           ' title="'+tip+'">'+face+'</button>';
  }

function lcdLabel(t,action){
    if(!action)return '';
    // A long press: the key's own label, marked, since the style names only the key.
    if(/.@long$/.test(action))return lcdLabel(t,action.slice(0,-5))+' (long)';
    // A side section's parts are sections, looked through as if they stood alone.
    const all=[];
    if(t&&Array.isArray(t.sections))for(const s of t.sections){
      if(Array.isArray(s)&&s[0]==='side')all.push(...s.slice(1).filter(Array.isArray));
      else all.push(s);
    }
    for(const s of all){
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
    }else if(k==='pot'){
      // A dial to turn. Its ring is what knobWire() turns, a press of the first
      // action per step clockwise and of the second per step back; the third is
      // an ordinary key in the middle, so a long press, a hold or lit: work on
      // it as on any other. Listing none means volume, with mute in the middle.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:null;
      const keys=s.slice(cfg?2:1);
      // One that sets a value ("set") has no turn actions — every way of moving
      // it sets the value — so the one action it may list is its middle key.
      const setA=cfg&&typeof cfg.set==='string'&&Object.prototype.hasOwnProperty.call(RMT_BTNS,cfg.set)?cfg.set:'';
      const d=setA?['','',keys[0]]:keys.length?keys:['volume_up','volume_down','mute'];
      // Whole numbers inside their bounds only, tested here as well as at
      // import: they reach an inline style.
      const num=(key,lo,hi)=>{const n=cfg?cfg[key]|0:0;return n>=lo&&n<=hi?n:0};
      const sz=num('size',RMT_KNOB[0],RMT_KNOB[1])||150;
      let cz=num('center',RMT_KNOB_C[0],RMT_KNOB_C[1]);
      if(!cz||cz>sz-2*RMT_KNOB_ROOM)cz=Math.round(sz/2);
      const step=num('step',RMT_KNOB_STEP[0],RMT_KNOB_STEP[1])||30;
      // Only actions this catalogue has: anything else would reach the device
      // as a name it does not know, and be typed to the host as text.
      const act=it=>{const a=Array.isArray(it)?it[0]:it;
        return typeof a==='string'&&Object.prototype.hasOwnProperty.call(RMT_BTNS,a)?a:''};
      const name=it=>{const a=act(it);
        return Array.isArray(it)&&typeof it[1]==='string'&&it[1]?it[1]:(a?RMT_BTNS[a].t:'nothing')};
      const tip=esc(setA?'Sets '+(cfg&&typeof cfg.label==='string'&&cfg.label?cfg.label:RMT_BTNS[setA].t):
                         'Clockwise: '+name(d[0])+' · anticlockwise: '+name(d[1]));
      // A reading in the middle, drawn the way an lcd line is and filled by the
      // same code. On the key's face when there is a key, on a plain cap if not.
      const show=cfg&&typeof cfg.show==='string'&&cfg.show?
        '<span class="rmt-knob-val" data-lcd="'+esc(cfg.show)+'">--</span>':'';
      const mid=(d.length>2&&btnHtml(d[2],show||null))||'<div class="rmt-knob-cap">'+show+'</div>';
      const lab=cfg&&typeof cfg.label==='string'?cfg.label:'';
      // What the ring between the rim and the middle key holds: dots near the
      // rim, the pointer further in, the tap marks between the two. Each is its
      // full size on a big knob and shrinks with the ring on a small one, so a
      // knob the size of a key still has all three, apart.
      const band=(sz-cz)/2,r1=v=>Math.round(v*10)/10;
      const inset=Math.min(8,Math.max(3,band*0.3));          // dots, from the rim
      const dot=r1(Math.min(3,Math.max(1.5,band*0.2)));
      const pd=r1(Math.min(17,(inset+band)/2)),pr=r1(Math.min(3.5,Math.max(1.5,band*0.12)));
      const mk=r1((inset+band)/2),mf=r1(Math.min(14,Math.max(8,band*0.5)));
      // The face that turns: a dot each step round the rim — one circle whose
      // dashes are all gap, pathLength making its lengths degrees — and the
      // pointer. Drawn in the knob's own pixels, so a dot is one size at any size.
      const h=sz/2,rim='cx="'+h+'" cy="'+h+'" r="'+r1(h-inset)+'" stroke-width="'+dot+'" pathLength="360"';
      // A level knob: its pointer follows a reading over `sweep` degrees, centred
      // on twelve o'clock, along a track that fills up to it — in place of the
      // dots, which would only crowd the track. The numbers are checked here as
      // well as at import: they reach attributes, and a stored style may not
      // have been through the importer.
      // One that sets a value is drawn the same way, its pointer at the reading
      // if it has one and at what it last set if not.
      let lvl=cfg&&typeof cfg.level==='string'&&cfg.level?cfg.level:'';
      let lo=cfg&&typeof cfg.min==='number'&&isFinite(cfg.min)?cfg.min:0;
      let hi=cfg&&typeof cfg.max==='number'&&isFinite(cfg.max)?cfg.max:100;
      if(!(hi>lo)){lvl='';lo=0;hi=100}
      const gm=!!(lvl||setA);
      const sw=num('sweep',RMT_KNOB_SWEEP[0],RMT_KNOB_SWEEP[1])||270;
      // A pot steps by inc too, to reach the level it is set to, and one that
      // sets a value keeps to inc's grid: 1 unless given.
      const pot=!!(lvl&&(cfg.analogue===true||cfg.analog===true));
      const inc=gm&&typeof cfg.inc==='number'&&isFinite(cfg.inc)&&cfg.inc>0?cfg.inc:pot||setA?1:0;
      const turnTo=' transform="rotate('+(-90-sw/2)+' '+h+' '+h+')"';
      const gauge=gm?'<svg class="rmt-knob-gauge" viewBox="0 0 '+sz+' '+sz+'"><circle class="tr" '+rim+
                      ' stroke-dasharray="'+sw+' 360"'+turnTo+'/><circle class="lv" '+rim+
                      ' stroke-dasharray="0 360"'+turnTo+'/></svg>':'';
      const face=gauge+'<svg class="rmt-knob-face" viewBox="0 0 '+sz+' '+sz+'">'+
                 (gm?'':'<circle class="tk" '+rim+' stroke-dasharray="0 '+step+'" transform="rotate(-90 '+h+' '+h+')"/>')+
                 '<circle class="pt" cx="'+h+'" cy="'+pd+'" r="'+pr+'"/></svg>';
      const lvAttrs=gm?(lvl?' data-level="'+esc(lvl)+'"':'')+(setA?' data-set="'+setA+'"':'')+
                       ' data-min="'+lo+'" data-max="'+hi+'" data-sweep="'+sw+'"'+
                       (inc?' data-inc="'+inc+'"':'')+(pot?' data-pot="1"':''):'';
      // "marks":false leaves the − and + off, and so does a ring too narrow to
      // hold them clear of the dots. A tap on either half still steps. On a level
      // knob the pointer passes nine and three o'clock, so they move into the gap
      // at the foot of its sweep instead, by the two ends of the track — where a
      // dial marks its minimum and maximum.
      const at=deg=>{
        if(!gm||sw>=330)return '';
        const t=deg*Math.PI/180,rho=h-mk;
        return ' style="left:'+r1(h+rho*Math.sin(t))+'px;top:'+r1(h-rho*Math.cos(t))+'px"';
      };
      const foot=180-(360-sw)/4;
      const marks=!(cfg&&cfg.marks===false)&&band>=12?
        '<span class="rmt-knob-mk dn"'+at(-foot)+'>&minus;</span><span class="rmt-knob-mk up"'+at(foot)+'>+</span>':'';
      // The caption goes over the knob, where a strip's label sits — level with
      // them when the knob is in a side row between two — or under it with
      // "label_at":"bottom".
      const cap=lab?'<span class="rmt-knob-label">'+esc(lab)+'</span>':'';
      const capTop=!(cfg&&cfg.label_at==='bottom');
      inner='<div class="rmt-knob'+(gm?' nolevel':'')+'" data-up="'+act(d[0])+'" data-down="'+act(d[1])+'" data-step="'+step+'"'+
            lvAttrs+' style="--rb-knob:'+sz+'px;--rb-knob-c:'+cz+'px;--rb-knob-mk:'+mk+'px;--rb-knob-mf:'+mf+'px">'+(capTop?cap:'')+
            '<div class="rmt-knob-dial"><div class="rmt-knob-ring" tabindex="0" role="spinbutton" title="'+tip+
            '" aria-label="'+tip+'">'+face+marks+'</div>'+
            '<div class="rmt-knob-c">'+mid+'</div></div>'+(capTop?'':cap)+'</div>';
    }else if(k==='slider'){
      // A slider: pressed or dragged to a value along a straight track, as a
      // knob's pot is round one, and sent when let go — the value itself to the
      // key named by "set", or the presses from a reading ("level") of its up
      // and down actions. knobWire() does the dragging for both.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:{};
      const keys=s.slice(s[1]===cfg?2:1);
      const act=it=>{const a=Array.isArray(it)?it[0]:it;
        return typeof a==='string'&&Object.prototype.hasOwnProperty.call(RMT_BTNS,a)?a:''};
      // A pot's settings and actions carry over, so changing "pot" to "slider"
      // is all a swap takes: size is the length, and the key a pot has in its
      // middle is pressed by a tap on the slider's thumb, which wears its icon.
      const setA=act(cfg.set);
      const d=setA?['','',keys[0]]:keys.length?keys:['volume_up','volume_down'];
      const ua=act(d[0]),da=act(d[1]);
      const pa=d.length>2?act(d[2]):'';
      const pl=Array.isArray(d[2])&&typeof d[2][1]==='string'&&d[2][1]?d[2][1]:pa?RMT_BTNS[pa].t:'';
      const thumb='<div class="rmt-slider-thumb"'+(pa?' title="'+esc('Tap: '+pl)+'">'+(RMT_BTNS[pa].i?icon(RMT_BTNS[pa].i):''):'>')+'</div>';
      // Checked here as well as at import: these reach attributes and a style.
      const lvl=typeof cfg.level==='string'&&cfg.level?cfg.level:'';
      let lo=typeof cfg.min==='number'&&isFinite(cfg.min)?cfg.min:0;
      let hi=typeof cfg.max==='number'&&isFinite(cfg.max)?cfg.max:100;
      if(!(hi>lo)){lo=0;hi=100}
      const inc=typeof cfg.inc==='number'&&isFinite(cfg.inc)&&cfg.inc>0?cfg.inc:1;
      const ln=(cfg.length!==undefined?cfg.length:cfg.size)|0;
      const len=ln>=RMT_SLIDER[0]&&ln<=RMT_SLIDER[1]?ln:ln>0&&ln<RMT_SLIDER[0]?RMT_SLIDER[0]:160;
      // "width": how thick it is, the rail and the thumb in proportion to it.
      const w=cfg.width|0,wide=w>=RMT_SLIDER_W[0]&&w<=RMT_SLIDER_W[1]?
        ';--rb-sl-w:'+w+'px;--rb-sl-rail:'+Math.max(2,Math.round(w*0.22))+'px;--rb-sl-th:'+Math.max(8,Math.round(w*0.72))+'px':'';
      // "thumb": its size apart from the width — a big key on a thin rail, say.
      const th=cfg.thumb|0,thumbSz=th>=RMT_SLIDER_T[0]&&th<=RMT_SLIDER_T[1]?';--rb-sl-th:'+th+'px':'';
      const lab=typeof cfg.label==='string'?cfg.label:'';
      // The caption, over the slider unless "label_at" says under, and beside it
      // the reading "show" names, filled as an lcd line's is.
      const show=typeof cfg.show==='string'&&cfg.show?'<span class="rmt-slider-val" data-lcd="'+esc(cfg.show)+'">--</span>':'';
      const cap=lab||show?'<div class="rmt-slider-cap">'+(lab?'<span class="rmt-knob-label">'+esc(lab)+'</span>':'')+show+'</div>':'';
      const capTop=cfg.label_at!=='bottom';
      const marks=cfg.marks!==false;
      const nm=a=>a?RMT_BTNS[a].t:'nothing';
      const tip=esc(setA?'Sets '+(lab||RMT_BTNS[setA].t):'Slide to a level: '+nm(ua)+' / '+nm(da));
      // Upright unless told across — either way of saying it.
      const vert=!(cfg.horizontal===true||cfg.vertical===false);
      inner='<div class="rmt-slider nolevel'+(vert?' vert':'')+'" data-up="'+ua+'" data-down="'+da+'"'+
            (lvl?' data-level="'+esc(lvl)+'"':'')+(setA?' data-set="'+setA+'"':'')+(pa?' data-press="'+pa+'"':'')+
            ' data-min="'+lo+'" data-max="'+hi+'" data-inc="'+inc+'" style="--rb-sl-len:'+len+'px'+wide+thumbSz+'">'+(capTop?cap:'')+
            '<div class="rmt-slider-row">'+(marks?'<span class="rmt-slider-mk dn">&minus;</span>':'')+
            '<div class="rmt-slider-track" tabindex="0" role="slider" title="'+tip+'" aria-label="'+tip+'">'+
            '<div class="rmt-slider-fill"></div>'+thumb+'</div>'+
            (marks?'<span class="rmt-slider-mk up">+</span>':'')+'</div>'+(capTop?'':cap)+'</div>';
    }else if(k==='side'){
      // Sections in a row instead of one under another — a knob between the
      // volume and channel keys, say. One level only: a side inside a side, or a
      // divider, draws nothing, as validateTpl refuses both.
      // An optional {"gap"} first: the room between the parts, a whole number
      // inside RMT_SIDE_GAP, checked here too since it reaches an inline style.
      const g=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1].gap:undefined;
      const gap=typeof g==='number'&&g===Math.floor(g)&&g>=RMT_SIDE_GAP[0]&&g<=RMT_SIDE_GAP[1]?' style="gap:'+g+'px"':'';
      inner='<div class="rmt-side"'+gap+'>'+s.slice(1).map(c=>
        Array.isArray(c)&&c[0]!=='side'&&c[0]!=='-'?sectionHtml(c):'').join('')+'</div>';
    }else if(k==='rocker'){
      // An optional {"width","h"} first: each column's width and each half's
      // height. Whole numbers inside RMT_KEY_H only, checked here as well as at
      // import: they reach an inline style.
      const cfg=(s[1]&&typeof s[1]==='object'&&!Array.isArray(s[1]))?s[1]:null;
      const px=v=>{const n=cfg?cfg[v]|0:0;return n>=RMT_KEY_H[0]&&n<=RMT_KEY_H[1]?n:0};
      const w=px('width'),h=px('h');
      inner='<div class="rmt-rocker'+(w?' rk-w':'')+(h?' rk-h':'')+'"'+
            (w||h?' style="'+(w?'--rb-rk-w:'+w+'px;':'')+(h?'--rb-rk-h:'+h+'px':'')+'"':'')+'>'+
            s.slice(cfg?2:1).map(g=>{
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
    // One section, '' when it is fine. A side section's parts are sections too,
    // checked by this same function, one level deep.
    const sectionBad=(s,inSide)=>{
      if(!Array.isArray(s)||typeof s[0]!=='string')return 'Each section is an array starting with its kind';
      if(RMT_KINDS.indexOf(s[0])<0)return 'Unknown section kind "'+s[0]+'" — use '+RMT_KINDS.join(', ');
      if(s[0]==='side'){
        if(inSide)return 'A side section cannot hold another side section';
        let parts=s.slice(1);
        // It may open with {"gap"}: the room between its parts.
        if(parts.length&&parts[0]&&typeof parts[0]==='object'&&!Array.isArray(parts[0])){
          const cfg=parts[0];
          parts=parts.slice(1);
          for(const k in cfg){
            if(k!=='gap')return 'A side section takes gap — "'+k+'" is not it';
            const n=cfg[k];
            if(typeof n!=='number'||n!==Math.floor(n)||n<RMT_SIDE_GAP[0]||n>RMT_SIDE_GAP[1])
              return 'side "gap" is '+RMT_SIDE_GAP[0]+'-'+RMT_SIDE_GAP[1]+' pixels, a whole number — e.g. {"gap":24}';
          }
        }
        if(parts.length<2)return 'A side section puts two or more sections side by side — ["side",["strip",…],["pot"]]';
        for(const p of parts){
          if(Array.isArray(p)&&p[0]==='-')return 'A divider cannot go in a side section';
          const bad=sectionBad(p,true);
          if(bad)return bad;
        }
        return '';
      }
      let items=[];
      if(s[0]==='strip'||s[0]==='rocker'){
        let groups=s.slice(1);
        // A rocker may open with {"width","h"} — its columns' width and the
        // height of each half — the way a grid opens with its layout.
        if(s[0]==='rocker'&&groups.length&&groups[0]&&typeof groups[0]==='object'&&!Array.isArray(groups[0])){
          const cfg=groups[0];
          groups=groups.slice(1);
          for(const k in cfg){
            if(k!=='width'&&k!=='h')return 'A rocker takes width and h — "'+k+'" is neither';
            const n=cfg[k];
            if(typeof n!=='number'||n!==Math.floor(n)||n<RMT_KEY_H[0]||n>RMT_KEY_H[1])
              return 'rocker "'+k+'" is '+RMT_KEY_H[0]+'-'+RMT_KEY_H[1]+' pixels, a whole number — e.g. {"width":60,"h":48}';
          }
        }
        for(const g of groups){
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
      }else if(s[0]==='pot'){
        // Turned, not pressed: a step clockwise presses the first action, one
        // anticlockwise the second, and the third is the key in the middle. The
        // settings come first if at all, as a ring's do.
        let keys=s.slice(1),sets=false;
        if(keys.length&&keys[0]&&typeof keys[0]==='object'&&!Array.isArray(keys[0])){
          const cfg=keys[0];
          keys=keys.slice(1);
          for(const k in cfg){
            const v=cfg[k];
            // The key whose Host Action takes the value, with {value} in it.
            if(k==='set'){
              if(typeof v!=='string'||!Object.prototype.hasOwnProperty.call(RMT_BTNS,v))
                return 'pot "set" names the key whose Host Action takes the value — a spare, e.g. {"set":"spare12"}';
              sets=true;
              continue;
            }
            if(k==='show'){
              // Named the way an lcd line names its value, and filled the same way.
              const ok=typeof v==='string'&&(v.charAt(0)==='@'?RMT_LCD_KEYS.indexOf(v)>=0:/^[a-z0-9_]{1,16}$/.test(v));
              if(!ok)return 'pot "show" names a value as an lcd line does — a sources key of 1-16 characters, a-z, 0-9 or _, or one of '+
                            RMT_LCD_KEYS.join(', ');
              continue;
            }
            if(k==='label'){
              if(typeof v!=='string'||v.length>16)return 'pot "label" is text of up to 16 characters';
              continue;
            }
            if(k==='label_at'){
              if(v!=='top'&&v!=='bottom')return 'pot "label_at" is "top" or "bottom"';
              if(typeof cfg.label!=='string'||!cfg.label)return 'pot "label_at" places the caption — add a "label"';
              continue;
            }
            // "analogue" (or "analog"): the position is the level, as on a pot.
            if(k==='marks'||k==='analogue'||k==='analog'){
              if(typeof v!=='boolean')return 'pot "'+k+'" is true or false';
              continue;
            }
            if(k==='level'){
              const ok=typeof v==='string'&&(v.charAt(0)==='@'?RMT_LCD_KEYS.indexOf(v)>=0:/^[a-z0-9_]{1,16}$/.test(v));
              if(!ok)return 'pot "level" names a reading as an lcd line does — a sources key of 1-16 characters, a-z, 0-9 or _';
              continue;
            }
            // The level's scale, and how far one step moves it: any number.
            if(k==='min'||k==='max'||k==='inc'){
              if(typeof v!=='number'||!isFinite(v))return 'pot "'+k+'" must be a number, e.g. {"level":"tv_volume","max":100}';
              if(k==='inc'&&v<=0)return 'pot "inc" is how much one step moves the level — more than 0';
              continue;
            }
            if(k!=='size'&&k!=='center'&&k!=='step'&&k!=='sweep')
              return 'A pot takes size, center, step, show, label, label_at, marks, level, set, min, max, sweep, inc and analogue — "'+k+'" is none of them';
            if(typeof v!=='number'||!isFinite(v)||v!==Math.floor(v))
              return 'pot "'+k+'" must be a whole number, e.g. {"size":160,"center":80,"step":30}';
          }
          if(cfg.level===undefined&&cfg.set===undefined){
            for(const k of ['min','max','sweep','inc','analogue','analog'])
              if(cfg[k]!==undefined)return 'pot "'+k+'" sets out a level — add "level" naming the reading it is for, or "set"';
          }else{
            const lo=cfg.min!==undefined?cfg.min:0,hi=cfg.max!==undefined?cfg.max:100;
            if(hi<=lo)return 'pot "max" '+hi+' must be above "min" '+lo;
          }
          if(cfg.sweep!==undefined&&(cfg.sweep<RMT_KNOB_SWEEP[0]||cfg.sweep>RMT_KNOB_SWEEP[1]))
            return 'pot "sweep" is '+RMT_KNOB_SWEEP[0]+'-'+RMT_KNOB_SWEEP[1]+' degrees — "'+cfg.sweep+'" is outside that';
          if(cfg.size!==undefined&&(cfg.size<RMT_KNOB[0]||cfg.size>RMT_KNOB[1]))
            return 'pot "size" is '+RMT_KNOB[0]+'-'+RMT_KNOB[1]+' pixels — "'+cfg.size+'" is outside that';
          if(cfg.center!==undefined&&(cfg.center<RMT_KNOB_C[0]||cfg.center>RMT_KNOB_C[1]))
            return 'pot "center" is '+RMT_KNOB_C[0]+'-'+RMT_KNOB_C[1]+' pixels — "'+cfg.center+'" is outside that';
          if(cfg.step!==undefined&&(cfg.step<RMT_KNOB_STEP[0]||cfg.step>RMT_KNOB_STEP[1]))
            return 'pot "step" is '+RMT_KNOB_STEP[0]+'-'+RMT_KNOB_STEP[1]+' degrees — "'+cfg.step+'" is outside that';
          const size=cfg.size!==undefined?cfg.size:150;
          if(cfg.center!==undefined&&cfg.center>size-2*RMT_KNOB_ROOM)
            return 'pot "center" '+cfg.center+' leaves no ring to turn in a '+size+'px pot — at most '+
                   (size-2*RMT_KNOB_ROOM)+', or a larger "size"';
        }
        // One that sets a value has nothing to turn by: the one action it may
        // list is the key in its middle.
        if(sets&&keys.length>1)
          return 'A pot with "set" lists one action at most, the key in its middle — the value goes to "set"';
        if(!sets&&keys.length&&keys.length!==2&&keys.length!==3)
          return 'A pot lists its actions turning up, turning down, then an optional key in the middle — '+
                 '["pot","volume_up","volume_down","mute"] — or none';
        // Turning shows no face, so a turn action takes a label (its tooltip, and
        // its name on an @last line) and nothing else.
        if(!sets)for(const it of keys.slice(0,2))
          if(Array.isArray(it)&&it.length!==2)
            return 'A pot turns by "action" or ["action","Label"] — '+JSON.stringify(it)+' is neither';
        items=keys;
      }else if(s[0]==='slider'){
        // Dragged to a value: sent to "set" once, or as presses of its two
        // actions from a reading named by "level". One of the two it must have.
        let keys=s.slice(1),cfg={};
        if(keys.length&&keys[0]&&typeof keys[0]==='object'&&!Array.isArray(keys[0])){cfg=keys[0];keys=keys.slice(1)}
        const lcdKey=v=>typeof v==='string'&&(v.charAt(0)==='@'?RMT_LCD_KEYS.indexOf(v)>=0:/^[a-z0-9_]{1,16}$/.test(v));
        for(const k in cfg){
          const v=cfg[k];
          if(k==='set'){
            if(typeof v!=='string'||!Object.prototype.hasOwnProperty.call(RMT_BTNS,v))
              return 'slider "set" names the key whose Host Action takes the value — a spare, e.g. {"set":"spare12"}';
          }else if(k==='level'||k==='show'){
            if(!lcdKey(v))return 'slider "'+k+'" names a reading as an lcd line does — a sources key of 1-16 characters, a-z, 0-9 or _';
          }else if(k==='label'){
            if(typeof v!=='string'||v.length>16)return 'slider "label" is text of up to 16 characters';
          }else if(k==='label_at'){
            if(v!=='top'&&v!=='bottom')return 'slider "label_at" is "top" or "bottom"';
          }else if(k==='marks'||k==='vertical'||k==='horizontal'||k==='analogue'||k==='analog'){
            // analogue is a knob's: a slider always goes where it is put.
            if(typeof v!=='boolean')return 'slider "'+k+'" is true or false';
          }else if(k==='min'||k==='max'||k==='inc'){
            if(typeof v!=='number'||!isFinite(v))return 'slider "'+k+'" must be a number, e.g. {"set":"spare12","max":255}';
            if(k==='inc'&&v<=0)return 'slider "inc" is how much one step moves the value — more than 0';
          }else if(k==='length'){
            if(typeof v!=='number'||v!==Math.floor(v)||v<RMT_SLIDER[0]||v>RMT_SLIDER[1])
              return 'slider "length" is '+RMT_SLIDER[0]+'-'+RMT_SLIDER[1]+' pixels, a whole number';
          }else if(k==='width'){
            if(typeof v!=='number'||v!==Math.floor(v)||v<RMT_SLIDER_W[0]||v>RMT_SLIDER_W[1])
              return 'slider "width" is '+RMT_SLIDER_W[0]+'-'+RMT_SLIDER_W[1]+' pixels, a whole number';
          }else if(k==='thumb'){
            if(typeof v!=='number'||v!==Math.floor(v)||v<RMT_SLIDER_T[0]||v>RMT_SLIDER_T[1])
              return 'slider "thumb" is '+RMT_SLIDER_T[0]+'-'+RMT_SLIDER_T[1]+' pixels, a whole number';
          }else if(k==='size'||k==='center'||k==='step'||k==='sweep'){
            // A knob's, taken so that "knob" can be changed to "slider" and
            // nothing else: size is the length, the rest have nothing to do here.
            if(typeof v!=='number'||v!==Math.floor(v)||v<1)return 'slider "'+k+'" must be a whole number';
          }else return 'A slider takes set, level, show, label, label_at, marks, horizontal, vertical, min, max, inc, length, width and thumb, '+
                       'and a pot\'s settings — "'+k+'" is none of them';
        }
        if(cfg.vertical!==undefined&&cfg.horizontal!==undefined&&cfg.vertical===cfg.horizontal)
          return 'slider "vertical" and "horizontal" disagree — give one of them';
        if(cfg.set===undefined&&cfg.level===undefined)
          return 'A slider sends a value to a key ("set") or steps to a reading ("level") — give it one, e.g. {"set":"spare12"}';
        const lo=cfg.min!==undefined?cfg.min:0,hi=cfg.max!==undefined?cfg.max:100;
        if(hi<=lo)return 'slider "max" '+hi+' must be above "min" '+lo;
        // A knob's actions: with "set", one at most — a key beside it; without,
        // up and down, and a third for that key.
        if(cfg.set!==undefined&&keys.length>1)
          return 'A slider with "set" lists one action at most, a key beside it — the value goes to "set"';
        if(cfg.set===undefined&&keys.length&&keys.length!==2&&keys.length!==3)
          return 'A slider steps by two actions, up then down, and a third is a key beside it — '+
                 '["slider",{"level":"tv_volume"},"volume_up","volume_down","mute"] — or none for volume';
        if(cfg.set===undefined)for(const it of keys.slice(0,2))
          if(Array.isArray(it)&&it.length!==2)
            return 'A slider steps by "action" or ["action","Label"] — '+JSON.stringify(it)+' is neither';
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
      return '';
    };
    for(const s of t.sections){
      const bad=sectionBad(s,false);
      if(bad)return bad;
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
.rmt-knob{display:flex;flex-direction:column;align-items:center;gap:4px;margin:6px 0}
.rmt-knob-dial{position:relative;width:var(--rb-knob,150px);height:var(--rb-knob,150px)}
.rmt-knob-ring{position:absolute;inset:0;border-radius:50%;cursor:grab;touch-action:none;outline:none;
  background:var(--rb-ring-bg,var(--rb-btn-bg,var(--bg)));border:1px solid var(--rb-btn-border,var(--border));
  color:var(--rb-ring-fg,var(--rb-btn-fg,var(--fg)));user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
.rmt-knob-ring:active{cursor:grabbing}
.rmt-knob-ring:focus-visible{box-shadow:0 0 0 2px var(--accent)}
.rmt-knob-ring.pf:focus-visible,.rmt-slider-track.pf:focus-visible{box-shadow:none}
.rmt-knob-face{position:absolute;inset:0;width:100%;height:100%;transition:transform .08s;pointer-events:none}
.rmt-knob-face .tk{fill:none;stroke:var(--rb-label,var(--muted));stroke-linecap:round}
.rmt-knob-face .pt{fill:currentColor}
.rmt-knob-mk{position:absolute;top:50%;transform:translate(-50%,-50%);font-size:var(--rb-knob-mf,14px);font-weight:700;
  line-height:1;opacity:.7;pointer-events:none}
.rmt-knob-mk.dn{left:var(--rb-knob-mk,23px)}
.rmt-knob-mk.up{left:calc(100% - var(--rb-knob-mk,23px))}
.rmt-knob-c{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);display:flex;pointer-events:none}
.rmt-knob-c>.rmt-btn{pointer-events:auto}
.rmt-knob-c>.rmt-btn,.rmt-knob-cap{width:var(--rb-knob-c,75px);height:var(--rb-knob-c,75px);border-radius:50%;
  background:var(--rb-ok-bg,var(--rb-btn-bg,var(--bg)));color:var(--rb-ok-fg,var(--rb-btn-fg,var(--fg)));
  border:1px solid var(--rb-btn-border,var(--border));box-shadow:0 1px 4px rgba(0,0,0,.35)}
.rmt-knob-c>.rmt-btn svg{width:min(20px,calc(var(--rb-knob-c,75px) * .5));height:min(20px,calc(var(--rb-knob-c,75px) * .5))}
.rmt-knob-c>.rmt-btn:active,.rmt-knob-c>.rmt-btn.p{background:var(--active);color:#fff;border-color:var(--active)}
.rmt-knob-c>.rmt-btn.held{background:var(--accent);color:#fff;border-color:var(--accent)}
.rmt-knob-cap{display:flex;align-items:center;justify-content:center}
.rmt-knob-val{max-width:88%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;
  font-size:calc(var(--rb-knob-c,75px) * .24);font-weight:700;font-variant-numeric:tabular-nums;line-height:1.15}
.rmt-knob-label{font-size:9px;color:var(--rb-label,var(--muted));font-weight:700;text-transform:uppercase;letter-spacing:.4px}
.rmt-knob.off:not([data-level]) .rmt-knob-face,.rmt-knob.off .rmt-knob-mk{visibility:hidden}
.rmt-knob.off .rmt-knob-ring{cursor:default}
.rmt-knob-gauge{position:absolute;inset:0;width:100%;height:100%;pointer-events:none}
.rmt-knob-gauge .tr{fill:none;stroke:var(--rb-label,var(--muted));opacity:.35;stroke-linecap:round}
.rmt-knob-gauge .lv{fill:none;stroke:var(--rb-lit-bg,var(--accent));stroke-linecap:round;transition:stroke-dasharray .2s}
.rmt-knob[data-level] .rmt-knob-face{transition:transform .2s}
.rmt-knob.nolevel .rmt-knob-face,.rmt-knob.nolevel .rmt-knob-gauge .lv{visibility:hidden}
.rmt-knob.fu .rmt-knob-mk.up,.rmt-knob.fd .rmt-knob-mk.dn{opacity:1;color:var(--accent)}
.rmt-knob.fu .rmt-knob-ring,.rmt-knob.fd .rmt-knob-ring{border-color:var(--accent)}
.rmt-slider{display:flex;flex-direction:column;align-items:center;gap:4px;margin:6px 0}
.rmt-slider-row{display:flex;align-items:center;gap:4px}
.rmt-slider.vert .rmt-slider-row{flex-direction:column-reverse}
.rmt-slider-track{position:relative;width:var(--rb-sl-len,160px);height:var(--rb-sl-w,28px);touch-action:pan-y;cursor:pointer;outline:none;
  border-radius:8px;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
.rmt-slider.vert .rmt-slider-track{width:var(--rb-sl-w,28px);height:var(--rb-sl-len,160px);touch-action:pan-x}
.rmt-slider-track:focus-visible{box-shadow:0 0 0 2px var(--accent)}
.rmt-slider-track::before{content:"";position:absolute;left:0;right:0;top:50%;height:var(--rb-sl-rail,6px);
  margin-top:calc(var(--rb-sl-rail,6px) / -2);border-radius:var(--rb-sl-rail,6px);
  background:var(--rb-label,var(--muted));opacity:.35}
.rmt-slider.vert .rmt-slider-track::before{left:50%;right:auto;top:0;bottom:0;width:var(--rb-sl-rail,6px);height:auto;
  margin:0 0 0 calc(var(--rb-sl-rail,6px) / -2)}
.rmt-slider-fill{position:absolute;left:0;top:50%;width:calc(var(--rb-sl-th,20px) / 2 + max(0px,100% - var(--rb-sl-th,20px)) * var(--rb-sl,0));height:var(--rb-sl-rail,6px);
  margin-top:calc(var(--rb-sl-rail,6px) / -2);border-radius:var(--rb-sl-rail,6px);
  background:var(--rb-lit-bg,var(--accent));transition:width .15s}
.rmt-slider.vert .rmt-slider-fill{left:50%;top:auto;bottom:0;width:var(--rb-sl-rail,6px);height:calc(var(--rb-sl-th,20px) / 2 + max(0px,100% - var(--rb-sl-th,20px)) * var(--rb-sl,0));
  margin:0 0 0 calc(var(--rb-sl-rail,6px) / -2);transition:height .15s}
.rmt-slider-thumb{position:absolute;top:50%;left:calc(var(--rb-sl-th,20px) / 2 + max(0px,100% - var(--rb-sl-th,20px)) * var(--rb-sl,0));width:var(--rb-sl-th,20px);height:var(--rb-sl-th,20px);
  margin:calc(var(--rb-sl-th,20px) / -2) 0 0 calc(var(--rb-sl-th,20px) / -2);border-radius:50%;
  background:var(--rb-ok-bg,var(--rb-btn-bg,var(--bg)));border:1px solid var(--rb-btn-border,var(--border));
  box-shadow:0 1px 4px rgba(0,0,0,.35);transition:left .15s,bottom .15s}
.rmt-slider-thumb{display:flex;align-items:center;justify-content:center;color:var(--rb-ok-fg,var(--rb-btn-fg,var(--fg)))}
.rmt-slider-thumb svg{width:60%;height:60%;fill:currentColor;pointer-events:none}
.rmt-slider-thumb.p{background:var(--active);color:#fff;border-color:var(--active)}
.rmt-slider[data-press] .rmt-slider-thumb{cursor:pointer}
.rmt-slider.nolevel[data-press]:not(.off) .rmt-slider-thumb{visibility:visible;opacity:.6}
.rmt-slider.nopress .rmt-slider-thumb svg{visibility:hidden}
.rmt-slider.vert .rmt-slider-thumb{left:50%;top:auto;bottom:calc(var(--rb-sl-th,20px) / 2 + max(0px,100% - var(--rb-sl-th,20px)) * var(--rb-sl,0));
  margin:0 0 calc(var(--rb-sl-th,20px) / -2) calc(var(--rb-sl-th,20px) / -2)}
.rmt-slider.drag .rmt-slider-fill,.rmt-slider.drag .rmt-slider-thumb{transition:none}
.rmt-slider.nolevel .rmt-slider-fill,.rmt-slider.nolevel .rmt-slider-thumb{visibility:hidden}
.rmt-slider-mk{display:flex;align-items:center;justify-content:center;flex:none;width:20px;height:20px;
  font-size:16px;font-weight:700;line-height:1;opacity:.7;cursor:pointer;
  color:var(--rb-btn-fg,var(--fg));user-select:none;-webkit-user-select:none;touch-action:manipulation}
.rmt-slider.fu .rmt-slider-mk.up,.rmt-slider.fd .rmt-slider-mk.dn{opacity:1;color:var(--accent)}
.rmt-slider-cap{display:flex;align-items:baseline;gap:6px}
.rmt-slider-val{font-size:12px;font-weight:700;font-variant-numeric:tabular-nums;color:var(--rb-btn-fg,var(--fg))}
.rmt-slider.off .rmt-slider-mk,.rmt-slider.off .rmt-slider-thumb{visibility:hidden}
.rmt-slider.off .rmt-slider-track{cursor:default}
.rmt-side{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:12px}
.rmt-side>.rmt-section{margin:0}
.rmt-rocker{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:14px}
.rmt-rocker-col{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;
  padding:4px 0;border-radius:999px;background:var(--rb-btn-bg,var(--bg));
  border:1px solid var(--rb-btn-border,var(--border))}
.rmt-rocker-col .rmt-btn{background:none;border-color:transparent;height:40px}
.rmt-rocker-col .rmt-btn:active,.rmt-rocker-col .rmt-btn.p{background:rgba(255,255,255,.18);border-color:transparent}
.rmt-rocker-label{font-size:9px;color:var(--rb-label,var(--muted));font-weight:700;text-transform:uppercase;letter-spacing:.4px}
.rmt-rocker-solo{background:none;border:none;padding:0;width:auto}
.rmt-rocker.rk-w .rmt-rocker-col:not(.rmt-rocker-solo){width:var(--rb-rk-w)}
.rmt-rocker.rk-w .rmt-rocker-col:not(.rmt-rocker-solo) .rmt-btn{width:100%}
.rmt-rocker.rk-h .rmt-rocker-col .rmt-btn{height:var(--rb-rk-h)}
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

export { RI, RMT_BTNS, RMT_VARS, RMT_BUILTIN, RMT_KINDS, RMT_OPTS, RMT_KEY_H, RMT_LCD_OPTS, RMT_LCD_COLOURS, RMT_LCD_KEYS, RMT_LCD_LABELLED, lcdLabel, RMT_HEX, RMT_CLIP, RMT_FETCH, icon, esc, RMT_ICON_NAME, RMT_ICON_LEN, RMT_ICON_MAX, RMT_ICONS, iconBad, useIcons, iconNames, themeValueBad, btnHtml, sectionHtml, validateTpl, knobWire, knobLevel };
