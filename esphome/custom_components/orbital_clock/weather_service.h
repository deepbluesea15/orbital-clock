#pragma once

#include <Arduino.h>

#include "clock_face_context.h"

class WeatherService {
 public:
  static void load();
  static void start();
  static void request();
  static void set_units(const char *units);
  static void set_timezone(const char *zone);
  static void set_city(const char *city);
  static const char *current_units();
  static const char *current_city();
  static const char *city_label();
  static uint8_t icon(int code);
  static const char *label(int code);

 private:
  static void task(void *argument);
  static bool geocode_city();
  static void fetch();
  static void save_location(float latitude, float longitude, const String &city, const String &timezone);

  static TaskHandle_t task_handle_;
  static WxData data_;
};
