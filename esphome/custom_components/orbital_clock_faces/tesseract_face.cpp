#include "tesseract_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE 6: TESSERACT --------------------------------
void TesseractFace::draw() {
  float t = millis() * 0.001f;
  float a = t * 0.55f, b = t * 0.37f;
  float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
  int px[16], py[16];
  for (int i = 0; i < 16; i++) {
    float x = (i & 1) ? 1.f : -1.f, y = (i & 2) ? 1.f : -1.f;
    float z = (i & 4) ? 1.f : -1.f, w = (i & 8) ? 1.f : -1.f;
    float x1 = x * ca - w * sa, w1 = x * sa + w * ca;   // rotate in XW
    float y1 = y * cb - z * sb, z1 = y * sb + z * cb;   // rotate in YZ
    float k  = 2.0f / (3.2f - w1);                      // 4D -> 3D
    float X = x1 * k, Y = y1 * k, Z = z1 * k;
    float k2 = 58.0f / (4.2f - Z);                      // 3D -> 2D
    px[i] = 64 + (int)(X * k2);
    py[i] = 32 + (int)(Y * k2);
  }
  for (int i = 0; i < 16; i++)
    for (int bit = 0; bit < 4; bit++) {
      int j = i ^ (1 << bit);
      if (j > i) cline(px[i], py[i], px[j], py[j]);
    }
}
