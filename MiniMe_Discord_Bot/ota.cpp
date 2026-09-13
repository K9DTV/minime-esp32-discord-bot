#include "minime.h"
#include <ArduinoOTA.h>

static bool otaReady = false;
static volatile bool otaInProgress = false;

bool otaIsBusy() {
  return otaInProgress;
}

void setupMiniMeOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setTimeout(60000);

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    setCpuFrequencyMhz(CPU_MHZ_ACTIVE);
    noteBotActivity();
    // Stop Discord websocket so OTA owns Wi-Fi / CPU
    gatewayWS.setReconnectInterval(3600000UL); // park auto-reconnect during flash
    gatewayWS.disconnect();
    gatewayConnected = false;
    identified = false;
    String t = (ArduinoOTA.getCommand() == U_FLASH) ? "Firmware" : "FS";
    showTransient("OTA", String("Start ") + t);
    MmLog.println(String("[OTA] start ") + t);
  });
  ArduinoOTA.onEnd([]() {
    showTransient("OTA", "Done reboot");
    MmLog.println("[OTA] end");
    // reboot follows; leave otaInProgress set
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) return;
    static unsigned int lastPct = 999;
    unsigned int pct = (progress * 100U) / total;
    if (pct == lastPct) return;
    if (pct % 10U != 0U && pct != 100U) return;
    lastPct = pct;
    showTransient("OTA", String(pct) + "%");
    MmLog.println(String("[OTA] ") + String(pct) + "%");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    otaInProgress = false;
    String e = "err ";
    e += String((int)err);
    if (err == OTA_AUTH_ERROR) e = "Auth fail";
    else if (err == OTA_BEGIN_ERROR) e = "Begin fail";
    else if (err == OTA_CONNECT_ERROR) e = "Connect fail";
    else if (err == OTA_RECEIVE_ERROR) e = "Receive fail";
    else if (err == OTA_END_ERROR) e = "End fail";
    showTransient("OTA fail", e);
    MmLog.println(String("[OTA] ") + e);
  });

  ArduinoOTA.begin();
  otaReady = true;
  MmLog.print("[OTA] ready hostname=");
  MmLog.print(OTA_HOSTNAME);
  MmLog.print(" ip=");
  MmLog.println(WiFi.localIP().toString());
  showTransient("OTA", WiFi.localIP().toString());
}

void pumpOta() {
  if (!otaReady) return;
  if (WiFi.status() != WL_CONNECTED) return;
  ArduinoOTA.handle();
}

String otaStatusText() {
  String s = "**MiniMe Wi-Fi OTA**\n";
  s += "• **IP:** `";
  s += WiFi.localIP().toString();
  s += "`\n";
  s += "• **Hostname:** `";
  s += OTA_HOSTNAME;
  s += ".local`\n";
  s += "• **Port:** 3232 (ArduinoOTA)";
  return s;
}
