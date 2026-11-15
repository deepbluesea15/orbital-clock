#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"
#include "bounce_motion.h"

class BounceFace final : public ClockFace {
 public:
  void draw() override;
 private:
  static BounceMotion motion_;
};
