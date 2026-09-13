#include "minime.h"

WebSocketsClient gatewayWS;
DynamicJsonDocument* gwDoc = nullptr;
bool gatewayConnected     = false;
bool identified           = false;
bool gotHello             = false;
int  heartbeatIntervalMs   = 0;
unsigned long lastHeartbeatMillis = 0;
int lastSeq               = 0;
String sessionId;
bool canResume            = false;
static String resumeGatewayHost; // host only, from READY resume_gateway_url
unsigned long lastBotActivityMillis = 0;
uint8_t botDiscordStatus = 0;

// Serial drop/reconnect diagnostics
static const uint8_t GW_LOG_MAX = 40;
static String gwLog[GW_LOG_MAX];
static uint8_t gwLogCount = 0;
static String gwLogLastAdded;
static String gwDropStartEvent;
static bool gwInDropState = false;
static unsigned long gwLastDropRemindMillis = 0;
static unsigned long gwLastFullLogMillis = 0;
static unsigned long gwReconnectIntervalMs = 5000;
static const unsigned long GW_RECONNECT_BASE_MS = 5000UL;
static const unsigned long GW_RECONNECT_MAX_MS = 5000UL; // was 60000; keep tries short so drop recovery stays under ~30s when Discord answers
static unsigned long gwLastWifiKickMillis = 0;
static String gwLastDropKind;
static bool gwLoggedConnectDuringDrop = false;

static String gwStamp() {
  return String(millis());
}

static void gwLogAppend(const String& ev) {
  if (gwLogCount > 0 && gwLogLastAdded == ev) return;
  gwLogLastAdded = ev;
  String line = String("[") + gwStamp() + "] " + ev;
  MmLog.print("[GW] ");
  MmLog.println(line);
  if (gwLogCount < GW_LOG_MAX) {
    gwLog[gwLogCount++] = line;
  } else {
    for (uint8_t i = 1; i < GW_LOG_MAX; i++) gwLog[i - 1] = gwLog[i];
    gwLog[GW_LOG_MAX - 1] = line;
  }
}

// kind = coarse category (dedupe); detail = full text for first DROP_START / new kinds
static void gwNoteDrop(const String& kind, const String& detail) {
  if (!gwInDropState) {
    gwInDropState = true;
    gwDropStartEvent = detail;
    gwLastDropKind = kind;
    gwLastDropRemindMillis = millis();
    gwLogAppend(String("DROP_START: ") + detail);
  } else if (kind != gwLastDropKind) {
    gwLastDropKind = kind;
    gwLogAppend(detail);
  }
}

static void gwClearDropState() {
  if (!gwInDropState) return;
  gwLogAppend("RECOVERED");
  gwInDropState = false;
  gwDropStartEvent = "";
  gwLastDropKind = "";
  gwLoggedConnectDuringDrop = false;
}

void gwSerialService() {
  unsigned long now = millis();
  // Alive pulse so you can confirm the COM port is live even with no drop.
  static unsigned long gwLastAliveMillis = 0;
  if (gwLastAliveMillis == 0) gwLastAliveMillis = now;
  if (now - gwLastAliveMillis >= 15000UL) {
    gwLastAliveMillis = now;
    MmLog.print("[GW] alive up_ms=");
    MmLog.print(now);
    MmLog.print(" wifi=");
    MmLog.print(WiFi.status() == WL_CONNECTED ? "up" : "DOWN");
    MmLog.print(" rssi=");
    MmLog.print(WiFi.RSSI());
    MmLog.print(" gw=");
    MmLog.print(gatewayConnected ? "1" : "0");
    MmLog.print(" id=");
    MmLog.print(identified ? "1" : "0");
    MmLog.print(" drop=");
    MmLog.println(gwInDropState ? "1" : "0");
  }
  if (gwInDropState && gwDropStartEvent.length() &&
      (now - gwLastDropRemindMillis >= 5000UL)) {
    gwLastDropRemindMillis = now;
    MmLog.print("[GW] DROP still (started): ");
    MmLog.println(gwDropStartEvent);
  }
  if (now - gwLastFullLogMillis >= 60000UL) {
    gwLastFullLogMillis = now;
    MmLog.println("[GW] === FULL LOG ===");
    if (gwLogCount == 0) {
      MmLog.println("  (empty)");
    } else {
      for (uint8_t i = 0; i < gwLogCount; i++) {
        MmLog.print("  ");
        MmLog.println(gwLog[i]);
      }
    }
    if (gwInDropState) {
      MmLog.print("  drop_start=");
      MmLog.println(gwDropStartEvent);
    }
    MmLog.println("[GW] === END LOG ===");
  }
}

