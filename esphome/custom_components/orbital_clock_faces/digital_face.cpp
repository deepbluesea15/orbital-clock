#include "digital_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: DIGITAL (plain) ----------------------------
// Deliberately undecorated: no starfield, no glitch, just the time.
void DigitalFace::draw() {
  char buf[24];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, tnow.tm_min, tnow.tm_sec);

  u8g2.setFont(u8g2_font_logisoso20_tn);
  int w = strW(buf);
  dStr((W - w) / 2, 42, buf);

  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(buf, sizeof(buf), "%s %02d %s %d", DOW[tnow.tm_wday % 7], tnow.tm_mday,
           MON[tnow.tm_mon % 12], tnow.tm_year + 1900);
  w = strW(buf);
  dStr((W - w) / 2, 15, buf);

  if (!use24h) {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char *ap = tnow.tm_hour < 12 ? "AM" : "PM";
    dStr((W - strW(ap)) / 2, 58, ap);
  }
}
