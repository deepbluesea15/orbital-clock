#pragma once
#include <stdint.h>
#include "esphome/components/orbital_clock/clock_face.h"

class NeonFace final : public ClockFace { public: void draw() override; };