static void gwSetReconnectBackoff(bool reset) {
  if (reset) {
    gwReconnectIntervalMs = GW_RECONNECT_BASE_MS;
  } else {
    unsigned long next = gwReconnectIntervalMs * 2UL;
    if (next < GW_RECONNECT_BASE_MS) next = GW_RECONNECT_BASE_MS;
    if (next > GW_RECONNECT_MAX_MS) next = GW_RECONNECT_MAX_MS;
    gwReconnectIntervalMs = next;
  }
  gatewayWS.setReconnectInterval(gwReconnectIntervalMs);
  gwLogAppend(String("RECONNECT_INTERVAL_MS=") + String(gwReconnectIntervalMs));
}

static void ensureWifiForGateway() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (now - gwLastWifiKickMillis < 10000UL) return;
  gwLastWifiKickMillis = now;
  gwLogAppend("WIFI_RETRY begin()");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Discord READY gives resume_gateway_url (e.g. wss://gateway-us-east1-b.discord.gg).
// Resume must reconnect there; identify uses gateway.discord.gg.
static void parseResumeGatewayHost(const char* url) {
  resumeGatewayHost = "";
  if (!url || !url[0]) return;
  const char* p = url;
  if (strncmp(p, "wss://", 6) == 0) p += 6;
  else if (strncmp(p, "ws://", 5) == 0) p += 5;
  else if (strncmp(p, "https://", 8) == 0) p += 8;
  else if (strncmp(p, "http://", 7) == 0) p += 7;
  while (*p && *p != '/' && *p != ':' && *p != '?') {
    resumeGatewayHost += *p++;
  }
}

static void bindGatewayHost(const char* host) {
  if (!host || !host[0]) host = "gateway.discord.gg";
  gatewayWS.beginSSL(host, 443, "/?v=10&encoding=json");
  gatewayWS.onEvent(gatewayEvent);
  gatewayWS.setReconnectInterval(gwReconnectIntervalMs);
  gwLogAppend(String("BIND_HOST ") + host);
}

void connectGateway() {
  resumeGatewayHost = "";
  bindGatewayHost("gateway.discord.gg");
}

static void bindGatewayForNextConnect(bool preferResume) {
  if (preferResume && resumeGatewayHost.length() > 0) {
    bindGatewayHost(resumeGatewayHost.c_str());
  } else {
    bindGatewayHost("gateway.discord.gg");
  }
}

void gwSendJson(JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  gatewayWS.sendTXT(payload);
}

void requestTrackedUserPresences() {
  if (cachedGuildCount == 0) return;
  bool any = false;
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
    if (trackedUsers[i].active && trackedUsers[i].userId.length()) {
      any = true;
      break;
    }
  }
  if (!any) return;

  for (uint8_t g = 0; g < cachedGuildCount; g++) {
    if (cachedGuildIds[g].length() < 16) continue;
    StaticJsonDocument<768> doc;
    doc["op"] = 8;
    JsonObject d = doc.createNestedObject("d");
    d["guild_id"] = cachedGuildIds[g];
    d["limit"] = 0;
    d["presences"] = true;
    JsonArray ids = d.createNestedArray("user_ids");
    for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
      if (trackedUsers[i].active && trackedUsers[i].userId.length()) {
        ids.add(trackedUsers[i].userId);
      }
    }
    gwSendJson(doc);
  }
}

void sendBotPresence(const char* status, bool afk) {
  if (!gatewayConnected || !identified) return;
  StaticJsonDocument<256> doc;
  doc["op"] = 3;
  JsonObject d = doc.createNestedObject("d");
  d["since"] = nullptr;
  d.createNestedArray("activities");
  d["status"] = status;
  d["afk"] = afk;
  gwSendJson(doc);
}

