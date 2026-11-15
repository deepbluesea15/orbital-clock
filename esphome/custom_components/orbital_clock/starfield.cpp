#include "starfield.h"

#include <Arduino.h>

extern const int W;
extern const int H;

void Starfield::initialize() {
  for (auto &star : this->stars_) {
    star.x = random(W);
    star.y = random(H);
    star.velocity = 0.12f + (random(100) / 100.0f) * 1.1f;
  }
}

void Starfield::step() {
  for (auto &star : this->stars_) {
    star.x -= star.velocity;
    if (star.x < 0) {
      star.x = W - 1;
      star.y = random(H);
    }
  }
}

void Starfield::draw(U8G2 &display, int max_y) const {
  for (const auto &star : this->stars_) {
    int x = static_cast<int>(star.x);
    if (star.y >= max_y) continue;
    display.drawPixel(x, star.y);
    if (star.velocity > 0.85f) display.drawPixel((x + 1) & (W - 1), star.y);
  }
}
