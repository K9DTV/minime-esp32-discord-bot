#ifndef MINIME_SECRETS_H
#define MINIME_SECRETS_H

// Copy this file to secrets.h in the same folder, then fill in real values.
// secrets.h is gitignored -- never commit real tokens or passwords.
// Use #define (not const char*) so multi-file Arduino builds link cleanly.

#define WIFI_SSID            "ssid"
#define WIFI_PASSWORD        "password"
#define BOT_TOKEN            "bot token"
#define WEATHER_API_KEY      "WEATHER_API_KEY"
#define NASA_API_KEY         "NASA_API_KEY"
#define DEEPSEEK_API_KEY     "DEEPSEEK_API_KEY"

#define BOT_GUILD_ID         "GUILD_ID"  // startup member fetch

#define OWNER_ID_STR         "OWNER_ID_STR"         // GPIO / servo
#define TARGET_CHANNEL_ID    "TARGET_CHANNEL_ID"    // commands only (no boot/auto posts)
#define TARGET_CHANNEL_ID1   "TARGET_CHANNEL_ID1"   // second command channel

// Wi-Fi firmware update (ArduinoOTA). Pick a real password in secrets.h.
#define OTA_HOSTNAME         "minime"
#define OTA_PASSWORD         "change-me-ota"

#endif
