#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

// LAN web UI CSS + JS (PROGMEM), same pattern as logo SVG headers.
#include <Arduino.h>

static const char WEB_UI_CSS[] PROGMEM = R"CSS(
:root{--k9-space:#121212;--k9-panel:#1a1a1a;--k9-panel-hover:#242424;--k9-card:#1e1e1e;--k9-text:#e0e0e0;--k9-muted:#b8b8b8;--k9-orange:#ffb020;--k9-cyan:#5eb3ff;--k9-border:#2c2c2c;--k9-green:#2ecc71;--k9-ui:"Segoe UI","Helvetica Neue",Arial,sans-serif;--k9-mono:ui-monospace,Consolas,monospace;--bg:var(--k9-space);--panel:var(--k9-panel);--line:var(--k9-border);--text:var(--k9-text);--muted:var(--k9-muted);--ok:var(--k9-green);--bad:var(--k9-orange);--cyan:var(--k9-cyan);--label:var(--k9-muted);--box-head:#141414;--row-line:#242424;--bar-track:#0a0a0a;--msg-border:#1a4050}
html{color-scheme:dark}
html[data-theme="light"]{color-scheme:light;--k9-space:#dde2ea;--k9-panel:#f3f5f8;--k9-panel-hover:#e8ecf2;--k9-card:#ffffff;--k9-text:#0f172a;--k9-muted:#334155;--k9-orange:#9a3412;--k9-cyan:#005f73;--k9-border:#8b95a5;--k9-green:#14532d;--box-head:#e8ecf2;--row-line:#c5ced9;--bar-track:#ffffff;--msg-border:#94a3b8}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font-family:var(--k9-mono);font-size:14px}
main{max-width:56rem;margin:0 auto;padding:1rem}
.top{margin:0 0 1rem;display:flex;flex-direction:column;align-items:center;gap:.45rem}
.top-row{display:flex;align-items:center;justify-content:center;gap:.75rem;width:100%;position:relative;padding-bottom:1.15rem}
.theme-chip-trigger{display:flex;flex-direction:column;align-items:center;justify-content:center;position:relative;margin:0;padding:0;border:none;background:transparent;cursor:pointer;line-height:0;-webkit-tap-highlight-color:transparent;flex:0 0 auto;align-self:center}
.theme-chip-trigger .menu-chip-icon{width:2.75rem;height:2.75rem;display:block;flex-shrink:0}
.theme-chip-trigger .menu-chip-label{position:absolute;top:calc(100% + .08rem);left:50%;transform:translateX(-50%);display:inline-flex;flex-direction:row;align-items:center;justify-content:center;gap:.22em;font-family:var(--k9-ui);font-size:.58rem;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--k9-muted);line-height:1;white-space:nowrap}
.theme-chip-trigger .theme-toggle-glyph{font-size:.85em;line-height:1;font-weight:400;letter-spacing:0;text-transform:none}
.theme-chip-trigger:focus{outline:none}
.theme-chip-trigger:focus:not(:focus-visible){outline:none}
.theme-chip-trigger:focus-visible{outline:2px solid var(--k9-cyan);outline-offset:3px}
.theme-chip-trigger:active{outline:none}
.brand{display:inline-block;text-align:center;flex:0 1 auto;line-height:0}
.brand a.logo-link{display:inline-block;line-height:0}
.brand .logo{width:min(100%,18rem);height:auto;display:block;margin:0 auto}
.top .sub{margin:0;font-size:.78rem;letter-spacing:.06em;color:var(--muted);text-transform:none;text-align:center}
.layout{display:grid;grid-template-columns:1fr 1fr;grid-template-areas:"display syslog" "logfile serial";gap:.75rem;align-items:stretch}
html[data-layout="log"] .layout{grid-template-areas:"display syslog"}
html[data-layout="log"] #box-logfile,html[data-layout="log"] #box-serial{display:none}
.box{border:1px solid var(--line);border-radius:.45rem;background:var(--panel);margin:0;overflow:hidden;display:flex;flex-direction:column;min-height:0}
.box h2{margin:0;padding:.45rem .7rem;font-size:.65rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);border-bottom:1px solid var(--line);background:var(--box-head)}
#box-sysinfo{grid-area:syslog}#box-display{grid-area:display}#box-logfile{grid-area:logfile}#box-serial{grid-area:serial}
#box-logfile,#box-serial{min-height:10em;max-height:18em}
.dash{padding:.6rem .7rem;flex:1;min-width:0;overflow:hidden}
.hdr{display:grid;grid-template-columns:1fr auto 1fr;gap:.35rem;margin:0 0 .45rem;padding-bottom:.35rem;border-bottom:1px solid var(--line)}
.hdr .c{text-align:center}.hdr .r{text-align:right}
.kv{display:grid;grid-template-columns:3.4rem 1fr;gap:.15rem .45rem;margin:0 0 .22rem}
.kv .k{color:var(--label);font-size:.8rem}
.mline{display:grid;grid-template-columns:3.2rem 7ch minmax(0,1fr);column-gap:.35rem;align-items:center;margin:0 0 .22rem;width:100%;max-width:100%}
.mline .k{color:var(--label);font-size:.8rem}
.mline .n{color:var(--muted);font-size:.82rem;white-space:nowrap;overflow:hidden}
.bar{display:block;width:100%;max-width:100%;height:.55rem;border:1px solid var(--line);background:var(--bar-track);overflow:hidden;min-width:0;box-sizing:border-box}
.bar>i{display:block;height:100%;background:var(--cyan);max-width:100%}
.users{margin:.55rem 0 0;padding-top:.45rem;border-top:1px solid var(--line)}
.urole{display:grid;grid-template-columns:1fr 3.2rem 3.2rem;gap:.3rem;font-size:.72rem;color:var(--muted);margin:0 0 .2rem;letter-spacing:.04em;text-transform:uppercase}
.urow{display:grid;grid-template-columns:1fr 3.2rem 3.2rem;gap:.3rem;padding:.14rem 0;border-bottom:1px solid var(--row-line)}
.urow:last-child{border-bottom:none}
.urow .st,.urow .bt{color:var(--muted);text-align:right}
.msg{margin:.45rem 0 0;padding:.35rem .45rem;border:1px solid var(--msg-border);color:var(--cyan);font-size:.85rem}
.grid{display:grid;grid-template-columns:6.2rem 1fr;gap:.25rem .5rem;padding:.55rem .65rem;flex:1}
#box-sysinfo .grid{gap:.14rem .5rem;padding:.35rem .65rem}
.grid .k{color:var(--label);font-size:.78rem}.grid .v{word-break:break-word;font-size:.78rem}
.muted{color:var(--muted)}.ok{color:var(--ok)}.bad{color:var(--bad)}
.err{color:var(--bad);padding:.4rem .7rem;font-size:.85rem;grid-column:1/-1}
.serial{padding:.3rem .55rem .45rem;font-size:.78rem;flex:1;min-height:0;overflow:auto}
.serial.noscroll{overflow:hidden;display:flex;flex-direction:column;justify-content:flex-end}
.serial div{padding:.12rem 0;border-bottom:1px solid var(--row-line);white-space:pre-wrap;word-break:break-word;color:var(--text);min-height:1.15em}
.serial.noscroll div{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;word-break:normal;flex:0 0 auto}
.serial div:last-child{border-bottom:none}.serial .empty{color:var(--muted)}
@media (max-width:720px){
.layout{grid-template-columns:1fr;grid-template-areas:"display" "syslog" "logfile" "serial"}
html[data-layout="log"] .layout{grid-template-areas:"display" "syslog"}
.top-row{flex-wrap:wrap;justify-content:center}
}
)CSS";

