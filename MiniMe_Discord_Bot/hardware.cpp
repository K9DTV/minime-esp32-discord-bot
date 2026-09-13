#include "minime.h"

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
Adafruit_NeoPixel pixels(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// Arduino-ESP32 3.x LEDC API (no driver/ledc.h)
static const int SERVO_LEDC_BITS = 14;
static const double SERVO_LEDC_HZ = 50.0;

void setupServo() {
  ledcAttach(PIN_SERVO, SERVO_LEDC_HZ, SERVO_LEDC_BITS);
}

void setServoAngle(int angleDeg) {
  if (angleDeg < 0) angleDeg = 0;
  if (angleDeg > 90) angleDeg = 90;
  lastServoDeg = angleDeg;
  int pulseUs = 500 + (1500 * angleDeg / 90);
  uint32_t max_duty = (1UL << SERVO_LEDC_BITS) - 1UL;
  uint32_t duty = (pulseUs * max_duty) / 20000UL;
  ledcWrite(PIN_SERVO, duty);
}

void setupPins() {
  pinMode(RGB_LED_PIN, OUTPUT);
  digitalWrite(RGB_LED_PIN, LOW);
  pixels.begin();
  pixels.show();
  pinMode(PIN_SET1, OUTPUT);
  pinMode(PIN_SET2, OUTPUT);
  digitalWrite(PIN_SET1, LOW);
  digitalWrite(PIN_SET2, LOW);
  pinMode(PIN_DS18B20, INPUT_PULLUP);
  setupServo();
  setServoAngle(45);
}

bool readTemperature(float& tempC, float& tempF) {
  pinMode(PIN_DS18B20, INPUT_PULLUP);
  sensors.requestTemperatures();
  float c = sensors.getTempCByIndex(0);
  if (c == DEVICE_DISCONNECTED_C) {
    return false;
  }
  tempC = c;
  tempF = c * 9.0f / 5.0f + 32.0f;
  return true;
}

bool isLedByteToken(const String& s) {
  if (s.length() == 0 || s.length() > 3) return false;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c < '0' || c > '9') return false;
  }
  int v = s.toInt();
  return v >= 0 && v <= 255;
}

void setLedRgb(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

bool parseRgbTriplet(const String& args, uint8_t& r, uint8_t& g, uint8_t& b) {
  String a = args;
  a.trim();
  int sp1 = a.indexOf(' ');
  int sp2 = (sp1 >= 0) ? a.indexOf(' ', sp1 + 1) : -1;
  if (sp1 <= 0 || sp2 <= sp1) return false;
  String rs = a.substring(0, sp1);
  String gs = a.substring(sp1 + 1, sp2);
  String bs = a.substring(sp2 + 1);
  bs.trim();
  int sp3 = bs.indexOf(' ');
  if (sp3 >= 0) bs = bs.substring(0, sp3);
  if (!isLedByteToken(rs) || !isLedByteToken(gs) || !isLedByteToken(bs)) return false;
  r = (uint8_t)rs.toInt();
  g = (uint8_t)gs.toInt();
  b = (uint8_t)bs.toInt();
  return true;
}

bool isOwner(const String& authorId) {
  return authorId == OWNER_ID_STR;
}

// 1 Hz flash, 50% duty: 500 ms on / 500 ms off per pin (independent).
static bool set1FlashActive = false;
static bool set2FlashActive = false;
static bool set1FlashOn = false;
static bool set2FlashOn = false;
static unsigned long set1FlashLastMs = 0;
static unsigned long set2FlashLastMs = 0;
static const unsigned long SET_FLASH_HALF_MS = 500; // 1 Hz, 50% cycle

void startSet1Flash() {
  set1FlashActive = true;
  set1FlashOn = true;
  set1FlashLastMs = millis();
  digitalWrite(PIN_SET1, HIGH);
}

void startSet2Flash() {
  set2FlashActive = true;
  set2FlashOn = true;
  set2FlashLastMs = millis();
  digitalWrite(PIN_SET2, HIGH);
}

void stopSet1Flash(bool leaveHigh) {
  set1FlashActive = false;
  set1FlashOn = false;
  digitalWrite(PIN_SET1, leaveHigh ? HIGH : LOW);
}

void stopSet2Flash(bool leaveHigh) {
  set2FlashActive = false;
  set2FlashOn = false;
  digitalWrite(PIN_SET2, leaveHigh ? HIGH : LOW);
}

void clearSetOutputs() {
  stopSet1Flash(false);
  stopSet2Flash(false);
}

void pumpSetFlash() {
  unsigned long now = millis();
  if (set1FlashActive && (now - set1FlashLastMs >= SET_FLASH_HALF_MS)) {
    set1FlashLastMs = now;
    set1FlashOn = !set1FlashOn;
    digitalWrite(PIN_SET1, set1FlashOn ? HIGH : LOW);
  }
  if (set2FlashActive && (now - set2FlashLastMs >= SET_FLASH_HALF_MS)) {
    set2FlashLastMs = now;
    set2FlashOn = !set2FlashOn;
    digitalWrite(PIN_SET2, set2FlashOn ? HIGH : LOW);
  }
}
