#include "clock_settings.h"

namespace esphome {
namespace orbital_clock {

void ClockSettings::restore(DisplayEngine &display, WeatherService &weather) {
  auto face = this->face_select_->current_option();
  auto format = this->format_select_->current_option();
  auto units = this->units_select_->current_option();
  auto timezone = this->timezone_select_->current_option();

  if (!face.empty()) display.set_face(face.str());
  if (!format.empty()) display.set_time_format(format.str());
  if (!units.empty()) weather.set_units(units.str());
  if (!timezone.empty()) weather.set_timezone(timezone.str());
  weather.set_city(this->city_text_->state);
  this->sync(display, weather);
}

void ClockSettings::sync(const DisplayEngine &display, const WeatherService &weather) {
  const std::string face = display.current_face();
  const std::string format = display.current_time_format();
  const std::string units = weather.current_units();
  const std::string city = weather.current_city();

  if (this->face_select_->current_option() != face) this->face_select_->publish_state(face);
  if (this->format_select_->current_option() != format) this->format_select_->publish_state(format);
  if (this->units_select_->current_option() != units) this->units_select_->publish_state(units);
  if (this->city_text_->state != city) this->city_text_->publish_state(city);
}

void ClockSettings::set_face(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  display.set_face(value);
  this->sync(display, weather);
}

void ClockSettings::set_time_format(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  display.set_time_format(value);
  this->sync(display, weather);
}

void ClockSettings::set_units(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_units(value);
  this->sync(display, weather);
}

void ClockSettings::set_timezone(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_timezone(value);
  if (this->timezone_select_->current_option() != value) this->timezone_select_->publish_state(value);
}

void ClockSettings::set_city(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_city(value);
  this->sync(display, weather);
}

void ClockSettings::reset(DisplayEngine &display, WeatherService &weather) {
  this->set_face("NEON", display, weather);
  this->set_time_format("12-hour", display, weather);
  this->set_units("Celsius", display, weather);
  this->set_timezone("Automatic", display, weather);
  this->set_city("", display, weather);
}

}  // namespace orbital_clock
}  // namespace esphome