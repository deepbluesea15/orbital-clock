#include "decrypt_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/network_status.h"

uint8_t DecryptFace::previous_[6] = { 99, 99, 99, 99, 99, 99 };
uint32_t DecryptFace::lock_until_[6] = {};
// ------------------------- FACE: DECRYPT ------------------------------------
// Each digit scrambles through random glyphs when it changes, then locks.
// Seconds are therefore always churning; hours sit still for an hour.
char DecryptFace::glyph(int slot, uint32_t tick) {
  uint32_t h = (uint32_t)slot * 2654435761u ^ tick * 40503u;
  h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
  return '0' + (h % 10);
}

void DecryptFace::draw() {
  uint32_t now = millis();
  int hr = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  uint8_t dig[6] = { (uint8_t)(hr / 10), (uint8_t)(hr % 10),
                     (uint8_t)(tnow.tm_min / 10), (uint8_t)(tnow.tm_min % 10),
                     (uint8_t)(tnow.tm_sec / 10), (uint8_t)(tnow.tm_sec % 10) };

  for (int i = 0; i < 6; i++) {
    if (dig[i] != previous_[i]) {
      previous_[i] = dig[i];
      lock_until_[i] = now + (i >= 4 ? 260 : 520);   // seconds settle faster
    }
  }

  // Live chip telemetry across the top. temperatureRead() jitters by a degree
  // or two between samples, so hold each reading for a second instead of
  // flickering at 25fps.
  static float coreC = 0;
  static uint32_t coreAt = 0;
  if (coreAt == 0 || now - coreAt > 1000) { coreAt = now; coreC = temperatureRead(); }

  char buf[26];
  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "CORE %.0f%c",
           cfg.metric ? coreC : coreC * 1.8f + 32.0f, cfg.metric ? 'C' : 'F');
  dStr(0, 7, buf);
  snprintf(buf, sizeof(buf), "HEAP %uK", (unsigned)(ESP.getFreeHeap() / 1024));
  dStrRight(W, 7, buf);

  // big HH:MM, one digit at a time so each can scramble independently
  u8g2.setFont(u8g2_font_logisoso28_tn);
  int dw = strW("8"), gap = 12;
  int total = 4 * dw + gap;
  int x0 = (W - total) / 2 - 7;
  if (x0 < 2) x0 = 2;
  const int base = 46;
  uint32_t tick = now / 45;                      // reshuffle ~22x/sec

  char one[2] = { 0, 0 };
  for (int i = 0; i < 4; i++) {
    int x = x0 + i * dw + (i >= 2 ? gap : 0);
    one[0] = (now < lock_until_[i]) ? glyph(i, tick) : ('0' + dig[i]);
    dStr(x, base, one);
    if (now < lock_until_[i]) dHLine(x, base + 2, dw - 2);   // "working" underline
  }

  // blinking colon
  if (subSecond() < 0.6f) {
    dBox(x0 + 2 * dw + 4, base - 21, 4, 4);
    dBox(x0 + 2 * dw + 4, base - 9,  4, 4);
  }

  // seconds
  u8g2.setFont(u8g2_font_6x10_tf);
  for (int i = 4; i < 6; i++) {
    one[0] = (now < lock_until_[i]) ? glyph(i, tick) : ('0' + dig[i]);
    dStr(x0 + total + 5 + (i - 4) * 6, base, one);
  }

  // corner brackets around the cipher block
  const int bx = 4, by = 12, bw2 = 120, bh2 = 40, L = 6;
  dHLine(bx, by, L);                dVLine(bx, by, L);
  dHLine(bx + bw2 - L, by, L);      dVLine(bx + bw2 - 1, by, L);
  dHLine(bx, by + bh2 - 1, L);      dVLine(bx, by + bh2 - L, L);
  dHLine(bx + bw2 - L, by + bh2 - 1, L);
  dVLine(bx + bw2 - 1, by + bh2 - L, L);

  // more telemetry along the bottom: uptime, clock speed, link quality
  uint32_t up = (millis() - bootMillis) / 1000;
  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "UP %02lu:%02lu:%02lu",
           (unsigned long)(up / 3600UL),
           (unsigned long)((up / 60UL) % 60UL),
           (unsigned long)(up % 60UL));
  dStr(0, 62, buf);

  snprintf(buf, sizeof(buf), "%uMHZ", (unsigned)getCpuFrequencyMhz());
  dStr((W - strW(buf)) / 2, 62, buf);

  if (NetworkStatus::is_connected()) snprintf(buf, sizeof(buf), "%ddBm", (int)NetworkStatus::rssi());
  else                               snprintf(buf, sizeof(buf), "NO LINK");
  dStrRight(W, 62, buf);
}
