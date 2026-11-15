#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include "esphome/components/time/real_time_clock.h"

#include "clock_settings.h"
#include "sketch_adapters.h"

namespace esphome {
namespace http_request {
class HttpRequestComponent;
}
namespace sun {
class Sun;
}
}  // namespace esphome

namespace esphome {
namespace orbital_clock {

class OrbitalClock : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_face_select(select::Select *entity) { settings_.set_face_select(entity); }
  void set_format_select(select::Select *entity) { settings_.set_format_select(entity); }
  void set_units_select(select::Select *entity) { settings_.set_units_select(entity); }
  void set_timezone_select(select::Select *entity) { settings_.set_timezone_select(entity); }
  void set_city_text(text::Text *entity) { settings_.set_city_text(entity); }
  void set_time_source(time::RealTimeClock *source) { time_source_ = source; }
  void set_http_request(http_request::HttpRequestComponent *request) { http_request_ = request; }
  void set_sun(sun::Sun *sun) { sun_ = sun; }

  void set_face(const std::string &value) { settings_.set_face(value, display_, weather_); }
  void set_time_format(const std::string &value) { settings_.set_time_format(value, display_, weather_); }
  void set_units(const std::string &value) { settings_.set_units(value, display_, weather_); }
  void set_timezone(const std::string &value) { settings_.set_timezone(value, display_, weather_); }
  void set_city(const std::string &value) { settings_.set_city(value, display_, weather_); }
  void reset_settings() { settings_.reset(display_, weather_); }
  void boot_button_pressed();
  void boot_button_released();
  void boot_button_format_hold();
  bool boot_button_factory_reset();

 private:
  ClockSettings settings_;
  SketchDisplayEngine display_;
  SketchWeatherService weather_;
  time::RealTimeClock *time_source_{nullptr};
  http_request::HttpRequestComponent *http_request_{nullptr};
  sun::Sun *sun_{nullptr};
};

}  // namespace orbital_clock
}  // namespace esphome