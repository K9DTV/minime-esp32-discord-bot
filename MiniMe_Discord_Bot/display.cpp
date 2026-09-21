#include "minime.h"

U8G2_SSD1327_WS_128X128_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

float dashTempC = -999.0f;
float dashTempF = -999.0f;
int lastServoDeg = 45;

String transientLine1 = "";
String transientLine2 = "";
String transientLine3 = "";
unsigned long transientUntilMs = 0;

unsigned long lastDashMillis = 0;
unsigned long lastDisplayActivityMillis = 0;
bool displayAsleep = false;

void noteDisplayActivity() {
  lastDisplayActivityMillis = millis();
  if (displayAsleep) {
    displayAsleep = false;
    u8g2.setPowerSave(0);
    lastDashMillis = 0;
  }
  u8g2.setContrast(DISPLAY_CONTRAST_FULL);
}

// Same fill math OLED draws; web API converts via dashBarPct.
int dashSigBarW(long rssi) {
  if (rssi >= -40) return DASH_SIG_HEAP_BAR_MAX;
  if (rssi <= -100) return 0;
  return (int)((rssi + 100) * DASH_SIG_HEAP_BAR_MAX / 60);
}

int dashHeapBarW(uint32_t memFree, uint32_t memTotal) {
  if (memTotal == 0) return 0;
  int w = (int)((memFree * (uint32_t)DASH_SIG_HEAP_BAR_MAX) / memTotal);
  if (w < 0) w = 0;
  if (w > DASH_SIG_HEAP_BAR_MAX) w = DASH_SIG_HEAP_BAR_MAX;
  return w;
}

int dashSrvBarW(int servoDeg) {
  int w = (servoDeg * DASH_SRV_BAR_MAX) / 90;
  if (w < 0) w = 0;
  if (w > DASH_SRV_BAR_MAX) w = DASH_SRV_BAR_MAX;
  return w;
}

