#include "orbital_clock.h"

#include "time_service.h"
#include "weather_service.h"

namespace esphome {
namespace orbital_clock {

static OrbitalClock *clock_instance = nullptr;

void OrbitalClock::setup() {
	clock_instance = this;
	if (this->time_source_ != nullptr) {
		TimeService::set_source(this->time_source_);
	}
	::WeatherService::set_http_request(this->http_request_);
	this->display_.setup();
	this->settings_.restore(this->display_, this->weather_);
}

void OrbitalClock::loop() {
	this->display_.loop();
	this->settings_.sync(this->display_, this->weather_);
}

}  // namespace orbital_clock
}  // namespace esphome

void orbital_reset_settings() {
	if (esphome::orbital_clock::clock_instance != nullptr)
		esphome::orbital_clock::clock_instance->reset_settings();
}