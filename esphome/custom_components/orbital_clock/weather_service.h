#pragma once

#include <Arduino.h>
#include <atomic>

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
  static void set_network_connected(bool connected);
  static void apply_pending_location();
  static void save_pending_location(bool force = false);
  static void set_units(const char *units);
  static void set_timezone(const char *zone);
  static void set_city(const char *city);
  static const char *current_units();
  static const char *current_city();
  static const char *city_label();
  static const char *location_message();
  static uint8_t icon(int code);
  static const char *label(int code);
  static const WxData &data();
  static esphome::Mutex &state_mutex();

 private:
  static void task(void *argument);
  static bool geocode_city(const String &city, uint32_t revision);
  static void set_location_error(LocationStatus status, uint32_t revision);
  static bool fetch(float latitude, float longitude, bool metric, uint32_t revision);
  static bool save_location(float latitude, float longitude, const String &city, const String &timezone);
  static esphome::http_request::HttpRequestComponent *http_request_;
  static esphome::sun::Sun *sun_;
  static TaskHandle_t task_handle_;
  static std::atomic<bool> network_connected_;
  static esphome::Mutex state_mutex_;
  static WxData data_;
  static uint32_t config_revision_;
  static bool location_save_pending_;
  static bool location_update_pending_;
  static bool location_save_failed_;
  static uint32_t last_location_save_failure_;
};
