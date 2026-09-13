#include "minime.h"

// Declared in minime.h / ota.cpp — keep if sketch-folder minime.h is behind
String otaStatusText();

unsigned long lastSysInfoMillis = 0;
int lastSentHour = -1;
bool askNeedPost = false;
String askPendingQuestion;
String askPendingChannelId;

String collapseWhitespace(String s) {
  s.replace("\n", " ");
  s.replace("\r", " ");
  s.replace("\t", " ");
  while (s.indexOf("  ") >= 0) {
    s.replace("  ", " ");
  }
  s.trim();
  return s;
}

String truncateText(const String& s, int maxLen) {
  if (s.length() <= maxLen) return s;
  return s.substring(0, maxLen - 3) + "...";
}

bool getWeather(const String& zip, String& outReport) {
  WiFiClient client;
  String url = "/data/2.5/weather?zip=" + zip + ",US&units=imperial&appid=" + WEATHER_API_KEY;
  uint8_t openErr = httpGetOpen(client, "api.openweathermap.org", url, 5000);
  if (openErr) {
    setHttpOpenError(outReport, openErr, "Weather service");
    return false;
  }
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, client);
  client.stop();
  if (err) {
    outReport = "Weather JSON parse error.";
    return false;
  }
  String city = doc["name"] | "Unknown";
  float tempF = doc["main"]["temp"] | 0.0f;
  float tempC = (tempF - 32.0f) * 5.0f / 9.0f;
  int humidity = doc["main"]["humidity"] | 0;
  String cond = doc["weather"][0]["description"] | "Unknown";
  outReport = "☁️ **Weather Report (" + city + " - " + zip + "):**\n" +
              "• **Condition:** " + cond + "\n" +
              "• **Temperature:** " + String(tempF, 1) + "°F (" + String(tempC, 1) + "°C)\n" +
              "• **Humidity:** " + String(humidity) + "%";
  return true;
}

bool getScienceNews(String& outReport) {
  uint8_t openErr = httpsGetOpen("api.spaceflightnewsapi.net", "/v4/articles/?limit=3", 8000);
  if (openErr) {
    setHttpOpenError(outReport, openErr, "Science news");
    return false;
  }
  StaticJsonDocument<192> filter;
  filter["results"][0]["title"] = true;
  filter["results"][0]["news_site"] = true;
  filter["results"][0]["url"] = true;
  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, httpsClient, DeserializationOption::Filter(filter));
  httpsClient.stop();
  if (err) {
    outReport = "Science news JSON parse error.";
    return false;
  }
  JsonArray results = doc["results"].as<JsonArray>();
  if (results.isNull() || results.size() == 0) {
    outReport = "No science headlines right now.";
    return false;
  }
  outReport = "🛰️ **Space & high-tech headlines:**\n";
  int n = 0;
  for (JsonObject item : results) {
    if (n >= 3) break;
    String title = collapseWhitespace(item["title"] | "Untitled");
    String site = item["news_site"] | "Source";
    String url = item["url"] | "";
    outReport += String(n + 1) + ". **" + truncateText(title, 140) + "** (" + site + ")";
    if (url.length()) outReport += "\n" + url;
    outReport += "\n";
    n++;
  }
  return n > 0;
}

