#include "minime.h"

TrackedUser trackedUsers[MAX_TRACKED_USERS];
String cachedGuildIds[MAX_CACHED_GUILDS];
uint8_t cachedGuildCount = 0;
unsigned long usesWindowStartMillis = 0;

uint8_t statusFromDiscord(const char* s) {
  if (!s) return 0;
  if (!strcmp(s, "online")) return 2;
  if (!strcmp(s, "idle")) return 1;
  if (!strcmp(s, "dnd")) return 3;
  return 0;  // offline / unknown
}

const char* statusToWord(uint8_t s) {
  static const char* w[] = {"Off", "Idle", "On", "DND"};
  return s < 4 ? w[s] : "Off";
}

void clearTrackedSlot(uint8_t i) {
  trackedUsers[i].active = false;
  trackedUsers[i].userId = "";
  trackedUsers[i].userName = "";
  trackedUsers[i].status = 0;
  trackedUsers[i].useCount24h = 0;
}

void fillTrackedSlot(uint8_t i, const String& userId, const String& userName) {
  trackedUsers[i].active = true;
  trackedUsers[i].userId = userId;
  trackedUsers[i].userName = userName;
  trackedUsers[i].status = 0;
  trackedUsers[i].useCount24h = 0;
}

void initTrackedUsers() {
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) clearTrackedSlot(i);
}

void resetUseWindowIfNeeded() {
  unsigned long now = millis();
  if (usesWindowStartMillis == 0) {
    usesWindowStartMillis = now;
    return;
  }
  if (now - usesWindowStartMillis >= USES_WINDOW_MS) {
    usesWindowStartMillis = now;
    for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
      trackedUsers[i].useCount24h = 0;
    }
  }
}

int findUserIndex(const String& userId) {
  if (userId.length() == 0) return -1;
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
    if (trackedUsers[i].active && trackedUsers[i].userId == userId) return (int)i;
  }
  return -1;
}

int findFreeTrackedSlot() {
  for (uint8_t i = 0; i < MAX_TRACKED_USERS; i++) {
    if (!trackedUsers[i].active) return (int)i;
  }
  return -1;
}

int addOrPickUserSlot(const String& userId, const String& userName) {
  int idx = findUserIndex(userId);
  if (idx >= 0) {
    if (userName.length()) trackedUsers[idx].userName = userName;
    return idx;
  }

  idx = findFreeTrackedSlot();
  if (idx >= 0) {
    fillTrackedSlot((uint8_t)idx, userId, userName);
    return idx;
  }

  // Full: overwrite lowest 24h-use slot.
  uint8_t worst = 0;
  for (uint8_t i = 1; i < MAX_TRACKED_USERS; i++) {
    if (trackedUsers[i].useCount24h < trackedUsers[worst].useCount24h) worst = i;
  }
  fillTrackedSlot(worst, userId, userName);
  return (int)worst;
}

void recordUserUse(const String& userId, const String& userName) {
  resetUseWindowIfNeeded();
  if (userId.length() == 0) return;
  int idx = findUserIndex(userId);
  if (idx < 0) {
    idx = addOrPickUserSlot(userId, userName);
  }
  if (idx < 0) return;
  if (userName.length()) trackedUsers[idx].userName = userName;
  trackedUsers[idx].status = 2;  // command use => On on OLED
  trackedUsers[idx].useCount24h++;
  noteDisplayActivity();
}

void applyPresencesArray(JsonArray presences) {
  if (presences.isNull()) return;
  for (JsonObject p : presences) {
    const char* uid = p["user"]["id"];
    if (!uid) continue;
    int idx = findUserIndex(String(uid));
    if (idx < 0) continue;
    const char* st = p["status"] | "offline";
    trackedUsers[idx].status = statusFromDiscord(st);
  }
}

String discordDisplayName(JsonVariantConst user) {
  String name = user["global_name"] | "";
  if (name.length() == 0) name = user["username"] | "";
  return name;
}

void handlePresenceUpdate(JsonObject d) {
  const char* uid = d["user"]["id"];
  const char* st  = d["status"] | "offline";
  if (!uid) return;

  String name = discordDisplayName(d["user"]);
  int idx = findUserIndex(String(uid));
  if (idx < 0) {
    idx = addOrPickUserSlot(String(uid), name);
    if (idx < 0) return;
  }
  if (name.length()) trackedUsers[idx].userName = name;
  trackedUsers[idx].status = statusFromDiscord(st);
}

void rememberGuildId(const String& gid) {
  String id = gid;
  id.trim();
  if (!discordIdLooksValid(id)) return;
  for (uint8_t i = 0; i < cachedGuildCount; i++) {
    if (cachedGuildIds[i] == id) return;
  }
  if (cachedGuildCount >= MAX_CACHED_GUILDS) return;
  cachedGuildIds[cachedGuildCount++] = id;
}
