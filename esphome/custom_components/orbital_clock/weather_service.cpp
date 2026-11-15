#include "weather_service.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>
#include <string.h>

#include "esphome/core/log.h"
#include "network_status.h"
#include "time_service.h"
#include "timezone_resolver.h"

static const char *const TAG = "orbital_clock.weather";

Cfg cfg;
TaskHandle_t WeatherService::task_handle_ = nullptr;
WxData WeatherService::data_{};
WxData &wthr = WeatherService::data_;

namespace {

bool json_number(const char *json, const char *key, float &out) {
  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char *position = strstr(json, pattern);
  if (position == nullptr) return false;
  position += strlen(pattern);
  while (*position == '[' || *position == ' ') position++;
  if (*position == 'n' || *position == '}' || *position == ']') return false;
  out = atof(position);
  return true;
}

int json_array(const char *scope, const char *key, float *out, int max_count) {
  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\":[", key);
  const char *position = strstr(scope, pattern);
  if (position == nullptr) return 0;
  position += strlen(pattern);
  int count = 0;
  while (count < max_count && *position && *position != ']') {
    while (*position == ' ') position++;
    out[count++] = (*position == 'n') ? 0.0f : static_cast<float>(atof(position));
    while (*position && *position != ',' && *position != ']') position++;
    if (*position == ',') position++;
  }
  return count;
}

bool json_string(const char *json, const char *key, String &out) {
  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char *position = strstr(json, pattern);
  if (position == nullptr) return false;
  position += strlen(pattern);
  out = "";
  while (*position && *position != '"') out += *position++;
  return out.length() > 0;
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
  Preferences preferences;
  preferences.begin("orbital", true);
  cfg.city = preferences.getString("city", "");
  cfg.tz = preferences.getString("tz", "auto");
  cfg.lat = preferences.getFloat("lat", 0);
  cfg.lon = preferences.getFloat("lon", 0);
  cfg.haveLoc = preferences.getBool("hasloc", false);
  preferences.end();
}

void WeatherService::request() {
  if (WeatherService::task_handle_ != nullptr) xTaskNotifyGive(WeatherService::task_handle_);
}

void WeatherService::start() {
  BaseType_t result = xTaskCreatePinnedToCore(WeatherService::task, "wx", 12288, nullptr, 1,
                                               &WeatherService::task_handle_, 0);
  if (result != pdPASS) ESP_LOGE(TAG, "Failed to start weather task");
}

void WeatherService::save_location(float latitude, float longitude, const String &city, const String &timezone) {
  Preferences preferences;
  preferences.begin("orbital", false);
  preferences.putFloat("lat", latitude);
  preferences.putFloat("lon", longitude);
  preferences.putBool("hasloc", true);
  if (city.length()) preferences.putString("city", city);
  if (timezone.length()) preferences.putString("tz", timezone);
  preferences.end();
}

