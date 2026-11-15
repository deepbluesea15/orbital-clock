#include "bounce_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

float BounceFace::x_ = 20, BounceFace::y_ = 20;
float BounceFace::vx_ = 1.15f, BounceFace::vy_ = 0.78f;
uint16_t BounceFace::hits_ = 0, BounceFace::corners_ = 0;
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

  x_ += vx_; y_ += vy_;
  bool hx = false, hy = false;
  if (x_ < 0)      { x_ = 0;      vx_ = -vx_; hx = true; }
  if (x_ > W - bw) { x_ = W - bw; vx_ = -vx_; hx = true; }
  if (y_ < 0)      { y_ = 0;      vy_ = -vy_; hy = true; }
  if (y_ > H - bh) { y_ = H - bh; vy_ = -vy_; hy = true; }
  if (hx || hy) hits_++;
  if (hx && hy) corners_++;

  u8g2.drawRFrame((int)x_, (int)y_, bw, bh, 4);
  u8g2.drawStr((int)x_ + tx, (int)y_ + 13, t);

  // AM/PM tucked under the box, only when it would mean something
  if (!use24h) {
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr((int)x_ + bw - 12, (int)y_ + bh - 1, tnow.tm_hour < 12 ? "AM" : "PM");
  }

  char b[28];
  snprintf(b, sizeof(b), "HITS %u   CORNERS %u", hits_, corners_);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setDrawColor(2);                 // XOR so it survives the box passing over
  u8g2.drawStr(2, 62, b);
  u8g2.setDrawColor(1);
}
