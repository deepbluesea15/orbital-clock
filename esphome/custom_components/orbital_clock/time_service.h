#pragma once

#include <time.h>

namespace esphome {
namespace time {
class RealTimeClock;
}
}  // namespace esphome

class TimeService {
 public:
  static void set_source(esphome::time::RealTimeClock *source);
  static void tick();
  static float sub_second();
  static float stardate();
  static int hour12(int hour);
  static bool is_valid();
  static const struct tm &now();
  static struct tm to_local(time_t timestamp);

 private:
  static esphome::time::RealTimeClock *source_;
  static struct tm now_;
  static bool valid_;
  static uint8_t last_second_;
  static uint32_t second_mark_;
};

