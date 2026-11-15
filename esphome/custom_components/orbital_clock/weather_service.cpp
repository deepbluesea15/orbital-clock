#include "weather_service.h"

#include <algorithm>
#include <cmath>
#include <stdlib.h>
#include <string.h>

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sun/sun.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#include "network_status.h"
#include "timezone_resolver.h"
#include "weather_retry.h"

static const char *const TAG = "orbital_clock.weather";

Cfg cfg;
esphome::http_request::HttpRequestComponent *WeatherService::http_request_ = nullptr;
esphome::sun::Sun *WeatherService::sun_ = nullptr;
TaskHandle_t WeatherService::task_handle_ = nullptr;
std::atomic<bool> WeatherService::network_connected_{false};
esphome::Mutex WeatherService::state_mutex_;
WxData WeatherService::data_{};
uint32_t WeatherService::config_revision_ = 0;
bool WeatherService::location_save_pending_ = false;
bool WeatherService::location_update_pending_ = false;
bool WeatherService::location_save_failed_ = false;
uint32_t WeatherService::last_location_save_failure_ = 0;
const WxData &wthr = WeatherService::data();

namespace {

constexpr size_t MAX_RESPONSE_SIZE = 8192;
constexpr size_t HTTP_READ_CHUNK_SIZE = 512;
constexpr uint32_t LOCATION_PREFERENCE_ID = 0x4F524243;

struct LocationCache {
  char city[81];
  char timezone[64];
  float latitude;
  float longitude;
  bool valid;
};

bool get_json(esphome::http_request::HttpRequestComponent *request, const String &url, JsonDocument &document) {
  if (request == nullptr) {
    ESP_LOGE(TAG, "HTTP request component is not configured");
    return false;
  }
  auto response = request->get(url.c_str());
  if (response == nullptr) {
    ESP_LOGW(TAG, "HTTP request failed to start");
    return false;
  }
  if (!esphome::http_request::is_success(response->status_code)) {
    ESP_LOGW(TAG, "HTTP request returned status %d", response->status_code);
    response->end();
    return false;
  }

  std::string body;
  uint8_t buffer[HTTP_READ_CHUNK_SIZE];
  uint32_t last_data_time = millis();
  bool complete = false;
  while (!response->is_read_complete()) {
    int bytes_read = response->read(buffer, sizeof(buffer));
    auto result = esphome::http_request::http_read_loop_result(
        bytes_read, last_data_time, request->get_timeout(), response->is_read_complete());
    if (result == esphome::http_request::HttpReadLoopResult::DATA) {
      if (body.size() + bytes_read > MAX_RESPONSE_SIZE) {
        ESP_LOGW(TAG, "HTTP response exceeded %u bytes", static_cast<unsigned>(MAX_RESPONSE_SIZE));
        response->end();
        return false;
      }
      body.append(reinterpret_cast<const char *>(buffer), bytes_read);
    } else if (result == esphome::http_request::HttpReadLoopResult::COMPLETE) {
      complete = true;
      break;
    } else if (result == esphome::http_request::HttpReadLoopResult::ERROR ||
               result == esphome::http_request::HttpReadLoopResult::TIMEOUT) {
      ESP_LOGW(TAG, "HTTP response read failed");
      response->end();
      return false;
    } else {
      vTaskDelay(1);
    }
  }
  complete = complete || response->is_read_complete();
  response->end();
  if (!complete) {
    ESP_LOGW(TAG, "HTTP response ended before completion");
    return false;
  }

  DeserializationError error = deserializeJson(document, body);
  if (error) {
    ESP_LOGW(TAG, "Invalid JSON response: %s", error.c_str());
    return false;
  }
  return true;
}

String url_encode(const String &input) {
  String output;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < input.length(); i++) {
    unsigned char character = input[i];
    if (isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~') {
      output += static_cast<char>(character);
    } else if (character == ' ') {
      output += "%20";
    } else {
      output += '%';
      output += hex[(character >> 4) & 0xF];
      output += hex[character & 0xF];
    }
  }
  return output;
}

}  // namespace

