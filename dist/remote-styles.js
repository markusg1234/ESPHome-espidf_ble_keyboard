// GENERATED FILE — do not edit by hand.
//
// Lifted from components/espidf_ble_keyboard/web_control.cpp so the Home
// Assistant card draws remotes with exactly the code the device's own web page
// uses. Regenerate after any change to the styles, catalogue, renderer or CSS:
//
//     node tools/gen-remote-styles.mjs
//
// Built-ins in this snapshot: default, style1, style2, style3, style4, style5
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
stop:'<path d="M6 6h12v12H6z"/>',
ff:'<path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/>',
next:'<path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/>',
rec:'<circle cx="12" cy="12" r="7"/>',
menu:'<path d="M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z"/>',
exit:'<path d="M10.09 15.59L11.5 17l5-5-5-5-1.41 1.41L12.67 11H3v2h9.67l-2.58 2.59zM19 3H5c-1.11 0-2 .9-2 2v4h2V5h14v14H5v-4H3v4c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2z"/>',
guide:'<path d="M21 3H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14zM5 7h6v2H5V7zm0 4h6v2H5v-2zm0 4h6v2H5v-2zm8-8h6v2h-6V7zm0 4h6v2h-6v-2zm0 4h6v2h-6v-2z"/>',
mic:'<path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3zm5-3c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z"/>',
cc:'<path d="M19 4H5c-1.11 0-2 .9-2 2v12c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2zm0 14H5V6h14v12zM7 15h3v-1.5H8.5v-3H10V9H7v6zm7 0h3v-1.5h-1.5v-3H17V9h-3v6z"/>',
tv:'<path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h5v2h8v-2h5c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12z"/>'};

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
prev_track:{t:'Previous',i:'prev',c:'media',g:3},
rewind:{t:'Rewind',i:'rew',c:'media',r:1,g:3},
play_pause:{t:'Play/Pause',i:'play',c:'media',g:3},
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
spare16:{t:'Spare 16',x:'16',g:8}};

const RMT_VARS={bg:'--rb-bg',border:'--rb-border',radius:'--rb-radius',pad:'--rb-pad',
maxw:'--rb-maxw',btn_bg:'--rb-btn-bg',btn_fg:'--rb-btn-fg',btn_border:'--rb-btn-border',
btn_radius:'--rb-btn-radius',ok_bg:'--rb-ok-bg',ok_fg:'--rb-ok-fg',
ring_bg:'--rb-ring-bg',ring_fg:'--rb-ring-fg',light_bg:'--rb-light-bg',light_fg:'--rb-light-fg',shadow:'--rb-shadow',
label:'--rb-label',divider:'--rb-divider',clip:'--rb-clip'};

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
 ['apps',['spare8','App 4','light wide']]]}];

const RMT_KINDS=['row','dpad','ring','strip','rocker','media','apps','-'];

const RMT_OPTS=['light','sm','lg','xl','wide','sq'];

const RMT_HEX=/^#[0-9a-f]{3,8}$/i;

const RMT_CLIP=/^polygon\(\s*[-0-9%.,\s]+\)$/i;

function icon(i){return '<svg viewBox="0 0 24 24">'+RI[i]+'</svg>'}

function esc(s){
    return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
  }

function btnHtml(item){
    const arr=Array.isArray(item);
    const a=arr?item[0]:item, lab=arr?item[1]:null, opt=arr?item[2]:null;
    const b=RMT_BTNS[a];
    if(!b)return '';   // a style naming a button this firmware doesn't have
    const face=(lab!=null&&lab!=='')?esc(lab):(b.i?icon(b.i):(b.x||''));
    const tip=(lab!=null&&lab!=='')?esc(lab)+' — runs '+a:b.t;
    let cls='',css='';
    if(typeof opt==='string'){
      for(const tok of opt.split(/\s+/)){
        if(!tok)continue;
        // The only value that reaches an inline style attribute, so it is
        // matched against a strict hex pattern and nothing else — a token that
        // got this far already passed the same test at import.
        if(RMT_HEX.test(tok))css='background:'+tok+';border-color:'+tok;
        else if(RMT_OPTS.indexOf(tok)>=0)cls+=' '+tok;
      }
    }
    return '<button class="rmt-btn'+(b.c?' '+b.c:'')+cls+'" data-action="'+a+'"'+
           (b.r?' data-repeat="1"':'')+(css?' style="'+css+'"':'')+
           ' title="'+tip+'">'+face+'</button>';
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
      const d=s.length>1?s.slice(1):['up','left','ok','right','down'];
      const at=(p,i)=>'<span class="'+p+'">'+btnHtml(d[i])+'</span>';
      inner='<div class="rmt-ring">'+at('n',0)+at('w',1)+at('c',2)+at('e',3)+at('s',4)+'</div>';
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
    }
    return '<div class="rmt-section">'+inner+'</div>';
  }

