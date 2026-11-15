#pragma once

#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"

#include "clock_interfaces.h"

namespace esphome {
namespace orbital_clock {

class ClockSettings {
 public:
  void set_face_select(select::Select *entity) { face_select_ = entity; }
  void set_format_select(select::Select *entity) { format_select_ = entity; }
  void set_units_select(select::Select *entity) { units_select_ = entity; }
  void set_timezone_select(select::Select *entity) { timezone_select_ = entity; }
  void set_city_text(text::Text *entity) { city_text_ = entity; }

  void restore(DisplayEngine &display, WeatherService &weather);
  void sync(const DisplayEngine &display, const WeatherService &weather);
  void set_face(const std::string &value, DisplayEngine &display, WeatherService &weather);
  void set_time_format(const std::string &value, DisplayEngine &display, WeatherService &weather);
  void set_units(const std::string &value, DisplayEngine &display, WeatherService &weather);
  void set_timezone(const std::string &value, DisplayEngine &display, WeatherService &weather);
  void set_city(const std::string &value, DisplayEngine &display, WeatherService &weather);
  void reset(DisplayEngine &display, WeatherService &weather);

 private:
  select::Select *face_select_{nullptr};
  select::Select *format_select_{nullptr};
  select::Select *units_select_{nullptr};
  select::Select *timezone_select_{nullptr};
  text::Text *city_text_{nullptr};
};

}  // namespace orbital_clock
}  // namespace esphome