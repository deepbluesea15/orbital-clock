#include "orbital_clock.h"

#include "clock_runtime.h"
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
	this->set_interval("clock_frame", 40, [this]() { this->display_.loop(); });
	this->set_interval("settings_sync", 1000, [this]() {
		this->settings_.sync(this->display_, this->weather_);
	});
}

void OrbitalClock::boot_button_pressed() { orbital_button_pressed(); }

void OrbitalClock::boot_button_released() { orbital_button_released(); }

void OrbitalClock::boot_button_format_hold() { orbital_button_format_hold(); }

bool OrbitalClock::boot_button_factory_reset() { return orbital_button_factory_reset(); }

}  // namespace orbital_clock
}  // namespace esphome

void orbital_reset_settings() {
	if (esphome::orbital_clock::clock_instance != nullptr)
		esphome::orbital_clock::clock_instance->reset_settings();
}