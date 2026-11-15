#include "jellyfish_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

JellyfishFace::Jelly JellyfishFace::jelly_[3] = {};
JellyfishFace::Bubble JellyfishFace::bubbles_[JellyfishFace::BUBBLE_COUNT] = {};
// ------------------------- FACE 7: JELLYFISH --------------------------------
void JellyfishFace::initialize() {
  for (uint8_t i = 0; i < 3; i++) {
    jelly_[i].r     = 8 + i * 3;
    jelly_[i].x     = 20 + i * 42;
    jelly_[i].y     = 16 + (i % 2) * 12;
    jelly_[i].vx    = (i % 2 ? 0.22f : -0.16f);
    jelly_[i].phase = i * 2.1f;
  }
  for (uint8_t i = 0; i < BUBBLE_COUNT; i++) {
    bubbles_[i].x = random(4, W - 4);
    bubbles_[i].y = random(4, H - 4);
    bubbles_[i].v = 0.18f + random(60) / 200.0f;
    bubbles_[i].sway = random(628) / 100.0f;
  }
}
void JellyfishFace::draw() {
  float t = millis() * 0.001f;

  for (uint8_t i = 0; i < BUBBLE_COUNT; i++) {
    bubbles_[i].y -= bubbles_[i].v;
    if (bubbles_[i].y < 3) { bubbles_[i].y = H - 4; bubbles_[i].x = random(4, W - 4); }
    int bx = (int)(bubbles_[i].x + sinf(t * 1.6f + bubbles_[i].sway) * 2.0f);
    if (bx > 1 && bx < W - 2) u8g2.drawCircle(bx, (int)bubbles_[i].y, 1);
  }

  for (uint8_t i = 0; i < 3; i++) {
    Jelly &j = jelly_[i];
    j.x += j.vx;
    if (j.x < j.r + 2)     { j.x = j.r + 2;     j.vx = -j.vx; }
    if (j.x > W - j.r - 2) { j.x = W - j.r - 2; j.vx = -j.vx; }

    int cx = (int)j.x;
    int cy = (int)(j.y + sinf(t * 1.25f + j.phase) * 3.0f);
    float pulse = 1.0f + 0.14f * sinf(t * 2.4f + j.phase);
    int r = (int)(j.r * pulse);

    // bell: upper dome plus a scalloped rim
    u8g2.drawCircle(cx, cy, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    for (int k = -r + 1; k <= r - 1; k += 3)
      u8g2.drawPixel(cx + k, cy + (((k + r) / 3) % 2 ? 1 : 0));

    // tentacles, each a chain of segments with a travelling wave
    for (int k = 0; k < 4; k++) {
      float off = (k - 1.5f) * (r * 0.5f);
      int lx = cx + (int)off, ly = cy + 2;
      for (int s = 1; s <= 5; s++) {
        int ny = cy + 2 + s * 3;
        int nx = cx + (int)(off + sinf(t * 2.3f + j.phase + s * 0.7f + k * 1.3f) * (s * 0.7f));
        cline(lx, ly, nx, ny);
        lx = nx; ly = ny;
      }
    }
  }
}
