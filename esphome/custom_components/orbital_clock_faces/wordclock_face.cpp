#include "wordclock_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

// ------------------------- FACE: WORDCLOCK ----------------------------------
// 14 x 8 letter grid. Unlit cells fall back to a single dot so the grid is
// implied without competing with the sentence.
static const char *WC_GRID[8] = {
  "ITLISXHALFXTEN",
  "QUARTERXTWENTY",
  "FIVEMINUTESXTO",
  "PASTXONEXTWOXX",
  "THREEFOURXFIVE",
  "SIXSEVENXEIGHT",
  "NINETENXELEVEN",
  "TWELVEXOCLOCKX",
};
struct WcSpan { uint8_t r, c0, c1; };
static const WcSpan WC_HOUR[12] = {
  {3, 5, 8}, {3, 9, 12}, {4, 0, 5}, {4, 5, 9}, {4, 10, 14}, {5, 0, 3},
  {5, 3, 8}, {5, 9, 14}, {6, 0, 4}, {6, 4, 7}, {6, 8, 14}, {7, 0, 6},
};
static const WcSpan WC_IT = {0, 0, 2}, WC_IS = {0, 3, 5};
static const WcSpan WC_FIVE = {2, 0, 4},  WC_TEN  = {0, 11, 14};
static const WcSpan WC_QTR  = {1, 0, 7},  WC_TWTY = {1, 8, 14};
static const WcSpan WC_HALF = {0, 6, 10}, WC_MINS = {2, 4, 11};
static const WcSpan WC_PAST = {3, 0, 4},  WC_TO   = {2, 12, 14};
static const WcSpan WC_OCLK = {7, 7, 13};

void WordclockFace::draw() {
  WcSpan lit[7];
  uint8_t n = 0;
  lit[n++] = WC_IT;
  lit[n++] = WC_IS;

  int m5 = ((tnow.tm_min + 2) / 5) * 5;        // nearest five minutes
  int hr = tnow.tm_hour;
  if (m5 >= 60) { m5 = 0; hr++; }
  if (m5 > 30) hr++;                            // past the half, name the next hour
  int hIdx = ((hr % 12) + 11) % 12;             // 0 -> TWELVE

  int mm = (m5 > 30) ? 60 - m5 : m5;
  bool useMins = (mm == 5 || mm == 10 || mm == 20 || mm == 25);
  switch (mm) {
    case 0:  break;
    case 5:  lit[n++] = WC_FIVE; break;
    case 10: lit[n++] = WC_TEN;  break;
    case 15: lit[n++] = WC_QTR;  break;
    case 20: lit[n++] = WC_TWTY; break;
    case 25: lit[n++] = WC_TWTY; lit[n++] = WC_FIVE; break;
    case 30: lit[n++] = WC_HALF; break;
  }
  if (useMins) lit[n++] = WC_MINS;
  if (mm != 0) lit[n++] = (m5 > 30) ? WC_TO : WC_PAST;
  lit[n++] = WC_HOUR[hIdx];
  if (mm == 0) lit[n++] = WC_OCLK;

  u8g2.setFont(u8g2_font_5x7_tf);
  char one[2] = { 0, 0 };
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 14; c++) {
      bool on = false;
      for (uint8_t k = 0; k < n; k++)
        if (lit[k].r == r && c >= lit[k].c0 && c < lit[k].c1) { on = true; break; }
      int x = 2 + c * 9, y = 7 + r * 8;
      if (on) { one[0] = WC_GRID[r][c]; dStr(x, y, one); }
      else    dPixel(x + 2, y - 3);
    }
  }
}
