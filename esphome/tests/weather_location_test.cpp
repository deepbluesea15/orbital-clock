#include "../custom_components/orbital_clock/weather_location.h"

constexpr bool equal(const char *a, const char *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

static_assert(equal(location_message(false, true, LocationStatus::PENDING), "NO LOCATION SET"));
static_assert(equal(location_message(true, false, LocationStatus::ERROR), "NO NETWORK"));
static_assert(equal(location_message(true, true, LocationStatus::PENDING), "LOCATING"));
static_assert(equal(location_message(true, true, LocationStatus::NOT_FOUND), "LOCATION NOT FOUND"));
static_assert(equal(location_message(true, true, LocationStatus::ERROR), "LOCATION ERROR"));