void WeatherService::load() {
  if (esphome::global_preferences == nullptr) {
    ESP_LOGW(TAG, "Preferences are unavailable; location will be geocoded");
    return;
  }
  auto preference = esphome::global_preferences->make_preference<LocationCache>(LOCATION_PREFERENCE_ID);
  LocationCache cache{};
  if (!preference.load(&cache) || !cache.valid || cache.city[0] == '\0') return;
  esphome::LockGuard lock(WeatherService::state_mutex_);
  cfg.cached_city = cache.city;
  cfg.cached_timezone = cache.timezone;
  cfg.lat = cache.latitude;
  cfg.lon = cache.longitude;
  if (WeatherService::sun_ != nullptr) {
    WeatherService::sun_->set_latitude(cfg.lat);
    WeatherService::sun_->set_longitude(cfg.lon);
  }
}

void WeatherService::set_http_request(esphome::http_request::HttpRequestComponent *request) {
  WeatherService::http_request_ = request;
}

void WeatherService::set_sun(esphome::sun::Sun *sun) {
  esphome::LockGuard lock(WeatherService::state_mutex_);
  WeatherService::sun_ = sun;
  if (WeatherService::sun_ != nullptr) {
    WeatherService::sun_->set_latitude(cfg.lat);
    WeatherService::sun_->set_longitude(cfg.lon);
  }
}

esphome::sun::Sun *WeatherService::sun() { return WeatherService::sun_; }

void WeatherService::request() {
  if (WeatherService::task_handle_ != nullptr) xTaskNotifyGive(WeatherService::task_handle_);
}

void WeatherService::set_network_connected(bool connected) {
  const bool was_connected = WeatherService::network_connected_.exchange(connected, std::memory_order_relaxed);
  if (connected && !was_connected) WeatherService::request();
}

void WeatherService::start() {
  if (WeatherService::http_request_ == nullptr) {
    ESP_LOGE(TAG, "HTTP request component is not configured");
    return;
  }
  BaseType_t result = xTaskCreatePinnedToCore(WeatherService::task, "wx", 12288, nullptr, 1,
                                               &WeatherService::task_handle_, 0);
  if (result != pdPASS) ESP_LOGE(TAG, "Failed to start weather task");
}

bool WeatherService::save_location(float latitude, float longitude, const String &city, const String &timezone) {
  if (esphome::global_preferences == nullptr) {
    ESP_LOGW(TAG, "Preferences are unavailable; location cache was not saved");
    return false;
  }
  LocationCache cache{};
  city.substring(0, sizeof(cache.city) - 1).toCharArray(cache.city, sizeof(cache.city));
  timezone.substring(0, sizeof(cache.timezone) - 1).toCharArray(cache.timezone, sizeof(cache.timezone));
  cache.latitude = latitude;
  cache.longitude = longitude;
  cache.valid = true;
  auto preference = esphome::global_preferences->make_preference<LocationCache>(LOCATION_PREFERENCE_ID);
  if (!preference.save(&cache) || !esphome::global_preferences->sync()) {
    ESP_LOGW(TAG, "Failed to save location cache");
    return false;
  }
  return true;
}

void WeatherService::apply_pending_location() {
  esphome::LockGuard lock(WeatherService::state_mutex_);
  if (!WeatherService::location_update_pending_) return;
  WeatherService::location_update_pending_ = false;
  if (!cfg.haveLoc || !cfg.city.equalsIgnoreCase(cfg.cached_city)) return;
  if (WeatherService::sun_ != nullptr) {
    WeatherService::sun_->set_latitude(cfg.lat);
    WeatherService::sun_->set_longitude(cfg.lon);
  }
  if (cfg.tz == "Automatic" && !TimezoneResolver::apply(cfg.tz, cfg.cached_timezone)) {
    ESP_LOGW(TAG, "No supported timezone mapping for '%s'; using UTC", cfg.cached_timezone.c_str());
  }
}

