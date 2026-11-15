/* ============================================================================
   ORBITAL CLOCK  v1.1
   A fourteen-face display for the Waveshare 1.51" transparent OLED
   driven by an ESP32-S3 SuperMini.

   https://github.com/deepbluesea15/orbital-clock

   FACES (tap BOOT to cycle; the current one is remembered across power cycles)
      1 NEON       big glitching digits over a drifting starfield
      2 DIGITAL    plain clock with seconds, readable across the room
      3 ANALOG     conventional dial, sweeping second hand, outside temp
      4 FLIPCLOCK  split-flap tiles; only the changed digit flips
      5 WORDCLOCK  the time spelled out of a lit letter grid
      6 RAIN       falling glyph columns, time punched through the middle
      7 DECRYPT    digits scramble and lock, framed by live chip telemetry
      8 WEATHER    current conditions, animated icon (Open-Meteo, no API key)
      9 FORECAST   five-day strip
     10 MOON       phase disc with illumination, age, next full and new
     11 SOLAR      sun's real position on the sky arc, sunrise/sunset, daylight
     12 ORRERY     six planets at their real heliocentric longitudes
     13 ORBIT      orbital dial, hour/minute/second bodies round a ringed sun
     14 TERMINAL   scrolling ship's log with a live clock
     15 FLYOVER    wireframe terrain scrolling past under a crescent moon
     16 TESSERACT  rotating 4D hypercube, projected 4D->3D->2D
     17 JELLYFISH  drifting jellies with pulsing bells and bubbles
     18 BOUNCE     the screensaver, now carrying the time

   BOOT BUTTON  tap = next face | hold 0.8s = 12/24h | hold 8s = factory reset
                The 12/24 choice is remembered, and can also be set in Home Assistant.

   WIRING (OLED 7-pin -> SuperMini)
     VCC->3V3  GND->GND  DIN->GPIO11  CLK->GPIO12
     CS->GPIO10  DC->GPIO9  RST->GPIO8

   BUILD  Board: "ESP32S3 Dev Module", USB CDC On Boot: Enabled
          Library: U8g2 by oliver

   Configuration and Wi-Fi setup are now managed by ESPHome and Home Assistant.
   Settings are controlled via ESPHome select/text entities.
   Time is synced via ESPHome's SNTP component.
   Network status is queried via ESPHome's Wi-Fi component.

   MIT licensed.
   ========================================================================== */

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <time.h>
#include <math.h>

#include "clock_runtime.h"
#include "clock_face_context.h"
#include "clock_faces.h"
#include "button_controller.h"
#include "display_schedule.h"
#include "network_status.h"
#include "starfield.h"
#include "time_service.h"
#include "weather_service.h"

bool use24h = false;
static const uint8_t  BRIGHT_DAY   = 255;
static const bool     GLITCH_FX    = true;
static const bool     ANTI_BURNIN  = true;

#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_CS    10
#define PIN_DC     9
#define PIN_RST    8

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, PIN_CS, PIN_DC, PIN_RST);

extern const int W = 128, H = 64;
const struct tm &tnow = TimeService::now();

static const char *FACE_NAMES[FACE_COUNT] = {
  "NEON", "DIGITAL", "ANALOG", "FLIPCLOCK", "WORDCLOCK",
  "RAIN", "DECRYPT",
  "WEATHER", "FORECAST",
  "MOON", "SOLAR", "ORRERY",
  "ORBIT", "TERMINAL",
  "FLYOVER", "TESSERACT", "JELLYFISH", "BOUNCE"
};
static uint8_t face = FACE_NEON;
static uint32_t labelUntil = 0;

static uint32_t lastFaceSw = 0;
static uint32_t face_auto_ms = 0;
static uint8_t day_brightness = 100;
static uint8_t night_brightness = 12;
static uint32_t night_start = 22 * 3600;
static uint32_t night_end = 7 * 3600;
uint32_t bootMillis = 0;
int gOfsX = 0, gOfsY = 0;
static uint32_t lastDrift = 0;
static uint32_t glitchUntil = 0, nextGlitch = 0;
static bool fmtFlash = false;

static Starfield starfield;
static ButtonController button_controller;
static bool reset_message = false;
static uint8_t boot_stage = 0;
static uint32_t boot_stage_at = 0;
static uint32_t boot_stage_duration = 420;

const char *DOW[7]  = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
const char *MON[12] = { "JAN","FEB","MAR","APR","MAY","JUN",
                               "JUL","AUG","SEP","OCT","NOV","DEC" };

