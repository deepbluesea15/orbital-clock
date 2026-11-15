#include "rain_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

float RainFace::y_[RainFace::COLUMN_COUNT] = {};
float RainFace::velocity_[RainFace::COLUMN_COUNT] = {};
uint8_t RainFace::length_[RainFace::COLUMN_COUNT] = {};
uint32_t RainFace::seed_ = 1, RainFace::seed_at_ = 0;
const char RainFace::GLYPHS[] = "0123456789ABCDEF<>/\\|=+*#$%&";
// ------------------------- FACE: RAIN ---------------------------------------
void RainFace::initialize() {
  for (int c = 0; c < COLUMN_COUNT; c++) {
    y_[c] = -(float)random(0, 30);
    velocity_[c] = 0.10f + random(70) / 300.0f;
    length_[c] = 4 + random(9);
  }
}
char RainFace::glyph(int c, int r) {
  uint32_t hsh = (uint32_t)c * 73856093u ^ (uint32_t)r * 19349663u ^ seed_;
  hsh ^= hsh >> 13; hsh *= 2654435761u; hsh ^= hsh >> 16;
  return GLYPHS[hsh % (sizeof(GLYPHS) - 1)];
}
void RainFace::draw() {
  if (millis() - seed_at_ > 130) { seed_at_ = millis(); seed_ = random(1, 100000); }
  u8g2.setFont(u8g2_font_4x6_tr);
  char g[2] = { 0, 0 };

  for (int c = 0; c < COLUMN_COUNT; c++) {
    y_[c] += velocity_[c];
    int head = (int)y_[c];
    if (head - length_[c] > ROW_COUNT) {
      y_[c] = -(float)random(0, 14);
      velocity_[c] = 0.10f + random(70) / 300.0f;
      length_[c] = 4 + random(9);
      continue;
    }
    int x = c * 6 + 1;
    for (int k = 0; k < length_[c]; k++) {
      int r = head - k;
      if (r < 0 || r >= ROW_COUNT) continue;
      int yb = 7 + r * 6;
      g[0] = glyph(c, r);
      if (k == 0) {                       // bright head
        u8g2.drawBox(x - 1, yb - 6, 6, 7);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x, yb, g);
        u8g2.setDrawColor(1);
      } else if (k < 3 || (k & 1) == 0) { // thinning tail
        u8g2.drawStr(x, yb, g);
      }
    }
  }

  // time panel punched through the rain
  char buf[12], sec[4];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d", h, tnow.tm_min);
  snprintf(sec, sizeof(sec), "%02d", tnow.tm_sec);

  const int px = 14, py = 19, pw = 100, ph = 27;
  u8g2.setDrawColor(0); u8g2.drawBox(px, py, pw, ph);
  u8g2.setDrawColor(1); u8g2.drawFrame(px, py, pw, ph);
  u8g2.drawHLine(px + 3, py + 2, pw - 6);

  u8g2.setFont(u8g2_font_logisoso20_tn);
  int wt = strW(buf);
  u8g2.setFont(u8g2_font_5x7_tf);
  int ws2 = strW(sec);
  int total = wt + 3 + ws2;
  int tx = px + (pw - total) / 2;
  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.drawStr(tx, py + 24, buf);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(tx + wt + 3, py + 24, sec);
}
