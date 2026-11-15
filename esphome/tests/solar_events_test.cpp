#include "../custom_components/orbital_clock_faces/solar_events.h"

constexpr bool ordinary_day() {
  SolarEvents events;
  events.add_rise(100, 150);
  events.add_set(200, 150);
  const auto period = events.period(true, 0);
  return period.valid() && period.rise == 100 && period.set == 200 && period.next_transition == 200;
}

constexpr bool night_with_sunset_before_next_sunrise() {
  SolarEvents events;
  events.add_rise(100, 250);
  events.add_set(200, 250);
  events.add_rise(310, 250);
  events.add_set(410, 250);
  const auto period = events.period(false, 150);
  return period.valid() && period.rise == 100 && period.set == 200 && period.next_transition == 310;
}

constexpr bool polar_transition_without_a_sunset() {
  SolarEvents events;
  events.add_rise(100, 150);
  return events.any() && !events.period(true, 0).valid();
}

constexpr bool before_sunrise_uses_upcoming_day() {
  SolarEvents events;
  events.add_rise(100, 250);
  events.add_set(200, 250);
  events.add_rise(310, 250);
  events.add_set(410, 250);
  const auto period = events.period(false, 240);
  return period.valid() && period.rise == 310 && period.set == 410 && period.next_transition == 310;
}

constexpr bool nearest_events_win() {
  SolarEvents events;
  events.add_rise(50, 150);
  events.add_rise(100, 150);
  events.add_rise(400, 150);
  events.add_rise(300, 150);
  return events.previous_rise == 100 && events.next_rise == 300;
}

static_assert(ordinary_day());
static_assert(night_with_sunset_before_next_sunrise());
static_assert(polar_transition_without_a_sunset());
static_assert(before_sunrise_uses_upcoming_day());
static_assert(nearest_events_win());
