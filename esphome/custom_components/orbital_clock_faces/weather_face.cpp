#include "weather_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/network_status.h"
#include "esphome/components/orbital_clock/weather_service.h"
#include "esphome/components/orbital_clock/weather_icons.h"

// weather icon drawing helpers
static void wxDrawSun(int cx, int cy, int r) {
  u8g2.drawDisc(cx, cy, r);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    int x1 = cx + (int)(cosf(a) * (r + 2)), y1 = cy + (int)(sinf(a) * (r + 2));
    int x2 = cx + (int)(cosf(a) * (r + 5)), y2 = cy + (int)(sinf(a) * (r + 5));
    cline(x1, y1, x2, y2);
  }
}

static void wxDrawMoon(int cx, int cy, int r) {
  u8g2.drawDisc(cx, cy, r);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(cx + r - 2, cy - 2, r);
  u8g2.setDrawColor(1);
}

static void wxDrawIcon(uint8_t ic, int cx, int cy, bool day) {
  float t = millis() * 0.001f;
  switch (ic) {
    case IC_SUN:
      if (day) wxDrawSun(cx, cy, 7); else wxDrawMoon(cx, cy, 8);
      break;
    case IC_PARTLY:
      if (day) wxDrawSun(cx - 4, cy - 7, 4); else wxDrawMoon(cx - 4, cy - 7, 5);
      wxDrawCloud(cx + 3, cy + 5, 6);
      break;
    case IC_CLOUD:
      wxDrawCloud(cx, cy, 8);
      break;
    case IC_FOG:
      wxDrawCloud(cx, cy - 4, 7);
      for (int i = 0; i < 3; i++) {
        int off = (int)(sinf(t * 1.4f + i) * 3.0f);
        u8g2.drawHLine(cx - 9 + off, cy + 7 + i * 4, 18);
      }
      break;
    case IC_RAIN:
      wxDrawCloud(cx, cy - 5, 7);
      for (int i = 0; i < 4; i++) {
        int ph = ((int)(t * 26) + i * 5) % 14;
        int dx = cx - 9 + i * 6;
        cline(dx, cy + 3 + ph, dx - 2, cy + 7 + ph);
      }
      break;
    case IC_SNOW:
      wxDrawCloud(cx, cy - 5, 7);
      for (int i = 0; i < 4; i++) {
        int ph = ((int)(t * 14) + i * 5) % 14;
        int dx = cx - 9 + i * 6, dy = cy + 4 + ph;
        u8g2.drawHLine(dx - 1, dy, 3);
        u8g2.drawVLine(dx, dy - 1, 3);
      }
      break;
    case IC_STORM:
      wxDrawCloud(cx, cy - 5, 7);
      if (fmodf(t, 1.6f) > 0.25f) {
        cline(cx + 1, cy + 3, cx - 4, cy + 10);
        cline(cx - 4, cy + 10, cx + 1, cy + 10);
        cline(cx + 1, cy + 10, cx - 3, cy + 16);
      }
      break;
  }
}

void WeatherFace::draw() {
  char buf[36];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 7, WeatherService::city_label());

  if (!wthr.valid) {
    const char *status = !NetworkStatus::is_connected() ? "NO LINK"
                       : cfg.haveLoc ? "FETCHING"
                       : !cfg.city.length() ? "NO LOCATION"
                       : cfg.location_status == LocationStatus::NOT_FOUND ? "NO MATCH"
                       : cfg.location_status == LocationStatus::ERROR ? "ERROR"
                       : "LOCATING";
    dStrRight(W, 7, status);
    dDashed(0, 10, W);
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = cfg.haveLoc ? "NO WEATHER DATA" : WeatherService::location_message();
    dStr((W - strW(m)) / 2, 34, m);
    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(buf, sizeof(buf), "TRIES %lu  FAILS %lu",
             (unsigned long)wthr.tries, (unsigned long)wthr.fails);
    dStr((W - strW(buf)) / 2, 46, buf);
    return;
  }

  uint32_t ageMin = (millis() - wthr.stamp) / 60000UL;
  if (ageMin == 0) snprintf(buf, sizeof(buf), "NOW");
  else             snprintf(buf, sizeof(buf), "%lum AGO", (unsigned long)ageMin);
  dStrRight(W, 7, buf);
  dDashed(0, 10, W);

  wxDrawIcon(WeatherService::icon(wthr.code), 23, 31, wthr.isDay != 0);

  // temperature, with a hand-drawn degree mark
  snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.tempF));
  u8g2.setFont(u8g2_font_logisoso20_tn);
  dStr(50, 36, buf);
  int tw = strW(buf);
  dCircle(50 + tw + 4, 20, 2);                 // degree mark
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(50 + tw + 8, 24, cfg.metric ? "C" : "F");

  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(50, 47, WeatherService::label(wthr.code));

  u8g2.setFont(u8g2_font_4x6_tr);
  if (wthr.highLowValid) snprintf(buf, sizeof(buf), "H%d L%d", (int)lroundf(wthr.hiF), (int)lroundf(wthr.loF));
  else snprintf(buf, sizeof(buf), "H-- L--");
  dStr(0, 62, buf);
  snprintf(buf, sizeof(buf), "FEELS %d", (int)lroundf(wthr.feelF));
  dStr(44, 62, buf);
  snprintf(buf, sizeof(buf), "%d%s %d%%", (int)lroundf(wthr.wind),
           cfg.metric ? "KMH" : "MPH", (int)lroundf(wthr.hum));
  dStrRight(W, 62, buf);
}