bool getPhysicsPapers(String& outReport) {
  const char* path =
    "/api/query?search_query=cat:physics.*&start=0&max_results=3&sortBy=submittedDate&sortOrder=descending";
  uint8_t openErr = httpsGetOpen("export.arxiv.org", path, 8000,
                                 "MiniMeBot/1.0 (ESP32 Discord bot)", "Accept-Encoding: identity\r\n");
  if (openErr) {
    setHttpOpenError(outReport, openErr, "arXiv");
    return false;
  }
  String xml;
  unsigned long start = millis();
  while (millis() - start < 8000) {
    while (httpsClient.available()) {
      xml += httpsClient.readString();
      if (xml.length() > 24000) break;
    }
    if (!httpsClient.connected() && !httpsClient.available()) break;
    delay(10);
  }
  httpsClient.stop();
  if (xml.length() < 50) {
    outReport = "arXiv response empty.";
    return false;
  }
  outReport = "⚛️ **Latest arXiv physics papers:**\n";
  int from = 0;
  int n = 0;
  while (n < 3) {
    int entry = xml.indexOf("<entry>", from);
    if (entry < 0) break;
    int entryEnd = xml.indexOf("</entry>", entry);
    if (entryEnd < 0) break;
    String block = xml.substring(entry, entryEnd);
    int t0 = block.indexOf("<title>");
    int t1 = block.indexOf("</title>");
    String title = "Untitled";
    if (t0 >= 0 && t1 > t0) {
      title = collapseWhitespace(block.substring(t0 + 7, t1));
    }
    int i0 = block.indexOf("<id>");
    int i1 = block.indexOf("</id>");
    String id = "";
    if (i0 >= 0 && i1 > i0) {
      id = collapseWhitespace(block.substring(i0 + 4, i1));
    }
    outReport += String(n + 1) + ". **" + truncateText(title, 140) + "**";
    if (id.length()) outReport += "\n" + id;
    outReport += "\n";
    n++;
    from = entryEnd + 8;
  }
  if (n == 0) {
    outReport = "No physics papers found.";
    return false;
  }
  return true;
}

bool getApod(String& outReport) {
  String path = String("/planetary/apod?api_key=") + NASA_API_KEY;
  uint8_t openErr = httpsGetOpen("api.nasa.gov", path, 8000);
  if (openErr) {
    setHttpOpenError(outReport, openErr, "NASA APOD");
    return false;
  }
  StaticJsonDocument<128> filter;
  filter["title"] = true;
  filter["explanation"] = true;
  filter["date"] = true;
  filter["url"] = true;
  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, httpsClient, DeserializationOption::Filter(filter));
  httpsClient.stop();
  if (err) {
    outReport = "NASA APOD JSON parse error.";
    return false;
  }
  String title = doc["title"] | "Astronomy Picture of the Day";
  String date = doc["date"] | "";
  String expl = collapseWhitespace(doc["explanation"] | "");
  String url = doc["url"] | "";
  outReport = "🌌 **NASA APOD";
  if (date.length()) outReport += " (" + date + ")";
  outReport += ":**\n• **" + title + "**\n" + truncateText(expl, 350);
  if (url.length()) outReport += "\n" + url;
  return true;
}

bool getIssPosition(String& outReport) {
  WiFiClient client;
  uint8_t openErr = httpGetOpen(client, "api.open-notify.org", "/iss-now.json", 5000);
  if (openErr) {
    setHttpOpenError(outReport, openErr, "ISS tracker");
    return false;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, client);
  client.stop();
  if (err) {
    outReport = "ISS tracker JSON parse error.";
    return false;
  }
  String lat = doc["iss_position"]["latitude"] | "?";
  String lon = doc["iss_position"]["longitude"] | "?";
  outReport = "🌍 **ISS now:**\n"
              "• **Latitude:** " + lat + "\n"
              "• **Longitude:** " + lon;
  return true;
}

