#pragma once

class ClockRuntime {
 public:
  static void setup();
  static void loop();
};

void orbital_sketch_setup();
void orbital_sketch_loop();
void orbital_set_face(const char *name);
void orbital_set_time_format(const char *format);
void orbital_set_units(const char *units);
void orbital_set_timezone(const char *zone);
void orbital_set_city(const char *city);
const char *orbital_current_face();
const char *orbital_current_time_format();
const char *orbital_current_units();
const char *orbital_current_city();
void orbital_reset_settings();