// Head: apply saved theme/layout before paint (no POLL_MS).
static const char WEB_UI_BOOT_JS[] PROGMEM = R"JS(
(function(){try{var k='k9-theme';var t=localStorage.getItem(k);
if(t==='light')document.documentElement.setAttribute('data-theme','light');
else if(t==='dark')document.documentElement.removeAttribute('data-theme');
else if(window.matchMedia&&window.matchMedia('(prefers-color-scheme: light)').matches)
document.documentElement.setAttribute('data-theme','light');
var L=localStorage.getItem('mm-layout');
if(L==='log')document.documentElement.setAttribute('data-layout','log');
else document.documentElement.removeAttribute('data-layout');}catch(e){}})();
)JS";

// Body app JS. Expects global POLL_MS (set by firmware before this script).
static const char WEB_UI_JS[] PROGMEM = R"JS(
var THEME_KEY='k9-theme';var LAYOUT_KEY='mm-layout';
function themeNow(){return document.documentElement.getAttribute('data-theme')==='light'?'light':'dark';}
function layoutNow(){return document.documentElement.getAttribute('data-layout')==='log'?'log':'display';}
function chipSrc(){return themeNow()==='light'?'/chip-bright.svg':'/chip.svg';}
function applyTheme(t,persist){
if(t==='light')document.documentElement.setAttribute('data-theme','light');
else document.documentElement.removeAttribute('data-theme');
if(persist){try{localStorage.setItem(THEME_KEY,t);}catch(e){}}
var light=t==='light';
var logo=document.getElementById('brand-logo');
var chip=document.getElementById('theme-chip-img');
var lchip=document.getElementById('layout-chip-img');
var glyph=document.getElementById('theme-chip-glyph');
var text=document.getElementById('theme-chip-text');
var btn=document.getElementById('theme-toggle');
if(logo)logo.src=light?'/logo-bright.svg':'/logo.svg';
if(chip)chip.src=chipSrc();
if(lchip)lchip.src=chipSrc();
if(glyph)glyph.textContent=light?'\u263D':'\u2600';
if(text)text.textContent=light?'Dark':'Light';
if(btn){btn.setAttribute('aria-pressed',light?'true':'false');
btn.setAttribute('aria-label',light?'Switch to dark mode':'Switch to light mode');}}
function applyLayout(m,persist){
if(m==='log')document.documentElement.setAttribute('data-layout','log');
else document.documentElement.removeAttribute('data-layout');
if(persist){try{localStorage.setItem(LAYOUT_KEY,m);}catch(e){}}
var log=m==='log';
var text=document.getElementById('layout-chip-text');
var btn=document.getElementById('layout-toggle');
if(text)text.textContent=log?'Log':'Display';
if(btn){btn.setAttribute('aria-pressed',log?'true':'false');
btn.setAttribute('aria-label',log?'Switch to display view':'Switch to log view');}}
applyTheme(themeNow(),false);
applyLayout(layoutNow(),false);
var tb=document.getElementById('theme-toggle');
if(tb)tb.addEventListener('click',function(){applyTheme(themeNow()==='light'?'dark':'light',true);tb.blur();});
var lb=document.getElementById('layout-toggle');
if(lb)lb.addEventListener('click',function(){applyLayout(layoutNow()==='log'?'display':'log',true);lb.blur();});
function esc(s){return String(s||'').replace(/[&<>"']/g,c=>({ '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;' }[c]));}
function bar(pct){pct=Math.max(0,Math.min(100,+pct||0));return '<span class="bar"><i style="width:'+pct+'%"></i></span>';}
function mline(lab,n,pct){return '<div class="mline"><span class="k">'+lab+'</span><span class="n">'+n+'</span>'+bar(pct)+'</div>';}
function row(k,v){return '<span class="k">'+esc(k)+'</span><span class="v">'+v+'</span>';}
function linesHtml(lines){
var a=(lines||[]).filter(function(l){return !!l;});
if(!a.length)return '<div class="empty">Waiting...</div>';
return a.map(function(l){return '<div>'+esc(l)+'</div>';}).join('');}
function render(j){
var gw=j.gw?'<span class="ok">GW:Good</span>':'<span class="bad">GW:Bad</span>';
var bot=j.botOnline?'<span class="ok">Online</span>':'<span class="muted">Idle</span>';
var temp=j.tempOk?(esc(j.tempF)+'F / '+esc(j.tempC)+'C'):'--Error--';
var msg='';if(j.msg1||j.msg2){msg='<div class="msg">'+esc(j.msg1||'')+(j.msg2?(' '+esc(j.msg2)):'')+'</div>';}
var users='<div class="users"><div class="urole"><span>User</span><span class="st">Status</span><span class="bt">Bot</span></div>';
(j.users||[]).forEach(function(u){users+='<div class="urow"><span>'+esc(u.name)+'</span><span class="st">'+esc(u.status)+'</span><span class="bt">'+esc(u.bot)+'</span></div>';});
users+='</div>';
document.getElementById('dash').innerHTML=
'<div class="hdr"><strong>MiniMe</strong><span class="c">'+gw+'</span><span class="r">'+esc(j.time)+'</span></div>'+
'<div class="kv"><span class="k">Bot</span><span class="v">'+bot+' <span class="muted">'+esc(j.date)+'</span></span></div>'+
'<div class="kv"><span class="k">Up</span><span class="v">'+esc(j.uptime)+'</span></div>'+
'<div class="kv"><span class="k">Temp</span><span class="v">'+temp+'</span></div>'+
mline('Sig',esc(j.rssi)+' dBm',j.sigPct)+mline('Heap',esc(j.heapFree),j.heapPct)+mline('Srv',esc(j.servo)+'\u00b0',j.srvPct)+users+msg;
var gwL=j.gw?'Connected':'Disconnected';
var botL=j.botOnline?'Online':'Idle';
var tempL=j.tempOk?(esc(j.tempF)+' F / '+esc(j.tempC)+' C'):'--Error--';
var tr='';if(j.msg1||j.msg2)tr=esc(j.msg1||'')+(j.msg2?(' '+esc(j.msg2)):'');
document.getElementById('sysinfo').className='grid';
document.getElementById('sysinfo').innerHTML=
row('IP',esc(j.ip))+row('OTA',esc(j.ota))+row('RSSI',esc(j.rssi)+' dBm')+
row('CPU',esc(j.cpuMhz)+' MHz')+row('Heap',esc(j.heapFree)+' / '+esc(j.heapTotal))+row('Uptime',esc(j.uptime))+
row('Time',esc(j.time)+'  '+esc(j.date))+row('Gateway',esc(gwL))+row('Bot',esc(botL))+
row('Servo',esc(j.servo)+' deg')+row('Temp',tempL)+row('USB VBUS',esc(j.vbus))+
row('OLED',esc(j.oled))+(tr?row('Transient',tr):'');
var fl=(j.fulllog||[]).filter(function(l){return !!l;});
var ser=(j.serial||[]).filter(function(l){return !!l;});
document.getElementById('logfile').innerHTML=linesHtml(fl);
document.getElementById('serial').innerHTML=linesHtml(ser);
document.getElementById('err').hidden=true;}
async function tick(){var e=document.getElementById('err');var j;try{
var r=await fetch('/api/status?t='+Date.now());var t=await r.text();
if(!r.ok){e.textContent='status HTTP '+r.status;e.hidden=false;return;}
j=JSON.parse(t);}catch(ex){e.textContent='status: '+(ex&&ex.message?ex.message:ex);e.hidden=false;return;}
try{render(j);}catch(ex){e.textContent='render: '+(ex&&ex.message?ex.message:ex);e.hidden=false;}}
tick();setInterval(tick,typeof POLL_MS==='number'?POLL_MS:2000);
)JS";

#endif
