/*
 MiniMe Discord bot (ESP32-S3 + SSD1327). Command list: Discord !help.

 Sketch map:
  secrets.h / secrets.example.h  -- Wi-Fi, tokens, IDs (gitignored)
  minime_config.h                -- pins, buffer sizes, timing constants
  minime.h                         -- shared declarations and globals
  time_util.cpp                    -- NTP, Pacific DST, updateLocalTime
  users.cpp                        -- tracked users, guild cache, presence
  display.cpp                      -- OLED dashboard, sleep, transient overlay
  touch.cpp                        -- touch wake, USB VBUS compensation
  hardware.cpp                     -- servo, NeoPixel, DS18B20, GPIO
  discord_rest.cpp                 -- HTTPS REST, sendDiscordMessage, members
  discord_gateway.cpp              -- websocket, heartbeat, identify, events
  serial_log.cpp                   -- Serial/USB CDC dual log (ESP32-S3)
  ota.cpp                          -- Wi-Fi ArduinoOTA firmware update
  web_ui.cpp                       -- LAN page: logo + display + log + serial
  k9dtv_logo_svg.h                 -- static K9DTV logo for /logo.svg
  commands.cpp                     -- handleCommand, APIs, DeepSeek, scheduled
  MiniMe_Discord_Bot.ino           -- setup / loop + Wi-Fi / gateway connect

 OLED sleep dims then blanks the panel only; ESP32 and Wi-Fi stay up.
 Touch is polled in loop() (no interrupt). Bot status: online on activity,
 idle after 5 min quiet; touch does not set Online.

 Serial (WeAct ESP32-S3 USB-C): Prefer Tools
  Board: ESP32S3 Dev Module (required — not generic ESP32 Dev Module)
  USB CDC On Boot = Enabled
  USB Mode = Hardware CDC and JTAG
 MmLog does not print to the Serial port; open https://<board-ip>/ for LOG.
 Port still enumerates for upload / OTA; Monitor will be quiet.

 Wi-Fi OTA: first flash still via USB. Then Tools -> Port -> minime network port.
 Partition: Flash Size 16MB; sketch partitions.csv = 2x ~7.9MB OTA apps, no SPIFFS/FS.
 Set OTA_PASSWORD in secrets.h.
 Owner Discord: !ota

 LAN web UI: Display|SysInfo; LOG|Serial under both (Serial no scrollbar, <= LOG lines). USB Serial quiet.

 Copy note: sketch folder must contain ONLY this one .ino (no second Discord_*.ino).
 Also copy partitions.csv with the sketch (needed for the 16MB OTA layout).
*/
#include "minime.h"

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // modem sleep breaks ArduinoOTA (port 3232)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  showTransient("WiFi", "Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  showTransient("WiFi", "Connected");
}

void setup() {
  mmSerialBegin();
  gwDoc = new DynamicJsonDocument(GW_DOC_PSRAM);
  initTrackedUsers();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  u8g2.begin();
  u8g2.setContrast(DISPLAY_CONTRAST_FULL);
  lastDisplayActivityMillis = millis();
  if (mmSerialCdcOnBoot()) {
    showTransient("Serial", "CDC ON 115200");
  } else {
    showTransient("Serial", "CDC OFF+USB");
  }
  delay(800);
  showTransient("Booting...", "ESP32-S3 Discord bot");
  setupPins();
  sensors.begin();
  connectWiFi();
  setupMiniMeOta();
  setupWebUi();
  timeClient.begin();
  showTransient("Discord", "Loading users...");
  if (fetchGuildMembersAtStartup()) {
    String n0 = trackedUsers[0].userName.length() ? trackedUsers[0].userName : "ok";
    showTransient("Users loaded", n0);
  } else {
    showTransient("Users", "Fetch failed");
  }
  delay(1200);
  connectGateway();
  setServoAngle(45);
  lastDashMillis = 0;
  setupTouch(); // after WiFi/I2C
  showTransient("Ready", "Idle presence");
  drawDashboard();
}

void loop() {
  pumpOta();
  pumpWebUi();
  // While flashing, do not run Discord / display work (starves OTA → timeouts / odd replies like '864')
  if (otaIsBusy()) {
    return;
  }
  pumpGateway();
  pumpSetFlash();
  backgroundTasks();
  pollTouchWake();
  updateBotPresenceIdle();
  updateDisplay();
  applyCpuForIdleState();
  delay(5);
}
