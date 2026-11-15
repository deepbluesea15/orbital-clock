#include "solar_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: SOLAR --------------------------------------
// Sunrise and sunset from the NOAA low-precision equations, using the lat/lon
// already stored for weather. Good to about a minute, which is finer than the
// arc can show anyway.
bool sunTimes(double lat, double lon, time_t when, time_t *rise, time_t *set) {
  const double D2R = 0.01745329251994;
  double jd = (double)when / 86400.0 + 2440587.5;
  double n  = floor(jd - 2451545.0 + 0.0008 + 0.5);
  double Js = n - lon / 360.0;
  double M  = fmod(357.5291 + 0.98560028 * Js, 360.0);
  double Mr = M * D2R;
  double C  = 1.9148 * sin(Mr) + 0.0200 * sin(2 * Mr) + 0.0003 * sin(3 * Mr);
  double lam = fmod(M + C + 180.0 + 102.9372, 360.0);
  double lamr = lam * D2R;
  double Jtr = 2451545.0 + Js + 0.0053 * sin(Mr) - 0.0069 * sin(2 * lamr);
  double sinDec = sin(lamr) * sin(23.44 * D2R);
  double dec = asin(sinDec);
  double cosW = (sin(-0.833 * D2R) - sin(lat * D2R) * sinDec)
              / (cos(lat * D2R) * cos(dec));
  if (cosW > 1.0 || cosW < -1.0) return false;      // polar day or polar night
  double w = acos(cosW) / D2R;
  *rise = (time_t)((Jtr - w / 360.0 - 2440587.5) * 86400.0);
  *set  = (time_t)((Jtr + w / 360.0 - 2440587.5) * 86400.0);
  return true;
}

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

  time_t nowT = time(nullptr), rise, set;
  bool ok = sunTimes(cfg.lat, cfg.lon, nowT, &rise, &set);

  const int cx = 64, cy = 54, rx = 56, ry = 22;
  for (int a = 180; a <= 360; a += 3) {
    float r = a * 0.01745329f;
    dPixel(cx + (int)(rx * cosf(r)), cy + (int)(ry * sinf(r)));
  }
  dHLine(2, 54, 124);

  if (!ok) {                                     // sun never rises or never sets
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = (cfg.lat > 0) == (tnow.tm_mon >= 3 && tnow.tm_mon <= 8)
                    ? "MIDNIGHT SUN" : "POLAR NIGHT";
    dStr((W - strW(m)) / 2, 36, m);
    return;
  }

  double frac = (double)(nowT - rise) / (double)(set - rise);
  bool day = (frac >= 0.0 && frac <= 1.0);
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
