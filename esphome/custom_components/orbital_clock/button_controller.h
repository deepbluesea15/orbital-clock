#pragma once

#include <stdint.h>

class ButtonController {
 public:
  void handle();
  void draw_reset_overlay();

 private:
  void next_face();
  void toggle_time_format();
  void factory_reset();
  uint32_t held_ms() const;

  bool button_down_{false};
  bool long_fired_{false};
  uint32_t button_down_at_{0};
};
