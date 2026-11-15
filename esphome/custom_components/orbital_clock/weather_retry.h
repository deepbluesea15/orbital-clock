#pragma once

#include <stdint.h>

namespace weather_retry {

constexpr uint32_t INITIAL_MS = 60000;
constexpr uint32_t MAX_MS = 900000;

constexpr uint32_t next_delay(uint32_t delay) {
  return delay >= MAX_MS / 2 ? MAX_MS : delay * 2;
}

constexpr uint32_t jittered_delay(uint32_t delay, uint32_t random_value) {
  if (delay >= MAX_MS) return MAX_MS - random_value % (MAX_MS / 10 + 1);
  const uint32_t jitter = random_value % (delay / 10 + 1);
  return delay + jitter > MAX_MS ? MAX_MS : delay + jitter;
}

}  // namespace weather_retry