int dashBarPct(int fill, int maxFill) {
  if (maxFill <= 0) return 0;
  int p = (fill * 100) / maxFill;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

// Frame at (25, baselineY-7) size 103x8; fill at (26, baselineY-6).
void drawDashBar(const char* label, uint8_t baselineY, int fillW) {
  u8g2.drawStr(0, baselineY, label);
  u8g2.drawFrame(25, baselineY - 7, 103, 8);
  if (fillW > 0) u8g2.drawBox(26, baselineY - 6, fillW, 6);
}

// Idle: full 1 min, dim 15 s, then blank. Clock/RSSI/heap ticks do not count as activity.
void updateDisplaySleep() {
  if (displayAsleep) return;
  unsigned long now = millis();
  if (lastDisplayActivityMillis == 0) {
    lastDisplayActivityMillis = now;
    return;
  }

  unsigned long idle = now - lastDisplayActivityMillis;
  if (idle < DISPLAY_IDLE_MS) return;

  if (idle < DISPLAY_IDLE_MS + DISPLAY_DIM_MS) {
    unsigned long dimElapsed = idle - DISPLAY_IDLE_MS;
    uint8_t contrast = (uint8_t)(DISPLAY_CONTRAST_FULL -
      (dimElapsed * (unsigned long)DISPLAY_CONTRAST_FULL) / DISPLAY_DIM_MS);
    u8g2.setContrast(contrast);
    return;
  }

  displayAsleep = true;
  u8g2.setPowerSave(1); // panel off until touch or a real event
}

// Overlay on y=119/127; do not paint here (stack + NTP unsafe inside Gateway).
void showTransient(const String& line1, const String& line2, const String& line3, unsigned long durationMs) {
  noteDisplayActivity();
  transientLine1 = line1;
  transientLine2 = line2;
  transientLine3 = line3;
  transientUntilMs = millis() + durationMs;
  lastDashMillis = 0;
}

// 5x7 -> 6px/char, 8px/row. drawStr y = baseline.
// y=7 header | 15 bot+date | 23 up/temp | 31 sig | 39 heap | 47 srv
// y=55..111 users | 119/127 transient
void drawDashboard() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

  updateLocalTime();
  String t = timeClient.getFormattedTime();
  u8g2.drawStr(0, 7, "MiniMe");
  u8g2.drawStr(42, 7, gatewayConnected ? "GW:Good" : "GW:Bad");
  int timeX = 128 - u8g2.getStrWidth(t.c_str());
  if (timeX < 0) timeX = 0;
  u8g2.drawStr(timeX, 7, t.c_str());

  {
    static const char* const DOW_NAME[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* const MON_NAME[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
    time_t localEpoch = (time_t)timeClient.getEpochTime();
    struct tm tmLocal;
    gmtime_r(&localEpoch, &tmLocal);
    char botBuf[12];
    snprintf(botBuf, sizeof(botBuf), "Bot:%-6s",
             (botDiscordStatus == 2) ? "Online" : "Idle");
    u8g2.drawStr(0, 15, botBuf);

    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s %s %2d %04d",
             DOW_NAME[tmLocal.tm_wday], MON_NAME[tmLocal.tm_mon],
             tmLocal.tm_mday, tmLocal.tm_year + 1900);
    // Fixed slot width so DOW column does not shift.
    const char* dateSlot = "Www Mmm 99 9999";
    int dateX = 128 - u8g2.getStrWidth(dateSlot);
    if (dateX < 0) dateX = 0;
    u8g2.drawStr(dateX, 15, dateBuf);
  }

  unsigned long d = 0, h = 0, m = 0, s = 0;
  uptimeDhms(d, h, m, s);
  char upTempBuf[40];
  if (dashTempC > -998.0f) {
    snprintf(upTempBuf, sizeof(upTempBuf), "Up:%lud %luh %lum %lus T:%3.0fF/%3.0fC",
             d, h, m, s, dashTempF, dashTempC);
  } else {
    snprintf(upTempBuf, sizeof(upTempBuf), "Up:%lud %luh %lum %lus T:--Error--", d, h, m, s);
  }
  u8g2.drawStr(0, 23, upTempBuf);

  long rssi = WiFi.RSSI();
  uint32_t memFree = 0, memTotal = 0;
  boardMemTotals(memFree, memTotal);

  drawDashBar("Sig:", 31, dashSigBarW(rssi));
  drawDashBar("Heap:", 39, dashHeapBarW(memFree, memTotal));
  drawDashBar("Srv:", 47, dashSrvBarW(lastServoDeg));

  const int gapPx = 4;
  const int statusW = u8g2.getStrWidth("Idle");
  const int botReserveW = u8g2.getStrWidth("Bot:999");
  int nameMaxPx = 128 - gapPx - statusW - gapPx - botReserveW;
  if (nameMaxPx < 16) nameMaxPx = 16;

  const char* nameSrc[MAX_TRACKED_USERS];
  int longestNamePx = 0;
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
    if (!trackedUsers[i].active) {
      nameSrc[i] = "---";
    } else if (trackedUsers[i].userName.length()) {
      nameSrc[i] = trackedUsers[i].userName.c_str();
    } else {
      nameSrc[i] = trackedUsers[i].userId.c_str();
    }
    int w = u8g2.getStrWidth(nameSrc[i]);
    if (w > longestNamePx) longestNamePx = w;
  }
  if (longestNamePx > nameMaxPx) longestNamePx = nameMaxPx;
  int statusX = longestNamePx + gapPx;

  for (uint8_t row = 0; row < MAX_TRACKED_USERS; row++) {
    uint8_t y = 55 + (row * 8);
    char name[40];
    const char* src = nameSrc[row];
    size_t n = 0;
    int w = 0;
    while (src[n] && n + 1 < sizeof(name)) {
      char one[2] = { src[n], '\0' };
      int gw = u8g2.getStrWidth(one);
      if (w + gw > longestNamePx) break;
      name[n] = src[n];
      w += gw;
      n++;
    }
    name[n] = '\0';
    if (n == 0 && src[0]) {
      name[0] = src[0];
      name[1] = '\0';
    }
    u8g2.drawStr(0, y, name);

    u8g2.drawStr(statusX, y,
                 statusToWord(trackedUsers[row].active ? trackedUsers[row].status : 0));

    uint32_t uses = trackedUsers[row].active ? trackedUsers[row].useCount24h : 0;
    char botBuf[12];
    snprintf(botBuf, sizeof(botBuf), "Bot:%lu", (unsigned long)uses);
    int botX = 128 - u8g2.getStrWidth(botBuf);
    if (botX < statusX + statusW + 2) botX = statusX + statusW + 2;
    u8g2.drawStr(botX, y, botBuf);
  }

  String row15 = "";
  String row16 = "";
  if (millis() < transientUntilMs) {
    row15 = transientLine1;
    row16 = transientLine2;
    if (transientLine3.length()) {
      if (row16.length()) row16 += " ";
      row16 += transientLine3;
    }
  }
  u8g2.drawStr(0, 119, row15.c_str());
  u8g2.drawStr(0, 127, row16.c_str());

  u8g2.sendBuffer();
}

void updateDisplay() {
  updateDisplaySleep();
  if (displayAsleep) return;
  unsigned long now = millis();
  if (transientUntilMs != 0 && now >= transientUntilMs) {
    transientUntilMs = 0;
    lastDashMillis = 0;
  }
  if (lastDashMillis == 0 || now - lastDashMillis >= DASH_REFRESH_MS) {
    lastDashMillis = now;
    float c, f;
    if (readTemperature(c, f)) {
      dashTempC = c;
      dashTempF = f;
    }
    drawDashboard();
  }
}