function validateTpl(t){
    if(!t||typeof t!=='object'||Array.isArray(t))return 'Top level must be a JSON object';
    if(typeof t.id!=='string'||!/^[a-z0-9_]{1,15}$/.test(t.id))
      return 'id must be 1-15 characters of a-z, 0-9 or _';
    if(RMT_BUILTIN.some(b=>b.id===t.id))return '"'+t.id+'" is a built-in style — give yours another id';
    if(typeof t.name!=='string'||!t.name.trim()||t.name.length>24)return 'name is required (max 24 characters)';
    if(!Array.isArray(t.sections)||!t.sections.length)return 'sections must be a non-empty array';
    if(t.theme!==undefined&&(typeof t.theme!=='object'||t.theme===null||Array.isArray(t.theme)))
      return 'theme must be an object';
    // Theme values are handed to setProperty, which rejects malformed CSS on its
    // own — but url() is well-formed and would have the page fetch from
    // somewhere else the moment a style is applied. Nothing here needs it.
    if(t.theme)for(const k in t.theme){
      if(typeof t.theme[k]!=='string')continue;
      if(/url\s*\(/i.test(t.theme[k]))
        return 'theme values cannot use url() — "'+k+'" would load from another host';
      if(k==='clip'&&!RMT_CLIP.test(t.theme[k].trim()))
        return 'clip must be a polygon(), e.g. polygon(0% 0%, 100% 0%, 82% 100%, 18% 100%)';
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
        if(s.length>1&&s.length!==6)
          return 'A '+s[0]+' section lists exactly 5 actions (up, left, centre, right, down) or none';
        items=s.slice(1);
      }else{
        items=s.slice(1);
      }
      for(const it of items){
        if(it==='|'&&s[0]==='row')continue;
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
          for(const tok of it[2].split(/\s+/)){
            if(!tok)continue;
            // Refused rather than ignored: a silently dropped typo looks like
            // the renderer is broken. The hex test here is the same one
            // btnHtml applies, and is what keeps arbitrary CSS out of the
            // inline style attribute it builds.
            if(!RMT_HEX.test(tok)&&RMT_OPTS.indexOf(tok)<0)
              return 'Unknown button option "'+tok+'" — use a #hex colour or '+RMT_OPTS.join(', ');
          }
        }
        const a=arr?it[0]:it;
        if(typeof a!=='string'||!RMT_BTNS[a])return 'Unknown button "'+a+'"';
      }
    }
    return '';
  }