bool askDeepSeek(const String& question, String& outReport) {
  if (strlen(DEEPSEEK_API_KEY) == 0 ||
      strcmp(DEEPSEEK_API_KEY, "DEEPSEEK_API_KEY") == 0) {
    outReport = "DeepSeek API key not set. Add DEEPSEEK_API_KEY in secrets.h.";
    return false;
  }
  String q = question;
  q.trim();
  if (q.length() == 0) {
    outReport = "Usage: !ask <question>";
    return false;
  }
  if (q.length() > 500) {
    q = q.substring(0, 500);
  }
  StaticJsonDocument<1536> req;
  req["model"] = "deepseek-chat";
  req["max_tokens"] = DEEPSEEK_MAX_TOKENS;
  req["temperature"] = 0.7;
  req["stream"] = false;
  JsonArray messages = req.createNestedArray("messages");
  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = "You are MiniMe on an ESP32 Discord bot. Answer clearly for science, tech, and physics. Keep the full answer under 2000 characters so it fits one Discord message.";
  JsonObject user = messages.createNestedObject();
  user["role"] = "user";
  user["content"] = q;
  String body;
  serializeJson(req, body);
  if (!httpsConnect("api.deepseek.com", 25000)) {
    outReport = "DeepSeek connection failed.";
    return false;
  }
  String request =
    "POST /chat/completions HTTP/1.1\r\n"
    "Host: api.deepseek.com\r\n"
    "Authorization: Bearer " + String(DEEPSEEK_API_KEY) + "\r\n"
    "Content-Type: application/json\r\n"
    "Accept: application/json\r\n"
    "Accept-Encoding: identity\r\n"
    "User-Agent: MiniMeBot/1.0\r\n"
    "Content-Length: " + String(body.length()) + "\r\n"
    "Connection: close\r\n\r\n" +
    body;
  httpsClient.print(request);
  unsigned long deadline = millis() + 45000UL;
  String statusLine;
  bool chunked = false;
  int contentLength = -1;
  if (!httpsAwaitHeaders(deadline, true, statusLine, chunked, contentLength)) {
    outReport = "DeepSeek timeout waiting for headers.";
    return false;
  }
  String respBody;
  if (!readHttpBodyAfterHeaders(httpsClient, chunked, contentLength, respBody, deadline)) {
    httpsClient.stop();
    outReport = "DeepSeek empty response. " + statusLine;
    return false;
  }
  httpsClient.stop();
  int jsonStart = respBody.indexOf('{');
  if (jsonStart < 0) {
    outReport = "DeepSeek: no JSON body. " + truncateText(statusLine, 80);
    return false;
  }
  if (jsonStart > 0) {
    respBody = respBody.substring(jsonStart);
  }
  StaticJsonDocument<128> filter;
  filter["choices"][0]["message"]["content"] = true;
  filter["error"]["message"] = true;
  StaticJsonDocument<DEEPSEEK_JSON_DOC> doc;
  DeserializationError err = deserializeJson(doc, respBody, DeserializationOption::Filter(filter));
  if (err) {
    outReport = "DeepSeek JSON parse error (" + String(err.c_str()) + ").";
    return false;
  }
  if (doc.containsKey("error")) {
    String emsg = doc["error"]["message"] | "API error";
    outReport = "DeepSeek error: " + truncateText(emsg, 200);
    return false;
  }
  String answer = collapseWhitespace(doc["choices"][0]["message"]["content"] | "");
  if (answer.length() == 0) {
    int ckey = respBody.indexOf("\"content\"");
    if (ckey >= 0) {
      int colon = respBody.indexOf(':', ckey);
      int q1 = respBody.indexOf('"', colon + 1);
      if (q1 >= 0) {
        String extracted;
        for (int i = q1 + 1; i < (int)respBody.length(); i++) {
          char c = respBody[i];
          if (c == '\\' && i + 1 < (int)respBody.length()) {
            char n = respBody[i + 1];
            if (n == 'n') { extracted += ' '; i++; continue; }
            if (n == '"' || n == '\\') { extracted += n; i++; continue; }
          }
          if (c == '"') break;
          extracted += c;
          if (extracted.length() > 1900) break;
        }
        answer = collapseWhitespace(extracted);
      }
    }
  }
  if (answer.length() == 0) {
    outReport = "DeepSeek returned an empty answer. " + truncateText(statusLine, 60);
    return false;
  }
  const char* prefix = "🧠 **DeepSeek:**\n";
  int room = DISCORD_CONTENT_MAX - (int)strlen(prefix);
  if (room < 100) room = 100;
  outReport = String(prefix) + truncateText(answer, room);
  return true;
}

