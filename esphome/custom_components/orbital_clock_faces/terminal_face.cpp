#include "terminal_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/network_status.h"
#include "esphome/components/orbital_clock/time_service.h"

char TerminalFace::logBuf[TerminalFace::LOG_ROWS][30] = {};
uint32_t TerminalFace::lastLogPush = 0;
// ------------------------- FACE 3: TERMINAL ---------------------------------

void TerminalFace::initialize() {
  for (uint8_t i = 0; i < LOG_ROWS; i++) logBuf[i][0] = '\0';
}

void TerminalFace::logPush(const char *s) {
  for (uint8_t i = 0; i < LOG_ROWS - 1; i++) strcpy(logBuf[i], logBuf[i + 1]);
  strncpy(logBuf[LOG_ROWS - 1], s, sizeof(logBuf[0]) - 1);
  logBuf[LOG_ROWS - 1][sizeof(logBuf[0]) - 1] = '\0';
}
void TerminalFace::logRandom() {
  char b[30];
  uint32_t up = (millis() - bootMillis) / 1000;
  switch (random(13)) {
    case 0:  snprintf(b, sizeof(b), "RSSI %d dBm / CH %d",
                      (int)NetworkStatus::rssi(), (int)NetworkStatus::channel()); break;
    case 1:  snprintf(b, sizeof(b), "STARDATE %.1f", TimeService::stardate()); break;
    case 2:  snprintf(b, sizeof(b), "UPTIME %02lu:%02lu:%02lu",
                      up / 3600UL, (up / 60UL) % 60UL, up % 60UL); break;
    case 3:  snprintf(b, sizeof(b), "HEAP %u KB FREE", (unsigned)(ESP.getFreeHeap() / 1024)); break;
    case 4:  snprintf(b, sizeof(b), "ICE INTACT / NO PROBES"); break;
    case 5:  snprintf(b, sizeof(b), "SOLAR WIND %d KM/S", (int)random(280, 620)); break;
    case 6:  snprintf(b, sizeof(b), "CHRONO DRIFT %+d ms", (int)random(-9, 10)); break;
    case 7:  snprintf(b, sizeof(b), "SCAN 10.0.0.0/24 ..%d", (int)random(2, 60)); break;
    case 8:  snprintf(b, sizeof(b), "REACTOR %d%% NOMINAL", (int)random(94, 100)); break;
    case 9:  snprintf(b, sizeof(b), "CAFFEINE RESERVE: LOW"); break;
    case 10: snprintf(b, sizeof(b), "HULL TEMP %d C", (int)(temperatureRead())); break;
    case 11: snprintf(b, sizeof(b), "NTP %s", TimeService::is_valid() ? "LOCKED" : "DRIFTING"); break;
    default: snprintf(b, sizeof(b), "NO ANOMALIES DETECTED"); break;
  }
  logPush(b);
}
void TerminalFace::draw() {
  char buf[30];
  u8g2.setFont(u8g2_font_4x6_tr);
  if (millis() - lastLogPush > 1400) { lastLogPush = millis(); logRandom(); }
  for (uint8_t i = 0; i < LOG_ROWS; i++) {
    if (logBuf[i][0]) { dStr(0, 6 + i * 7, "."); dStr(6, 6 + i * 7, logBuf[i]); }
  }
  dDashed(0, 50, W);
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "> %02d:%02d:%02d %s", h, tnow.tm_min, tnow.tm_sec,
           use24h ? "" : (tnow.tm_hour < 12 ? "AM" : "PM"));
  u8g2.setFont(u8g2_font_6x10_tf);
  dStr(0, 62, buf);
  if (subSecond() < 0.5f) dBox(strW(buf) + 2, 55, 5, 8);
}
void TerminalFace::boot_line(const char *text) {
  logPush(text);
}

void TerminalFace::draw_boot() {
  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < LOG_ROWS; i++)
    if (logBuf[i][0]) u8g2.drawStr(6, 6 + i * 7, logBuf[i]);
  u8g2.drawBox(0, 56, 4, 6);
}