void WeatherService::save_pending_location(bool force) {
  float latitude;
  float longitude;
  String city;
  String timezone;
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (!WeatherService::location_save_pending_) return;
    if (!force && WeatherService::location_save_failed_ &&
        millis() - WeatherService::last_location_save_failure_ < 60000) return;
    latitude = cfg.lat;
    longitude = cfg.lon;
    city = cfg.cached_city;
    timezone = cfg.cached_timezone;
    WeatherService::location_save_pending_ = false;
  }
  const bool saved = WeatherService::save_location(latitude, longitude, city, timezone);
  esphome::LockGuard lock(WeatherService::state_mutex_);
  WeatherService::location_save_failed_ = !saved;
  if (!saved) {
    WeatherService::location_save_pending_ = true;
    WeatherService::last_location_save_failure_ = millis();
  }
}

bool WeatherService::geocode_city(const String &city, uint32_t revision) {
  if (!city.length()) return false;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + url_encode(city) +
               "&count=1&language=en&format=json";
  JsonDocument document;
  if (!get_json(WeatherService::http_request_, url, document)) {
    WeatherService::set_location_error(LocationStatus::ERROR, revision);
    return false;
  }

  JsonArrayConst results = document["results"].as<JsonArrayConst>();
  if (document["results"].isNull() || (!results.isNull() && results.size() == 0)) {
    ESP_LOGW(TAG, "No location found for '%s'", city.c_str());
    WeatherService::set_location_error(LocationStatus::NOT_FOUND, revision);
    return false;
  }
  JsonObjectConst result = results[0];
  if (results.isNull() || !result["latitude"].is<float>() || !result["longitude"].is<float>()) {
    ESP_LOGW(TAG, "Geocoding response is missing coordinates");
    WeatherService::set_location_error(LocationStatus::ERROR, revision);
    return false;
  }

  float latitude = result["latitude"].as<float>();
  float longitude = result["longitude"].as<float>();
  if (!std::isfinite(latitude) || !std::isfinite(longitude) || latitude < -90 || latitude > 90 ||
      longitude < -180 || longitude > 180) {
    ESP_LOGW(TAG, "Geocoding response contains invalid coordinates");
    WeatherService::set_location_error(LocationStatus::ERROR, revision);
    return false;
  }
  String iana_timezone = result["timezone"] | "";
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision != WeatherService::config_revision_) return false;
    cfg.lat = latitude;
    cfg.lon = longitude;
    cfg.haveLoc = true;
    cfg.location_status = LocationStatus::PENDING;
    cfg.cached_city = cfg.city;
    cfg.cached_timezone = iana_timezone;
    WeatherService::location_save_pending_ = true;
    WeatherService::location_update_pending_ = true;
  }
  return true;
}

void WeatherService::set_location_error(LocationStatus status, uint32_t revision) {
  esphome::LockGuard lock(WeatherService::state_mutex_);
  if (revision == WeatherService::config_revision_ && !cfg.haveLoc) cfg.location_status = status;
}

