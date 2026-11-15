#include "forecast_face.h"
#include "esphome/components/orbital_clock/clock_face_context.h"
#include "esphome/components/orbital_clock/weather_service.h"
#include "esphome/components/orbital_clock/weather_icons.h"

// ------------------------- FACE: FORECAST -----------------------------------
// Compact, static versions of the weather glyphs -- five animating icons at
// once would be a mess.
static void wxIconSmall(uint8_t ic, int cx, int cy) {
  switch (ic) {
    case IC_SUN:
      u8g2.drawDisc(cx, cy, 3);
      for (int i = 0; i < 8; i++) {
        float a = i * 0.785398f;
        cline(cx + (int)(cosf(a) * 5), cy + (int)(sinf(a) * 5),
              cx + (int)(cosf(a) * 7), cy + (int)(sinf(a) * 7));
      }
      break;
    case IC_PARTLY:
      u8g2.drawDisc(cx - 3, cy - 4, 2);
      wxDrawCloud(cx + 1, cy + 3, 4);
      break;
    case IC_CLOUD:
      wxDrawCloud(cx, cy, 5);
      break;
    case IC_FOG:
      wxDrawCloud(cx, cy - 3, 4);
      u8g2.drawHLine(cx - 6, cy + 4, 12);
      u8g2.drawHLine(cx - 4, cy + 7, 12);
      break;
    case IC_RAIN:
      wxDrawCloud(cx, cy - 3, 4);
      for (int i = 0; i < 3; i++)
        cline(cx - 4 + i * 4, cy + 4, cx - 6 + i * 4, cy + 8);
      break;
    case IC_SNOW:
      wxDrawCloud(cx, cy - 3, 4);
      for (int i = 0; i < 3; i++) {
        int dx = cx - 4 + i * 4, dy = cy + 6;
        u8g2.drawHLine(dx - 1, dy, 3);
        u8g2.drawVLine(dx, dy - 1, 3);
      }
      break;
    case IC_STORM:
      wxDrawCloud(cx, cy - 3, 4);
      cline(cx + 1, cy + 3, cx - 3, cy + 7);
      cline(cx - 3, cy + 7, cx + 1, cy + 7);
      cline(cx + 1, cy + 7, cx - 2, cy + 11);
      break;
  }
}

void ForecastFace::draw() {
  char buf[24];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 8, cfg.metric ? "5-DAY  C" : "5-DAY  F");
  dStrRight(W, 8, cityLabel());
  dDashed(0, 11, W);

  if (!wthr.valid || wthr.fcDays == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = !cfg.haveLoc ? WeatherService::location_message()
                  : (wthr.valid ? "NO FORECAST DATA" : "AWAITING UPLINK");
    dStr((W - strW(m)) / 2, 38, m);
    return;
  }

  const int cx[5] = { 13, 38, 64, 90, 115 };
  int days = wthr.fcDays; if (days > 5) days = 5;

  for (int i = 0; i < days; i++) {
    if (i > 0) for (int y = 15; y < 62; y += 3)
      u8g2.drawPixel((cx[i] + cx[i - 1]) / 2, y);

    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(buf, sizeof(buf), "%02u/%02u", static_cast<unsigned>(wthr.fcMonth[i]),
             static_cast<unsigned>(wthr.fcDay[i]));
    dStr(cx[i] - strW(buf) / 2, 20, buf);

    wxIconSmall(WeatherService::icon(wthr.fcCode[i]), cx[i], 34);

    u8g2.setFont(u8g2_font_5x7_tf);
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.fcHi[i]));
    dStr(cx[i] - strW(buf) / 2, 53, buf);

    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.fcLo[i]));
    dStr(cx[i] - strW(buf) / 2, 61, buf);
  }
}
