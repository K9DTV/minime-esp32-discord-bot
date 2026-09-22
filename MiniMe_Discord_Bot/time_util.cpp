#include "minime.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

int civilDayOfWeek(int year, int month, int day) { // 0 = Sunday
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int y = year;
  if (month < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + t[month - 1] + day) % 7;
}

int nthSundayOfMonth(int year, int month, int nth) {
  int dow1 = civilDayOfWeek(year, month, 1);
  int firstSunday = 1 + ((7 - dow1) % 7);
  return firstSunday + (nth - 1) * 7;
}

bool isPacificDaylightTime(unsigned long utcEpoch) {
  time_t t = (time_t)utcEpoch;
  struct tm tmUtc;
  gmtime_r(&t, &tmUtc);
  int year = tmUtc.tm_year + 1900;
  int month = tmUtc.tm_mon + 1;
  int day = tmUtc.tm_mday;
  int hour = tmUtc.tm_hour;
  if (month < 3 || month > 11) return false;
  if (month > 3 && month < 11) return true;
  if (month == 3) {
    int startDay = nthSundayOfMonth(year, 3, 2); // 2nd Sunday, 2:00am PST = 10:00 UTC
    if (day < startDay) return false;
    if (day > startDay) return true;
    return hour >= 10;
  }
  int endDay = nthSundayOfMonth(year, 11, 1); // 1st Sunday, 2:00am PDT = 09:00 UTC
  if (day < endDay) return true;
  if (day > endDay) return false;
  return hour < 9;
}

void updateLocalTime() {
  timeClient.setTimeOffset(0);
  timeClient.update();
  unsigned long utc = timeClient.getEpochTime();
  timeClient.setTimeOffset(isPacificDaylightTime(utc) ? PDT_OFFSET_SEC : PST_OFFSET_SEC);
}

void formatLocalDateStr(char* buf, size_t bufLen) {
  if (!buf || bufLen == 0) return;
  static const char* const DOW_NAME[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  static const char* const MON_NAME[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                         "Jul","Aug","Sep","Oct","Nov","Dec"};
  time_t localEpoch = (time_t)timeClient.getEpochTime();
  struct tm tmLocal;
  gmtime_r(&localEpoch, &tmLocal);
  snprintf(buf, bufLen, "%s %s %2d %04d",
           DOW_NAME[tmLocal.tm_wday], MON_NAME[tmLocal.tm_mon],
           tmLocal.tm_mday, tmLocal.tm_year + 1900);
}

void formatUptimeStr(char* buf, size_t bufLen) {
  if (!buf || bufLen == 0) return;
  unsigned long d = 0, h = 0, m = 0, s = 0;
  uptimeDhms(d, h, m, s);
  snprintf(buf, bufLen, "%lud %luh %lum %lus", d, h, m, s);
}
