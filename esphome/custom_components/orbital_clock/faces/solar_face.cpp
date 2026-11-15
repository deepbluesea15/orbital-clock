#include "solar_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/weather_service.h"
#include "esphome/components/sun/sun.h"
#include "esphome/core/time.h"

void SolarFace::draw() {
  char buf[26];
  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(0, 6, "SOLAR");
  dStrRight(W, 6, cityLabel());
  dDashed(0, 9, W);

  if (!cfg.haveLoc) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = "NO LOCATION SET";
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  constexpr double SUNRISE_ELEVATION = -0.83333;
  auto *sun = WeatherService::sun();
  if (sun == nullptr) {
    const char *m = "SUN DATA UNAVAILABLE";
    u8g2.setFont(u8g2_font_6x10_tf);
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  time_t nowT = time(nullptr);
  auto localMidnight = esphome::ESPTime::from_epoch_local(nowT);
  localMidnight.hour = 0;
  localMidnight.minute = 0;
  localMidnight.second = 0;
  localMidnight.recalc_timestamp_local();
  // The Sun API takes a UTC date; use the UTC instant of local midnight to select this local day's events.
  const auto localDayReference = esphome::ESPTime::from_epoch_utc(localMidnight.timestamp);
  auto riseEvent = sun->sunrise(localDayReference, SUNRISE_ELEVATION);
  auto setEvent = sun->sunset(localDayReference, SUNRISE_ELEVATION);

  const int cx = 64, cy = 54, rx = 56, ry = 22;
  for (int a = 180; a <= 360; a += 3) {
    float r = a * 0.01745329f;
    dPixel(cx + (int)(rx * cosf(r)), cy + (int)(ry * sinf(r)));
  }
  dHLine(2, 54, 124);

  if (!riseEvent.has_value() || !setEvent.has_value()) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = sun->is_above_horizon(SUNRISE_ELEVATION) ? "MIDNIGHT SUN" : "POLAR NIGHT";
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  if (setEvent->timestamp <= riseEvent->timestamp) {
    const char *m = "SUN DATA UNAVAILABLE";
    u8g2.setFont(u8g2_font_6x10_tf);
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  time_t rise = riseEvent->timestamp;
  time_t set = setEvent->timestamp;
  double frac = (double)(nowT - rise) / (double)(set - rise);
  bool day = nowT >= rise && nowT <= set;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  float a = (180.0f + 180.0f * (float)frac) * 0.01745329f;
  int sx = cx + (int)(rx * cosf(a)), sy = cy + (int)(ry * sinf(a));
  if (day) {
    dDisc(sx, sy, 3);
    for (int i = 0; i < 8; i++) {
      float t = i * 0.785398f;
      cline(sx + (int)(5 * cosf(t)), sy + (int)(5 * sinf(t)),
            sx + (int)(7 * cosf(t)), sy + (int)(7 * sinf(t)));
    }
    for (int y = sy + 9; y < 54; y += 3) dPixel(sx, y);
  } else {
    dCircle(sx, sy, 3);                          // below the horizon: hollow
  }

  struct tm rt, st;
  rt = TimeService::to_local(rise);
  st = TimeService::to_local(set);
  int rh = use24h ? rt.tm_hour : hour12(rt.tm_hour);
  int sh = use24h ? st.tm_hour : hour12(st.tm_hour);

  long until = day ? (long)(set - nowT) : (long)(rise - nowT);
  if (until < 0) until += 86400;                 // after sunset: tomorrow's rise
  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(buf, sizeof(buf), day ? "SETS IN %ld:%02ld" : "RISES IN %ld:%02ld",
           until / 3600, (until / 60) % 60);
  dStr((W - strW(buf)) / 2, 21, buf);

  long dayLen = (long)(set - rise);
  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%d:%02d", rh, rt.tm_min);
  dStr(1, 62, buf);
  snprintf(buf, sizeof(buf), "DAY %ld:%02ld", dayLen / 3600, (dayLen / 60) % 60);
  dStr((W - strW(buf)) / 2, 62, buf);
  snprintf(buf, sizeof(buf), "%d:%02d", sh, st.tm_min);
  dStrRight(W - 1, 62, buf);
}
