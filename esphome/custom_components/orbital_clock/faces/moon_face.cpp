#include "moon_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: MOON ---------------------------------------
// Phase from the Sun-Moon elongation using the main periodic terms of each
// body's motion. A flat "days since a known new moon" model is simpler but
// drifts up to 7% illumination either way, enough to name the wrong phase
// near a quarter; this holds inside 1.5% and needs nothing from the network.
static const double SYNODIC_M = 29.530588853;

static double moonElong() {                    // degrees, 0 = new, 180 = full
  double d = ((double)time(nullptr) - 946728000.0) / 86400.0;   // since J2000
  double Ls = 280.460 + 0.9856474 * d;
  double Ms = radians(fmod(357.528 + 0.9856003 * d, 360.0));
  double lamS = Ls + 1.915 * sin(Ms) + 0.020 * sin(2 * Ms);
  double Lm = 218.316 + 13.176396 * d;
  double Mm = radians(fmod(134.963 + 13.064993 * d, 360.0));
  double lamM = Lm + 6.289 * sin(Mm);
  double D = fmod(lamM - lamS, 360.0);
  if (D < 0) D += 360.0;
  return D;
}
static const char *moonName(double age) {
  if (age <  1.85) return "NEW MOON";
  if (age <  5.54) return "WAXING CRESCENT";
  if (age <  9.23) return "FIRST QUARTER";
  if (age < 12.91) return "WAXING GIBBOUS";
  if (age < 16.61) return "FULL MOON";
  if (age < 20.30) return "WANING GIBBOUS";
  if (age < 23.99) return "LAST QUARTER";
  if (age < 27.68) return "WANING CRESCENT";
  return "NEW MOON";
}

void MoonFace::draw() {
  char buf[26];
  double D   = moonElong();
  double th  = radians(D);
  double age = D / 360.0 * SYNODIC_M;
  float  lit = (float)((1.0 - cos(th)) * 0.5);

  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(0, 7, "MOON");
  snprintf(buf, sizeof(buf), "%.0f%% LIT", lit * 100.0f);
  dStrRight(W, 7, buf);
  dDashed(0, 10, W);

  // Outline the whole disc, then fill only the sunlit part. The terminator is
  // an ellipse, so per scanline the boundary sits at halfwidth * cos(D).
  const int cx = 29, cy = 37, R = 19;
  dCircle(cx, cy, R);
  for (int y = -R + 1; y <= R - 1; y++) {
    float hw = sqrtf((float)(R * R - y * y));
    float tx = hw * (float)cos(th);
    int xa, xb;
    if (D < 180.0) { xa = (int)ceilf(tx); xb = (int)hw; }            // waxing: lit right
    else           { xa = -(int)hw;       xb = (int)floorf(-tx); }   // waning: lit left
    if (xb >= xa) dHLine(cx + xa, cy + y, xb - xa + 1);
  }

  const int px = 56;
  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(px, 21, moonName(age));

  double perDay = 360.0 / SYNODIC_M;
  double toFull = fmod(180.0 - D + 360.0, 360.0) / perDay;
  double toNew  = fmod(360.0 - D, 360.0) / perDay;

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "AGE %.1fd", age);      dStr(px, 34, buf);
  snprintf(buf, sizeof(buf), "FULL %.1fd", toFull);  dStr(px, 45, buf);
  snprintf(buf, sizeof(buf), "NEW  %.1fd", toNew);   dStr(px, 56, buf);
}
