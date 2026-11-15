#pragma once

#include <stdint.h>
#include "clock_face.h"
#include "esphome/components/orbital_clock_faces/neon_face.h"
#include "esphome/components/orbital_clock_faces/orbit_face.h"
#include "esphome/components/orbital_clock_faces/terminal_face.h"
#include "esphome/components/orbital_clock_faces/flyover_face.h"
#include "esphome/components/orbital_clock_faces/tesseract_face.h"
#include "esphome/components/orbital_clock_faces/jellyfish_face.h"
#include "esphome/components/orbital_clock_faces/bounce_face.h"
#include "esphome/components/orbital_clock_faces/digital_face.h"
#include "esphome/components/orbital_clock_faces/rain_face.h"
#include "esphome/components/orbital_clock_faces/decrypt_face.h"
#include "esphome/components/orbital_clock_faces/weather_face.h"
#include "esphome/components/orbital_clock_faces/forecast_face.h"
#include "esphome/components/orbital_clock_faces/moon_face.h"
#include "esphome/components/orbital_clock_faces/orrery_face.h"
#include "esphome/components/orbital_clock_faces/analog_face.h"
#include "esphome/components/orbital_clock_faces/flipclock_face.h"
#include "esphome/components/orbital_clock_faces/wordclock_face.h"
#include "esphome/components/orbital_clock_faces/solar_face.h"

enum Face : uint8_t {
  FACE_NEON = 0, FACE_DIGITAL, FACE_ANALOG, FACE_FLIP, FACE_WORD,
  FACE_RAIN, FACE_DECRYPT,
  FACE_WEATHER, FACE_FORECAST,
  FACE_MOON, FACE_SOLAR, FACE_ORRERY,
  FACE_ORBIT, FACE_TERMINAL,
  FACE_FLYOVER, FACE_TESSERACT, FACE_JELLY, FACE_BOUNCE,
  FACE_COUNT
};

class ClockFaces {
 public:
  static void initialize();
  static void boot_line(const char *text);
  static void draw_boot();
  static void draw(uint8_t face);
};