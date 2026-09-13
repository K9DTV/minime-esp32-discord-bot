#include "minime.h"
#include <WebServer.h>
#include "k9dtv_logo_svg.h"

// Display | SysInfo; under both LOG | Serial.
// Serial: fixed ring (drop top when full, new line at bottom); no scrollbar.
// MmLog still feeds web only (USB Serial quiet). FULL/END headers stripped from LOG.

static WebServer webServer(WEB_UI_PORT);
static bool webUiReady = false;

static const uint8_t WEB_FULL_N = 200;           // room to approach 20KB before wipe
static const uint8_t WEB_SERIAL_N = 12;  // fits Serial panel; oldest dropped
static const uint8_t WEB_LOG_COLS = 96;
static const size_t WEB_FULL_MAX_BYTES = 20480UL; // clear LOG if over this

static char webFullLines[WEB_FULL_N][WEB_LOG_COLS + 1];
static uint8_t webFullHead = 0;
static uint8_t webFullCount = 0;
static size_t webFullBytes = 0;
static bool webInFullLog = false;

static char webSerialLines[WEB_SERIAL_N][WEB_LOG_COLS + 1];
static uint8_t webSerialHead = 0;
static uint8_t webSerialCount = 0;

static char webLogAcc[WEB_LOG_COLS + 1];
static uint8_t webLogAccLen = 0;

static void ringPush(char lines[][WEB_LOG_COLS + 1], uint8_t n,
                     uint8_t& head, uint8_t& count, const char* text) {
  strncpy(lines[head], text, WEB_LOG_COLS);
  lines[head][WEB_LOG_COLS] = '\0';
  head = (uint8_t)((head + 1) % n);
  if (count < n) count++;
}

static void webFullClear() {
  webFullHead = 0;
  webFullCount = 0;
  webFullBytes = 0;
  for (uint8_t i = 0; i < WEB_FULL_N; i++) webFullLines[i][0] = '\0';
}

static void webFullPush(const char* text) {
  if (!text) return;
  size_t add = 0;
  while (add < WEB_LOG_COLS && text[add]) add++;
  // Over 20KB (or line cap): wipe LOG, then keep the new line.
  if (webFullCount >= WEB_FULL_N || webFullBytes + add > WEB_FULL_MAX_BYTES) {
    webFullClear();
  }
  ringPush(webFullLines, WEB_FULL_N, webFullHead, webFullCount, text);
  webFullBytes += add;
}

static bool lineIsFullStart(const char* s) {
  return s && strcmp(s, "[GW] === FULL LOG ===") == 0;
}

static bool lineIsFullEnd(const char* s) {
  return s && strcmp(s, "[GW] === END LOG ===") == 0;
}

static void webLogCommitLine() {
  webLogAcc[webLogAccLen] = '\0';
  if (lineIsFullStart(webLogAcc)) {
    webInFullLog = true;
    webFullClear();
    webLogAccLen = 0;
    return;
  }
  if (lineIsFullEnd(webLogAcc)) {
    webInFullLog = false;
    webLogAccLen = 0;
    return;
  }
  if (webInFullLog) {
    webFullPush(webLogAcc);
  } else {
    ringPush(webSerialLines, WEB_SERIAL_N, webSerialHead, webSerialCount, webLogAcc);
  }
  webLogAccLen = 0;
}

void webLogFeed(const uint8_t* buffer, size_t size) {
  if (!buffer || size == 0) return;
  for (size_t i = 0; i < size; i++) {
    char c = (char)buffer[i];
    if (c == '\r') continue;
    if (c == '\n') {
      webLogCommitLine();
      continue;
    }
    if (webLogAccLen < WEB_LOG_COLS) {
      webLogAcc[webLogAccLen++] = c;
    } else {
      webLogCommitLine();
      webLogAcc[webLogAccLen++] = c;
    }
  }
}

static void jsonEscapeAppend(String& out, const char* s) {
  if (!s) return;
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n' || c == '\r') {
      out += ' ';
    } else if ((uint8_t)c < 0x20) {
      // skip
    } else {
      out += c;
    }
  }
}