// Apply glitch effect to display buffer
static void applyGlitch() {
  if (!GLITCH_FX) return;
  uint32_t t = millis();
  if (t > nextGlitch) {
    glitchUntil = t + random(80, 220);
    nextGlitch  = t + random(7000, 22000);
  }
  if (t > glitchUntil) return;
  uint8_t *buf = u8g2.getBufferPtr();
  static uint8_t tmp[W];
  uint8_t bands = random(1, 4);
  for (uint8_t b = 0; b < bands; b++) {
    uint8_t row = random(0, 8);
    int sh = random(-7, 8);
    if (sh == 0) continue;
    uint8_t *p = buf + (uint16_t)row * W;
    for (int x = 0; x < W; x++) tmp[x] = p[(x - sh + W) & (W - 1)];
    memcpy(p, tmp, W);
  }
  if (random(5) == 0) u8g2.drawHLine(0, random(H), W);
}

// Draw face label (current face name or time format when flashing)
static void drawFaceLabel() {
  if (millis() > labelUntil) return;
  u8g2.setFont(u8g2_font_4x6_tr);
  const char *n = fmtFlash ? (use24h ? "24-HOUR" : "12-HOUR") : FACE_NAMES[face];
  int w = u8g2.getStrWidth(n);
  int x = (W - w) / 2;
  u8g2.setDrawColor(0); u8g2.drawBox(x - 4, 51, w + 8, 11);
  u8g2.setDrawColor(1); u8g2.drawFrame(x - 4, 51, w + 8, 11);
  u8g2.drawStr(x, 59, n);
}

const char *linkStr() {
  if (!NetworkStatus::is_connected()) return "NO LINK";
  return TimeService::is_valid() ? "NET LOCK" : "SYNCING";
}

const char *cityLabel() {
  return WeatherService::city_label();
}

// Adapter functions for sketch compatibility and ESPHome lambda access
void orbital_set_face(const char *name) {
  for (uint8_t i = 0; i < FACE_COUNT; i++) {
    if (strcmp(name, FACE_NAMES[i]) == 0) {
      lastFaceSw = millis();
      face = i;
      fmtFlash = false;
      labelUntil = millis() + 1000;
      return;
    }
  }
}

void orbital_set_time_format(const char *format) {
  use24h = (strcmp(format, "24-hour") == 0);
  fmtFlash = true;
  labelUntil = millis() + 1200;
}

void orbital_toggle_time_format() {
  use24h = !use24h;
  fmtFlash = true;
  labelUntil = millis() + 1200;
}

void orbital_next_face() {
  fmtFlash = false;
  face = (face + 1) % FACE_COUNT;
  lastFaceSw = millis();
  labelUntil = millis() + 1000;
}

void orbital_set_units(const char *units) {
  WeatherService::set_units(units);
}

void orbital_set_timezone(const char *zone) {
  WeatherService::set_timezone(zone);
}

void orbital_set_city(const char *city) {
  WeatherService::set_city(city);
}

void orbital_button_pressed() { button_controller.press(); }

void orbital_button_released() { button_controller.release(); }

void orbital_button_format_hold() { button_controller.format_hold(); }

bool orbital_button_factory_reset() {
  reset_message = button_controller.factory_reset();
  return reset_message;
}

const char *orbital_current_face() { 
  return FACE_NAMES[face]; 
}

const char *orbital_current_time_format() { 
  return use24h ? "24-hour" : "12-hour"; 
}

const char *orbital_current_units() { 
  return WeatherService::current_units(); 
}

const char *orbital_current_city() { 
  return WeatherService::current_city(); 
}

// Entry points for sketch and ESPHome component
void orbital_sketch_setup() { ClockRuntime::setup(); }
void orbital_sketch_loop() { ClockRuntime::loop(); }

void ClockRuntime::setup() {
  bootMillis = millis();
  lastFaceSw = bootMillis;
  randomSeed(esp_random());

  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  u8g2.setBusClock(8000000);
  u8g2.begin();
  u8g2.setContrast(BRIGHT_DAY);
  u8g2.setFontMode(1);

  starfield.initialize();
  ClockFaces::initialize();
  ClockFaces::boot_line("ORBITAL CLOCK v1.1");
  u8g2.clearBuffer();
  ClockFaces::draw_boot();
  u8g2.sendBuffer();
  boot_stage_at = millis();
  
  WeatherService::load();
  TimeService::tick();
  WeatherService::set_network_connected(NetworkStatus::is_connected());
  WeatherService::start();

}

