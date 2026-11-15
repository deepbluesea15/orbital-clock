#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class JellyfishFace final : public ClockFace {
 public:
  struct Jelly { float x, y, vx, phase; int r; };
  struct Bubble { float x, y, v, sway; };
  static constexpr uint8_t BUBBLE_COUNT = 10;
  static void initialize();
  void draw() override;
 private:
  static Jelly jelly_[3];
  static Bubble bubbles_[BUBBLE_COUNT];
};
