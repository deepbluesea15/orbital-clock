#pragma once

#include "clock_interfaces.h"

namespace esphome {
namespace orbital_clock {

class SketchDisplayEngine final : public DisplayEngine {
 public:
  void setup() override;
  void loop() override;
  void set_face(const std::string &value) override;
  void set_time_format(const std::string &value) override;
  const char *current_face() const override;
  const char *current_time_format() const override;
};

class SketchWeatherService final : public WeatherService {
 public:
  void set_units(const std::string &value) override;
  void set_timezone(const std::string &value) override;
  void set_city(const std::string &value) override;
  const char *current_units() const override;
  const char *current_city() const override;
};

}  // namespace orbital_clock
}  // namespace esphome