bool WeatherService::fetch(float latitude, float longitude, bool metric, uint32_t revision) {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
               "is_day,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min"
               "&timezone=auto&forecast_days=5" +
               String(metric ? "&temperature_unit=celsius&wind_speed_unit=kmh"
                             : "&temperature_unit=fahrenheit&wind_speed_unit=mph");
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision != WeatherService::config_revision_) return false;
    WeatherService::data_.tries++;
  }
  JsonDocument document;
  if (!get_json(WeatherService::http_request_, url, document)) {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision == WeatherService::config_revision_) WeatherService::data_.fails++;
    return false;
  }

  JsonObjectConst current = document["current"];
  if (current.isNull() || !current["temperature_2m"].is<float>() || !current["weather_code"].is<int>() ||
      !current["relative_humidity_2m"].is<float>() || !current["apparent_temperature"].is<float>() ||
      !current["is_day"].is<int>() || !current["wind_speed_10m"].is<float>()) {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision == WeatherService::config_revision_) WeatherService::data_.fails++;
    ESP_LOGW(TAG, "Weather response is missing current conditions");
    return false;
  }

  WxData updated{};
  updated.tempF = current["temperature_2m"].as<float>();
  updated.hum = current["relative_humidity_2m"].as<float>();
  updated.feelF = current["apparent_temperature"].as<float>();
  updated.isDay = current["is_day"].as<int>();
  updated.code = current["weather_code"].as<int>();
  updated.wind = current["wind_speed_10m"].as<float>();

  JsonObjectConst daily = document["daily"];
  JsonArrayConst codes = daily["weather_code"].as<JsonArrayConst>();
  JsonArrayConst highs = daily["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst lows = daily["temperature_2m_min"].as<JsonArrayConst>();
  JsonArrayConst dates = daily["time"].as<JsonArrayConst>();
  if (!highs.isNull() && !lows.isNull() && highs[0].is<float>() && lows[0].is<float>()) {
    updated.hiF = highs[0].as<float>();
    updated.loF = lows[0].as<float>();
    updated.highLowValid = true;
  }
  int count = static_cast<int>(
      std::min<size_t>(5, std::min(dates.size(), std::min(codes.size(), std::min(highs.size(), lows.size())))));
  if (count == 0) ESP_LOGW(TAG, "Weather response is missing forecast data or dates");
  for (int i = 0; i < count; i++) {
    const char *date = dates[i].as<const char *>();
    esphome::ESPTime parsed{};
    if (date == nullptr || strlen(date) != 10 || !esphome::ESPTime::strptime(date, parsed) ||
        !parsed.is_valid(false, false) || !codes[i].is<int>() || !highs[i].is<float>() || !lows[i].is<float>()) {
      ESP_LOGW(TAG, "Weather response contains an invalid forecast entry");
      count = 0;
      break;
    }
    updated.fcCode[i] = codes[i].as<int>();
    updated.fcHi[i] = highs[i].as<float>();
    updated.fcLo[i] = lows[i].as<float>();
    updated.fcMonth[i] = parsed.month;
    updated.fcDay[i] = parsed.day_of_month;
  }
  updated.fcDays = count;
  updated.stamp = millis();
  updated.valid = true;
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision != WeatherService::config_revision_) return false;
    updated.tries = WeatherService::data_.tries;
    updated.fails = WeatherService::data_.fails + (count == 0 ? 1 : 0);
    WeatherService::data_ = updated;
  }
  return count > 0;
}

void WeatherService::task(void *) {
  uint32_t retry_delay = weather_retry::INITIAL_MS;
  uint32_t last_revision = 0;
  for (;;) {
    bool retry = false;
    if (WeatherService::network_connected_.load(std::memory_order_relaxed)) {
      String city;
      float latitude;
      float longitude;
      bool have_location;
      bool metric;
      uint32_t revision;
      {
        esphome::LockGuard lock(WeatherService::state_mutex_);
        city = cfg.city;
        latitude = cfg.lat;
        longitude = cfg.lon;
        have_location = cfg.haveLoc;
        metric = cfg.metric;
        revision = WeatherService::config_revision_;
      }
      if (revision != last_revision) {
        retry_delay = weather_retry::INITIAL_MS;
        last_revision = revision;
      }
      if (!have_location && WeatherService::geocode_city(city, revision)) {
        esphome::LockGuard lock(WeatherService::state_mutex_);
        latitude = cfg.lat;
        longitude = cfg.lon;
        have_location = cfg.haveLoc;
        metric = cfg.metric;
        revision = WeatherService::config_revision_;
      }
      if (have_location) retry = !WeatherService::fetch(latitude, longitude, metric, revision);
      else if (city.length()) retry = true;
    }
    bool valid;
    {
      esphome::LockGuard lock(WeatherService::state_mutex_);
      valid = WeatherService::data_.valid;
    }
    uint32_t wait_ms;
    if (retry) {
      wait_ms = weather_retry::jittered_delay(retry_delay, esp_random());
      retry_delay = weather_retry::next_delay(retry_delay);
    } else {
      if (WeatherService::network_connected_.load(std::memory_order_relaxed) && valid)
        retry_delay = weather_retry::INITIAL_MS;
      wait_ms = valid ? weather_retry::MAX_MS : weather_retry::INITIAL_MS;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
  }
}

void WeatherService::set_units(const char *units) {
  bool metric = (strcmp(units, "Fahrenheit") != 0);
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (cfg.metric == metric) return;
    cfg.metric = metric;
    WeatherService::config_revision_++;
    WeatherService::data_.valid = false;
  }
  WeatherService::request();
}

