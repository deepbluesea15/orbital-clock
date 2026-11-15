#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include <time.h>
#include "time_service.h"
#include "weather_location.h"

#define CALLSIGN "NX-01"

struct Cfg {
  String city, tz, cached_city, cached_timezone;
  float lat = 0, lon = 0;
  bool haveLoc = false;
  bool metric = true;
  LocationStatus location_status = LocationStatus::PENDING;
};

struct WxData {
  bool valid = false;
  bool highLowValid = false;
  float tempF = 0, feelF = 0, hiF = 0, loF = 0, wind = 0, hum = 0;
  int code = 0, isDay = 1;
  float fcHi[5], fcLo[5];
  int fcCode[5];
  uint8_t fcMonth[5], fcDay[5];
  int fcDays = 0;
  uint32_t stamp = 0;
  uint32_t tries = 0, fails = 0;
};

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;
extern const int W;
extern const int H;
extern bool use24h;
extern const struct tm &tnow;
extern uint32_t bootMillis;
extern int gOfsX;
extern int gOfsY;
extern Cfg cfg;
extern const WxData &wthr;
extern const char *DOW[7];
extern const char *MON[12];
enum WxIcon : uint8_t { IC_SUN, IC_PARTLY, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };

inline float subSecond() { return TimeService::sub_second(); }
inline int hour12(int hour) { return TimeService::hour12(hour); }
inline float stardate() { return TimeService::stardate(); }
const char *linkStr();
const char *cityLabel();

inline void dPixel(int x, int y) { u8g2.drawPixel(x + gOfsX, y + gOfsY); }
inline void dStr(int x, int y, const char *text) { u8g2.drawStr(x + gOfsX, y + gOfsY, text); }
inline void dBox(int x, int y, int width, int height) { u8g2.drawBox(x + gOfsX, y + gOfsY, width, height); }
inline void dFrame(int x, int y, int width, int height) { u8g2.drawFrame(x + gOfsX, y + gOfsY, width, height); }
inline void dHLine(int x, int y, int width) { u8g2.drawHLine(x + gOfsX, y + gOfsY, width); }
inline void dVLine(int x, int y, int height) { u8g2.drawVLine(x + gOfsX, y + gOfsY, height); }
inline void dDisc(int x, int y, int radius) { u8g2.drawDisc(x + gOfsX, y + gOfsY, radius); }
inline void dCircle(int x, int y, int radius) { u8g2.drawCircle(x + gOfsX, y + gOfsY, radius); }

inline void dDashed(int x, int y, int width, int on = 2, int off = 2) {
  for (int i = 0; i < width; i += on + off) {
    int segment = (width - i < on) ? (width - i) : on;
    if (segment > 0) dHLine(x + i, y, segment);
  }
}

inline int strW(const char *text) { return u8g2.getStrWidth(text); }
inline void dStrRight(int right_x, int y, const char *text) { dStr(right_x - strW(text), y, text); }

inline uint8_t face_outcode(int x, int y) {
  uint8_t code = 0;
  if (x < 0) code |= 1; else if (x > W - 1) code |= 2;
  if (y < 0) code |= 4; else if (y > H - 1) code |= 8;
  return code;
}

inline void cline(int x0, int y0, int x1, int y1) {
  uint8_t code0 = face_outcode(x0, y0), code1 = face_outcode(x1, y1);
  for (int guard = 0; guard < 8; guard++) {
    if (!(code0 | code1)) { u8g2.drawLine(x0, y0, x1, y1); return; }
    if (code0 & code1) return;
    uint8_t code = code0 ? code0 : code1;
    int x = 0, y = 0;
    if      (code & 8) { x = x0 + (int)((long)(x1 - x0) * (H - 1 - y0) / (y1 - y0)); y = H - 1; }
    else if (code & 4) { x = x0 + (int)((long)(x1 - x0) * (0 - y0) / (y1 - y0)); y = 0; }
    else if (code & 2) { y = y0 + (int)((long)(y1 - y0) * (W - 1 - x0) / (x1 - x0)); x = W - 1; }
    else               { y = y0 + (int)((long)(y1 - y0) * (0 - x0) / (x1 - x0)); x = 0; }
    if (code == code0) { x0 = x; y0 = y; code0 = face_outcode(x0, y0); }
    else               { x1 = x; y1 = y; code1 = face_outcode(x1, y1); }
  }
}

inline void dCLine(int x0, int y0, int x1, int y1) {
  cline(x0 + gOfsX, y0 + gOfsY, x1 + gOfsX, y1 + gOfsY);
}