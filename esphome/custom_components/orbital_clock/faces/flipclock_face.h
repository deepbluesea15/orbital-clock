#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class FlipclockFace final : public ClockFace {
 public:
  static void flip_tile(int x, int y, int w, int h, char now, char old, float progress);
  void draw() override;
 private:
  static constexpr uint16_t FLIP_MS = 320;
  static uint8_t previous_[4];
  static uint32_t flip_at_[4];
};
