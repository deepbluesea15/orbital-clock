#pragma once

#include <stdint.h>

enum class LocationStatus : uint8_t { PENDING, NOT_FOUND, ERROR };

constexpr const char *location_message(bool has_city, bool connected, LocationStatus status) {
  if (!has_city) return "NO LOCATION SET";
  if (!connected) return "NO NETWORK";
  if (status == LocationStatus::NOT_FOUND) return "LOCATION NOT FOUND";
  if (status == LocationStatus::ERROR) return "LOCATION ERROR";
  return "LOCATING";
}
