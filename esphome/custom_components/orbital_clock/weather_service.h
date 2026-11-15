#pragma once

#include <Arduino.h>

#include "clock_face_context.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace http_request {
class HttpRequestComponent;
}
namespace sun {
class Sun;
}
}  // namespace esphome

class WeatherService {
 public:
  static void set_http_request(esphome::http_request::HttpRequestComponent *request);
  static void set_sun(esphome::sun::Sun *sun);
  static esphome::sun::Sun *sun();
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
  static const WxData &data();
  static esphome::Mutex &state_mutex();

 private:
  static void task(void *argument);
  static bool geocode_city(const String &city, uint32_t revision);
  static void fetch(float latitude, float longitude, bool metric, uint32_t revision);
  static void save_location(float latitude, float longitude, const String &city, const String &timezone);
  static esphome::http_request::HttpRequestComponent *http_request_;
  static esphome::sun::Sun *sun_;
  static TaskHandle_t task_handle_;
  static esphome::Mutex state_mutex_;
  static WxData data_;
  static uint32_t config_revision_;
};
