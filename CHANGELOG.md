# Changelog

Older sections describe that release as shipped. Current firmware and docs are **0.5.2** (see `VERSION` and README).

## 0.5.2

- HTTPS REST: CA cert bundle again (no `setInsecure`); Discord bot token and API keys verified TLS.
- OLED uptime row shows seconds; OLED refresh **4 s**; LAN web `/api/status` poll **2 s**.
- Shared `formatLocalDateStr` / `formatUptimeStr`; command tokenizer with `cmdConsumesRest`.
- DeepSeek JSON buffer **24576**; removed scrape fallback. Science news filter/doc sized for SNAPI v4 `results`.
- Removed no-op `applyCpuForIdleState` (Discord Idle after 5 min quiet unchanged). `WIFI_PS_NONE` + HB ack grace kept.
- LAN CSS + boot/app JS moved to `web_assets.h`; `/api/status` built with ArduinoJson. Confirm flash via `Display · v0.5.2`.

## 0.5.1

- CPU locked at **240 MHz** (no idle downclock to 80 MHz when OLED blank + Discord Idle). Test for unexplained full-chip resets. Confirm flash via `Display · v0.5.1`.

## 0.5.00

- LAN web UI: k9dtv.com light/dark theme (local assets, `k9-theme`, OS default when unset); IC chip toggle with sun/moon + target label; sticky click focus cleared.
- LAN web UI: second IC chip for **Display** (all four panels) vs **Log** (Display + SysInfo only); preference in `mm-layout`.
- Discord: public `!sys` and `!ota` (ArduinoOTA IP / hostname / port 3232); uptime as `d h m s`; set1/set2 flash at 1 Hz 50%.
- LAN web UI on port **80** (`http://<board-ip>/`). Confirm flash via `Display · v0.5.00`.
- Gateway: skip resume (never worked here); after OP7/OP9/disconnect use 200 ms IDENTIFY reconnect (no 5 s climb). No boot channel `!sys` / `!help` posts.

## 0.4.95

- LAN web UI back on port **80** (`http://<board-ip>/`). Dropped the 8080 workaround. Confirm flash via `Display · v0.4.95`.

## 0.4.94

- LAN web UI briefly on port 8080 (Chrome HTTPS auto-upgrade workaround); reverted in 0.4.95.

## 0.4.93

- LAN web UI back to plain HTTP on port 80 (self-signed HTTPS removed; browser warning was not useful trust). Confirm flash via `Display · v0.4.93`.
- CI: compile FQBN uses `PartitionScheme=custom` so GitHub builds against sketch `partitions.csv` (not the default 1.25MB APP limit that failed on v0.4.92).

## 0.4.92

- LAN web UI: HTTPS on port 443 (self-signed); HTTP :80 redirects to HTTPS. Discord outbound TLS back to `setInsecure` / `beginSSL` (no CA verify). Confirm flash via `Display · v0.4.92`.

## 0.4.91

- HTTPS REST: verify server certificates with the ESP32 CA cert bundle (removed `setInsecure()`). Confirm flash via `Display · v0.4.91`.

## 0.4.90

- Fix: LAN **LOG** panel clears when content would exceed **20 KB** (was able to grow without a hard size wipe). SysInfo uptime spacing `Nd Nh Nm`. Confirm flash via `Display · v0.4.90`.

## 0.4.89

- Shipped breadboard firmware (no known bugs). Idle CPU **80 MHz** (ESP32-S3 has no 100 MHz step). Channel auto reports off; boot posts sysinfo + help once only. Confirm flash via `Display · v0.4.89`.

## 0.4.88

- Gateway reconnect: cap interval at 5s (no climb to 60s); reset backoff on OP9 so identify retry is not delayed. Best-effort under ~30s offline when Discord answers. Confirm flash via `Display · v0.4.88`.

## 0.4.87

- Web Serial panel: fixed 12-line ring (oldest drops off the top, new line at bottom); no scrollbar. Confirm flash via `Display · v0.4.87`.

## 0.4.86

- Gateway: save `resume_gateway_url` from READY and reconnect to that host after OP7 / resume paths (not always `gateway.discord.gg`), to cut OP9 invalid-session after Discord reconnect.

