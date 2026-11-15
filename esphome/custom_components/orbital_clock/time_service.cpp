#include "time_service.h"

#include <Arduino.h>
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"

esphome::time::RealTimeClock *TimeService::source_ = nullptr;
struct tm TimeService::now_ = {};
bool TimeService::valid_ = false;
uint8_t TimeService::last_second_ = 61;
uint32_t TimeService::second_mark_ = 0;

void TimeService::set_source(esphome::time::RealTimeClock *source) {
  TimeService::source_ = source;
}

void TimeService::tick() {
  if (TimeService::source_ == nullptr) {
    TimeService::valid_ = false;
    TimeService::now_ = {};
    return;
  }
  esphome::ESPTime current = TimeService::source_->now();
  TimeService::valid_ = current.is_valid();
  if (!TimeService::valid_) {
    TimeService::now_ = {};
    return;
  }
  TimeService::now_ = current.to_c_tm();
  if (TimeService::now_.tm_sec != TimeService::last_second_) {
    TimeService::last_second_ = TimeService::now_.tm_sec;
    TimeService::second_mark_ = millis();
  }
}

float TimeService::sub_second() {
  float fraction = (millis() - TimeService::second_mark_) / 1000.0f;
  return fraction < 0 ? 0 : (fraction > 1 ? 1 : fraction);
}

float TimeService::stardate() {
  float day_fraction =
      (TimeService::now_.tm_hour * 3600 + TimeService::now_.tm_min * 60 + TimeService::now_.tm_sec) / 86400.0f;
  return (TimeService::now_.tm_year + 1900 - 2000) * 1000.0f +
         ((TimeService::now_.tm_yday + day_fraction) * 1000.0f / 365.0f);
}

int TimeService::hour12(int hour) {
  int result = hour % 12;
  return result == 0 ? 12 : result;
}

bool TimeService::is_valid() { return TimeService::valid_; }

const struct tm &TimeService::now() { return TimeService::now_; }

struct tm TimeService::to_local(time_t timestamp) {
  esphome::ESPTime local = esphome::ESPTime::from_epoch_local(timestamp);
  return local.to_c_tm();
}
