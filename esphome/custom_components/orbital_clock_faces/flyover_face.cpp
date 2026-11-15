#include "flyover_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

float FlyoverFace::flyZ = 0;
// ------------------------- FACE 5: FLYOVER ----------------------------------
static float terrainH(float x, float z) {
  return sinf(x * 0.45f + sinf(z * 0.21f)) * 2.6f
       + sinf(x * 0.17f - z * 0.11f) * 5.0f
       + sinf(z * 0.33f) * 1.4f;
}
void FlyoverFace::draw() {
  flyZ += 0.05f;
  const int ROWS = 12, COLS = 18;
  const float STEP = 1.6f, ZNEAR = 3.0f, DEPTH = 150.0f;
  const int anchor = 22;            // y that rows converge to at infinity

  u8g2.drawCircle(103, 11, 7);
  u8g2.drawDisc(100, 9, 1);
  dDashed(0, 28, W, 3, 5);          // horizon, just above the far ridges

  float base = floorf(flyZ / STEP) * STEP;
  for (int r = ROWS - 1; r >= 0; r--) {
    float wz = base + r * STEP;
    float z  = wz - flyZ + ZNEAR;
    if (z < 0.4f) continue;
    float sc = 34.0f / z;
    int px = 0, py = 0;
    for (int c = 0; c <= COLS; c++) {
      float wx = (c - COLS * 0.5f) * 1.15f;
      float h  = terrainH(wx, wz);
      int sx = 64 + (int)(wx * sc);
      int sy = anchor + (int)(DEPTH / z) - (int)(h * sc * 0.18f);
      if (c > 0) cline(px, py, sx, sy);
      px = sx; py = sy;
    }
  }
}
