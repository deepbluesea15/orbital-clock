#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class ForecastFace final : public ClockFace { public: void draw() override; };
