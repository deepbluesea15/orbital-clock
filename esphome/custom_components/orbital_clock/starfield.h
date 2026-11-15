#pragma once

#include <U8g2lib.h>

class Starfield {
 public:
  void initialize();
  void step();
  void draw(U8G2 &display, int max_y) const;

 private:
  struct Star {
    float x;
    uint8_t y;
    float velocity;
  };

  static constexpr uint8_t STAR_COUNT = 26;
  Star stars_[STAR_COUNT]{};
};
