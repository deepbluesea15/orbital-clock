#include "bounce_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

BounceMotion BounceFace::motion_;
// ------------------------- FACE 8: BOUNCE -----------------------------------
void BounceFace::draw() {
  // The time rides in the box. The box is sized to the widest possible string
  // and the digits centred inside it: measuring the live string instead would
  // resize the box when 9 o'clock becomes 10, which can shove it through a
  // wall and score a bounce that never happened.
  char t[12];
  int hr = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(t, sizeof(t), use24h ? "%02d:%02d:%02d" : "%d:%02d:%02d",
           hr, tnow.tm_min, tnow.tm_sec);

  u8g2.setFont(u8g2_font_6x12_tf);
  const int bw = u8g2.getStrWidth("00:00:00") + 13;
  const int bh = 18;
  const int tx = (bw - u8g2.getStrWidth(t)) / 2;

  motion_.step(W - bw, H - bh, static_cast<uint32_t>(random(0x7fffffffL)));

  u8g2.drawRFrame((int)motion_.x(), (int)motion_.y(), bw, bh, 4);
  u8g2.drawStr((int)motion_.x() + tx, (int)motion_.y() + 13, t);

  // AM/PM tucked under the box, only when it would mean something
  if (!use24h) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr((int)motion_.x() + bw - 12, (int)motion_.y() + bh - 1, tnow.tm_hour < 12 ? "AM" : "PM");
  }

  char b[28];
  snprintf(b, sizeof(b), "HITS %u   CORNERS %u",
           static_cast<unsigned>(motion_.hits()), static_cast<unsigned>(motion_.corners()));
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setDrawColor(2);                 // XOR so it survives the box passing over
  u8g2.drawStr(2, 62, b);
  u8g2.setDrawColor(1);
}