## 0.4.85

- Web Display: `.dash` gets `min-width:0;overflow:hidden` so meter `1fr` bars cannot spill past the Display panel. Confirm flash via `Display · v0.4.85`.

## 0.4.84

- Web Display meters: bar column is remaining width (`1fr`), not fixed `9rem`, so bars stop at the Display panel edge. Confirm flash via `Display · v0.4.84`.

## 0.4.83

- Web Display meters: value column `10ch` -> `7ch` so bars sit ~3 chars left. Confirm flash via `Display · v0.4.83`.

## 0.4.82

- Web Display meters: restore first-page fixed `9rem` `.bar` + original `bar()` fill. Each row is `label | 10ch value | bar` so Srv bar left edge matches Heap (and Sig). Confirm flash via `Display · v0.4.82`.

## 0.4.81

- Fix (on us): meters HTML is built on the ESP (not JS grid). Every bar track is `position:absolute;left:148px` so value length cannot shift bar starts. Confirm flash via `Display · v0.4.81`.

## 0.4.80

- Fix (on us): 0.4.78 table CSS did not lock bar columns. Sig/Heap/Srv now use one inline `display:grid` with columns `40px | 100px | 1fr` so all bar left edges match. Confirm flash via `Display · v0.4.80`.

## 0.4.79

- Web Display meters use absolute pixel layout: label at 0, value at 40px, every bar starts at 140px. Subtitle and Display header show `v0.4.79`.

## 0.4.78

- Web Display Sig/Heap/Srv use a fixed-layout HTML table so bar left edges share one column. Page subtitle shows `v0.4.78` so a successful flash is obvious.

## 0.4.77

- Web Display: each Sig/Heap/Srv row is its own identical grid (`label | 7rem value | bar`) so all three bar left edges match Sig.

## 0.4.76

- Web Display Sig/Heap/Srv values left-justified in the number column.

## 0.4.75

- Web Display meters: label | number (left) | bar (right); fixed value column keeps Sig/Heap/Srv bars aligned.

## 0.4.74

- Web UI: `sendNoCacheHeaders()` on `/` and `/api/status` plus HTML cache meta so browsers do not keep a stale page.

## 0.4.73

- Web Display meters left-justified: label | bar | value (Sig/Heap/Srv bars share left edge).

## 0.4.72

- Web Display Sig/Heap/Srv use one CSS grid (label | fixed value col | bar) so all three bars share the same left edge.

## 0.4.71

- Web Display meters: bar first (shared left edge after label), value on the right -- Heap/Srv/Sig bars align.

## 0.4.70

- Web Display: Sig/Heap/Srv meter bars share one left edge (fixed-width value column). OLED bar layout unchanged from pre-0.4.69. Idle CPU remains 100 MHz.

## 0.4.69

- Idle CPU (OLED off + Discord Idle, web not holding CPU) is 100 MHz.

## 0.4.68

- DM to bot flashes set1 at 10 Hz; @OWNER_ID mention flashes set2 at 10 Hz. Owner `!clear` stops both and forces OFF. `!set1`/`!set2` stop that pin's flash.

## 0.4.67

- USB Serial fully quiet: no `Serial.begin` / no MmLog to the port. All MmLog still goes to the web panels via `webLogFeed`. Copy `serial_log.cpp` when flashing.

## 0.4.66

- Serial panel back beside LOG; same box height; Serial has no scrollbar; Serial lines capped to LOG line count. USB Serial port still quiet.

## 0.4.65

- MmLog no longer writes to the USB Serial port; same lines still go to the web LOG via `webLogFeed`. Web UI unchanged.

## 0.4.64

- Kill Serial panel. Same MmLog stream still feeds the web LOG panel (FULL/END headers stripped). Layout: Display|SysInfo, LOG full width under both.

## 0.4.63

- Serial keeps its own live MmLog feed again (not cleared/clipped when LOG is empty). Same max depth as LOG.

## 0.4.62

- Rename system panel to SysInfo. LOG and Serial sit under Display+SysInfo. Serial line count capped to LOG line count.