static void appendRingJson(String& out, const char lines[][WEB_LOG_COLS + 1],
                           uint8_t n, uint8_t head, uint8_t count) {
  uint8_t start = (uint8_t)((head + n - count) % n);
  for (uint8_t i = 0; i < count; i++) {
    if (i) out += ',';
    out += '"';
    uint8_t idx = (uint8_t)((start + i) % n);
    jsonEscapeAppend(out, lines[idx]);
    out += '"';
  }
}

static int barPct(int fill, int maxFill) {
  if (maxFill <= 0) return 0;
  int p = (fill * 100) / maxFill;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

static void dashFields(String& timeStr, String& dateStr, String& upStr,
                       int& sigPct, int& heapPct, int& srvPct,
                       long& rssi, uint32_t& memFree, uint32_t& memTotal,
                       String& msg1, String& msg2) {
  updateLocalTime();
  timeStr = timeClient.getFormattedTime();

  static const char* const DOW_NAME[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  static const char* const MON_NAME[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                         "Jul","Aug","Sep","Oct","Nov","Dec"};
  time_t localEpoch = (time_t)timeClient.getEpochTime();
  struct tm tmLocal;
  gmtime_r(&localEpoch, &tmLocal);
  char dateBuf[24];
  snprintf(dateBuf, sizeof(dateBuf), "%s %s %2d %04d",
           DOW_NAME[tmLocal.tm_wday], MON_NAME[tmLocal.tm_mon],
           tmLocal.tm_mday, tmLocal.tm_year + 1900);
  dateStr = dateBuf;

  unsigned long sec = millis() / 1000UL;
  unsigned long days = sec / 86400UL;
  if (days > 999UL) days = 999UL;
  unsigned long mins = (sec % 86400UL) / 60UL;
  unsigned long secs = sec % 60UL;
  char upBuf[24];
  // Fixed-width fields so d/m/s stay put: "128d  55m 28s" / "  0d   5m  3s"
  snprintf(upBuf, sizeof(upBuf), "%3lud %4lum %2lus", days, mins, secs);
  upStr = upBuf;

  rssi = WiFi.RSSI();
  int sigBarW = 0;
  if (rssi >= -40) sigBarW = 79;
  else if (rssi <= -100) sigBarW = 0;
  else sigBarW = (int)((rssi + 100) * 79 / 60);
  sigPct = barPct(sigBarW, 79);

  memFree = 0;
  memTotal = 0;
  boardMemTotals(memFree, memTotal);
  int heapBarW = 0;
  if (memTotal > 0) {
    heapBarW = (int)((memFree * 79UL) / memTotal);
    if (heapBarW < 0) heapBarW = 0;
    if (heapBarW > 79) heapBarW = 79;
  }
  heapPct = barPct(heapBarW, 79);

  const int srvInnerW = 101;
  int srvBarW = (lastServoDeg * srvInnerW) / 90;
  if (srvBarW < 0) srvBarW = 0;
  if (srvBarW > srvInnerW) srvBarW = srvInnerW;
  srvPct = barPct(srvBarW, srvInnerW);

  msg1 = "";
  msg2 = "";
  if (millis() < transientUntilMs) {
    msg1 = transientLine1;
    msg2 = transientLine2;
    if (transientLine3.length()) {
      if (msg2.length()) msg2 += " ";
      msg2 += transientLine3;
    }
  }
}

static const char CSS[] PROGMEM = R"CSS(
:root{--bg:#0b0d12;--panel:#141820;--line:#2a3344;--text:#e8edf5;--muted:#7f8fa3;--ok:#2ecc71;--bad:#e67e22;--cyan:#00d4ff;--label:#a8b4c4}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font-family:Consolas,monospace;font-size:14px}
main{max-width:56rem;margin:0 auto;padding:1rem}
.top{margin:0 0 1rem;text-align:center}
.brand{display:inline-block;text-align:center}
.brand a.logo-link{display:inline-block;line-height:0}
.brand .logo{width:min(100%,18rem);height:auto;display:block;margin:0 auto}
.brand .sub{margin:.45rem 0 0;font-size:.78rem;letter-spacing:.06em;color:var(--muted);text-transform:none}
.layout{display:grid;grid-template-columns:1fr 1fr;grid-template-areas:"display syslog" "logfile serial";gap:.75rem;align-items:stretch}
.box{border:1px solid var(--line);border-radius:.45rem;background:var(--panel);margin:0;overflow:hidden;display:flex;flex-direction:column;min-height:0}
.box h2{margin:0;padding:.45rem .7rem;font-size:.65rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);border-bottom:1px solid var(--line);background:#10141c}
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
.bar{display:block;width:100%;max-width:100%;height:.55rem;border:1px solid var(--line);background:#0a0c10;overflow:hidden;min-width:0;box-sizing:border-box}
.bar>i{display:block;height:100%;background:var(--cyan);max-width:100%}
.users{margin:.55rem 0 0;padding-top:.45rem;border-top:1px solid var(--line)}
.urole{display:grid;grid-template-columns:1fr 3.2rem 3.2rem;gap:.3rem;font-size:.72rem;color:var(--muted);margin:0 0 .2rem;letter-spacing:.04em;text-transform:uppercase}
.urow{display:grid;grid-template-columns:1fr 3.2rem 3.2rem;gap:.3rem;padding:.14rem 0;border-bottom:1px solid #1c2430}
.urow:last-child{border-bottom:none}
.urow .st,.urow .bt{color:var(--muted);text-align:right}
.msg{margin:.45rem 0 0;padding:.35rem .45rem;border:1px solid #1a4050;color:var(--cyan);font-size:.85rem}
.grid{display:grid;grid-template-columns:6.2rem 1fr;gap:.25rem .5rem;padding:.55rem .65rem;flex:1}
#box-sysinfo .grid{gap:.14rem .5rem;padding:.35rem .65rem}
.grid .k{color:var(--label);font-size:.78rem}.grid .v{word-break:break-word;font-size:.78rem}
.muted{color:var(--muted)}.ok{color:var(--ok)}.bad{color:var(--bad)}
.err{color:var(--bad);padding:.4rem .7rem;font-size:.85rem;grid-column:1/-1}
.serial{padding:.3rem .55rem .45rem;font-size:.78rem;flex:1;min-height:0;overflow:auto}
.serial.noscroll{overflow:hidden;display:flex;flex-direction:column;justify-content:flex-end}
.serial div{padding:.12rem 0;border-bottom:1px solid #1c2430;white-space:pre-wrap;word-break:break-word;color:#c5d0de;min-height:1.15em}
.serial.noscroll div{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;word-break:normal;flex:0 0 auto}
.serial div:last-child{border-bottom:none}.serial .empty{color:var(--muted)}
@media (max-width:720px){
.layout{grid-template-columns:1fr;grid-template-areas:"display" "syslog" "logfile" "serial"}
}
)CSS";

static void appendBrand(String& html) {
  html += F("<div class=\"top\"><header class=\"brand\">");
  html += F("<a class=\"logo-link\" href=\"https://k9dtv.com\" target=\"_blank\" rel=\"noopener\">");
  html += F("<img class=\"logo\" src=\"/logo.svg\" width=\"343\" height=\"107\" alt=\"K9DTV\"></a>");
  html += F("<p class=\"sub\">MiniMe A Discord Server APP</p></header></div>");
}

static void sendNoCacheHeaders() {
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
}

static String buildRootHtml() {
  String html;
  html.reserve(16000);
  html += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<meta http-equiv=\"Cache-Control\" content=\"no-store, no-cache, must-revalidate, max-age=0\">");
  html += F("<meta http-equiv=\"Pragma\" content=\"no-cache\">");
  html += F("<meta http-equiv=\"Expires\" content=\"0\">");
  html += F("<title>MiniMe</title><style>");
  html += FPSTR(CSS);
  html += F("</style></head><body><main>");
  appendBrand(html);

  html += F("<div class=\"layout\">");
  html += F("<section class=\"box\" id=\"box-display\"><h2>Display · v0.4.93</h2>");
  html += F("<div id=\"dash\" class=\"dash muted\">Loading...</div></section>");
  html += F("<section class=\"box\" id=\"box-sysinfo\"><h2>SysInfo</h2>");
  html += F("<div id=\"sysinfo\" class=\"grid muted\">Loading...</div></section>");
  html += F("<section class=\"box\" id=\"box-logfile\"><h2>LOG</h2>");
  html += F("<div id=\"logfile\" class=\"serial\"><div class=\"empty\">Waiting...</div></div></section>");
  html += F("<section class=\"box\" id=\"box-serial\"><h2>Serial</h2>");
  html += F("<div id=\"serial\" class=\"serial noscroll\"><div class=\"empty\">Waiting...</div></div></section>");
  html += F("<div id=\"err\" class=\"err\" hidden></div>");
  html += F("</div></main><script>");
  html += F("function esc(s){return String(s||'').replace(/[&<>\"']/g,c=>({ '&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;' }[c]));}");
  html += F("function bar(pct){pct=Math.max(0,Math.min(100,+pct||0));return '<span class=\"bar\"><i style=\"width:'+pct+'%\"></i></span>';}");
  html += F("function mline(lab,n,pct){return '<div class=\"mline\"><span class=\"k\">'+lab+'</span><span class=\"n\">'+n+'</span>'+bar(pct)+'</div>';}");
  html += F("function row(k,v){return '<span class=\"k\">'+esc(k)+'</span><span class=\"v\">'+v+'</span>';}");
  html += F("function linesHtml(lines){");
  html += F("var a=(lines||[]).filter(function(l){return !!l;});");
  html += F("if(!a.length)return '<div class=\"empty\">Waiting...</div>';");
  html += F("return a.map(function(l){return '<div>'+esc(l)+'</div>';}).join('');}");
  html += F("function render(j){");
  html += F("var gw=j.gw?'<span class=\"ok\">GW:Good</span>':'<span class=\"bad\">GW:Bad</span>';");
  html += F("var bot=j.botOnline?'<span class=\"ok\">Online</span>':'<span class=\"muted\">Idle</span>';");
  html += F("var temp=j.tempOk?(esc(j.tempF)+'F / '+esc(j.tempC)+'C'):'--Error--';");
  html += F("var msg='';if(j.msg1||j.msg2){msg='<div class=\"msg\">'+esc(j.msg1||'')+(j.msg2?(' '+esc(j.msg2)):'')+'</div>';}");
  html += F("var users='<div class=\"users\"><div class=\"urole\"><span>User</span><span class=\"st\">Status</span><span class=\"bt\">Bot</span></div>';");
  html += F("(j.users||[]).forEach(function(u){users+='<div class=\"urow\"><span>'+esc(u.name)+'</span><span class=\"st\">'+esc(u.status)+'</span><span class=\"bt\">'+esc(u.bot)+'</span></div>';});");
  html += F("users+='</div>';");
  html += F("document.getElementById('dash').innerHTML=");
  html += F("'<div class=\"hdr\"><strong>MiniMe</strong><span class=\"c\">'+gw+'</span><span class=\"r\">'+esc(j.time)+'</span></div>'+");
  html += F("'<div class=\"kv\"><span class=\"k\">Bot</span><span class=\"v\">'+bot+' <span class=\"muted\">'+esc(j.date)+'</span></span></div>'+");
  html += F("'<div class=\"kv\"><span class=\"k\">Up</span><span class=\"v\">'+esc(j.uptime)+'</span></div>'+");
  html += F("'<div class=\"kv\"><span class=\"k\">Temp</span><span class=\"v\">'+temp+'</span></div>'+");
  html += F("mline('Sig',esc(j.rssi)+' dBm',j.sigPct)+mline('Heap',esc(j.heapFree),j.heapPct)+mline('Srv',esc(j.servo)+'\\u00b0',j.srvPct)+users+msg;");
  html += F("var gwL=j.gw?'Connected':'Disconnected';");
  html += F("var botL=j.botOnline?'Online':'Idle';");
  html += F("var tempL=j.tempOk?(esc(j.tempF)+' F / '+esc(j.tempC)+' C'):'--Error--';");
  html += F("var tr='';if(j.msg1||j.msg2)tr=esc(j.msg1||'')+(j.msg2?(' '+esc(j.msg2)):'');");
  html += F("document.getElementById('sysinfo').className='grid';");
  html += F("document.getElementById('sysinfo').innerHTML=");
  html += F("row('IP',esc(j.ip))+row('OTA',esc(j.ota))+row('RSSI',esc(j.rssi)+' dBm')+");
  html += F("row('CPU',esc(j.cpuMhz)+' MHz')+row('Heap',esc(j.heapFree)+' / '+esc(j.heapTotal))+row('Uptime',esc(j.uptime))+");
  html += F("row('Time',esc(j.time)+'  '+esc(j.date))+row('Gateway',esc(gwL))+row('Bot',esc(botL))+");
  html += F("row('Servo',esc(j.servo)+' deg')+row('Temp',tempL)+row('USB VBUS',esc(j.vbus))+");
  html += F("row('OLED',esc(j.oled))+(tr?row('Transient',tr):'');");
  html += F("var fl=(j.fulllog||[]).filter(function(l){return !!l;});");
  html += F("var ser=(j.serial||[]).filter(function(l){return !!l;});");
  html += F("document.getElementById('logfile').innerHTML=linesHtml(fl);");
  html += F("document.getElementById('serial').innerHTML=linesHtml(ser);");
  html += F("document.getElementById('err').hidden=true;}");
  html += F("async function tick(){var e=document.getElementById('err');try{");
  html += F("var r=await fetch('/api/status?t='+Date.now());var t=await r.text();");
  html += F("if(!r.ok){e.textContent='status HTTP '+r.status;e.hidden=false;return;}");
  html += F("render(JSON.parse(t));}catch(ex){e.textContent='status: '+(ex&&ex.message?ex.message:ex);e.hidden=false;}}");
  html += F("tick();setInterval(tick,1000);</script></body></html>");
  return html;
}

static String buildStatusJson() {
  String timeStr, dateStr, upStr, msg1, msg2;
  int sigPct = 0, heapPct = 0, srvPct = 0;
  long rssi = 0;
  uint32_t memFree = 0, memTotal = 0;
  dashFields(timeStr, dateStr, upStr, sigPct, heapPct, srvPct, rssi, memFree, memTotal, msg1, msg2);

  String out;
  out.reserve(3200);
  out += '{';
  out += F("\"gw\":");
  out += gatewayConnected ? F("true") : F("false");
  out += F(",\"botOnline\":");
  out += (botDiscordStatus == 2) ? F("true") : F("false");
  out += F(",\"time\":\"");
  jsonEscapeAppend(out, timeStr.c_str());
  out += F("\",\"date\":\"");
  jsonEscapeAppend(out, dateStr.c_str());
  out += F("\",\"uptime\":\"");
  jsonEscapeAppend(out, upStr.c_str());
  out += F("\",\"tempOk\":");
  out += (dashTempC > -998.0f) ? F("true") : F("false");
  out += F(",\"tempF\":");
  out += String((dashTempC > -998.0f) ? (int)(dashTempF >= 0 ? dashTempF + 0.5f : dashTempF - 0.5f) : 0);
  out += F(",\"tempC\":");
  out += String((dashTempC > -998.0f) ? (int)(dashTempC >= 0 ? dashTempC + 0.5f : dashTempC - 0.5f) : 0);
  out += F(",\"rssi\":");
  out += String((int)rssi);
  out += F(",\"sigPct\":");
  out += String(sigPct);
  out += F(",\"heapFree\":");
  out += String(memFree);
  out += F(",\"heapTotal\":");
  out += String(memTotal);
  out += F(",\"heapPct\":");
  out += String(heapPct);
  out += F(",\"cpuMhz\":");
  out += String((unsigned)getCpuFrequencyMhz());
  out += F(",\"servo\":");
  out += String(lastServoDeg);
  out += F(",\"srvPct\":");
  out += String(srvPct);
  out += F(",\"ip\":\"");
  jsonEscapeAppend(out, WiFi.localIP().toString().c_str());
  out += F("\",\"ota\":\"");
  {
    String ota = String(OTA_HOSTNAME) + ".local";
    jsonEscapeAppend(out, ota.c_str());
  }
  out += F("\",\"vbus\":\"");
  {
    char vb[16];
    snprintf(vb, sizeof(vb), "%.3f V", (float)readUsbVbusMilliVolts() / 1000.0f);
    jsonEscapeAppend(out, vb);
  }
  out += F("\",\"oled\":\"");
  jsonEscapeAppend(out, displayAsleep ? "asleep" : "awake");
  out += F("\",\"msg1\":\"");
  jsonEscapeAppend(out, msg1.c_str());
  out += F("\",\"msg2\":\"");
  jsonEscapeAppend(out, msg2.c_str());
  out += F("\",\"users\":[");
  for (uint8_t row = 0; row < MAX_TRACKED_USERS; row++) {
    if (row) out += ',';
    const char* name = "---";
    if (trackedUsers[row].active) {
      if (trackedUsers[row].userName.length()) name = trackedUsers[row].userName.c_str();
      else name = trackedUsers[row].userId.c_str();
    }
    out += F("{\"name\":\"");
    jsonEscapeAppend(out, name);
    out += F("\",\"status\":\"");
    jsonEscapeAppend(out, statusToWord(trackedUsers[row].active ? trackedUsers[row].status : 0));
    out += F("\",\"bot\":\"");
    char botBuf[12];
    snprintf(botBuf, sizeof(botBuf), "%lu",
             (unsigned long)(trackedUsers[row].active ? trackedUsers[row].useCount24h : 0));
    jsonEscapeAppend(out, botBuf);
    out += F("\"}");
  }
  out += F("],\"fulllog\":[");
  appendRingJson(out, webFullLines, WEB_FULL_N, webFullHead, webFullCount);
  out += F("],\"serial\":[");
  appendRingJson(out, webSerialLines, WEB_SERIAL_N, webSerialHead, webSerialCount);
  out += F("]}");
  return out;
}

static void handleRoot() {
  sendNoCacheHeaders();
  webServer.send(200, "text/html; charset=utf-8", buildRootHtml());
}

static void handleStatus() {
  sendNoCacheHeaders();
  webServer.send(200, "application/json", buildStatusJson());
}

static void handleLogo() {
  webServer.sendHeader("Cache-Control", "public, max-age=86400");
  webServer.send_P(200, "image/svg+xml", K9DTV_LOGO_SVG);
}

void setupWebUi() {
  webFullClear();
  webInFullLog = false;
  for (uint8_t i = 0; i < WEB_SERIAL_N; i++) webSerialLines[i][0] = '\0';
  webSerialHead = 0;
  webSerialCount = 0;
  webLogAccLen = 0;
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/logo.svg", HTTP_GET, handleLogo);
  webServer.on("/api/status", HTTP_GET, handleStatus);
  webServer.begin();
  webUiReady = true;
  MmLog.print("[WEB] http://");
  MmLog.print(WiFi.localIP().toString());
  MmLog.print(":");
  MmLog.println(WEB_UI_PORT);
  MmLog.flushAll();
}

void pumpWebUi() {
  if (!webUiReady) return;
  webServer.handleClient();
}

bool webUiKeepsCpuActive() {
  return webUiReady;
}
