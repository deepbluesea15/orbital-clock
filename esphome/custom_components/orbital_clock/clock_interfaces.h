#pragma once

#include <string>

namespace esphome {
namespace orbital_clock {

class DisplayEngine {
 public:
  virtual ~DisplayEngine() = default;
  virtual void setup() = 0;
  virtual void loop() = 0;
  virtual void set_face(const std::string &value) = 0;
  virtual void set_time_format(const std::string &value) = 0;
  virtual const char *current_face() const = 0;
  virtual const char *current_time_format() const = 0;
};

class WeatherService {
 public:
  virtual ~WeatherService() = default;
  virtual void set_units(const std::string &value) = 0;
  virtual void set_timezone(const std::string &value) = 0;
  virtual void set_city(const std::string &value) = 0;
  virtual const char *current_units() const = 0;
  virtual const char *current_city() const = 0;
};

}  // namespace orbital_clock
}  // namespace esphome