void WeatherService::set_timezone(const char *zone) {
  String choice(zone);
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    cfg.tz = choice;
    WeatherService::config_revision_++;
    if (choice == "Automatic") {
      if (cfg.cached_city.length() && cfg.city.equalsIgnoreCase(cfg.cached_city)) {
        cfg.haveLoc = true;
        if (WeatherService::sun_ != nullptr) {
          WeatherService::sun_->set_latitude(cfg.lat);
          WeatherService::sun_->set_longitude(cfg.lon);
        }
        if (!TimezoneResolver::apply(cfg.tz, cfg.cached_timezone)) {
          ESP_LOGW(TAG, "No supported timezone mapping for '%s'; using UTC", cfg.cached_timezone.c_str());
        }
      } else {
        cfg.haveLoc = false;
        TimezoneResolver::apply("UTC");
      }
    } else {
      if (!TimezoneResolver::apply(cfg.tz)) {
        ESP_LOGW(TAG, "No supported timezone mapping for '%s'; using UTC", choice.c_str());
      }
    }
  }
  WeatherService::request();
}

void WeatherService::set_city(const char *city) {
  String next(city);
  next.trim();
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (next == cfg.city) return;
    cfg.city = next;
    cfg.location_status = LocationStatus::PENDING;
    WeatherService::config_revision_++;
    if (cfg.cached_city.length() && cfg.city.equalsIgnoreCase(cfg.cached_city)) {
      cfg.haveLoc = true;
      if (WeatherService::sun_ != nullptr) {
        WeatherService::sun_->set_latitude(cfg.lat);
        WeatherService::sun_->set_longitude(cfg.lon);
      }
      if (cfg.tz == "Automatic" && !TimezoneResolver::apply(cfg.tz, cfg.cached_timezone)) {
        ESP_LOGW(TAG, "No supported timezone mapping for '%s'; using UTC", cfg.cached_timezone.c_str());
      }
    } else {
      cfg.haveLoc = false;
      if (cfg.tz == "Automatic") TimezoneResolver::apply("UTC");
    }
    WeatherService::data_.valid = false;
  }
  WeatherService::request();
}

const char *WeatherService::current_units() {
  esphome::LockGuard lock(WeatherService::state_mutex_);
  return cfg.metric ? "Celsius" : "Fahrenheit";
}

const char *WeatherService::current_city() {
  static char buffer[81];
  esphome::LockGuard lock(WeatherService::state_mutex_);
  cfg.city.substring(0, sizeof(buffer) - 1).toCharArray(buffer, sizeof(buffer));
  return buffer;
}

const WxData &WeatherService::data() { return WeatherService::data_; }

esphome::Mutex &WeatherService::state_mutex() { return WeatherService::state_mutex_; }

const char *WeatherService::location_message() {
  return ::location_message(cfg.city.length() != 0, NetworkStatus::is_connected(), cfg.location_status);
}

const char *WeatherService::city_label() {
  static char buffer[14];
  if (!cfg.city.length()) {
    strcpy(buffer, "LOCAL");
    return buffer;
  }
  size_t length = cfg.city.length();
  if (length > sizeof(buffer) - 1) length = sizeof(buffer) - 1;
  for (size_t i = 0; i < length; i++)
    buffer[i] = static_cast<char>(toupper(static_cast<unsigned char>(cfg.city[i])));
  buffer[length] = '\0';
  return buffer;
}

uint8_t WeatherService::icon(int code) {
  if (code == 0) return IC_SUN;
  if (code <= 2) return IC_PARTLY;
  if (code == 3) return IC_CLOUD;
  if (code <= 48) return IC_FOG;
  if (code <= 67) return IC_RAIN;
  if (code <= 77) return IC_SNOW;
  if (code <= 82) return IC_RAIN;
  if (code <= 86) return IC_SNOW;
  return IC_STORM;
}

const char *WeatherService::label(int code) {
  if (code == 0) return "CLEAR";
  if (code <= 2) return "PARTLY CLOUDY";
  if (code == 3) return "OVERCAST";
  if (code <= 48) return "FOG";
  if (code <= 57) return "DRIZZLE";
  if (code <= 67) return "RAIN";
  if (code <= 77) return "SNOW";
  if (code <= 82) return "SHOWERS";
  if (code <= 86) return "SNOW SHOWERS";
  return "THUNDERSTORM";
}
