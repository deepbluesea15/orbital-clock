#include "flipclock_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"

uint8_t FlipclockFace::previous_[4] = { 99, 99, 99, 99 };
uint32_t FlipclockFace::flip_at_[4] = {};
// ------------------------- FACE: FLIPCLOCK ----------------------------------
// Split-flap. Each tile remembers the digit it was showing and the moment it
// changed; only that tile animates, so the seconds churn while the hour tile
// sits still for an hour.
void FlipclockFace::flip_tile(int x, int y, int w, int h, char now_c, char old_c, float t) {
  if (now_c == ' ' && (t >= 1.0f || old_c == ' ')) return;   // no leading-zero tile
  const int hinge = y + h / 2;
  u8g2.drawRFrame(x + gOfsX, y + gOfsY, w, h, 3);

  u8g2.setFont(u8g2_font_logisoso28_tn);
  char one[2] = { 0, 0 };
  int base = y + h - 7;

  if (t >= 1.0f) {                                  // settled
    one[0] = now_c;
    dStr(x + (w - strW(one)) / 2, base, one);
  } else {
    // Top half already shows the new digit; the bottom still shows the old
    // one until the falling leaf lands over it.
    one[0] = now_c;
    int nx = x + (w - strW(one)) / 2;
    u8g2.setClipWindow(x + 1 + gOfsX, y + 1 + gOfsY, x + w - 1 + gOfsX, hinge + gOfsY);
    dStr(nx, base, one);
    u8g2.setMaxClipWindow();

    one[0] = old_c;
    int ox = x + (w - strW(one)) / 2;
    u8g2.setClipWindow(x + 1 + gOfsX, hinge + 1 + gOfsY, x + w - 1 + gOfsX, y + h - 1 + gOfsY);
    dStr(ox, base, one);
    u8g2.setMaxClipWindow();

    // The leaf: first half it is the old top folding down, second half the new
    // bottom swinging up. Height tracks the cosine so it eases at the edges.
    int halfH = h / 2 - 1;
    int lh;
    char lc;
    if (t < 0.5f) { lh = (int)(halfH * cosf(t * 2.0f * 1.5708f));        lc = old_c; }
    else          { lh = (int)(halfH * cosf((1.0f - t) * 2.0f * 1.5708f)); lc = now_c; }
    if (lh > 0) {
      int ly0 = (t < 0.5f) ? (hinge - lh) : (hinge + 1);
      int ly1 = (t < 0.5f) ? hinge        : (hinge + lh);
      u8g2.setDrawColor(0);
      dBox(x + 1, ly0, w - 2, ly1 - ly0 + 1);
      u8g2.setDrawColor(1);
      dFrame(x, ly0, w, ly1 - ly0 + 1);
      one[0] = lc;
      int lx = x + (w - strW(one)) / 2;
      u8g2.setClipWindow(x + 2 + gOfsX, ly0 + 1 + gOfsY, x + w - 2 + gOfsX, ly1 - 1 + gOfsY);
      // Both leaves draw their digit at the resting baseline and let the band
      // do the cutting: the falling leaf then peels the old top away, and the
      // landing one reveals the new bottom exactly where it will come to rest.
      dStr(lx, base, one);
      u8g2.setMaxClipWindow();
    }
  }
  u8g2.drawHLine(x + 1 + gOfsX, hinge + gOfsY, w - 2);
}

void FlipclockFace::draw() {
  char buf[20];
  uint32_t now = millis();
  int hr = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  char d[4] = { (char)('0' + hr / 10), (char)('0' + hr % 10),
                (char)('0' + tnow.tm_min / 10), (char)('0' + tnow.tm_min % 10) };
  if (!use24h && hr < 10) d[0] = ' ';          // no leading zero in 12-hour

  for (int i = 0; i < 4; i++) {
    if (previous_[i] == 99) previous_[i] = d[i];
    if (d[i] != previous_[i] && flip_at_[i] == 0) flip_at_[i] = now;
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "%s %02d %s", DOW[tnow.tm_wday % 7],
           tnow.tm_mday, MON[tnow.tm_mon % 12]);
  dStr(0, 6, buf);
  if (wthr.valid) snprintf(buf, sizeof(buf), "%d%c", (int)lroundf(wthr.tempF),
                           cfg.metric ? 'C' : 'F');
  else            snprintf(buf, sizeof(buf), "--");
  dStrRight(W - 2, 6, buf);

  const int tw = 26, th = 44, ty = 11;
  const int xs[4] = { 4, 33, 69, 98 };
  for (int i = 0; i < 4; i++) {
    float t = 1.0f;
    if (flip_at_[i]) {
      t = (float)(now - flip_at_[i]) / (float)FLIP_MS;
      if (t >= 1.0f) { t = 1.0f; flip_at_[i] = 0; previous_[i] = d[i]; }
    }
    flip_tile(xs[i], ty, tw, th, d[i], previous_[i], t);
  }
  if (subSecond() < 0.6f) {                     // colon blinks with the second
    dBox(62, 24, 3, 3);
    dBox(62, 40, 3, 3);
  }
}