## 0.4.61

- Layout: Display left, system Log right; under Display LOG then Serial. LOG = body between `[GW] === FULL LOG ===` and `END LOG` (headers omitted). Serial = all other MmLog lines.

## 0.4.60

- Web layout: centered logo (opens k9dtv.com), subtitle MiniMe A Discord Server APP; system Log left of Display; LOG (serial ring) under Display with Serial to its right (MmLog only).

## 0.4.59

- Web UI one page: K9DTV logo + Display + system Log + Serial(5). Separate `/log` removed.

## 0.4.58

- Full K9DTV logo restored; Display again includes tracked users. Two pages kept (`/` + `/log`). Use a ~4MB app partition if link overflows.

## 0.4.57

- Web UI back to two pages: `/` Display + Serial(5) with logo; `/log` system Log panel (opens in new window). Compact logo kept.

## 0.4.56

- Shrink web UI flash use: compact logo SVG + leaner CSS so the app fits the board text section.

## 0.4.55

- Web UI one page: Display (no tracked users), **Log** system panel restored, **Serial** shows only 5 live lines (RAM ring, no log file).

## 0.4.54

- Web UI: static K9DTV logo at top (`/logo.svg`, cached; not part of status refresh).

## 0.4.53

- Web UI: one page with display + live **Log** (serial/MmLog lines). Removed separate /long window.

## 0.4.52

- Keep CPU at 240 MHz while LAN web UI is up so OLED sleep / Discord Idle no longer drops Wi-Fi and kills the browser page.

## 0.4.51

- Web UI formatting: OLED-style label/value rows, meter bars, user columns; long-data page uses a system grid + user table.

## 0.4.50

- Web UI: display box only (fixed status JSON load); removed serial log panel and footer note; **Long data** opens `/long` in a new window.

## 0.4.49

- LAN web UI on port 80: OLED-style dashboard box (same fields, not pixels) plus a live 5-line serial log box (`web_ui.cpp`).

## 0.4.40

- README: status line under the photo; OLED and touch deep dives in `<details>`; **Why this is hard** bullets after How it works.

## 0.4.39

- README: hint under secrets fill-in to expand the Discord/API setup `<details>` sections.

## 0.4.38

- README: short **How it works** architecture diagram (Gateway / REST / OLED / touch) as Mermaid, placed after **What this bot can do**.

## 0.4.37

- GitHub Actions compile check for ESP32-S3 (OPI PSRAM, 16MB flash) via `arduino-cli`; README build badge. CI compiles only (does not prove board/Discord).

## 0.4.36

- README: collapse Discord/OpenWeatherMap/NASA/DeepSeek/ID howto sections into `<details>` so the top stays product + photo + features.

## 0.4.35

- LICENSE inventory updated for multi-file sketch sources; notes that local `secrets.h` is not in the repo.
- GitHub About description synced to current version.

## 0.4.34

- README: Ongoing project section (OTA updates, set1/set2 when a configured user ID — normally the owner — is mentioned or DMed, PCB + desk case).

## 0.4.33

- README: short AI-use note (same idea as Space Wars — AI helped with edits; James owned architecture and board decisions).

## 0.4.32

- Secrets are `#define` macros in `secrets.h` (not `const char*` variables) so multi-file link no longer reports multiple definition of Wi-Fi/token symbols.

## 0.4.31

- Split monolithic `MiniMe_Discord_Bot.ino` into multi-file Arduino sketch: `config.h`, `minime.h`, and `.cpp` modules (time, users, display, touch, hardware, discord REST/gateway, commands). Behavior unchanged. Thin `.ino` holds setup/loop only.


## 0.4.30

- Secrets moved out of the `.ino` into `MiniMe_Discord_Bot/secrets.h` (gitignored). Committed template: `secrets.example.h`. Sketch `#include "secrets.h"`.

## 0.4.29

- `!sysinfo` firmware link updated to `minime-esp32-discord-bot` (old repo name removed).
- README fill-in block matches sketch: `OWNER_ID_STR` / channel IDs are `const char*` (same as 0.4.27 firmware).

## 0.4.28

