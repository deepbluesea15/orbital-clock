#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class TerminalFace final : public ClockFace {
 public:
  static constexpr uint8_t LOG_ROWS = 7;
  static void initialize();
  static void logPush(const char *text);
  static void logRandom();
  static void boot_line(const char *text);
  static void draw_boot();
  void draw() override;
 private:
  static char logBuf[LOG_ROWS][30];
  static uint32_t lastLogPush;
};
