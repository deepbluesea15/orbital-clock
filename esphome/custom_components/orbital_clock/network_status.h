#pragma once

#include <stdint.h>

class NetworkStatus {
 public:
  static bool is_connected();
  static int8_t rssi();
  static int32_t channel();
};
