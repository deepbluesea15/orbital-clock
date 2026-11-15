#include "orrery_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: ORRERY -------------------------------------
// Real heliocentric positions: each planet's J2000 mean longitude plus one
// turn per orbital period. Mean elements only, no eccentricity, which runs a
// couple of degrees off the truth and is indistinguishable at this size.
// Orbits are drawn as an inclined view so the ellipses use the panel's width.
struct Planet {
  const char *name; float period; float l0; float peri; float ecc;
  uint8_t rx; uint8_t rad;
};
static const Planet PLAN[] = {
  { "MERCURY",    87.969f, 252.25084f,  77.45645f, 0.205630f, 12, 1 },
  { "VENUS",     224.701f, 181.97973f, 131.60246f, 0.006773f, 19, 1 },
  { "EARTH",     365.256f, 100.46435f, 102.94719f, 0.016709f, 27, 2 },
  { "MARS",      686.980f, 355.45332f, 336.04084f, 0.093405f, 35, 1 },
  { "JUPITER",  4332.589f,  34.40438f,  14.72847f, 0.048498f, 46, 3 },
  { "SATURN",  10759.220f,  49.94432f,  92.59887f, 0.055546f, 58, 2 },
};

// Mean longitude alone treats every orbit as a circle, which puts Mercury 24
// degrees from where it actually is and Mars 11. The equation of center is
// three sine terms and brings the whole set inside a degree.
static float planetLon(const Planet &pl, double d) {
  double L = pl.l0 + 360.0 * d / (double)pl.period;
  double M = radians(fmod(L - pl.peri, 360.0));
  double e = pl.ecc;
  double C = (2 * e - e * e * e / 4) * sin(M)
           + 1.25 * e * e * sin(2 * M)
           + (13.0 / 12.0) * e * e * e * sin(3 * M);
  double lon = fmod(L + degrees(C), 360.0);
  if (lon < 0) lon += 360.0;
  return (float)lon;
}
static const int PLAN_N = sizeof(PLAN) / sizeof(PLAN[0]);
static const float ORR_TILT = 0.36f;         // vertical squash; keeps Saturn clear of the footer

void OrreryFace::draw() {
  char buf[30];
  const int cx = 64, cy = 34;

  double days = ((double)time(nullptr) - 946728000.0) / 86400.0;   // since J2000
  int sel = (int)((millis() / 2600) % PLAN_N);                     // cycling callout

  // dotted orbit rings -- engraved look, and far less lit area than solid
  for (int i = 0; i < PLAN_N; i++) {
    float rx = PLAN[i].rx, ry = PLAN[i].rx * ORR_TILT;
    for (int a = 0; a < 360; a += 12) {
      float r = a * 0.01745329f;
      dPixel(cx + (int)(cosf(r) * rx), cy + (int)(sinf(r) * ry));
    }
  }

  // the sun
  dDisc(cx, cy, 2);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    dPixel(cx + (int)(cosf(a) * 5), cy + (int)(sinf(a) * 5 * ORR_TILT));
  }

  for (int i = 0; i < PLAN_N; i++) {
    float r = planetLon(PLAN[i], days) * 0.01745329f;
    int x = cx + (int)(cosf(r) * PLAN[i].rx);
    int y = cy + (int)(sinf(r) * PLAN[i].rx * ORR_TILT);
    dDisc(x, y, PLAN[i].rad);
    if (i == sel) dCircle(x, y, PLAN[i].rad + 3);   // tie the dot to the label
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(0, 7, "ORRERY");
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           tnow.tm_year + 1900, tnow.tm_mon + 1, tnow.tm_mday);
  dStrRight(W, 7, buf);

  float lon = planetLon(PLAN[sel], days);
  snprintf(buf, sizeof(buf), "%s  %3d  %.0fd",
           PLAN[sel].name, (int)lon, PLAN[sel].period);
  dStr(0, 63, buf);
}
