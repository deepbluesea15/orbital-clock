#pragma once

#include <time.h>

struct SolarPeriod {
  time_t rise;
  time_t set;
  time_t next_transition;

  constexpr bool valid() const { return rise != 0 && set > rise && next_transition != 0; }
};

struct SolarEvents {
  time_t previous_rise = 0;
  time_t next_rise = 0;
  time_t previous_set = 0;
  time_t next_set = 0;

  constexpr void add_rise(time_t event, time_t now) { record(event, now, previous_rise, next_rise); }
  constexpr void add_set(time_t event, time_t now) { record(event, now, previous_set, next_set); }
  constexpr bool any() const { return previous_rise || next_rise || previous_set || next_set; }

  constexpr SolarPeriod period(bool day, time_t local_midnight) const {
    if (day) return {previous_rise, next_set, next_set};
    if (previous_rise && previous_set > previous_rise && previous_set >= local_midnight)
      return {previous_rise, previous_set, next_rise};
    return {next_rise, next_set, next_rise};
  }

 private:
  static constexpr void record(time_t event, time_t now, time_t &previous, time_t &next) {
    if (event <= now) {
      if (event > previous) previous = event;
    } else if (!next || event < next) {
      next = event;
    }
  }
};
