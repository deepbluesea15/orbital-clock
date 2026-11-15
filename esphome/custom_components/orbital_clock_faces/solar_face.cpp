#include "solar_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/weather_service.h"
#include "esphome/components/sun/sun.h"
#include "esphome/core/time.h"

namespace {
constexpr double SUNRISE_ELEVATION = -0.83333;
constexpr time_t SECONDS_PER_DAY = 86400;
}

void SolarFace::update_events(time_t now) {
  if (cached_ && now / 60 == cached_minute_ && now >= last_update_ &&
      cfg.lat == cached_lat_ && cfg.lon == cached_lon_ &&
      (!events_.next_rise || now < events_.next_rise) &&
      (!events_.next_set || now < events_.next_set)) {
    return;
  }

  auto *sun = WeatherService::sun();
  events_ = {};
  const time_t utc_midnight = now - now % SECONDS_PER_DAY;
  // Solar events can fall on an adjacent UTC date, especially near the date line.
  for (int day = -2; day <= 2; day++) {
    const auto reference = esphome::ESPTime::from_epoch_utc(utc_midnight + day * SECONDS_PER_DAY);
    auto rise = sun->sunrise(reference, SUNRISE_ELEVATION);
    auto set = sun->sunset(reference, SUNRISE_ELEVATION);
    if (rise.has_value()) events_.add_rise(rise->timestamp, now);
    if (set.has_value()) events_.add_set(set->timestamp, now);
  }
  day_ = sun->is_above_horizon(SUNRISE_ELEVATION);
  cached_minute_ = now / 60;
  last_update_ = now;
  cached_lat_ = cfg.lat;
  cached_lon_ = cfg.lon;
  cached_ = true;
}

void SolarFace::draw() {
  char buf[26];
  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(0, 6, "SOLAR");
  dStrRight(W, 6, cityLabel());
  dDashed(0, 9, W);

  if (!cfg.haveLoc) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = WeatherService::location_message();
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  auto *sun = WeatherService::sun();
  if (sun == nullptr) {
    const char *m = "SUN DATA UNAVAILABLE";
    u8g2.setFont(u8g2_font_6x10_tf);
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  time_t nowT = time(nullptr);
  this->update_events(nowT);

  const int cx = 64, cy = 54, rx = 56, ry = 22;
  for (int a = 180; a <= 360; a += 3) {
    float r = a * 0.01745329f;
    dPixel(cx + (int)(rx * cosf(r)), cy + (int)(ry * sinf(r)));
  }
  dHLine(2, 54, 124);

  auto local_midnight = esphome::ESPTime::from_epoch_local(nowT);
  local_midnight.hour = local_midnight.minute = local_midnight.second = 0;
  local_midnight.recalc_timestamp_local();
  const SolarPeriod period = events_.period(day_, local_midnight.timestamp);
  if (!period.valid()) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = events_.any() ? (day_ ? "SUN ABOVE HORIZON" : "SUN BELOW HORIZON")
                                  : (day_ ? "MIDNIGHT SUN" : "POLAR NIGHT");
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  time_t rise = period.rise;
  time_t set = period.set;
  double frac = (double)(nowT - rise) / (double)(set - rise);
  bool day = day_;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  float a = (180.0f + 180.0f * (float)frac) * 0.01745329f;
  int sx = cx + (int)(rx * cosf(a)), sy = cy + (int)(ry * sinf(a));
  if (day) {
    dDisc(sx, sy, 3);
    for (int i = 0; i < 8; i++) {
      float t = i * 0.785398f;
      dCLine(sx + (int)(5 * cosf(t)), sy + (int)(5 * sinf(t)),
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

  long until = static_cast<long>(period.next_transition - nowT);
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
