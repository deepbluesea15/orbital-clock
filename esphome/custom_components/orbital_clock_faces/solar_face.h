#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"
#include "solar_events.h"

class SolarFace final : public ClockFace {
 public:
  void draw() override;

 private:
  void update_events(time_t now);
  SolarEvents events_;
  time_t cached_minute_ = 0;
  time_t last_update_ = 0;
  float cached_lat_ = 0;
  float cached_lon_ = 0;
  bool cached_ = false;
  bool day_ = false;
};