- Owner `!led`: keep `on` / `off`; add `!led <r> <g> <b>` (integers **0–255**) on the onboard RGB NeoPixel (GPIO 48, `NEO_GRB`). `on` is white 255 255 255. Helpers: `setLedRgb`, `parseRgbTriplet`.
- Mid-line `!led` keeps multi-word RGB args (same as `!ask` / `!display`). Discord `!help` / README use `!led on/off` wording.
- Firmware polish: `gwSendJson`, `drawDashBar`, `findFreeTrackedSlot`, `uptimeDhms`, HTTP open connect/timeout codes, dashboard user rows without `String names[]`, `sendDiscordMessage` via `httpsAwaitHeaders`, drop `botDiscordStatusSent` (status `0` = unset).
- CHANGELOG 0.4.13 mid-line note corrected for `!led`.

## 0.4.27

- Firmware cleanup: strip dead Serial debug and unused helpers; trim sketch comment noise (command list lives in Discord `!help`).
- Shared HTTPS/HTTP open helpers, fetch-command helpers, `discordDisplayName`, and tracked-user slot clear/fill.
- Owner/channel IDs as `const char*`; shared `boardMemTotals()` for OLED heap bar and `!sysinfo`.

## 0.4.26

- Remove unused `dashLastCmd`, `dashLastEvent`, `dashLastCmdMillis`, `TEMP_CHANNEL_ID_STR`, `drawTransient()`, `debugTouchSerial()`, `TOUCH_DEBUG_MS`, and `lastTouchDebugMillis`.
- README matches current firmware: poll-only OLED wake (no touch interrupt, no Serial touch debug); `!ask` Discord cap **2000** (not 3600); presence updates do not wake the OLED; `TOUCH_THRESHOLD` default **2000**; CPU 80 MHz when OLED is off and bot is Idle.

## 0.4.25

- OLED: uptime and temp on row 3; Sig / Heap / Srv shift down one row. User rows 7–14 and rows 15–16 unchanged.

## 0.4.24

- DS18B20 on GPIO 10: enable `INPUT_PULLUP` at boot and before each temperature read.
- README breadboard photo now includes the DS18B20 (`docs/minime-breadboard-v2.jpg`).

## 0.4.23

- OLED sleep: wake only when compensated touch is at or above trip. Ignore sticky hardware IRQ status, and attach the interrupt at the trip point (not 0) so idle below trip can dim/off.

## 0.4.22

- Comment out Serial (test logging off). Touch trip gap remains 2000.

## 0.4.21

- Touch trip gap default is **2000**.

## 0.4.20

- CPU 80 MHz when OLED is off and Discord bot status is Idle; 240 MHz otherwise. Serial remains test-only.

## 0.4.19

- Serial: dedicated `usb power:` line with VBUS millivolts and volts (divider on GPIO 1).

## 0.4.18

- Serial 115200 on; touch init and raw/comp/idle/trip/usb/touched lines every 500 ms.

## 0.4.17

- README touch tuning: lower `TOUCH_THRESHOLD` if the pad is hard to trigger; raise it if it false-triggers.

## 0.4.16

- Touch trip gap default is **450** (was 3500) to match a ~400–500 raw delta on this pad.

## 0.4.15

- Touch cal: 16-sample rolling idle average; trip is a constant gap above that average. USB VBUS ADC on GPIO 1 (10k/10k divider) scales touch raw so USB voltage does not walk the trigger. `!sysinfo` shows VBUS mV and idle/trip.

## 0.4.14

- Add `LICENSE` (MIT for original MiniMe files only; third-party libraries and APIs stay under their own terms). README points to it.

## 0.4.13

- Mid-sentence one-word args work for most commands (e.g. `… !weather 90210 …`). `!ask` / `!display` / `!led` keep multi-word args for the rest of the line.

## 0.4.12

- Commands may appear anywhere in a message (e.g. `Should I !ask what time is it.`); they are handled the same as if they started the line.

## 0.4.11

- Touch pad still wakes the OLED; it no longer sets Discord presence to Online.

## 0.4.10

- OLED row 14: `Bot:Online` / `Bot:Idle  ` left; fixed-slot `Www Mmm dd YYYY` right (space-padded day, DOW does not shift).

