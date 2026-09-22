#include "minime.h"
#include <WebServer.h>
#include "k9dtv_logo_svg.h"
#include "k9dtv_logo_bright_svg.h"
#include "menu_chip_svg.h"
#include "web_assets.h"

// Display | SysInfo; under both LOG | Serial.
// Serial: fixed ring (drop top when full, new line at bottom); no scrollbar.
// MmLog still feeds web only (USB Serial quiet). FULL/END headers stripped from LOG.

static WebServer webServer(WEB_UI_PORT);
static bool webUiReady = false;

static const unsigned WEB_FULL_N = 200;           // ring size; oldest dropped when full
static const unsigned WEB_SERIAL_N = 12;  // fits Serial panel; oldest dropped
static const uint8_t WEB_LOG_COLS = 96;
static const size_t WEB_FULL_MAX_BYTES = 20480UL; // clear LOG if over this
static_assert(WEB_FULL_N >= 1 && WEB_FULL_N <= 255, "WEB_FULL_N must fit uint8_t head/count");
static_assert(WEB_SERIAL_N >= 1 && WEB_SERIAL_N <= 255, "WEB_SERIAL_N must fit uint8_t head/count");

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

  // Ring full: drop oldest at head and subtract its stored length.
  if (webFullCount >= WEB_FULL_N) {
    size_t drop = 0;
    while (drop < WEB_LOG_COLS && webFullLines[webFullHead][drop]) drop++;
    if (webFullBytes >= drop) webFullBytes -= drop;
    else webFullBytes = 0;
  }

  // Over 20KB after eviction accounting: wipe LOG, then keep the new line.
  if (webFullBytes + add > WEB_FULL_MAX_BYTES) {
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

static void dashFields(String& timeStr, String& dateStr, String& upStr,
                       int& sigPct, int& heapPct, int& srvPct,
                       long& rssi, uint32_t& memFree, uint32_t& memTotal,
                       String& msg1, String& msg2) {
  updateLocalTime();
  timeStr = timeClient.getFormattedTime();

  char dateBuf[24];
  formatLocalDateStr(dateBuf, sizeof(dateBuf));
  dateStr = dateBuf;

  char upBuf[28];
  formatUptimeStr(upBuf, sizeof(upBuf));
  upStr = upBuf;

  // Percents from OLED bar fills (dashSigBarW / dashHeapBarW / dashSrvBarW).
  rssi = WiFi.RSSI();
  sigPct = dashBarPct(dashSigBarW(rssi), DASH_SIG_HEAP_BAR_MAX);

  memFree = 0;
  memTotal = 0;
  boardMemTotals(memFree, memTotal);
  heapPct = dashBarPct(dashHeapBarW(memFree, memTotal), DASH_SIG_HEAP_BAR_MAX);
  srvPct = dashBarPct(dashSrvBarW(lastServoDeg), DASH_SRV_BAR_MAX);

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

// CSS lives in web_assets.h (WEB_UI_CSS).

static void appendBrand(String& html) {
  html += F("<div class=\"top\"><div class=\"top-row\">");
  html += F("<button type=\"button\" id=\"theme-toggle\" class=\"theme-chip-trigger\" aria-pressed=\"false\" aria-label=\"Switch to light mode\">");
  html += F("<img class=\"menu-chip-icon\" id=\"theme-chip-img\" src=\"/chip.svg\" width=\"64\" height=\"64\" alt=\"\" aria-hidden=\"true\">");
  html += F("<span class=\"menu-chip-label\" aria-hidden=\"true\">");
  html += F("<span class=\"theme-toggle-glyph\" id=\"theme-chip-glyph\">&#9728;</span>");
  html += F("<span class=\"theme-toggle-text\" id=\"theme-chip-text\">Light</span></span></button>");
  html += F("<header class=\"brand\">");
  html += F("<a class=\"logo-link\" href=\"https://k9dtv.com\" target=\"_blank\" rel=\"noopener\">");
  html += F("<img class=\"logo\" id=\"brand-logo\" src=\"/logo.svg\" width=\"343\" height=\"107\" alt=\"K9DTV\"></a></header>");
  html += F("<button type=\"button\" id=\"layout-toggle\" class=\"theme-chip-trigger\" aria-pressed=\"false\" aria-label=\"Switch to log view\">");
  html += F("<img class=\"menu-chip-icon\" id=\"layout-chip-img\" src=\"/chip.svg\" width=\"64\" height=\"64\" alt=\"\" aria-hidden=\"true\">");
  html += F("<span class=\"menu-chip-label\" aria-hidden=\"true\">");
  html += F("<span class=\"theme-toggle-text\" id=\"layout-chip-text\">Display</span></span></button>");
  html += F("</div><p class=\"sub\">MiniMe A Discord Server APP</p></div>");
}

static void sendNoCacheHeaders() {
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "0");
}

static String buildRootHtml() {
  String html;
  html.reserve(19000);
  html += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<meta http-equiv=\"Cache-Control\" content=\"no-store, no-cache, must-revalidate, max-age=0\">");
  html += F("<meta http-equiv=\"Pragma\" content=\"no-cache\">");
  html += F("<meta http-equiv=\"Expires\" content=\"0\">");
  html += F("<script>");
  html += FPSTR(WEB_UI_BOOT_JS);
  html += F("</script>");
  html += F("<title>MiniMe</title><style>");
  html += FPSTR(WEB_UI_CSS);
  html += F("</style></head><body><main>");
  appendBrand(html);

  html += F("<div class=\"layout\">");
  html += F("<section class=\"box\" id=\"box-display\"><h2>Display · v0.5.3</h2>");
  html += F("<div id=\"dash\" class=\"dash muted\">Loading...</div></section>");
  html += F("<section class=\"box\" id=\"box-sysinfo\"><h2>SysInfo</h2>");
  html += F("<div id=\"sysinfo\" class=\"grid muted\">Loading...</div></section>");
  html += F("<section class=\"box\" id=\"box-logfile\"><h2>LOG</h2>");
  html += F("<div id=\"logfile\" class=\"serial\"><div class=\"empty\">Waiting...</div></div></section>");
  html += F("<section class=\"box\" id=\"box-serial\"><h2>Serial</h2>");
  html += F("<div id=\"serial\" class=\"serial noscroll\"><div class=\"empty\">Waiting...</div></div></section>");
  html += F("<div id=\"err\" class=\"err\" hidden></div>");
  html += F("</div></main><script>var POLL_MS=");
  html += String((unsigned long)WEB_STATUS_POLL_MS);
  html += F(";</script><script>");
  html += FPSTR(WEB_UI_JS);
  html += F("</script></body></html>");
  return html;
}