// The remote's stylesheet. It falls back to the page palette (--bg, --fg,
// --border, --muted, --active, --accent), so whatever hosts this must map those
// onto its own theme — inside a shadow root there is no :root to inherit from.
export const RMT_CSS = `
.rmt-body{background:var(--rb-bg,transparent);border:1px solid var(--rb-border,transparent);border-radius:var(--rb-radius,0);padding:var(--rb-pad,0);max-width:var(--rb-maxw,none);margin:0 auto;box-shadow:var(--rb-shadow,none);clip-path:var(--rb-clip,none)}
.rmt-section{margin-bottom:10px}
.rmt-section:last-child{margin-bottom:0}
.rmt-row{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:8px;margin-bottom:8px}
.rmt-row:last-child{margin-bottom:0}
.rmt-btn{width:48px;height:48px;border:1px solid var(--rb-btn-border,var(--border));border-radius:var(--rb-btn-radius,50%);background:var(--rb-btn-bg,var(--bg));color:var(--rb-btn-fg,var(--fg));font-size:12px;font-weight:500;cursor:pointer;touch-action:manipulation;display:flex;align-items:center;justify-content:center;transition:background .1s,transform .1s;user-select:none;-webkit-user-select:none}
.rmt-btn:active,.rmt-btn.p{background:var(--active);color:#fff;border-color:var(--active);transform:scale(.93)}
.rmt-btn svg{width:20px;height:20px;fill:currentColor;pointer-events:none}
.rmt-btn.power{background:#c62828;color:#fff;border-color:#c62828}
.rmt-btn.power:active,.rmt-btn.power.p{background:#e53935}
.rmt-btn.held{background:var(--accent);color:#fff;border-color:var(--accent)}
.rmt-dpad{display:grid;grid-template-columns:48px 48px 48px;grid-template-rows:48px 48px 48px;gap:4px;justify-content:center;margin:8px 0}
.rmt-dpad .rmt-btn{border-radius:12px}
.rmt-dpad .center{background:var(--rb-ok-bg,var(--active));color:var(--rb-ok-fg,#fff);border-color:var(--rb-ok-bg,var(--active));font-size:11px;font-weight:700;border-radius:50%}
.rmt-dpad .center:active{background:var(--accent)}
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
.rmt-btn.app{width:auto;height:38px;border-radius:19px;padding:0 14px;font-size:11px}
.rmt-ring{position:relative;width:168px;height:168px;margin:10px auto;border-radius:50%;
  background:var(--rb-ring-bg,var(--rb-btn-bg,var(--bg)));border:1px solid var(--rb-btn-border,var(--border))}
.rmt-ring>span{position:absolute}
.rmt-ring>span.n{top:6px;left:50%;margin-left:-24px}
.rmt-ring>span.s{bottom:6px;left:50%;margin-left:-24px}
.rmt-ring>span.w{left:6px;top:50%;margin-top:-24px}
.rmt-ring>span.e{right:6px;top:50%;margin-top:-24px}
.rmt-ring>span.c{left:50%;top:50%;margin:-42px 0 0 -42px}
.rmt-ring .rmt-btn{background:none;border-color:transparent;color:var(--rb-ring-fg,var(--rb-btn-fg,var(--fg)))}
.rmt-ring .rmt-btn:active,.rmt-ring .rmt-btn.p{background:rgba(255,255,255,.18);border-color:transparent}
.rmt-ring .center{width:84px;height:84px}
.rmt-rocker{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:14px}
.rmt-rocker-col{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;
  padding:4px 0;width:50px;border-radius:25px;background:var(--rb-btn-bg,var(--bg));
  border:1px solid var(--rb-btn-border,var(--border))}
.rmt-rocker-col .rmt-btn{background:none;border-color:transparent;height:40px}
.rmt-rocker-col .rmt-btn:active,.rmt-rocker-col .rmt-btn.p{background:rgba(255,255,255,.18);border-color:transparent}
.rmt-rocker-label{font-size:9px;color:var(--rb-label,var(--muted));font-weight:700;text-transform:uppercase;letter-spacing:.4px}
.rmt-rocker-solo{background:none;border:none;padding:0;width:auto}
.rmt-btn.sm{width:36px;height:36px;font-size:11px}
.rmt-btn.sm svg{width:16px;height:16px}
.rmt-btn.lg{width:56px;height:56px}
.rmt-btn.xl{width:64px;height:64px;font-size:13px}
.rmt-btn.xl svg{width:26px;height:26px}
.rmt-btn.wide{width:auto;min-width:56px;padding:0 14px;border-radius:22px}
.rmt-btn.sq{border-radius:10px}
.rmt-btn.light{background:var(--rb-light-bg,#e9e9ee);color:var(--rb-light-fg,#16161a);border-color:var(--rb-light-bg,#e9e9ee)}
.rmt-btn.light:active,.rmt-btn.light.p{background:#fff;color:#000}
.rmt-head{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin-left:auto}
.rmt-head .macro-edit-btn{margin-left:0}
`;

export { RI, RMT_BTNS, RMT_VARS, RMT_BUILTIN, RMT_KINDS, RMT_OPTS, RMT_HEX, RMT_CLIP, icon, esc, btnHtml, sectionHtml, validateTpl };