void noteBotActivity() {
  lastBotActivityMillis = millis();
  if (!gatewayConnected || !identified) return;
  if (botDiscordStatus == 2) return;
  botDiscordStatus = 2;
  sendBotPresence("online", false);
}

void updateBotPresenceIdle() {
  if (!gatewayConnected || !identified) return;
  if (lastBotActivityMillis == 0) {
    lastBotActivityMillis = millis();
    return;
  }
  if (botDiscordStatus == 1) return;
  if (millis() - lastBotActivityMillis < BOT_PRESENCE_IDLE_MS) return;
  botDiscordStatus = 1;
  sendBotPresence("idle", true);
}

// 100 MHz only when OLED off, Discord Idle, and web UI is not serving.
// Dropping CPU with the LAN web server up breaks Wi-Fi / browser access.
void applyCpuForIdleState() {
  if (otaIsBusy() || webUiKeepsCpuActive()) {
    if (getCpuFrequencyMhz() != CPU_MHZ_ACTIVE) setCpuFrequencyMhz(CPU_MHZ_ACTIVE);
    return;
  }
  bool slow = displayAsleep && botDiscordStatus == 1 && identified;
  uint32_t want = slow ? CPU_MHZ_OLED_OFF_BOT_IDLE : CPU_MHZ_ACTIVE;
  if (getCpuFrequencyMhz() == want) return;
  setCpuFrequencyMhz(want);
}

void sendIdentify() {
  StaticJsonDocument<1024> doc;
  doc["op"] = 2;
  JsonObject d = doc.createNestedObject("d");
  d["token"] = BOT_TOKEN;
  JsonObject props = d.createNestedObject("properties");
  props["os"]     = "linux";
  props["browser"] = "esp32";
  props["device"]  = "esp32";
  d["compress"]         = false;
  d["large_threshold"] = 250;
  d["intents"] = 37635; // GUILDS + MEMBERS + PRESENCES + MESSAGES + DMs + MESSAGE_CONTENT
  JsonObject presence = d.createNestedObject("presence");
  presence["since"] = nullptr;
  presence.createNestedArray("activities");
  presence["status"] = "online";
  presence["afk"] = false;
  gwSendJson(doc);
  lastBotActivityMillis = millis();
  botDiscordStatus = 2;
  gwLogAppend("SENT_IDENTIFY");
}

void sendResume() {
  StaticJsonDocument<512> doc;
  doc["op"] = 6;
  JsonObject d = doc.createNestedObject("d");
  d["token"] = BOT_TOKEN;
  d["session_id"] = sessionId;
  d["seq"] = lastSeq;
  gwSendJson(doc);
  gwLogAppend(String("SENT_RESUME seq=") + String(lastSeq));
}

void sendHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["op"] = 1;
  if (lastSeq == 0) {
    doc["d"] = nullptr;
  } else {
    doc["d"] = lastSeq;
  }
  gwSendJson(doc);
}

void pumpGateway() {
  gatewayWS.loop();
  gwSerialService();

  if (!gatewayConnected && WiFi.status() != WL_CONNECTED) {
    ensureWifiForGateway();
  }

  // Heartbeat after Hello (Discord), not only after READY.
  if (heartbeatIntervalMs > 0 && gatewayConnected && gotHello) {
    unsigned long now = millis();
    if (now - lastHeartbeatMillis >= (unsigned long)heartbeatIntervalMs) {
      lastHeartbeatMillis = now;
      sendHeartbeat();
    }
  }
}

void gatewayEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
      gatewayConnected = false;
      identified       = false;
      gotHello         = false;
      botDiscordStatus = 0;
      heartbeatIntervalMs = 0;

      bool wifiUp = (WiFi.status() == WL_CONNECTED);
      String kind = wifiUp ? "WS_DISCONNECTED_WIFI_UP" : "WS_DISCONNECTED_WIFI_DOWN";
      String detail = kind;
      if (payload && length > 0) {
        detail += " reason=";
        size_t n = length < 80 ? length : 80;
        for (size_t i = 0; i < n; i++) {
          char c = (char)payload[i];
          if (c >= 32 && c < 127) detail += c;
        }
      }
      detail += " rssi=";
      detail += String(WiFi.RSSI());
      detail += " seq=";
      detail += String(lastSeq);
      detail += " session=";
      detail += sessionId.length() ? "yes" : "no";
      detail += " canResume=";
      detail += canResume ? "1" : "0";

      gwNoteDrop(kind, detail);
      gwLoggedConnectDuringDrop = false;
      gwSetReconnectBackoff(false);
      // Auto-reconnect must hit resume host after OP7, not always gateway.discord.gg.
      bindGatewayForNextConnect(canResume && sessionId.length() > 0 && lastSeq > 0);
      ensureWifiForGateway();
      showTransient("Gateway", "Disconnected");
      break;
    }
    case WStype_CONNECTED:
      gatewayConnected = true;
      if (!gwInDropState || !gwLoggedConnectDuringDrop) {
        gwLogAppend("WS_CONNECTED");
        if (gwInDropState) gwLoggedConnectDuringDrop = true;
      }
      showTransient("Gateway", "Connected");
      break;
    case WStype_ERROR: {
      String detail = "WS_ERROR";
      if (payload && length > 0) {
        detail += " ";
        size_t n = length < 80 ? length : 80;
        for (size_t i = 0; i < n; i++) {
          char c = (char)payload[i];
          if (c >= 32 && c < 127) detail += c;
        }
      }
      gwNoteDrop("WS_ERROR", detail);
      break;
    }
    case WStype_TEXT: {
      static StaticJsonDocument<384> gwFilter;
      static bool gwFilterInit = false;
      if (!gwFilterInit) {
        gwFilter["op"] = true;
        gwFilter["s"] = true;
        gwFilter["t"] = true;
        gwFilter["d"]["heartbeat_interval"] = true;
        gwFilter["d"]["session_id"] = true;
        gwFilter["d"]["resume_gateway_url"] = true;
        gwFilter["d"]["status"] = true;
        gwFilter["d"]["user"]["id"] = true;
        gwFilter["d"]["user"]["username"] = true;
        gwFilter["d"]["user"]["global_name"] = true;
        gwFilter["d"]["content"] = true;
        gwFilter["d"]["channel_id"] = true;
        gwFilter["d"]["guild_id"] = true;
        gwFilter["d"]["author"]["id"] = true;
        gwFilter["d"]["author"]["username"] = true;
        gwFilter["d"]["author"]["global_name"] = true;
        gwFilter["d"]["author"]["bot"] = true;
        gwFilter["d"]["mentions"][0]["id"] = true;
        gwFilter["d"]["presences"][0]["user"]["id"] = true;
        gwFilter["d"]["presences"][0]["status"] = true;
        gwFilter["d"]["guilds"][0]["presences"][0]["user"]["id"] = true;
        gwFilter["d"]["guilds"][0]["presences"][0]["status"] = true;
        gwFilterInit = true;
      }

      if (!gwDoc) return;
      gwDoc->clear();
      DeserializationError err = deserializeJson(*gwDoc, payload, length, DeserializationOption::Filter(gwFilter));
      if (err) {
        gwLogAppend(String("JSON_ERR ") + err.c_str());
        return;
      }
      int op = (*gwDoc)["op"] | -1;
      if (gwDoc->containsKey("s") && !(*gwDoc)["s"].isNull()) {
        lastSeq = (*gwDoc)["s"].as<int>();
      }

      // Hello: start HB, then Resume or Identify
      if (op == 10) {
        heartbeatIntervalMs = (*gwDoc)["d"]["heartbeat_interval"] | 0;
        lastHeartbeatMillis = millis();
        gotHello = true;
        gwLogAppend(String("OP10_HELLO hb_ms=") + String(heartbeatIntervalMs));
        if (canResume && sessionId.length() > 0 && lastSeq > 0) {
          sendResume();
        } else {
          sendIdentify();
        }
        return;
      }

      // Reconnect: close; next socket uses resume_gateway_url host when possible
      if (op == 7) {
        canResume = sessionId.length() > 0;
        gwNoteDrop("OP7_RECONNECT", "OP7_RECONNECT");
        showTransient("Gateway", "Op7 reconnect");
        gatewayWS.disconnect();
        bindGatewayForNextConnect(canResume && lastSeq > 0);
        return;
      }

      // Invalid Session: d is boolean (parse without filter)
      if (op == 9) {
        bool resumable = false;
        StaticJsonDocument<96> small;
        if (!deserializeJson(small, payload, length)) {
          resumable = small["d"] | false;
        }
        String detail = String("OP9_INVALID_SESSION resumable=") + (resumable ? "1" : "0");
        gwNoteDrop(detail, detail);
        if (!resumable) {
          sessionId = "";
          lastSeq = 0;
          canResume = false;
          resumeGatewayHost = "";
        } else {
          canResume = sessionId.length() > 0;
        }
        showTransient("Gateway", "Op9 session");
        gwSetReconnectBackoff(true);
        gatewayWS.disconnect();
        bindGatewayForNextConnect(canResume && lastSeq > 0 && resumeGatewayHost.length() > 0);
        return;
      }

      if (op == 11) return; // Heartbeat ACK

      if (op == 0) {
        const char* t = (*gwDoc)["t"];
        if (!t) return;
        if (strcmp(t, "READY") == 0) {
          identified = true;
          sessionId = (*gwDoc)["d"]["session_id"] | "";
          {
            const char* rurl = (*gwDoc)["d"]["resume_gateway_url"] | "";
            parseResumeGatewayHost(rurl);
            if (resumeGatewayHost.length()) {
              gwLogAppend(String("RESUME_URL_HOST ") + resumeGatewayHost);
            } else {
              gwLogAppend("RESUME_URL_HOST (none)");
            }
          }
          canResume = sessionId.length() > 0;
          gwSetReconnectBackoff(true);
          gwClearDropState();
          gwLogAppend(String("READY session=") + (canResume ? "yes" : "no"));
          JsonArray guilds = (*gwDoc)["d"]["guilds"].as<JsonArray>();
          if (!guilds.isNull()) {
            for (JsonObject g : guilds) {
              applyPresencesArray(g["presences"].as<JsonArray>());
            }
          }
          requestTrackedUserPresences();
          return;
        }
        if (strcmp(t, "RESUMED") == 0) {
          identified = true;
          canResume = sessionId.length() > 0;
          gwSetReconnectBackoff(true);
          gwClearDropState();
          gwLogAppend("RESUMED");
          requestTrackedUserPresences();
          return;
        }
        if (strcmp(t, "GUILD_CREATE") == 0) {
          applyPresencesArray((*gwDoc)["d"]["presences"].as<JsonArray>());
          requestTrackedUserPresences();
          return;
        }
        if (strcmp(t, "GUILD_MEMBERS_CHUNK") == 0) {
          applyPresencesArray((*gwDoc)["d"]["presences"].as<JsonArray>());
          return;
        }
        if (strcmp(t, "PRESENCE_UPDATE") == 0) {
          handlePresenceUpdate((*gwDoc)["d"]);
          return;
        }
        if (strcmp(t, "MESSAGE_CREATE") == 0) {
          if (httpsInUse) return; // DeepSeek holds HTTPS; defer commands until free
          JsonObject d = (*gwDoc)["d"];
          if (d["author"]["bot"] == true) return;
          String content   = d["content"].as<String>();
          String channelId = d["channel_id"].as<String>();
          String authorId  = d["author"]["id"].as<String>();
          String authorName = discordDisplayName(d["author"]);
          bool isDM = d["guild_id"].isNull();

          // Owner alert outputs: DM to bot -> set1 @ 10 Hz; @owner mention -> set2 @ 10 Hz
          if (isDM) {
            startSet1Flash();
          } else {
            bool ownerMentioned = false;
            JsonArray mentions = d["mentions"].as<JsonArray>();
            if (!mentions.isNull()) {
              for (JsonObject m : mentions) {
                const char* mid = m["id"] | "";
                if (mid[0] && strcmp(mid, OWNER_ID_STR) == 0) {
                  ownerMentioned = true;
                  break;
                }
              }
            }
            if (!ownerMentioned) {
              String ping = String("<@") + OWNER_ID_STR + ">";
              String pingNick = String("<@!") + OWNER_ID_STR + ">";
              if (content.indexOf(ping) >= 0 || content.indexOf(pingNick) >= 0) {
                ownerMentioned = true;
              }
            }
            if (ownerMentioned) startSet2Flash();
          }

          handleCommand(content, authorId, authorName, channelId, isDM);
        }
      }
      break;
    }
    default:
      break;
  }
}