void ClockRuntime::set_display_settings(uint8_t day, uint8_t night, uint32_t start, uint32_t end,
                                        uint32_t rotation_minutes) {
  day_brightness = day;
  night_brightness = night;
  night_start = start;
  night_end = end;
  uint32_t interval_ms = rotation_minutes * 60000UL;
  if (face_auto_ms != interval_ms) {
    face_auto_ms = interval_ms;
    lastFaceSw = millis();
  }
}

void ClockRuntime::loop() {
  WeatherService::set_network_connected(NetworkStatus::is_connected());
  WeatherService::apply_pending_location();
  if (display_schedule::rotation_due(millis(), lastFaceSw, face_auto_ms)) {
    orbital_next_face();
  }

  {
    esphome::LockGuard lock(WeatherService::state_mutex());
    TimeService::tick();
  }

  if (ANTI_BURNIN && millis() - lastDrift > 60000) {
    lastDrift = millis();
    gOfsX = random(-2, 3);
    gOfsY = random(-1, 2);
  }

  static int lastBright = -1;
  const struct tm &now = TimeService::now();
  uint32_t seconds = now.tm_hour * 3600UL + now.tm_min * 60UL + now.tm_sec;
  bool night = display_schedule::is_night(seconds, night_start, night_end);
  int want = display_schedule::contrast(night ? night_brightness : day_brightness);
  if (want != lastBright) { lastBright = want; u8g2.setContrast(want); }

  starfield.step();
  u8g2.clearBuffer();

  if (reset_message) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *reset = "SETTINGS RESET";
    const char *restart = "RESTARTING";
    u8g2.drawStr((W - u8g2.getStrWidth(reset)) / 2, 28, reset);
    u8g2.drawStr((W - u8g2.getStrWidth(restart)) / 2, 42, restart);
    u8g2.sendBuffer();
    return;
  }

  if (boot_stage < 6) {
    if (millis() - boot_stage_at >= boot_stage_duration) {
      boot_stage_at = millis();
      switch (++boot_stage) {
        case 1:
          ClockFaces::boot_line("WAKE // COLD BOOT");
          boot_stage_duration = 260;
          break;
        case 2:
          ClockFaces::boot_line("SSD1309 PANEL .... OK");
          boot_stage_duration = 240;
          break;
        case 3:
          ClockFaces::boot_line(NetworkStatus::is_connected() ? "UPLINK ........... OK" : "UPLINK ....... WAITING");
          boot_stage_duration = 220;
          break;
        case 4:
          ClockFaces::boot_line(TimeService::is_valid() ? "NTP HANDSHAKE .... OK" : "NTP HANDSHAKE ... WAIT");
          boot_stage_duration = 260;
          break;
        case 5:
          ClockFaces::boot_line(NetworkStatus::is_connected() && TimeService::is_valid()
                                    ? "ALL SYSTEMS NOMINAL" : "STARTUP IN PROGRESS");
          boot_stage_duration = 500;
          break;
      }
    }
    if (boot_stage < 6) {
      ClockFaces::draw_boot();
      button_controller.draw_reset_overlay();
      u8g2.sendBuffer();
      return;
    }
  }

  if (!TimeService::is_valid()) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *message = "WAITING FOR TIME";
    u8g2.drawStr((W - u8g2.getStrWidth(message)) / 2, 30, message);
    u8g2.setFont(u8g2_font_4x6_tr);
    const char *status = NetworkStatus::is_connected() ? "SYNCING WITH SNTP" : "NO NETWORK";
    u8g2.drawStr((W - u8g2.getStrWidth(status)) / 2, 42, status);
    button_controller.draw_reset_overlay();
    u8g2.sendBuffer();
    return;
  }

  if (face == FACE_NEON || face == FACE_ORRERY || face == FACE_ORBIT ||
      face == FACE_TESSERACT) starfield.draw(u8g2, H);
  else if (face == FACE_FLYOVER) starfield.draw(u8g2, 20);

  ClockFaces::draw(face);

  if (face == FACE_NEON || face == FACE_RAIN || face == FACE_DECRYPT ||
      face == FACE_ORBIT || face == FACE_TERMINAL) applyGlitch();
  drawFaceLabel();
  button_controller.draw_reset_overlay();
  u8g2.sendBuffer();
}
