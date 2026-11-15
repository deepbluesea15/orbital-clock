#include "orbit_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE 2: ORBIT ------------------------------------
static void polar(int cx, int cy, float deg, int r, int &x, int &y) {
  float a = (deg - 90.0f) * 0.01745329f;
  x = cx + (int)(cosf(a) * r);
  y = cy + (int)(sinf(a) * r);
}
void OrbitFace::draw() {
  char buf[24];
  const int cx = 31, cy = 32;
  int x, y;
  for (uint8_t i = 0; i < 12; i++) {
    polar(cx, cy, i * 30.0f, 29, x, y); dPixel(x, y);
    if (i % 3 == 0) { polar(cx, cy, i * 30.0f, 26, x, y); dPixel(x, y); }
  }
  dCircle(cx, cy, 21);
  dDisc(cx, cy, 4);
  dCircle(cx, cy, 7);

  float secF  = tnow.tm_sec + subSecond();
  float minF  = tnow.tm_min + secF / 60.0f;
  float hourF = (tnow.tm_hour % 12) + minF / 60.0f;
  polar(cx, cy, secF * 6.0f, 28, x, y);   dDisc(x, y, 1);
  polar(cx, cy, minF * 6.0f, 21, x, y);   dDisc(x, y, 2);
  polar(cx, cy, hourF * 30.0f, 14, x, y); dDisc(x, y, 3);
  dCircle(x, y, 5);

  dVLine(63, 2, 60);
  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%s %02d.%02d", DOW[tnow.tm_wday % 7],
           tnow.tm_mon + 1, tnow.tm_mday);
  dStr(68, 10, buf);

  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d", h, tnow.tm_min);
  u8g2.setFont(u8g2_font_logisoso20_tn);
  dStrRight(W - 1, 36, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%02d%s", tnow.tm_sec,
           use24h ? "" : (tnow.tm_hour < 12 ? " AM" : " PM"));
  dStrRight(W - 1, 46, buf);
  snprintf(buf, sizeof(buf), "SD %.1f", stardate());
  dStr(68, 60, buf);
  dDashed(66, 51, 60);
}
