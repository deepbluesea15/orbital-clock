#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class BounceFace final : public ClockFace {
 public:
  void draw() override;
 private:
  static float x_, y_, vx_, vy_;
  static uint16_t hits_, corners_;
};
