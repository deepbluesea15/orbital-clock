#include "sketch_adapters.h"
#include "clock_runtime.h"

namespace esphome {
namespace orbital_clock {

void SketchDisplayEngine::setup() { orbital_sketch_setup(); }

void SketchDisplayEngine::loop() { orbital_sketch_loop(); }

void SketchDisplayEngine::set_face(const std::string &value) { orbital_set_face(value.c_str()); }

void SketchDisplayEngine::set_time_format(const std::string &value) {
  orbital_set_time_format(value.c_str());
}

const char *SketchDisplayEngine::current_face() const { return orbital_current_face(); }

const char *SketchDisplayEngine::current_time_format() const {
  return orbital_current_time_format();
}

void SketchWeatherService::set_units(const std::string &value) { orbital_set_units(value.c_str()); }

void SketchWeatherService::set_timezone(const std::string &value) {
  orbital_set_timezone(value.c_str());
}

void SketchWeatherService::set_city(const std::string &value) { orbital_set_city(value.c_str()); }

const char *SketchWeatherService::current_units() const { return orbital_current_units(); }

const char *SketchWeatherService::current_city() const { return orbital_current_city(); }

}  // namespace orbital_clock
}  // namespace esphome