#pragma once

#include <stdint.h>
#include "clock_face.h"
#include "faces/neon_face.h"
#include "faces/orbit_face.h"
#include "faces/terminal_face.h"
#include "faces/flyover_face.h"
#include "faces/tesseract_face.h"
#include "faces/jellyfish_face.h"
#include "faces/bounce_face.h"
#include "faces/digital_face.h"
#include "faces/rain_face.h"
#include "faces/decrypt_face.h"
#include "faces/weather_face.h"
#include "faces/forecast_face.h"
#include "faces/moon_face.h"
#include "faces/orrery_face.h"
#include "faces/analog_face.h"
#include "faces/flipclock_face.h"
#include "faces/wordclock_face.h"
#include "faces/solar_face.h"

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
  static void boot_line(const char *text, uint16_t hold);
  static void draw(uint8_t face);
};