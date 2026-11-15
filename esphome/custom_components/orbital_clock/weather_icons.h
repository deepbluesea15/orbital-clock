#pragma once

#include "clock_face_context.h"

inline void wxDrawCloud(int cx, int cy, int size) {
  int radius = size / 2;
  u8g2.drawCircle(cx - radius, cy - radius / 2, radius);
  u8g2.drawCircle(cx, cy - radius - 2, radius);
  u8g2.drawCircle(cx + radius, cy - radius / 2, radius);
  u8g2.drawBox(cx - radius - 2, cy, 2 * radius + 4, radius);
}
