#include "neon_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE 1: NEON -------------------------------------
void NeonFace::draw() {
  char buf[24];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 6, CALLSIGN);
  dStrRight(W, 6, linkStr());
  dDashed(0, 9, W);

  char hh[4], mm[4];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(hh, sizeof(hh), use24h ? "%02d" : "%d", h);
  snprintf(mm, sizeof(mm), "%02d", tnow.tm_min);

  u8g2.setFont(u8g2_font_logisoso28_tn);
  int wh = strW(hh), wm = strW(mm), gap = 13;
  int x0 = (W - (wh + gap + wm)) / 2 - 6;
  if (x0 < 1) x0 = 1;
  const int base = 44;
  dStr(x0, base, hh);
  dStr(x0 + wh + gap, base, mm);
  if (subSecond() < 0.55f) {
    dBox(x0 + wh + 5, base - 21, 4, 4);
    dBox(x0 + wh + 5, base - 9,  4, 4);
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%02d", tnow.tm_sec);
  int sx = x0 + wh + gap + wm + 4;
  if (sx > W - 12) sx = W - 12;
  dStr(sx, 26, buf);
  if (!use24h) dStr(sx, 44, tnow.tm_hour < 12 ? "AM" : "PM");

  snprintf(buf, sizeof(buf), "%s %s %02d", DOW[tnow.tm_wday % 7],
           MON[tnow.tm_mon % 12], tnow.tm_mday);
  dStr(0, 53, buf);
  snprintf(buf, sizeof(buf), "SD%.0f", stardate());
  dStrRight(W, 53, buf);

  dFrame(0, 56, W, 6);
  float p = (tnow.tm_sec + subSecond()) / 60.0f;
  int fill = (int)(p * (W - 4));
  if (fill > 0) dBox(2, 58, fill, 2);
}
