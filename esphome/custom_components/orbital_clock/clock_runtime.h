#pragma once

#include <stdint.h>

class ClockRuntime {
 public:
  static void setup();
  static void loop();
  static void set_display_settings(uint8_t day_brightness, uint8_t night_brightness,
                                   uint32_t night_start, uint32_t night_end, uint32_t rotation_minutes);
};

void orbital_sketch_setup();
void orbital_sketch_loop();
void orbital_set_face(const char *name);
void orbital_set_time_format(const char *format);
void orbital_next_face();
void orbital_toggle_time_format();
void orbital_set_units(const char *units);
void orbital_set_timezone(const char *zone);
void orbital_set_city(const char *city);
void orbital_button_pressed();
void orbital_button_released();
void orbital_button_format_hold();
bool orbital_button_factory_reset();
const char *orbital_current_face();
const char *orbital_current_time_format();
const char *orbital_current_units();
const char *orbital_current_city();
void orbital_reset_settings();