void runAskFromLoop() {
  if (!askNeedPost) return;
  askNeedPost = false;
  String channelId = askPendingChannelId;
  String report;
  httpsInUse = true; // keep Gateway from starting other HTTPS during DeepSeek
  bool ok = askDeepSeek(askPendingQuestion, report);
  httpsInUse = false;
  askPendingQuestion = "";
  askPendingChannelId = "";
  if (!sendDiscordMessage(channelId, report)) {
    String fallback = ok
      ? "DeepSeek answered, but Discord rejected the post (try a shorter question)."
      : truncateText(report, DISCORD_CONTENT_MAX);
    if (!sendDiscordMessage(channelId, fallback)) {
      showTransient("DeepSeek", "Post fail");
      return;
    }
  }
  if (ok) {
    showTransient("DeepSeek", "Sent");
  } else {
    showTransient("DeepSeek", "Error");
  }
}

typedef bool (*FetchReportFn)(String&);

void sendFetchResult(const String& channelId, const char* label, bool ok, const String& report,
                     const String& okLine2 = "Sent", const String& okLine3 = "") {
  sendDiscordMessage(channelId, report);
  if (ok) showTransient(label, okLine2, okLine3);
  else showTransient(label, "Error");
}

void runFetchCommand(const String& channelId, const char* label, const char* fetching,
                     FetchReportFn fetch) {
  String report;
  showTransient(label, fetching);
  sendFetchResult(channelId, label, fetch(report), report);
}

