#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class RainFace final : public ClockFace {
 public:
  static constexpr int COLUMN_COUNT = 21;
  static constexpr int ROW_COUNT = 10;
  static void initialize();
  static char glyph(int column, int row);
  void draw() override;
 private:
  static float y_[COLUMN_COUNT], velocity_[COLUMN_COUNT];
  static uint8_t length_[COLUMN_COUNT];
  static uint32_t seed_, seed_at_;
  static const char GLYPHS[];
};