bool WeatherService::geocode_city() {
  if (!cfg.city.length()) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(9000);

  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + url_encode(cfg.city) +
               "&count=1&language=en&format=json";
  HTTPClient http;
  http.setTimeout(9000);
  if (!http.begin(client, url)) return false;
  if (http.GET() != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  const char *json = body.c_str();
  const char *results = strstr(json, "\"results\":");
  if (results == nullptr) return false;

  float latitude, longitude;
  if (!json_number(results, "latitude", latitude) || !json_number(results, "longitude", longitude)) return false;

  String iana_timezone, city_name;
  json_string(results, "timezone", iana_timezone);
  json_string(results, "name", city_name);

  cfg.lat = latitude;
  cfg.lon = longitude;
  cfg.haveLoc = true;
  if (city_name.length()) cfg.city = city_name;
  cfg.tz = TimezoneResolver::resolve(cfg.tz, iana_timezone);
  WeatherService::save_location(latitude, longitude, cfg.city, cfg.tz);
  TimeService::set_timezone(cfg.tz.c_str());
  return true;
}

void WeatherService::fetch() {
  if (!cfg.haveLoc) return;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(9000);

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(cfg.lat, 4) +
               "&longitude=" + String(cfg.lon, 4) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
               "is_day,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min"
               "&timezone=auto&forecast_days=5" +
               String(cfg.metric ? "&temperature_unit=celsius&wind_speed_unit=kmh"
                                 : "&temperature_unit=fahrenheit&wind_speed_unit=mph");

  HTTPClient http;
  http.setTimeout(9000);
  WeatherService::data_.tries++;
  if (!http.begin(client, url)) {
    WeatherService::data_.fails++;
    return;
  }
  int status = http.GET();
  if (status != 200) {
    http.end();
    WeatherService::data_.fails++;
    return;
  }

  String body = http.getString();
  http.end();
  const char *json = body.c_str();
  const char *current = strstr(json, "\"current\":");
  const char *daily = strstr(json, "\"daily\":");
  if (current == nullptr) {
    WeatherService::data_.fails++;
    return;
  }

  float temperature, humidity = 0, apparent = 0, is_day = 1, code = 0, wind = 0;
  if (!json_number(current, "temperature_2m", temperature) ||
      !json_number(current, "weather_code", code)) {
    WeatherService::data_.fails++;
    return;
  }
  json_number(current, "relative_humidity_2m", humidity);
  json_number(current, "apparent_temperature", apparent);
  json_number(current, "is_day", is_day);
  json_number(current, "wind_speed_10m", wind);

  WeatherService::data_.tempF = temperature;
  WeatherService::data_.hum = humidity;
  WeatherService::data_.feelF = apparent;
  WeatherService::data_.isDay = static_cast<int>(is_day);
  WeatherService::data_.code = static_cast<int>(code);
  WeatherService::data_.wind = wind;

  if (daily != nullptr) {
    float highs[5], lows[5], codes[5];
    int count = json_array(daily, "weather_code", codes, 5);
    int high_count = json_array(daily, "temperature_2m_max", highs, 5);
    int low_count = json_array(daily, "temperature_2m_min", lows, 5);
    if (high_count < count) count = high_count;
    if (low_count < count) count = low_count;
    for (int i = 0; i < count; i++) {
      WeatherService::data_.fcCode[i] = static_cast<int>(codes[i]);
      WeatherService::data_.fcHi[i] = highs[i];
      WeatherService::data_.fcLo[i] = lows[i];
    }
    WeatherService::data_.fcDays = count;
    if (count > 0) {
      WeatherService::data_.hiF = highs[0];
      WeatherService::data_.loF = lows[0];
    }
  }

  WeatherService::data_.stamp = millis();
  WeatherService::data_.valid = true;
}

void WeatherService::task(void *) {
  for (;;) {
    if (NetworkStatus::is_connected()) {
      if (!cfg.haveLoc) WeatherService::geocode_city();
      WeatherService::fetch();
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WeatherService::data_.valid ? 900000UL : 60000UL));
  }
}

void WeatherService::set_units(const char *units) {
  bool metric = (strcmp(units, "Fahrenheit") != 0);
  if (cfg.metric == metric) return;
  cfg.metric = metric;
  WeatherService::data_.valid = false;
  WeatherService::request();
}

void WeatherService::set_timezone(const char *zone) {
  String choice(zone);
  if (choice == "Automatic") {
    cfg.tz = "auto";
    cfg.haveLoc = false;
  } else {
    cfg.tz = TimezoneResolver::posix_for(choice);
  }
  TimeService::set_timezone(TimezoneResolver::resolve(cfg.tz, "").c_str());
  WeatherService::request();
}

void WeatherService::set_city(const char *city) {
  String next(city);
  next.trim();
  if (next == cfg.city) return;
  cfg.city = next;
  cfg.haveLoc = false;
  cfg.lat = 0;
  cfg.lon = 0;
  WeatherService::data_.valid = false;
  WeatherService::request();
}

const char *WeatherService::current_units() { return cfg.metric ? "Celsius" : "Fahrenheit"; }

const char *WeatherService::current_city() { return cfg.city.c_str(); }

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
