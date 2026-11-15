#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class DecryptFace final : public ClockFace {
 public:
  static char glyph(int slot, uint32_t tick);
  void draw() override;
 private:
  static uint8_t previous_[6];
  static uint32_t lock_until_[6];
};
