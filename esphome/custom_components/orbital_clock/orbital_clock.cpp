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
	::WeatherService::set_sun(this->sun_);
	this->display_.setup();
	this->settings_.restore(this->display_, this->weather_);
	this->set_interval("clock_frame", 40, [this]() {
		if (this->day_brightness_->has_state() && this->night_brightness_->has_state() &&
		    this->night_start_->has_state() && this->night_end_->has_state() &&
		    this->rotation_interval_->has_state()) {
			ClockRuntime::set_display_settings(
			    this->day_brightness_->state, this->night_brightness_->state,
			    this->night_start_->hour * 3600UL + this->night_start_->minute * 60UL + this->night_start_->second,
			    this->night_end_->hour * 3600UL + this->night_end_->minute * 60UL + this->night_end_->second,
			    this->rotation_interval_->state);
		}
		this->display_.loop();
	});
	this->set_interval("settings_sync", 1000, [this]() {
		::WeatherService::save_pending_location();
		this->settings_.sync(this->display_, this->weather_);
	});
}

void OrbitalClock::on_shutdown() { ::WeatherService::save_pending_location(true); }

void OrbitalClock::reset_settings() {
	this->settings_.reset(this->display_, this->weather_);
	this->day_brightness_->make_call().set_value(100).perform();
	this->night_brightness_->make_call().set_value(12).perform();
	this->night_start_->make_call().set_time(22, 0, 0).perform();
	this->night_end_->make_call().set_time(7, 0, 0).perform();
	this->rotation_interval_->make_call().set_value(0).perform();
}

void OrbitalClock::network_connected() { ::WeatherService::set_network_connected(true); }

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