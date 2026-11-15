#include "weather_service.h"

#include <algorithm>
#include <stdlib.h>
#include <string.h>

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sun/sun.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "network_status.h"
#include "timezone_resolver.h"

static const char *const TAG = "orbital_clock.weather";

Cfg cfg;
esphome::http_request::HttpRequestComponent *WeatherService::http_request_ = nullptr;
esphome::sun::Sun *WeatherService::sun_ = nullptr;
TaskHandle_t WeatherService::task_handle_ = nullptr;
esphome::Mutex WeatherService::state_mutex_;
WxData WeatherService::data_{};
uint32_t WeatherService::config_revision_ = 0;
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
    esphome::App.feed_wdt();
    yield();
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
    }
  }
  complete = complete || response->is_read_complete();
  response->end();
  if (!complete) return false;

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

void WeatherService::start() {
  if (WeatherService::http_request_ == nullptr) {
    ESP_LOGE(TAG, "HTTP request component is not configured");
    return;
  }
  BaseType_t result = xTaskCreatePinnedToCore(WeatherService::task, "wx", 12288, nullptr, 1,
                                               &WeatherService::task_handle_, 0);
  if (result != pdPASS) ESP_LOGE(TAG, "Failed to start weather task");
}

void WeatherService::save_location(float latitude, float longitude, const String &city, const String &timezone) {
  if (esphome::global_preferences == nullptr) {
    ESP_LOGW(TAG, "Preferences are unavailable; location cache was not saved");
    return;
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
  }
}

bool WeatherService::geocode_city(const String &city, uint32_t revision) {
  if (!city.length()) return false;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + url_encode(city) +
               "&count=1&language=en&format=json";
  JsonDocument document;
  if (!get_json(WeatherService::http_request_, url, document)) return false;

  JsonArrayConst results = document["results"].as<JsonArrayConst>();
  if (results.isNull() || results.size() == 0) return false;
  JsonObjectConst result = results[0];
  if (!result["latitude"].is<float>() || !result["longitude"].is<float>()) return false;

  float latitude = result["latitude"].as<float>();
  float longitude = result["longitude"].as<float>();
  String iana_timezone = result["timezone"] | "";
  String cached_city;
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision != WeatherService::config_revision_) return false;
    cfg.lat = latitude;
    cfg.lon = longitude;
    cfg.haveLoc = true;
    if (WeatherService::sun_ != nullptr) {
      WeatherService::sun_->set_latitude(latitude);
      WeatherService::sun_->set_longitude(longitude);
    }
    cached_city = cfg.city;
    cfg.cached_city = cached_city;
    cfg.cached_timezone = iana_timezone;
    if (cfg.tz == "Automatic" && !TimezoneResolver::apply(cfg.tz, iana_timezone)) {
      ESP_LOGW(TAG, "No supported timezone mapping for '%s'; using UTC", iana_timezone.c_str());
    }
  }
  WeatherService::save_location(latitude, longitude, cached_city, iana_timezone);
  return true;
}

void WeatherService::fetch(float latitude, float longitude, bool metric, uint32_t revision) {
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
    if (revision != WeatherService::config_revision_) return;
    WeatherService::data_.tries++;
  }
  JsonDocument document;
  if (!get_json(WeatherService::http_request_, url, document)) {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision == WeatherService::config_revision_) WeatherService::data_.fails++;
    return;
  }

  JsonObjectConst current = document["current"];
  if (current.isNull() || !current["temperature_2m"].is<float>() || !current["weather_code"].is<int>()) {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision == WeatherService::config_revision_) WeatherService::data_.fails++;
    ESP_LOGW(TAG, "Weather response is missing current conditions");
    return;
  }

  WxData updated{};
  updated.tempF = current["temperature_2m"].as<float>();
  updated.hum = current["relative_humidity_2m"] | 0.0f;
  updated.feelF = current["apparent_temperature"] | 0.0f;
  updated.isDay = current["is_day"] | 1;
  updated.code = current["weather_code"].as<int>();
  updated.wind = current["wind_speed_10m"] | 0.0f;

  JsonObjectConst daily = document["daily"];
  JsonArrayConst codes = daily["weather_code"].as<JsonArrayConst>();
  JsonArrayConst highs = daily["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst lows = daily["temperature_2m_min"].as<JsonArrayConst>();
  int count = static_cast<int>(std::min<size_t>(5, std::min(codes.size(), std::min(highs.size(), lows.size()))));
  for (int i = 0; i < count; i++) {
    updated.fcCode[i] = codes[i].as<int>();
    updated.fcHi[i] = highs[i].as<float>();
    updated.fcLo[i] = lows[i].as<float>();
  }
  updated.fcDays = count;
  if (count > 0) {
    updated.hiF = highs[0].as<float>();
    updated.loF = lows[0].as<float>();
  }
  updated.stamp = millis();
  updated.valid = true;
  {
    esphome::LockGuard lock(WeatherService::state_mutex_);
    if (revision != WeatherService::config_revision_) return;
    updated.tries = WeatherService::data_.tries;
    updated.fails = WeatherService::data_.fails;
    WeatherService::data_ = updated;
  }
}

void WeatherService::task(void *) {
  for (;;) {
    if (NetworkStatus::is_connected()) {
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
      if (!have_location && WeatherService::geocode_city(city, revision)) {
        esphome::LockGuard lock(WeatherService::state_mutex_);
        latitude = cfg.lat;
        longitude = cfg.lon;
        have_location = cfg.haveLoc;
        metric = cfg.metric;
        revision = WeatherService::config_revision_;
      }
      if (have_location) WeatherService::fetch(latitude, longitude, metric, revision);
    }
    bool valid;
    {
      esphome::LockGuard lock(WeatherService::state_mutex_);
      valid = WeatherService::data_.valid;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(valid ? 900000UL : 60000UL));
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

const char *WeatherService::city_label() {
  static char buffer[14];
  if (!cfg.city.length()) {
    strcpy(buffer, "LOCAL");
    return buffer;
  }
  size_t length = cfg.city.length();
  if (length > sizeof(buffer) - 1) length = sizeof(buffer) - 1;
  for (size_t i = 0; i < length; i++) buffer[i] = toupper(cfg.city[i]);
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