## 0.4.9

- Restore command handling that had regressed: lowercase the command word only (args like `!display` text keep their case).
- Unknown `!` commands reply `That is not a command.` (normal chat without `!` is ignored).
- `!weather` accepts only a 5-digit US ZIP; anything else returns `Invalid ZIP code.`

## 0.4.8

- OLED row 5 combines uptime and temp as fixed-width `Up:xxxxdxxhxxm T:xxxF/xxxC` (space-padded; sensor fail shows `T:--Error--`).
- User rows shift up to rows 6–13; row 14 left open.

## 0.4.7

- Fix `!ask` silent failures: Discord 2000-char post limit, larger REST JSON buffer, HTTP status check, HTTPS busy lock while DeepSeek runs, short fallback if the reply post fails.

## 0.4.6

- Boot auto `!sysinfo` waits until Gateway is connected and identified (no more false "Disconnected").

## 0.4.5

- Bot Discord presence: Online on commands, scheduled posts, and touch; Idle after 5 minutes quiet (Gateway OP 3).
- Serial logging commented out (including touch debug in `loop`) to reduce CPU load.

## 0.4.4

- Capacitive touch wake on GPIO 4: tap pad to turn the OLED back on after dim/off (1 min idle, 15 s fade).
- README: touch pad wiring, calibration, Serial debug, and threshold tuning. Display sleep timing matches firmware.
- Sketch section headers for user tracking, touch wake, Discord REST, and setup/loop.

## 0.4.3

- `!ask` is queued from the Gateway callback; DeepSeek HTTPS runs from `loop()` with heartbeats pumped so Discord stays connected.

## 0.4.2

- OLED: eight user rows; `Srv:` bar (0–90°, boot at 45°); command / `!display` text on rows 15–16 (dashboard is not wiped).
- `!display` is public; payload only, 50 characters (25 + 25), 6 seconds, overwrite restarts the timer.
- Member names load from `BOT_GUILD_ID` and both command-channel guilds (two servers).
- `!ask`: `max_tokens` 900, 12288-byte JSON parse, 3600-character Discord post.
- `!sysinfo` includes a GitHub firmware URL with Discord link embeds suppressed.
- `!help` and README command lists are alphabetical. README matches the current dashboard.

## 0.4.1

- Dashboard user stats: seven rows (was five).
- Sketch section comments for each major block (config, OLED, users, APIs, Gateway, setup/loop).
- README: 0.4.1 notes, U8g2 baseline (no setCursor), OLED sleep does not power down MCU or Wi-Fi, member load is 7.

## 0.4.0

- SSD1327 128×128 dashboard: MiniMe header, gateway, Pacific time, Sig/Heap bars, temp, uptime, five users with presence and 24h command counts.
- Startup REST member load (`BOT_GUILD_ID` / channel guild resolve) plus Presence Intent for On / Idle / DND / Off.
- OLED sleeps after 10 minutes with no real events (Sig/time/heap ticks do not count); commands, presence changes, and gateway messages wake it.
- Public `!ask`; `!status` is not in this firmware. Commands work in DMs and two allowed channels.
- US Pacific DST time, 8MB PSRAM gateway JSON, `!sysinfo` heap includes PSRAM.
- README rewrite, breadboard photo, no secrets in the GitHub sketch.

## 0.3.2

- Make `!ask` owner-only; update README and help text.

## 0.3.1

- Fix `!ask` DeepSeek parsing: handle chunked HTTP bodies, refuse gzip, clearer errors.

## 0.3.0

- Public `!ask <question>` via DeepSeek chat API (`DEEPSEEK_API_KEY`).

## 0.2.0

- Public science commands: `!news` (space/high-tech headlines), `!physics` (arXiv), `!apod` (NASA), `!iss` (ISS position).

## 0.1.1

- Minor firmware comment and formatting tweaks.

## 0.1.0

- Initial MiniMe ESP32-S3 Discord bot firmware (commands, weather, sensors, GPIO, OLED, scheduled reports).
- README for Discord token, weather API key, owner ID, and channel IDs.
