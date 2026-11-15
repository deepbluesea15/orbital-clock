#pragma once

#include <stdint.h>

namespace display_schedule {

constexpr bool is_night(uint32_t now, uint32_t start, uint32_t end) {
  return start < end ? now >= start && now < end
                     : start > end && (now >= start || now < end);
}

constexpr uint8_t contrast(uint8_t percent) {
  return static_cast<uint8_t>(static_cast<uint16_t>(percent) * 255 / 100);
}

constexpr bool rotation_due(uint32_t now, uint32_t last_change, uint32_t interval_ms) {
  return interval_ms != 0 && now - last_change >= interval_ms;
}

}  // namespace display_schedule