template <size_t N>
static void appendRingToJsonArray(JsonArray arr, const char (&lines)[N][WEB_LOG_COLS + 1],
                                  uint8_t head, uint8_t count) {
  uint8_t start = (uint8_t)((head + N - count) % N);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (uint8_t)((start + i) % N);
    arr.add(lines[idx]);
  }
}

static int roundTempHalfAway(float v) {
  return (int)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}

// Allocated once in setupWebUi() (after PSRAM is up), then reused with clear().
static DynamicJsonDocument* statusDoc = nullptr;
static const size_t STATUS_DOC_BYTES = 32768;

static String buildStatusJson() {
  String timeStr, dateStr, upStr, msg1, msg2;
  int sigPct = 0, heapPct = 0, srvPct = 0;
  long rssi = 0;
  uint32_t memFree = 0, memTotal = 0;
  dashFields(timeStr, dateStr, upStr, sigPct, heapPct, srvPct, rssi, memFree, memTotal, msg1, msg2);

  if (!statusDoc) {
    return String(F("{\"err\":\"statusDoc\"}"));
  }
  DynamicJsonDocument& doc = *statusDoc;
  doc.clear();
  doc["gw"] = gatewayConnected;
  doc["botOnline"] = (botDiscordStatus == 2);
  doc["time"] = timeStr;
  doc["date"] = dateStr;
  doc["uptime"] = upStr;
  bool tempOk = (dashTempC > -998.0f);
  doc["tempOk"] = tempOk;
  doc["tempF"] = tempOk ? roundTempHalfAway(dashTempF) : 0;
  doc["tempC"] = tempOk ? roundTempHalfAway(dashTempC) : 0;
  doc["rssi"] = (int)rssi;
  doc["sigPct"] = sigPct;
  doc["heapFree"] = memFree;
  doc["heapTotal"] = memTotal;
  doc["heapPct"] = heapPct;
  doc["cpuMhz"] = (unsigned)getCpuFrequencyMhz();
  doc["servo"] = lastServoDeg;
  doc["srvPct"] = srvPct;
  doc["ip"] = WiFi.localIP().toString();
  doc["ota"] = String(OTA_HOSTNAME) + ".local";
  {
    char vb[16];
    snprintf(vb, sizeof(vb), "%.3f V", (float)readUsbVbusMilliVolts() / 1000.0f);
    doc["vbus"] = vb;
  }
  doc["oled"] = displayAsleep ? "asleep" : "awake";
  doc["msg1"] = msg1;
  doc["msg2"] = msg2;

  JsonArray users = doc.createNestedArray("users");
  for (uint8_t row = 0; row < MAX_TRACKED_USERS; row++) {
    JsonObject u = users.createNestedObject();
    const char* name = "---";
    if (trackedUsers[row].active) {
      if (trackedUsers[row].userName.length()) name = trackedUsers[row].userName.c_str();
      else name = trackedUsers[row].userId.c_str();
    }
    u["name"] = name;
    u["status"] = statusToWord(trackedUsers[row].active ? trackedUsers[row].status : 0);
    char botBuf[12];
    snprintf(botBuf, sizeof(botBuf), "%lu",
             (unsigned long)(trackedUsers[row].active ? trackedUsers[row].useCount24h : 0));
    u["bot"] = botBuf;
  }

  JsonArray fulllog = doc.createNestedArray("fulllog");
  appendRingToJsonArray(fulllog, webFullLines, webFullHead, webFullCount);
  JsonArray serialArr = doc.createNestedArray("serial");
  appendRingToJsonArray(serialArr, webSerialLines, webSerialHead, webSerialCount);

  String out;
  out.reserve(measureJson(doc) + 16);
  serializeJson(doc, out);
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

static void handleLogoBright() {
  webServer.sendHeader("Cache-Control", "public, max-age=86400");
  webServer.send_P(200, "image/svg+xml", K9DTV_LOGO_BRIGHT_SVG);
}

static void handleChip() {
  webServer.sendHeader("Cache-Control", "public, max-age=86400");
  webServer.send_P(200, "image/svg+xml", MENU_CHIP_SVG);
}

static void handleChipBright() {
  webServer.sendHeader("Cache-Control", "public, max-age=86400");
  webServer.send_P(200, "image/svg+xml", MENU_CHIP_BRIGHT_SVG);
}

void setupWebUi() {
  webFullClear();
  webInFullLog = false;
  for (uint8_t i = 0; i < WEB_SERIAL_N; i++) webSerialLines[i][0] = '\0';
  webSerialHead = 0;
  webSerialCount = 0;
  webLogAccLen = 0;
  if (!statusDoc) {
    // Same pattern as gwDoc: allocate after boot so large JSON can use PSRAM.
    statusDoc = new DynamicJsonDocument(STATUS_DOC_BYTES);
  }
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/logo.svg", HTTP_GET, handleLogo);
  webServer.on("/logo-bright.svg", HTTP_GET, handleLogoBright);
  webServer.on("/chip.svg", HTTP_GET, handleChip);
  webServer.on("/chip-bright.svg", HTTP_GET, handleChipBright);
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