void handleCommand(const String& content, const String& authorId, const String& authorName,
                   const String& channelId, bool isDM)
{
  if (!isDM && channelId != TARGET_CHANNEL_ID && channelId != TARGET_CHANNEL_ID1) {
    return;
  }
  String raw = content;
  raw.trim();
  bool midLine = false;
  if (!raw.startsWith("!")) {
    int bang = -1;
    for (int i = 0; i < raw.length(); i++) {
      if (raw.charAt(i) != '!') continue;
      char next = (i + 1 < raw.length()) ? raw.charAt(i + 1) : 0;
      if (!((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z'))) continue;
      if (i > 0) {
        char prev = raw.charAt(i - 1);
        if (prev != ' ' && prev != '\t' && prev != '\n') continue;
      }
      bang = i;
      break;
    }
    if (bang < 0) return;
    raw = raw.substring(bang);
    midLine = true;
  }

  noteBotActivity();
  int spIdx = raw.indexOf(' ');
  String cmdWord = (spIdx > 0) ? raw.substring(0, spIdx) : raw;
  String args = (spIdx > 0) ? raw.substring(spIdx + 1) : "";
  cmdWord.toLowerCase();
  args.trim();
  // Mid-line bang: one-word args for most cmds. !ask / !display / !led keep multi-word args.
  if (midLine && args.length() > 0 &&
      cmdWord != "!ask" && cmdWord != "!display" && cmdWord != "!led") {
    int argSp = args.indexOf(' ');
    if (argSp > 0) args = args.substring(0, argSp);
    while (args.length() > 0) {
      char last = args.charAt(args.length() - 1);
      if (last == '.' || last == ',' || last == '!' || last == '?' || last == ';' || last == ':') {
        args.remove(args.length() - 1);
      } else {
        break;
      }
    }
  }

  recordUserUse(authorId, authorName);

  if (cmdWord == "!help") {
    String helpMsg =
      "🤖 **MiniMe Bot Commands**\n\n"
      "**👤 Public Commands:**\n"
      "• `!apod` — NASA Astronomy Picture of the Day.\n"
      "• `!ask <question>` — Asks DeepSeek (text AI reply in chat).\n"
      "• `!display <text>` — Writes custom text to the OLED screen.\n"
      "• `!help` — Shows this command list.\n"
      "• `!iss` — Current International Space Station position.\n"
      "• `!news` — Space and high-tech science headlines.\n"
      "• `!physics` — Latest arXiv physics papers.\n"
      "• `!sys` — Displays system diagnostics (uptime, heap, RSSI, etc.).\n"
      "• `!temp` — Reads the current indoor temperature sensor.\n"
      "• `!time` — Displays the current bot time.\n"
      "• `!weather <zip>` — Fetches the weather report for a US ZIP code.\n"
      "• `!ota` — Wi-Fi firmware update info (IP / hostname).\n\n"
      "**👑 Owner-Only Commands:**\n"
      "• `!led on/off` / `!led <r> <g> <b>` — RGB NeoPixel (0–255 per channel).\n"
      "• `!servo <0-90>` — Moves the servo motor to a specific angle.\n"
      "• `!set1 on` / `!set1 off` — Controls digital output pin 1.\n"
      "• `!set2 on` / `!set2 off` — Controls digital output pin 2.\n"
      "• `!clear` — Stops set1/set2 flash and forces both outputs off.";
    sendDiscordMessage(channelId, helpMsg);
    showTransient("Help", "Command Sent");
    return;
  }
  if (cmdWord == "!weather") {
    if (args.length() == 0) {
      sendDiscordMessage(channelId, "Usage: !weather <zip>");
      return;
    }
    String zip = args;
    bool validZip = (zip.length() == 5);
    if (validZip) {
      for (unsigned i = 0; i < 5; i++) {
        char c = zip.charAt(i);
        if (c < '0' || c > '9') { validZip = false; break; }
      }
    }
    if (!validZip) {
      sendDiscordMessage(channelId, "Invalid ZIP code.");
      return;
    }
    String report;
    showTransient("Weather", "Fetching " + zip + "...");
    bool ok = getWeather(zip, report);
    sendFetchResult(channelId, "Weather", ok, report, zip, "Sent");
    return;
  }
  if (cmdWord == "!news") {
    runFetchCommand(channelId, "News", "Fetching...", getScienceNews);
    return;
  }
  if (cmdWord == "!physics") {
    runFetchCommand(channelId, "Physics", "Fetching arXiv...", getPhysicsPapers);
    return;
  }
  if (cmdWord == "!apod") {
    runFetchCommand(channelId, "APOD", "Fetching NASA...", getApod);
    return;
  }
  if (cmdWord == "!iss") {
    runFetchCommand(channelId, "ISS", "Fetching...", getIssPosition);
    return;
  }
  if (cmdWord == "!temp") {
    float c, f;
    if (readTemperature(c, f)) {
      dashTempC = c;
      dashTempF = f;
      String msg = "Current Temp: " + String(c, 1) + "°C / " + String(f, 1) + "°F";
      sendDiscordMessage(channelId, msg);
      showTransient("Temp", String(f, 1) + "F/" + String(c, 1) + "C");
    } else {
      sendDiscordMessage(channelId, "Temperature sensor error.");
      showTransient("Temp", "Sensor error");
    }
    return;
  }
  if (cmdWord == "!sys") {
    sendDiscordMessage(channelId, getSystemInfo(), true);
    showTransient("Sys", "Sent");
    return;
  }
  if (cmdWord == "!ota") {
    sendDiscordMessage(channelId, otaStatusText());
    showTransient("OTA", WiFi.localIP().toString());
    return;
  }
  if (cmdWord == "!time") {
    updateLocalTime();
    String currentTime = timeClient.getFormattedTime();
    String msg = "🕒 Current Bot Time: " + currentTime;
    sendDiscordMessage(channelId, msg);
    showTransient("Time", currentTime);
    return;
  }
  if (cmdWord == "!ask") {
    if (args.length() == 0) {
      sendDiscordMessage(channelId, "Usage: !ask <question>");
      return;
    }
    if (askNeedPost) {
      sendDiscordMessage(channelId, "DeepSeek is already answering. Try again in a moment.");
      return;
    }
    String question = args;
    askPendingQuestion = question;
    askPendingChannelId = channelId;
    askNeedPost = true;
    showTransient("DeepSeek", "Queued");
    return;
  }
  if (cmdWord == "!display") {
    if (args.length() == 0) {
      sendDiscordMessage(channelId, "Usage: !display <text>");
      return;
    }
    String text = args;
    if (text.length() > 50) text = text.substring(0, 50);
    String line15 = text.substring(0, text.length() > 25 ? 25 : text.length());
    String line16 = text.length() > 25 ? text.substring(25) : "";
    showTransient(line15, line16, "", 6000);
    sendDiscordMessage(channelId, "Display updated.");
    return;
  }
  int setN = 0;
  if (cmdWord.length() == 5 && cmdWord.startsWith("!set")) {
    char d = cmdWord.charAt(4);
    if (d == '1' || d == '2') setN = d - '0';
  }
  if (cmdWord == "!led" || setN != 0 || cmdWord == "!servo" || cmdWord == "!clear") {
    if (!isOwner(authorId)) {
      if (!isDM) {
        sendDiscordMessage(channelId, "You are not allowed to use this command.");
      }
      return;
    }
    if (cmdWord == "!clear") {
      clearSetOutputs();
      sendDiscordMessage(channelId, "set1/set2 cleared (off)");
      showTransient("clear", "set1 set2 OFF");
      return;
    }
    if (cmdWord == "!led") {
      String a = args;
      a.trim();
      String al = a;
      al.toLowerCase();
      if (al == "on") {
        setLedRgb(255, 255, 255);
        sendDiscordMessage(channelId, "LED ON (255 255 255)");
        showTransient("LED", "ON");
      } else if (al == "off") {
        setLedRgb(0, 0, 0);
        sendDiscordMessage(channelId, "LED OFF");
        showTransient("LED", "OFF");
      } else {
        uint8_t r, g, b;
        if (!parseRgbTriplet(a, r, g, b)) {
          sendDiscordMessage(channelId, "Usage: !led on/off  or  !led <r> <g> <b> (0-255)");
          return;
        }
        setLedRgb(r, g, b);
        String msg = "LED RGB " + String(r) + " " + String(g) + " " + String(b);
        sendDiscordMessage(channelId, msg);
        showTransient("LED", String(r) + "," + String(g) + "," + String(b));
      }
      return;
    }
    if (setN != 0) {
      String a = args;
      a.toLowerCase();
      if (a != "on" && a != "off") {
        sendDiscordMessage(channelId, "Usage: !set1 on/off  or  !set2 on/off");
        return;
      }
      bool on = (a == "on");
      if (setN == 1) stopSet1Flash(on);
      else stopSet2Flash(on);
      String label = "set" + String(setN);
      String val = on ? "ON" : "OFF";
      sendDiscordMessage(channelId, label + " " + val);
      showTransient(label, val);
      return;
    }
    if (cmdWord == "!servo") {
      if (args.length() == 0) {
        sendDiscordMessage(channelId, "Usage: !servo <0-90>");
        return;
      }
      int angle = args.toInt();
      if (angle < 0 || angle > 90) {
        sendDiscordMessage(channelId, "Angle out of range. Allowed: 0-90 degrees.");
        return;
      }
      setServoAngle(angle);
      String msg = "Servo set to " + String(angle) + " degrees.";
      sendDiscordMessage(channelId, msg);
      showTransient("Servo", String(angle) + " deg");
      return;
    }
  }

  sendDiscordMessage(channelId, "That is not a command.");
  showTransient("Unknown", cmdWord);
}

void backgroundTasks() {
  runAskFromLoop();
  // Boot only: one !sys + !help in TARGET_CHANNEL_ID after Gateway is up. No repeating auto reports.
  if (lastSysInfoMillis != 0) return;
  if (!gatewayConnected || !identified) return;
  lastSysInfoMillis = millis();
  noteBotActivity();
  sendDiscordMessage(TARGET_CHANNEL_ID, getSystemInfo(), true);
  handleCommand("!help", String(""), String("boot"), TARGET_CHANNEL_ID, false);
}
