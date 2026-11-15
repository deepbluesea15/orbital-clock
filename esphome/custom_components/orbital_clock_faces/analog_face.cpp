#include "analog_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: ANALOG -------------------------------------
void AnalogFace::draw() {
  char buf[20];
  const int cx = 32, cy = 32, R = 29;

  dCircle(cx, cy, R);
  for (int i = 0; i < 60; i++) {
    float a = (i * 6 - 90) * 0.01745329f;
    if (i % 5 == 0)
      dCLine((int)(cx + (R - 5) * cosf(a)), (int)(cy + (R - 5) * sinf(a)),
            (int)(cx + (R - 2) * cosf(a)), (int)(cy + (R - 2) * sinf(a)));
    else
      dPixel((int)(cx + (R - 3) * cosf(a)), (int)(cy + (R - 3) * sinf(a)));
  }

  float secF  = tnow.tm_sec + subSecond();
  float minF  = tnow.tm_min + secF / 60.0f;
  float hourF = (tnow.tm_hour % 12) + minF / 60.0f;
  float ah = (hourF * 30.0f - 90.0f) * 0.01745329f;
  float am = (minF * 6.0f - 90.0f) * 0.01745329f;
  float as = (secF * 6.0f - 90.0f) * 0.01745329f;

  // Tapered hands: a plain line at this size reads as a scratch, and a thick
  // one swallows the dial. A narrow triangle gives it a direction.
  for (int k = 0; k < 2; k++) {
    float a  = k ? am : ah;
    float ln = k ? 23.0f : 15.0f;
    float wd = k ? 2.0f : 3.0f;
    int tx = (int)(cx + ln * cosf(a)), ty = (int)(cy + ln * sinf(a));
    int px = (int)(-sinf(a) * wd),     py = (int)(cosf(a) * wd);
    u8g2.drawTriangle(tx + gOfsX, ty + gOfsY,
                      cx + px + gOfsX, cy + py + gOfsY,
                      cx - px + gOfsX, cy - py + gOfsY);
  }
  dCLine((int)(cx - 5 * cosf(as)), (int)(cy - 5 * sinf(as)),
        (int)(cx + (R - 4) * cosf(as)), (int)(cy + (R - 4) * sinf(as)));
  dDisc(cx, cy, 2);

  dVLine(66, 3, 58);
  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "%s %02d %s", DOW[tnow.tm_wday % 7],
           tnow.tm_mday, MON[tnow.tm_mon % 12]);
  dStr(71, 10, buf);

  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d", h, tnow.tm_min);
  u8g2.setFont(u8g2_font_logisoso16_tn);
  dStr(71, 34, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), ":%02d%s", tnow.tm_sec,
           use24h ? "" : (tnow.tm_hour < 12 ? " AM" : " PM"));
  dStr(71, 46, buf);

  u8g2.setFont(u8g2_font_4x6_tr);
  if (wthr.valid) snprintf(buf, sizeof(buf), "%d%c OUT",
                           (int)lroundf(wthr.tempF), cfg.metric ? 'C' : 'F');
  else            snprintf(buf, sizeof(buf), "-- OUT");
  dStr(71, 58, buf);
}
