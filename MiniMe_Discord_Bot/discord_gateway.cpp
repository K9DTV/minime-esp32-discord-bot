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
static const unsigned long GW_RECONNECT_FAST_MS = 200UL; // after drop: IDENTIFY ASAP (no resume path)
static unsigned long gwLastWifiKickMillis = 0;
static String gwLastDropKind;
static bool gwLoggedConnectDuringDrop = false;
static unsigned long gwDropStartedMillis = 0;
static bool gwFastIdentifyPending = false; // DISCONNECTED must not climb back to 5s after OP7/OP9
static bool hbAckPending = false;
static unsigned long hbSentMillis = 0;

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

void gwLogEvent(const String& ev) {
  gwLogAppend(ev);
}

// kind = coarse category (dedupe); detail = full text for first DROP_START / new kinds
static void gwNoteDrop(const String& kind, const String& detail) {
  if (!gwInDropState) {
    gwInDropState = true;
    gwDropStartedMillis = millis();
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
  gwDropStartedMillis = 0;
  gwFastIdentifyPending = false;
}

// Resume never worked on this board: clear session and IDENTIFY on gateway.discord.gg.
static void gwAbandonResume(const char* reason) {
  sessionId = "";
  lastSeq = 0;
  canResume = false;
  resumeGatewayHost = "";
  gwLogAppend(String("ABANDON_RESUME ") + (reason ? reason : ""));
}

static void gwSetReconnectIntervalMs(unsigned long ms) {
  gwReconnectIntervalMs = ms;
  gatewayWS.setReconnectInterval(gwReconnectIntervalMs);
  gwLogAppend(String("RECONNECT_INTERVAL_MS=") + String(gwReconnectIntervalMs));
}

// Drop path: skip resume; short reconnect so DISCONNECTED cannot climb to 5s.
static void gwArmFastIdentify(const char* reason) {
  gwAbandonResume(reason);
  gwFastIdentifyPending = true;
  gwSetReconnectIntervalMs(GW_RECONNECT_FAST_MS);
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
    gwSetReconnectIntervalMs(GW_RECONNECT_BASE_MS);
  } else {
    unsigned long next = gwReconnectIntervalMs * 2UL;
    if (next < GW_RECONNECT_BASE_MS) next = GW_RECONNECT_BASE_MS;
    if (next > GW_RECONNECT_MAX_MS) next = GW_RECONNECT_MAX_MS;
    gwSetReconnectIntervalMs(next);
  }
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
  // One beginSSL for the life of the bot. After drops, only setReconnectInterval +
  // disconnect(); do not beginSSL again (fights the library reconnect timer).
  bindGatewayHost("gateway.discord.gg");
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

// 80 MHz when OLED off and Discord Idle (identified). ESP32-S3 has no 100 MHz step.
// Locked at 240 MHz (idle downclock correlated with full-chip resets).
void applyCpuForIdleState() {
  uint32_t want = CPU_MHZ_ACTIVE;
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
  hbAckPending = true;
  hbSentMillis = millis();
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
    // Missed OP11 before next HB window -> zombied socket; drop and let library reconnect.
    if (hbAckPending &&
        (now - hbSentMillis >= (unsigned long)heartbeatIntervalMs)) {
      gwLogAppend("HB_ACK_TIMEOUT");
      gwArmFastIdentify("hb_ack");
      hbAckPending = false;
      gatewayWS.disconnect();
      return;
    }
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
      hbAckPending = false;

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

      // Never resume (never worked here). Keep fast IDENTIFY interval if already armed.
      // Do not beginSSL again — WebSocketsClient reconnect uses the original host.
      if (gwFastIdentifyPending) {
        gwSetReconnectIntervalMs(GW_RECONNECT_FAST_MS);
      } else if (wifiUp) {
        gwArmFastIdentify("disconnect");
      } else {
        gwAbandonResume("wifi_down");
        gwSetReconnectBackoff(false);
      }
      ensureWifiForGateway();
      showTransient("Gateway", "Disconnected");
      break;
    }
    case WStype_CONNECTED:
      gatewayConnected = true;
      gwFastIdentifyPending = false;
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

      // Hello: start HB (jittered first), then Identify only (resume never succeeds here)
      if (op == 10) {
        heartbeatIntervalMs = (*gwDoc)["d"]["heartbeat_interval"] | 0;
        hbAckPending = false;
        if (heartbeatIntervalMs > 0) {
          unsigned long jitter = (unsigned long)(esp_random() % (uint32_t)heartbeatIntervalMs);
          lastHeartbeatMillis = millis() - ((unsigned long)heartbeatIntervalMs - jitter);
        } else {
          lastHeartbeatMillis = millis();
        }
        gotHello = true;
        gwLogAppend(String("OP10_HELLO hb_ms=") + String(heartbeatIntervalMs));
        sendIdentify();
        return;
      }

      // Reconnect: drop session; library reconnects to same BIND_HOST (no second beginSSL)
      if (op == 7) {
        gwNoteDrop("OP7_RECONNECT", "OP7_RECONNECT");
        showTransient("Gateway", "Op7 reconnect");
        gwArmFastIdentify("op7");
        gatewayWS.disconnect();
        return;
      }

      // Invalid Session: always fresh IDENTIFY (resume path unused)
      if (op == 9) {
        bool resumable = false;
        StaticJsonDocument<96> small;
        if (!deserializeJson(small, payload, length)) {
          resumable = small["d"] | false;
        }
        String detail = String("OP9_INVALID_SESSION resumable=") + (resumable ? "1" : "0");
        gwNoteDrop(detail, detail);
        showTransient("Gateway", "Op9 session");
        gwArmFastIdentify("op9");
        gatewayWS.disconnect();
        return;
      }

      if (op == 11) {
        hbAckPending = false;
        return;
      }

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
          canResume = false; // resume disabled; always IDENTIFY after drops
          gwSetReconnectBackoff(true);
          gwClearDropState();
          gwLogAppend(String("READY session=") + (sessionId.length() ? "yes" : "no"));
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
          canResume = false;
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

          // Owner alert outputs: DM to bot -> set1 @ 1 Hz; @owner mention -> set2 @ 1 Hz
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
