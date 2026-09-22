#ifndef MINIME_CONFIG_H
#define MINIME_CONFIG_H

// Discord content max is 2000. !ask max_tokens / JSON buffer sized to fit one message.
const int DISCORD_CONTENT_MAX = 2000;
const int DEEPSEEK_MAX_TOKENS = 900;
const size_t DEEPSEEK_JSON_DOC = 24576; // larger so !ask answers parse without scrape fallback

// ====== GPIO CONFIG ======
// WeAct ESP32-S3-N16R8 defaults. Change here if wiring differs.
const int RGB_LED_PIN = 48;
const int PIN_SERVO   = 47;
const int PIN_SET1    = 6;
const int PIN_SET2    = 7;
const int PIN_DS18B20 = 10;
// I2C SSD1327 128x128 (GND / VCC / SCL / SDA on the module)
const int PIN_I2C_SDA = 8;
const int PIN_I2C_SCL = 9;
const int PIN_TOUCH   = 4; // TOUCH4 -- wire pad here
const uint32_t TOUCH_THRESHOLD = 2000; // constant gap: trip = rolling idle avg + this
// USB 5V (VBUS) -> 10k -> PIN_USB_VBUS_ADC -> 10k -> GND. Do not feed 5V straight into the pin.
const int PIN_USB_VBUS_ADC = 1;
const uint32_t USB_VBUS_R_HI = 10000; // ohms, 5V side
const uint32_t USB_VBUS_R_LO = 10000; // ohms, GND side
// #define so array sizes work even if another library ships a config.h
#define TOUCH_AVG_N 16
const unsigned long USB_VBUS_READ_MS = 500;

// ====== TIME CONFIG (NTP) -- US Pacific DST ======
const long PST_OFFSET_SEC = -28800; // UTC-8
const long PDT_OFFSET_SEC = -25200; // UTC-7

// ====== SCHEDULED & INTERVAL TASKS ======
// (periodic channel posts removed; lastSysInfoMillis is only a boot marker)

// ====== DISCORD GATEWAY ======
const size_t GW_DOC_PSRAM = 262144;   // 256KB
const uint32_t BOARD_PSRAM_BYTES = 8UL * 1024UL * 1024UL; // this ESP32-S3 board
const unsigned long BOT_PRESENCE_IDLE_MS = 300000UL; // 5 minutes quiet -> Idle
// Extra wait past Discord heartbeat_interval before HB_ACK_TIMEOUT kills the socket.
// Stops false zombies when OP11 is late (ESP32 TLS / Wi-Fi jitter).
const unsigned long GW_HB_ACK_GRACE_MS = 15000UL;
const uint32_t CPU_MHZ_ACTIVE = 240; // OTA forces this; no idle downclock

// ====== DISPLAY STATE ======
const unsigned long DASH_REFRESH_MS = 4000UL; // OLED redraw interval
const unsigned long DISPLAY_IDLE_MS = 60000UL; // 1 minute full brightness
const unsigned long DISPLAY_DIM_MS = 15000UL; // 15 seconds fade to off
const unsigned long TOUCH_DEBOUNCE_MS = 300;
const uint8_t DISPLAY_CONTRAST_FULL = 255;

// ====== USER TRACKING (8 dashboard rows) ======
const uint8_t MAX_TRACKED_USERS = 8;
const unsigned long USES_WINDOW_MS = 86400000UL;  // 24h
const uint8_t MAX_CACHED_GUILDS = 3;

// ====== LAN WEB UI (dashboard + log; plain HTTP) ======
const uint16_t WEB_UI_PORT = 80;
#define WEB_STATUS_POLL_MS 2000UL // browser /api/status poll (ms); edit here to override

#endif
