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

  if (this->face_select_->current_option() != face) this->face_select_->make_call().set_option(face).perform();
  if (this->format_select_->current_option() != format) this->format_select_->make_call().set_option(format).perform();
  if (this->units_select_->current_option() != units) this->units_select_->make_call().set_option(units).perform();
  if (this->city_text_->state != city) this->city_text_->make_call().set_value(city).perform();
}

void ClockSettings::set_face(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  display.set_face(value);
  const std::string face = display.current_face();
  if (this->face_select_->current_option() != face) this->face_select_->publish_state(face);
}

void ClockSettings::set_time_format(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  display.set_time_format(value);
  const std::string format = display.current_time_format();
  if (this->format_select_->current_option() != format) this->format_select_->publish_state(format);
}

void ClockSettings::set_units(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_units(value);
  const std::string units = weather.current_units();
  if (this->units_select_->current_option() != units) this->units_select_->publish_state(units);
}

void ClockSettings::set_timezone(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_timezone(value);
  if (this->timezone_select_->current_option() != value) this->timezone_select_->publish_state(value);
}

void ClockSettings::set_city(const std::string &value, DisplayEngine &display, WeatherService &weather) {
  weather.set_city(value);
  const std::string city = weather.current_city();
  if (this->city_text_->state != city) this->city_text_->publish_state(city);
}

void ClockSettings::reset(DisplayEngine &display, WeatherService &weather) {
  this->face_select_->make_call().set_option("NEON").perform();
  this->format_select_->make_call().set_option("12-hour").perform();
  this->units_select_->make_call().set_option("Celsius").perform();
  this->timezone_select_->make_call().set_option("Automatic").perform();
  this->city_text_->make_call().set_value("").perform();
}

}  // namespace orbital_clock
}  // namespace esphome