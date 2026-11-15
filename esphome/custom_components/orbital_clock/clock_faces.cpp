#include "clock_faces.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/weather_service.h"

void ClockFaces::initialize() {
  TerminalFace::initialize();
  JellyfishFace::initialize();
  RainFace::initialize();
}

void ClockFaces::boot_line(const char *text) {
  TerminalFace::boot_line(text);
}

void ClockFaces::draw_boot() {
  TerminalFace::draw_boot();
}

void ClockFaces::draw(uint8_t face) {
  esphome::LockGuard lock(WeatherService::state_mutex());
  switch (face) {
    case FACE_NEON:      { static NeonFace f; f.draw(); break; }
    case FACE_DIGITAL:   { static DigitalFace f; f.draw(); break; }
    case FACE_ANALOG:    { static AnalogFace f; f.draw(); break; }
    case FACE_FLIP:      { static FlipclockFace f; f.draw(); break; }
    case FACE_WORD:      { static WordclockFace f; f.draw(); break; }
    case FACE_RAIN:      { static RainFace f; f.draw(); break; }
    case FACE_DECRYPT:   { static DecryptFace f; f.draw(); break; }
    case FACE_WEATHER:   { static WeatherFace f; f.draw(); break; }
    case FACE_FORECAST:  { static ForecastFace f; f.draw(); break; }
    case FACE_MOON:      { static MoonFace f; f.draw(); break; }
    case FACE_SOLAR:     { static SolarFace f; f.draw(); break; }
    case FACE_ORRERY:    { static OrreryFace f; f.draw(); break; }
    case FACE_ORBIT:     { static OrbitFace f; f.draw(); break; }
    case FACE_TERMINAL:  { static TerminalFace f; f.draw(); break; }
    case FACE_FLYOVER:   { static FlyoverFace f; f.draw(); break; }
    case FACE_TESSERACT: { static TesseractFace f; f.draw(); break; }
    case FACE_JELLY:     { static JellyfishFace f; f.draw(); break; }
    case FACE_BOUNCE:    { static BounceFace f; f.draw(); break; }
  }
}