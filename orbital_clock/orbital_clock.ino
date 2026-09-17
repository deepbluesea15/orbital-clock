/* ============================================================================
   ORBITAL // eight-face display toy
   ESP32-S3 SuperMini  +  Waveshare 1.51" Transparent OLED (SSD1309, 128x64)

   BOOT button: tap = next face, hold 0.8s = 12/24h, hold 8s = factory reset.

   Faces:
     1. NEON      - glitching digits, starfield, second bar
     2. ORBIT     - orbital dial + readout panel
     3. TERMINAL  - scrolling ship's log with live clock
     4. DECRYPT   - digits scramble and lock like a cipher resolving
     5. WEATHER   - current conditions (Open-Meteo, no API key)
     6. FORECAST  - five-day strip
     7. FLYOVER   - wireframe terrain scrolling toward you under a moon
     8. TESSERACT - rotating 4D hypercube, projected 4D->3D->2D
     9. JELLYFISH - drifting jellies with pulsing bells and bubbles
    10. BOUNCE    - the screensaver, still chasing that corner

   FIRST BOOT
     The clock opens an open Wi-Fi network called ORBITAL-xxxx and shows the
     join instructions on screen. Connect a phone, the setup page appears (or
     browse to 192.168.4.1), enter your network and city, save. Settings live
     in NVS. To erase them: hold BOOT for 8 seconds while running, or hold it
     while powering on.

   WIRING (OLED 7-pin -> SuperMini)
     VCC->3V3  GND->GND  DIN->GPIO11  CLK->GPIO12
     CS->GPIO10  DC->GPIO9  RST->GPIO8
   ========================================================================== */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <time.h>
#include <math.h>

// ------------------------- USER CONFIG --------------------------------------
// No credentials or coordinates live in this file. Everything the device needs
// about your network and location is entered once through the setup portal and
// kept in NVS. Hold BOOT while powering on to wipe it and start over.

#define CALLSIGN    "NX-01"
#define AP_PREFIX   "ORBITAL-"

static bool use24h = false;
static const uint32_t FACE_AUTO_MS = 0;      // ms per face, 0 = button only
static const uint8_t  BRIGHT_DAY   = 255;
static const uint8_t  BRIGHT_NIGHT = 30;
static const bool     GLITCH_FX    = true;
static const bool     ANTI_BURNIN  = true;

// ------------------------- PINS ---------------------------------------------
#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_CS    10
#define PIN_DC     9
#define PIN_RST    8
#define PIN_BTN    0

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, PIN_CS, PIN_DC, PIN_RST);

// ------------------------- STATE --------------------------------------------
static const int W = 128, H = 64;
static const uint16_t FRAME_MS = 40;

enum Face : uint8_t {
  FACE_NEON = 0, FACE_DIGITAL, FACE_RAIN, FACE_DECRYPT, FACE_WEATHER, FACE_FORECAST,
  FACE_ORBIT, FACE_TERMINAL,
  FACE_FLYOVER, FACE_TESSERACT, FACE_JELLY, FACE_BOUNCE,
  FACE_COUNT
};
static const char *FACE_NAMES[FACE_COUNT] = {
  "NEON", "DIGITAL", "RAIN", "DECRYPT", "WEATHER", "FORECAST",
  "ORBIT", "TERMINAL", "FLYOVER", "TESSERACT", "JELLYFISH", "BOUNCE"
};
static uint8_t face = FACE_NEON;
static uint32_t labelUntil = 0;

static struct tm tnow;
static bool     timeValid  = false;
static uint8_t  lastSec    = 61;
static uint32_t secMark    = 0;
static uint32_t lastFrame  = 0;
static uint32_t lastFaceSw = 0;
static uint32_t bootMillis = 0;

static int gOfsX = 0, gOfsY = 0;
static uint32_t lastDrift = 0;
static uint32_t glitchUntil = 0, nextGlitch = 0;

// ------------------------- CONFIG STORE -------------------------------------
struct Cfg {
  String ssid, pass, city, tz;
  float  lat = 0, lon = 0;
  bool   haveLoc = false;
} cfg;

static Preferences prefs;

static void cfgLoad() {
  prefs.begin("orbital", true);
  cfg.ssid    = prefs.getString("ssid", "");
  cfg.pass    = prefs.getString("pass", "");
  cfg.city    = prefs.getString("city", "");
  cfg.tz      = prefs.getString("tz", "auto");
  cfg.lat     = prefs.getFloat("lat", 0);
  cfg.lon     = prefs.getFloat("lon", 0);
  cfg.haveLoc = prefs.getBool("hasloc", false);
  prefs.end();
}
static void cfgSaveNet(const String &ss, const String &pw, const String &ct, const String &tz) {
  prefs.begin("orbital", false);
  prefs.putString("ssid", ss);
  prefs.putString("pass", pw);
  prefs.putString("city", ct);
  prefs.putString("tz", tz);
  prefs.putBool("hasloc", false);        // force a fresh geocode
  prefs.end();
}
static void cfgSaveLoc(float la, float lo, const String &city, const String &tz) {
  prefs.begin("orbital", false);
  prefs.putFloat("lat", la);
  prefs.putFloat("lon", lo);
  prefs.putBool("hasloc", true);
  if (city.length()) prefs.putString("city", city);
  if (tz.length())   prefs.putString("tz", tz);
  prefs.end();
}
static void cfgClear() { prefs.begin("orbital", false); prefs.clear(); prefs.end(); }

// Uppercased, trimmed city for the weather headers.
static const char *cityLabel() {
  static char buf[14];
  if (!cfg.city.length()) { strcpy(buf, "LOCAL"); return buf; }
  size_t n = cfg.city.length(); if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
  for (size_t i = 0; i < n; i++) buf[i] = toupper(cfg.city[i]);
  buf[n] = '\0';
  return buf;
}

// ------------------------- TIMEZONES ----------------------------------------
// The ESP has no tz database, so POSIX rule strings are carried here. The IANA
// name is what Open-Meteo hands back when we geocode, used to auto-select.
struct TzEnt { const char *iana; const char *posix; const char *label; };
static const TzEnt TZS[] = {
  { "America/New_York",    "EST5EDT,M3.2.0,M11.1.0",       "US Eastern" },
  { "America/Detroit",     "EST5EDT,M3.2.0,M11.1.0",       "US Eastern (Detroit)" },
  { "America/Toronto",     "EST5EDT,M3.2.0,M11.1.0",       "Toronto" },
  { "America/Chicago",     "CST6CDT,M3.2.0,M11.1.0",       "US Central" },
  { "America/Denver",      "MST7MDT,M3.2.0,M11.1.0",       "US Mountain" },
  { "America/Phoenix",     "MST7",                         "Arizona" },
  { "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0",       "US Pacific" },
  { "America/Vancouver",   "PST8PDT,M3.2.0,M11.1.0",       "Vancouver" },
  { "America/Anchorage",   "AKST9AKDT,M3.2.0,M11.1.0",     "Alaska" },
  { "Pacific/Honolulu",    "HST10",                        "Hawaii" },
  { "America/Mexico_City", "CST6",                         "Mexico City" },
  { "America/Sao_Paulo",   "<-03>3",                       "Sao Paulo" },
  { "Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0",     "UK / Ireland" },
  { "Europe/Lisbon",       "WET0WEST,M3.5.0/1,M10.5.0",    "Portugal" },
  { "Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3",   "Central Europe" },
  { "Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3",   "Berlin" },
  { "Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3",   "Madrid" },
  { "Europe/Rome",         "CET-1CEST,M3.5.0,M10.5.0/3",   "Rome" },
  { "Europe/Amsterdam",    "CET-1CEST,M3.5.0,M10.5.0/3",   "Amsterdam" },
  { "Europe/Warsaw",       "CET-1CEST,M3.5.0,M10.5.0/3",   "Warsaw" },
  { "Europe/Athens",       "EET-2EEST,M3.5.0/3,M10.5.0/4", "Eastern Europe" },
  { "Europe/Helsinki",     "EET-2EEST,M3.5.0/3,M10.5.0/4", "Helsinki" },
  { "Europe/Kyiv",         "EET-2EEST,M3.5.0/3,M10.5.0/4", "Kyiv" },
  { "Europe/Moscow",       "MSK-3",                        "Moscow" },
  { "Asia/Dubai",          "<+04>-4",                      "Dubai" },
  { "Asia/Kolkata",        "IST-5:30",                     "India" },
  { "Asia/Bangkok",        "<+07>-7",                      "Bangkok" },
  { "Asia/Singapore",      "<+08>-8",                      "Singapore" },
  { "Asia/Shanghai",       "CST-8",                        "China" },
  { "Asia/Hong_Kong",      "HKT-8",                        "Hong Kong" },
  { "Asia/Tokyo",          "JST-9",                        "Japan" },
  { "Asia/Seoul",          "KST-9",                        "Korea" },
  { "Australia/Perth",     "AWST-8",                       "Perth" },
  { "Australia/Brisbane",  "AEST-10",                      "Brisbane" },
  { "Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3", "Sydney" },
  { "Pacific/Auckland",    "NZST-12NZDT,M9.5.0,M4.1.0/3",  "New Zealand" },
  { "UTC",                 "UTC0",                         "UTC" },
};
static const int TZ_N = sizeof(TZS) / sizeof(TZS[0]);

static const char *tzPosixFor(const String &iana) {
  for (int i = 0; i < TZ_N; i++) if (iana == TZS[i].iana) return TZS[i].posix;
  return "UTC0";
}
// Resolve whatever is stored into an actual POSIX string.
static String tzResolved(const String &ianaHint) {
  if (cfg.tz.length() && cfg.tz != "auto") return cfg.tz;
  if (ianaHint.length()) return String(tzPosixFor(ianaHint));
  return String("UTC0");
}

// ------------------------- SETUP PORTAL -------------------------------------
static WebServer  portal(80);
static DNSServer  dnsSrv;
static String     apName, netOptions;
static bool       portalSaved = false;
static uint32_t   portalSavedAt = 0;

static String htmlEsc(const String &in) {
  String o;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if      (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}
static String urlEnc(const String &in) {
  String o;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else if (c == ' ') o += "%20";
    else { o += '%'; o += hex[(c >> 4) & 0xF]; o += hex[c & 0xF]; }
  }
  return o;
}

static void scanNets() {
  netOptions = "";
  int n = WiFi.scanNetworks(false, false);
  for (int i = 0; i < n && i < 20; i++) {
    String ss = WiFi.SSID(i);
    if (!ss.length()) continue;
    netOptions += "<option value=\"" + htmlEsc(ss) + "\">" + htmlEsc(ss)
                + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
  }
  if (!netOptions.length())
    netOptions = "<option value=\"\">-- no networks found --</option>";
}

static void handleRoot() {
  String h;
  h.reserve(4000);
  h += F("<!doctype html><html><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Orbital Setup</title><style>"
         "body{background:#0b0e13;color:#dff6ea;font-family:system-ui,-apple-system,sans-serif;margin:0;padding:20px}"
         "h1{font-size:18px;letter-spacing:3px;color:#5ef0bd;margin:0 0 4px}"
         ".s{font-size:12px;color:#66798a;margin-bottom:18px}"
         "label{display:block;margin:16px 0 5px;font-size:12px;letter-spacing:1px;color:#8aa0b0;text-transform:uppercase}"
         "input,select{width:100%;box-sizing:border-box;padding:12px;border-radius:8px;"
         "border:1px solid #27313d;background:#131a24;color:#eafff7;font-size:16px}"
         "button{margin-top:24px;width:100%;padding:15px;border:0;border-radius:8px;"
         "background:#5ef0bd;color:#06231a;font-size:17px;font-weight:600;letter-spacing:1px}"
         ".n{font-size:12px;color:#6d7f8c;margin-top:6px;line-height:1.4}"
         "</style></head><body>"
         "<h1>ORBITAL SETUP</h1><div class=s>One-time configuration</div>"
         "<form method=POST action=/save>"
         "<label>Wi-Fi network</label><select name=net>");
  h += netOptions;
  h += F("</select>"
         "<label>Or type a network name</label>"
         "<input name=ssid2 placeholder='leave blank to use the list above' autocapitalize=off autocorrect=off>"
         "<label>Wi-Fi password</label>"
         "<input name=pass type=password autocapitalize=off autocorrect=off>"
         "<label>City (for time zone and weather)</label>"
         "<input name=city placeholder='e.g. New York, NY' autocapitalize=words>"
         "<div class=n>Looked up after the clock joins your network. "
         "Add a state or country if the name is ambiguous.</div>"
         "<label>Time zone</label><select name=tz>"
         "<option value=auto selected>Automatic (from city)</option>");
  for (int i = 0; i < TZ_N; i++) {
    h += "<option value=\""; h += TZS[i].posix; h += "\">";
    h += TZS[i].label; h += "</option>";
  }
  h += F("</select><button type=submit>Save and restart</button></form></body></html>");
  portal.send(200, "text/html", h);
}

static void handleSave() {
  String ss = portal.arg("ssid2"); ss.trim();
  if (!ss.length()) ss = portal.arg("net");
  String pw = portal.arg("pass");
  String ct = portal.arg("city"); ct.trim();
  String tz = portal.arg("tz");

  if (!ss.length()) {
    portal.send(200, "text/html",
      F("<body style='background:#0b0e13;color:#dff6ea;font-family:system-ui;padding:24px'>"
        "<h2>Pick a network first</h2><a style='color:#5ef0bd' href='/'>Go back</a></body>"));
    return;
  }
  cfgSaveNet(ss, pw, ct, tz);
  portal.send(200, "text/html",
    F("<body style='background:#0b0e13;color:#dff6ea;font-family:system-ui;padding:24px'>"
      "<h2 style='color:#5ef0bd'>Saved</h2><p>The clock is restarting and will join "
      "your network. You can close this page.</p>"
      "<p style='color:#6d7f8c;font-size:13px'>If it can't connect, the setup network "
      "will come back on its own.</p></body>"));
  portalSaved = true;
  portalSavedAt = millis();
}

static void drawPortalScreen(const String &ip, const char *note) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  const char *t = "ORBITAL SETUP";
  u8g2.drawStr((W - u8g2.getStrWidth(t)) / 2, 8, t);
  for (int i = 0; i < W; i += 4) u8g2.drawHLine(i, 11, 2);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 21, "1  JOIN THIS WI-FI");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((W - u8g2.getStrWidth(apName.c_str())) / 2, 32, apName.c_str());

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 43, "2  OPEN IN BROWSER");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((W - u8g2.getStrWidth(ip.c_str())) / 2, 54, ip.c_str());

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr((W - u8g2.getStrWidth(note)) / 2, 63, note);
  u8g2.sendBuffer();
}

// Blocks forever: either the user saves (and we reboot) or they power-cycle.
static void runPortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(true);
  delay(100);

  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  char suffix[8];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  apName = String(AP_PREFIX) + suffix;

  WiFi.softAP(apName.c_str());          // open network, up only during setup
  delay(300);
  String ip = WiFi.softAPIP().toString();

  drawPortalScreen(ip, "SCANNING...");
  scanNets();

  dnsSrv.start(53, "*", WiFi.softAPIP());
  portal.on("/", HTTP_GET, handleRoot);
  portal.on("/save", HTTP_POST, handleSave);
  portal.onNotFound([]() {            // bounce captive-portal probes to the form
    portal.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    portal.send(302, "text/plain", "");
  });
  portal.begin();

  uint32_t lastDraw = 0;
  bool blink = false;
  for (;;) {
    dnsSrv.processNextRequest();
    portal.handleClient();

    if (portalSaved && millis() - portalSavedAt > 1500) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tf);
      const char *a = "SAVED";
      const char *b = "CONNECTING...";
      u8g2.drawStr((W - u8g2.getStrWidth(a)) / 2, 28, a);
      u8g2.drawStr((W - u8g2.getStrWidth(b)) / 2, 42, b);
      u8g2.sendBuffer();
      delay(900);
      ESP.restart();
    }

    if (millis() - lastDraw > 500) {
      lastDraw = millis();
      blink = !blink;
      const char *note;
      if (WiFi.softAPgetStationNum() > 0) note = blink ? "PHONE CONNECTED - OPEN PAGE" : "PHONE CONNECTED";
      else                                note = blink ? "WAITING FOR PHONE" : "";
      drawPortalScreen(ip, note);
    }
    delay(5);
  }
}

// ------------------------- DRAW HELPERS -------------------------------------
static inline void dPixel(int x, int y)               { u8g2.drawPixel(x + gOfsX, y + gOfsY); }
static inline void dStr(int x, int y, const char *s)  { u8g2.drawStr(x + gOfsX, y + gOfsY, s); }
static inline void dBox(int x, int y, int w, int h)   { u8g2.drawBox(x + gOfsX, y + gOfsY, w, h); }
static inline void dFrame(int x, int y, int w, int h) { u8g2.drawFrame(x + gOfsX, y + gOfsY, w, h); }
static inline void dHLine(int x, int y, int w)        { u8g2.drawHLine(x + gOfsX, y + gOfsY, w); }
static inline void dVLine(int x, int y, int h)        { u8g2.drawVLine(x + gOfsX, y + gOfsY, h); }
static inline void dDisc(int x, int y, int r)         { u8g2.drawDisc(x + gOfsX, y + gOfsY, r); }
static inline void dCircle(int x, int y, int r)       { u8g2.drawCircle(x + gOfsX, y + gOfsY, r); }

static void dDashed(int x, int y, int w, int on = 2, int off = 2) {
  for (int i = 0; i < w; i += on + off) {
    int seg = (w - i < on) ? (w - i) : on;
    if (seg > 0) dHLine(x + i, y, seg);
  }
}
static int strW(const char *s) { return u8g2.getStrWidth(s); }
static void dStrRight(int xr, int y, const char *s) { dStr(xr - strW(s), y, s); }

// U8g2 coordinates are unsigned, so negatives wrap instead of clipping.
// Clip lines ourselves (Cohen-Sutherland) before handing them over.
static uint8_t outcode(int x, int y) {
  uint8_t c = 0;
  if (x < 0) c |= 1; else if (x > W - 1) c |= 2;
  if (y < 0) c |= 4; else if (y > H - 1) c |= 8;
  return c;
}
static void cline(int x0, int y0, int x1, int y1) {
  uint8_t c0 = outcode(x0, y0), c1 = outcode(x1, y1);
  for (int guard = 0; guard < 8; guard++) {
    if (!(c0 | c1)) { u8g2.drawLine(x0, y0, x1, y1); return; }
    if (c0 & c1) return;
    uint8_t c = c0 ? c0 : c1;
    int x = 0, y = 0;
    if      (c & 8) { x = x0 + (int)((long)(x1 - x0) * (H - 1 - y0) / (y1 - y0)); y = H - 1; }
    else if (c & 4) { x = x0 + (int)((long)(x1 - x0) * (0     - y0) / (y1 - y0)); y = 0; }
    else if (c & 2) { y = y0 + (int)((long)(y1 - y0) * (W - 1 - x0) / (x1 - x0)); x = W - 1; }
    else            { y = y0 + (int)((long)(y1 - y0) * (0     - x0) / (x1 - x0)); x = 0; }
    if (c == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0); }
    else         { x1 = x; y1 = y; c1 = outcode(x1, y1); }
  }
}

// ------------------------- STARFIELD ----------------------------------------
struct Star { float x; uint8_t y; float v; };
static const uint8_t STAR_N = 26;
static Star stars[STAR_N];

static void starsInit() {
  for (uint8_t i = 0; i < STAR_N; i++) {
    stars[i].x = random(W); stars[i].y = random(H);
    stars[i].v = 0.12f + (random(100) / 100.0f) * 1.1f;
  }
}
static void starsStep() {
  for (uint8_t i = 0; i < STAR_N; i++) {
    stars[i].x -= stars[i].v;
    if (stars[i].x < 0) { stars[i].x = W - 1; stars[i].y = random(H); }
  }
}
static void starsDraw(int maxY) {
  for (uint8_t i = 0; i < STAR_N; i++) {
    int x = (int)stars[i].x, y = stars[i].y;
    if (y >= maxY) continue;
    u8g2.drawPixel(x, y);
    if (stars[i].v > 0.85f) u8g2.drawPixel((x + 1) & (W - 1), y);
  }
}

// ------------------------- TIME ---------------------------------------------
static float subSecond() {
  float f = (millis() - secMark) / 1000.0f;
  return f < 0 ? 0 : (f > 1 ? 1 : f);
}
static void timeTick() {
  time_t raw = time(nullptr);
  timeValid = (raw > 1700000000);
  localtime_r(&raw, &tnow);
  if (tnow.tm_sec != lastSec) { lastSec = tnow.tm_sec; secMark = millis(); }
}
static int hour12(int h) { int r = h % 12; return r == 0 ? 12 : r; }
static float stardate() {
  float dayFrac = (tnow.tm_hour * 3600 + tnow.tm_min * 60 + tnow.tm_sec) / 86400.0f;
  return (tnow.tm_year + 1900 - 2000) * 1000.0f
       + ((tnow.tm_yday + dayFrac) * 1000.0f / 365.0f);
}
static const char *DOW[7]  = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
static const char *MON[12] = { "JAN","FEB","MAR","APR","MAY","JUN",
                               "JUL","AUG","SEP","OCT","NOV","DEC" };
static const char *linkStr() {
  if (WiFi.status() != WL_CONNECTED) return "NO LINK";
  return timeValid ? "NET LOCK" : "SYNCING";
}

// ------------------------- WEATHER ------------------------------------------
struct WxData {
  volatile bool valid = false;
  float tempF = 0, feelF = 0, hiF = 0, loF = 0, wind = 0, hum = 0;
  int   code = 0, isDay = 1;
  float fcHi[5], fcLo[5];
  int   fcCode[5];
  int   fcDays = 0;
  uint32_t stamp = 0;          // millis of last good fetch
  uint32_t tries = 0, fails = 0;
};
static WxData wthr;

// Minimal JSON scrape. The payload is small and fixed-shape, so pulling
// numbers by key beats dragging in a whole parser.
static bool jnum(const char *json, const char *key, float &out) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *p = strstr(json, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p == '[' || *p == ' ') p++;
  if (*p == 'n' || *p == '}' || *p == ']') return false;   // null / missing
  out = atof(p);
  return true;
}

// Same idea for JSON arrays: pull up to maxN numbers out of "key":[...]
static int jarr(const char *scope, const char *key, float *out, int maxN) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":[", key);
  const char *p = strstr(scope, pat);
  if (!p) return 0;
  p += strlen(pat);
  int n = 0;
  while (n < maxN && *p && *p != ']') {
    while (*p == ' ') p++;
    out[n++] = (*p == 'n') ? 0.0f : (float)atof(p);
    while (*p && *p != ',' && *p != ']') p++;
    if (*p == ',') p++;
  }
  return n;
}

// Pull a string value out of JSON: "key":"value"
static bool jstr(const char *json, const char *key, String &out) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char *p = strstr(json, pat);
  if (!p) return false;
  p += strlen(pat);
  out = "";
  while (*p && *p != '"') { out += *p; p++; }
  return out.length() > 0;
}

// Turn the city the user typed into coordinates + an IANA zone.
static bool geocodeCity() {
  if (!cfg.city.length()) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(9000);

  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEnc(cfg.city)
             + "&count=1&language=en&format=json";
  HTTPClient http;
  http.setTimeout(9000);
  if (!http.begin(client, url)) return false;
  if (http.GET() != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();

  const char *j = body.c_str();
  const char *res = strstr(j, "\"results\":");
  if (!res) return false;

  float la, lo;
  if (!jnum(res, "latitude", la) || !jnum(res, "longitude", lo)) return false;

  String iana, nice;
  jstr(res, "timezone", iana);
  jstr(res, "name", nice);

  cfg.lat = la; cfg.lon = lo; cfg.haveLoc = true;
  if (nice.length()) cfg.city = nice;
  String tz = tzResolved(iana);
  cfg.tz = tz;
  cfgSaveLoc(la, lo, cfg.city, tz);
  return true;
}

static void wxFetch() {
  WiFiClientSecure client;
  client.setInsecure();                 // no cert pinning; public read-only API
  client.setTimeout(9000);

  if (!cfg.haveLoc) return;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(cfg.lat, 4)
             + "&longitude=" + String(cfg.lon, 4)
             + "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
               "is_day,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min"
               "&timezone=auto&forecast_days=5"
               "&temperature_unit=fahrenheit&wind_speed_unit=mph";

  HTTPClient http;
  http.setTimeout(9000);
  wthr.tries++;
  if (!http.begin(client, url)) { wthr.fails++; return; }
  int code = http.GET();
  if (code != 200) { http.end(); wthr.fails++; return; }

  String body = http.getString();
  http.end();
  const char *j = body.c_str();

  // Anchor to the "current" and "daily" objects. Open-Meteo emits
  // "current_units" first, where every key's value is a unit STRING -- parsing
  // from the top of the document would read the temperature as 0.
  const char *cur = strstr(j, "\"current\":");
  const char *dly = strstr(j, "\"daily\":");
  if (!cur) { wthr.fails++; return; }

  float t, rh = 0, ap = 0, isd = 1, wc = 0, ws = 0;
  if (!jnum(cur, "temperature_2m", t) || !jnum(cur, "weather_code", wc)) {
    wthr.fails++; return;
  }
  jnum(cur, "relative_humidity_2m", rh);
  jnum(cur, "apparent_temperature", ap);
  jnum(cur, "is_day", isd);
  jnum(cur, "wind_speed_10m", ws);

  wthr.tempF = t; wthr.hum = rh; wthr.feelF = ap; wthr.isDay = (int)isd;
  wthr.code = (int)wc; wthr.wind = ws;

  if (dly) {
    float a[5], b[5], c[5];
    int nc = jarr(dly, "weather_code", c, 5);
    int na = jarr(dly, "temperature_2m_max", a, 5);
    int nb = jarr(dly, "temperature_2m_min", b, 5);
    int n = nc; if (na < n) n = na; if (nb < n) n = nb;
    for (int i = 0; i < n; i++) {
      wthr.fcCode[i] = (int)c[i];
      wthr.fcHi[i]   = a[i];
      wthr.fcLo[i]   = b[i];
    }
    wthr.fcDays = n;
    if (n > 0) { wthr.hiF = a[0]; wthr.loF = b[0]; }
  }

  wthr.stamp = millis();
  wthr.valid = true;
}

static void wxTask(void *) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!cfg.haveLoc && geocodeCity()) {
        String tz = tzResolved("");          // city resolved late: fix the clock too
        configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
      }
      wxFetch();
    }
    vTaskDelay(pdMS_TO_TICKS(wthr.valid ? 900000UL : 60000UL));  // 15min / retry 1min
  }
}

// WMO weather code -> icon id + label
enum WxIcon : uint8_t { IC_SUN, IC_PARTLY, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };
static uint8_t wxIcon(int c) {
  if (c == 0) return IC_SUN;
  if (c <= 2)  return IC_PARTLY;
  if (c == 3)  return IC_CLOUD;
  if (c <= 48) return IC_FOG;
  if (c <= 57) return IC_RAIN;
  if (c <= 67) return IC_RAIN;
  if (c <= 77) return IC_SNOW;
  if (c <= 82) return IC_RAIN;
  if (c <= 86) return IC_SNOW;
  return IC_STORM;
}
static const char *wxLabel(int c) {
  if (c == 0)  return "CLEAR";
  if (c <= 2)  return "PARTLY CLOUDY";
  if (c == 3)  return "OVERCAST";
  if (c <= 48) return "FOG";
  if (c <= 57) return "DRIZZLE";
  if (c <= 67) return "RAIN";
  if (c <= 77) return "SNOW";
  if (c <= 82) return "SHOWERS";
  if (c <= 86) return "SNOW SHOWERS";
  return "THUNDERSTORM";
}

// ------------------------- GLITCH -------------------------------------------
static void applyGlitch() {
  if (!GLITCH_FX) return;
  uint32_t t = millis();
  if (t > nextGlitch) {
    glitchUntil = t + random(80, 220);
    nextGlitch  = t + random(7000, 22000);
  }
  if (t > glitchUntil) return;
  uint8_t *buf = u8g2.getBufferPtr();
  static uint8_t tmp[W];
  uint8_t bands = random(1, 4);
  for (uint8_t b = 0; b < bands; b++) {
    uint8_t row = random(0, 8);
    int sh = random(-7, 8);
    if (sh == 0) continue;
    uint8_t *p = buf + (uint16_t)row * W;
    for (int x = 0; x < W; x++) tmp[x] = p[(x - sh + W) & (W - 1)];
    memcpy(p, tmp, W);
  }
  if (random(5) == 0) u8g2.drawHLine(0, random(H), W);
}

// ------------------------- FACE 1: NEON -------------------------------------
static void faceNeon() {
  char buf[24];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 6, CALLSIGN);
  dStrRight(W, 6, linkStr());
  dDashed(0, 9, W);

  char hh[4], mm[4];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(hh, sizeof(hh), use24h ? "%02d" : "%d", h);
  snprintf(mm, sizeof(mm), "%02d", tnow.tm_min);

  u8g2.setFont(u8g2_font_logisoso28_tn);
  int wh = strW(hh), wm = strW(mm), gap = 13;
  int x0 = (W - (wh + gap + wm)) / 2 - 6;
  if (x0 < 1) x0 = 1;
  const int base = 44;
  dStr(x0, base, hh);
  dStr(x0 + wh + gap, base, mm);
  if (subSecond() < 0.55f) {
    dBox(x0 + wh + 5, base - 21, 4, 4);
    dBox(x0 + wh + 5, base - 9,  4, 4);
  }

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%02d", tnow.tm_sec);
  int sx = x0 + wh + gap + wm + 4;
  if (sx > W - 12) sx = W - 12;
  dStr(sx, 26, buf);
  if (!use24h) dStr(sx, 44, tnow.tm_hour < 12 ? "AM" : "PM");

  snprintf(buf, sizeof(buf), "%s %s %02d", DOW[tnow.tm_wday % 7],
           MON[tnow.tm_mon % 12], tnow.tm_mday);
  dStr(0, 53, buf);
  snprintf(buf, sizeof(buf), "SD%.0f", stardate());
  dStrRight(W, 53, buf);

  dFrame(0, 56, W, 6);
  float p = (tnow.tm_sec + subSecond()) / 60.0f;
  int fill = (int)(p * (W - 4));
  if (fill > 0) dBox(2, 58, fill, 2);
}

// ------------------------- FACE 2: ORBIT ------------------------------------
static void polar(int cx, int cy, float deg, int r, int &x, int &y) {
  float a = (deg - 90.0f) * 0.01745329f;
  x = cx + (int)(cosf(a) * r);
  y = cy + (int)(sinf(a) * r);
}
static void faceOrbit() {
  char buf[24];
  const int cx = 31, cy = 32;
  int x, y;
  for (uint8_t i = 0; i < 12; i++) {
    polar(cx, cy, i * 30.0f, 29, x, y); dPixel(x, y);
    if (i % 3 == 0) { polar(cx, cy, i * 30.0f, 26, x, y); dPixel(x, y); }
  }
  dCircle(cx, cy, 21);
  dDisc(cx, cy, 4);
  dCircle(cx, cy, 7);

  float secF  = tnow.tm_sec + subSecond();
  float minF  = tnow.tm_min + secF / 60.0f;
  float hourF = (tnow.tm_hour % 12) + minF / 60.0f;
  polar(cx, cy, secF * 6.0f, 28, x, y);   dDisc(x, y, 1);
  polar(cx, cy, minF * 6.0f, 21, x, y);   dDisc(x, y, 2);
  polar(cx, cy, hourF * 30.0f, 14, x, y); dDisc(x, y, 3);
  dCircle(x, y, 5);

  dVLine(63, 2, 60);
  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%s %02d.%02d", DOW[tnow.tm_wday % 7],
           tnow.tm_mon + 1, tnow.tm_mday);
  dStr(68, 10, buf);

  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d", h, tnow.tm_min);
  u8g2.setFont(u8g2_font_logisoso20_tn);
  dStrRight(W - 1, 36, buf);

  u8g2.setFont(u8g2_font_5x7_tf);
  snprintf(buf, sizeof(buf), "%02d%s", tnow.tm_sec,
           use24h ? "" : (tnow.tm_hour < 12 ? " AM" : " PM"));
  dStrRight(W - 1, 46, buf);
  snprintf(buf, sizeof(buf), "SD %.1f", stardate());
  dStr(68, 60, buf);
  dDashed(66, 51, 60);
}

// ------------------------- FACE 3: TERMINAL ---------------------------------
static const uint8_t LOG_ROWS = 7;
static char logBuf[LOG_ROWS][30];
static uint32_t lastLogPush = 0;

static void logPush(const char *s) {
  for (uint8_t i = 0; i < LOG_ROWS - 1; i++) strcpy(logBuf[i], logBuf[i + 1]);
  strncpy(logBuf[LOG_ROWS - 1], s, sizeof(logBuf[0]) - 1);
  logBuf[LOG_ROWS - 1][sizeof(logBuf[0]) - 1] = '\0';
}
static void logRandom() {
  char b[30];
  uint32_t up = (millis() - bootMillis) / 1000;
  switch (random(13)) {
    case 0:  snprintf(b, sizeof(b), "RSSI %d dBm / CH %d",
                      (int)WiFi.RSSI(), (int)WiFi.channel()); break;
    case 1:  snprintf(b, sizeof(b), "STARDATE %.1f", stardate()); break;
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
    case 11: snprintf(b, sizeof(b), "NTP %s", timeValid ? "LOCKED" : "DRIFTING"); break;
    default: snprintf(b, sizeof(b), "NO ANOMALIES DETECTED"); break;
  }
  logPush(b);
}
static void faceTerminal() {
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

// ------------------------- FACE 5: FLYOVER ----------------------------------
static float flyZ = 0;
static float terrainH(float x, float z) {
  return sinf(x * 0.45f + sinf(z * 0.21f)) * 2.6f
       + sinf(x * 0.17f - z * 0.11f) * 5.0f
       + sinf(z * 0.33f) * 1.4f;
}
static void faceFlyover() {
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

// ------------------------- FACE 6: TESSERACT --------------------------------
static void faceTesseract() {
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

// ------------------------- FACE 7: JELLYFISH --------------------------------
struct Jelly { float x, y, vx, phase; int r; };
static Jelly jel[3];
struct Bub { float x, y, v, sway; };
static const uint8_t BUB_N = 10;
static Bub bubs[BUB_N];

static void jellyInit() {
  for (uint8_t i = 0; i < 3; i++) {
    jel[i].r     = 8 + i * 3;
    jel[i].x     = 20 + i * 42;
    jel[i].y     = 16 + (i % 2) * 12;
    jel[i].vx    = (i % 2 ? 0.22f : -0.16f);
    jel[i].phase = i * 2.1f;
  }
  for (uint8_t i = 0; i < BUB_N; i++) {
    bubs[i].x = random(4, W - 4);
    bubs[i].y = random(4, H - 4);
    bubs[i].v = 0.18f + random(60) / 200.0f;
    bubs[i].sway = random(628) / 100.0f;
  }
}
static void faceJelly() {
  float t = millis() * 0.001f;

  for (uint8_t i = 0; i < BUB_N; i++) {
    bubs[i].y -= bubs[i].v;
    if (bubs[i].y < 3) { bubs[i].y = H - 4; bubs[i].x = random(4, W - 4); }
    int bx = (int)(bubs[i].x + sinf(t * 1.6f + bubs[i].sway) * 2.0f);
    if (bx > 1 && bx < W - 2) u8g2.drawCircle(bx, (int)bubs[i].y, 1);
  }

  for (uint8_t i = 0; i < 3; i++) {
    Jelly &j = jel[i];
    j.x += j.vx;
    if (j.x < j.r + 2)     { j.x = j.r + 2;     j.vx = -j.vx; }
    if (j.x > W - j.r - 2) { j.x = W - j.r - 2; j.vx = -j.vx; }

    int cx = (int)j.x;
    int cy = (int)(j.y + sinf(t * 1.25f + j.phase) * 3.0f);
    float pulse = 1.0f + 0.14f * sinf(t * 2.4f + j.phase);
    int r = (int)(j.r * pulse);

    // bell: upper dome plus a scalloped rim
    u8g2.drawCircle(cx, cy, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    for (int k = -r + 1; k <= r - 1; k += 3)
      u8g2.drawPixel(cx + k, cy + (((k + r) / 3) % 2 ? 1 : 0));

    // tentacles, each a chain of segments with a travelling wave
    for (int k = 0; k < 4; k++) {
      float off = (k - 1.5f) * (r * 0.5f);
      int lx = cx + (int)off, ly = cy + 2;
      for (int s = 1; s <= 5; s++) {
        int ny = cy + 2 + s * 3;
        int nx = cx + (int)(off + sinf(t * 2.3f + j.phase + s * 0.7f + k * 1.3f) * (s * 0.7f));
        cline(lx, ly, nx, ny);
        lx = nx; ly = ny;
      }
    }
  }
}

// ------------------------- FACE 8: BOUNCE -----------------------------------
static float bx = 20, by = 20, bvx = 1.15f, bvy = 0.78f;
static uint16_t bHits = 0, bCorners = 0;

static void faceBounce() {
  const int bw = 36, bh = 15;
  bx += bvx; by += bvy;
  bool hx = false, hy = false;
  if (bx < 0)          { bx = 0;          bvx = -bvx; hx = true; }
  if (bx > W - bw)     { bx = W - bw;     bvx = -bvx; hx = true; }
  if (by < 0)          { by = 0;          bvy = -bvy; hy = true; }
  if (by > H - bh)     { by = H - bh;     bvy = -bvy; hy = true; }
  if (hx || hy) bHits++;
  if (hx && hy) bCorners++;

  u8g2.drawRFrame((int)bx, (int)by, bw, bh, 3);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr((int)bx + 6, (int)by + 10, "ESP32");

  char b[28];
  snprintf(b, sizeof(b), "HITS %u   CORNERS %u", bHits, bCorners);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setDrawColor(2);                 // XOR so it survives the box passing over
  u8g2.drawStr(2, 62, b);
  u8g2.setDrawColor(1);
}

// ------------------------- FACE: DIGITAL (plain) ----------------------------
// Deliberately undecorated: no starfield, no glitch, just the time.
static void faceDigital() {
  char buf[24];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, tnow.tm_min, tnow.tm_sec);

  u8g2.setFont(u8g2_font_logisoso20_tn);
  int w = strW(buf);
  dStr((W - w) / 2, 42, buf);

  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(buf, sizeof(buf), "%s %02d %s %d", DOW[tnow.tm_wday % 7], tnow.tm_mday,
           MON[tnow.tm_mon % 12], tnow.tm_year + 1900);
  w = strW(buf);
  dStr((W - w) / 2, 15, buf);

  if (!use24h) {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char *ap = tnow.tm_hour < 12 ? "AM" : "PM";
    dStr((W - strW(ap)) / 2, 58, ap);
  }
}

// ------------------------- FACE: RAIN ---------------------------------------
static const int RAIN_COLS = 21, RAIN_ROWS = 10;
static float   rainY[RAIN_COLS];
static float   rainV[RAIN_COLS];
static uint8_t rainLen[RAIN_COLS];
static uint32_t rainSeed = 1, rainSeedAt = 0;
static const char RAIN_SET[] = "0123456789ABCDEF<>/\\|=+*#$%&";

static void rainInit() {
  for (int c = 0; c < RAIN_COLS; c++) {
    rainY[c]   = -(float)random(0, 30);
    rainV[c]   = 0.10f + random(70) / 300.0f;
    rainLen[c] = 4 + random(9);
  }
}
static char rainGlyph(int c, int r) {
  uint32_t hsh = (uint32_t)c * 73856093u ^ (uint32_t)r * 19349663u ^ rainSeed;
  hsh ^= hsh >> 13; hsh *= 2654435761u; hsh ^= hsh >> 16;
  return RAIN_SET[hsh % (sizeof(RAIN_SET) - 1)];
}
static void faceRain() {
  if (millis() - rainSeedAt > 130) { rainSeedAt = millis(); rainSeed = random(1, 100000); }
  u8g2.setFont(u8g2_font_4x6_tr);
  char g[2] = { 0, 0 };

  for (int c = 0; c < RAIN_COLS; c++) {
    rainY[c] += rainV[c];
    int head = (int)rainY[c];
    if (head - rainLen[c] > RAIN_ROWS) {
      rainY[c]   = -(float)random(0, 14);
      rainV[c]   = 0.10f + random(70) / 300.0f;
      rainLen[c] = 4 + random(9);
      continue;
    }
    int x = c * 6 + 1;
    for (int k = 0; k < rainLen[c]; k++) {
      int r = head - k;
      if (r < 0 || r >= RAIN_ROWS) continue;
      int yb = 7 + r * 6;
      g[0] = rainGlyph(c, r);
      if (k == 0) {                       // bright head
        u8g2.drawBox(x - 1, yb - 6, 6, 7);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x, yb, g);
        u8g2.setDrawColor(1);
      } else if (k < 3 || (k & 1) == 0) { // thinning tail
        u8g2.drawStr(x, yb, g);
      }
    }
  }

  // time panel punched through the rain
  char buf[12], sec[4];
  int h = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  snprintf(buf, sizeof(buf), "%d:%02d", h, tnow.tm_min);
  snprintf(sec, sizeof(sec), "%02d", tnow.tm_sec);

  const int px = 14, py = 19, pw = 100, ph = 27;
  u8g2.setDrawColor(0); u8g2.drawBox(px, py, pw, ph);
  u8g2.setDrawColor(1); u8g2.drawFrame(px, py, pw, ph);
  u8g2.drawHLine(px + 3, py + 2, pw - 6);

  u8g2.setFont(u8g2_font_logisoso20_tn);
  int wt = strW(buf);
  u8g2.setFont(u8g2_font_5x7_tf);
  int ws2 = strW(sec);
  int total = wt + 3 + ws2;
  int tx = px + (pw - total) / 2;
  u8g2.setFont(u8g2_font_logisoso20_tn);
  u8g2.drawStr(tx, py + 24, buf);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(tx + wt + 3, py + 24, sec);
}

// ------------------------- FACE: DECRYPT ------------------------------------
// Each digit scrambles through random glyphs when it changes, then locks.
// Seconds are therefore always churning; hours sit still for an hour.
static uint8_t  decPrev[6] = { 99, 99, 99, 99, 99, 99 };
static uint32_t decLock[6] = { 0, 0, 0, 0, 0, 0 };

static char decGlyph(int slot, uint32_t tick) {
  uint32_t h = (uint32_t)slot * 2654435761u ^ tick * 40503u;
  h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
  return '0' + (h % 10);
}
static char decHex(int slot, uint32_t tick) {
  static const char HX[] = "0123456789ABCDEF";
  uint32_t h = (uint32_t)slot * 374761393u ^ tick * 668265263u;
  h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
  return HX[h % 16];
}

static void faceDecrypt() {
  uint32_t now = millis();
  int hr = use24h ? tnow.tm_hour : hour12(tnow.tm_hour);
  uint8_t dig[6] = { (uint8_t)(hr / 10), (uint8_t)(hr % 10),
                     (uint8_t)(tnow.tm_min / 10), (uint8_t)(tnow.tm_min % 10),
                     (uint8_t)(tnow.tm_sec / 10), (uint8_t)(tnow.tm_sec % 10) };

  uint8_t locked = 0;
  for (int i = 0; i < 6; i++) {
    if (dig[i] != decPrev[i]) {
      decPrev[i] = dig[i];
      decLock[i] = now + (i >= 4 ? 260 : 520);   // seconds settle faster
    }
    if (now >= decLock[i]) locked++;
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  dStr(0, 7, "DECRYPT");
  char buf[24];
  snprintf(buf, sizeof(buf), "KEY %d/6", locked);
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
    one[0] = (now < decLock[i]) ? decGlyph(i, tick) : ('0' + dig[i]);
    dStr(x, base, one);
    if (now < decLock[i]) dHLine(x, base + 2, dw - 2);   // "working" underline
  }

  // blinking colon
  if (subSecond() < 0.6f) {
    dBox(x0 + 2 * dw + 4, base - 21, 4, 4);
    dBox(x0 + 2 * dw + 4, base - 9,  4, 4);
  }

  // seconds
  u8g2.setFont(u8g2_font_6x10_tf);
  for (int i = 4; i < 6; i++) {
    one[0] = (now < decLock[i]) ? decGlyph(i, tick) : ('0' + dig[i]);
    dStr(x0 + total + 5 + (i - 4) * 6, base, one);
  }

  // corner brackets around the cipher block
  const int bx = 4, by = 12, bw2 = 120, bh2 = 40, L = 6;
  dHLine(bx, by, L);                dVLine(bx, by, L);
  dHLine(bx + bw2 - L, by, L);      dVLine(bx + bw2 - 1, by, L);
  dHLine(bx, by + bh2 - 1, L);      dVLine(bx, by + bh2 - L, L);
  dHLine(bx + bw2 - L, by + bh2 - 1, L);
  dVLine(bx + bw2 - 1, by + bh2 - L, L);

  // key material churning along the bottom
  u8g2.setFont(u8g2_font_4x6_tr);
  char key[17];
  uint32_t ktick = now / 90;
  for (int k = 0; k < 16; k++)
    key[k] = (k < locked * 2) ? decHex(k, 0) : decHex(k, ktick);
  key[16] = '\0';
  dStr(0, 62, key);

  // lock pips
  for (int i = 0; i < 6; i++) {
    int x = 78 + i * 8;
    if (now >= decLock[i]) dBox(x, 56, 6, 6);
    else                   dFrame(x, 56, 6, 6);
  }
}

// ------------------------- FACE: WEATHER ------------------------------------
static void wxDrawCloud(int cx, int cy, int s) {
  u8g2.drawDisc(cx - s, cy, s);
  u8g2.drawDisc(cx + s - 1, cy, s - 1);
  u8g2.drawDisc(cx, cy - s + 1, s);
  u8g2.drawBox(cx - s, cy, s * 2, s);
}
static void wxDrawSun(int cx, int cy, int r) {
  u8g2.drawDisc(cx, cy, r);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    int x1 = cx + (int)(cosf(a) * (r + 2)), y1 = cy + (int)(sinf(a) * (r + 2));
    int x2 = cx + (int)(cosf(a) * (r + 5)), y2 = cy + (int)(sinf(a) * (r + 5));
    cline(x1, y1, x2, y2);
  }
}
static void wxDrawMoon(int cx, int cy, int r) {
  u8g2.drawDisc(cx, cy, r);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(cx + r - 2, cy - 2, r);
  u8g2.setDrawColor(1);
}
static void wxDrawIcon(uint8_t ic, int cx, int cy, bool day) {
  float t = millis() * 0.001f;
  switch (ic) {
    case IC_SUN:
      if (day) wxDrawSun(cx, cy, 7); else wxDrawMoon(cx, cy, 8);
      break;
    case IC_PARTLY:
      if (day) wxDrawSun(cx - 4, cy - 7, 4); else wxDrawMoon(cx - 4, cy - 7, 5);
      wxDrawCloud(cx + 3, cy + 5, 6);
      break;
    case IC_CLOUD:
      wxDrawCloud(cx, cy, 8);
      break;
    case IC_FOG:
      wxDrawCloud(cx, cy - 4, 7);
      for (int i = 0; i < 3; i++) {
        int off = (int)(sinf(t * 1.4f + i) * 3.0f);
        u8g2.drawHLine(cx - 9 + off, cy + 7 + i * 4, 18);
      }
      break;
    case IC_RAIN:
      wxDrawCloud(cx, cy - 5, 7);
      for (int i = 0; i < 4; i++) {
        int ph = ((int)(t * 26) + i * 5) % 14;
        int dx = cx - 9 + i * 6;
        cline(dx, cy + 3 + ph, dx - 2, cy + 7 + ph);
      }
      break;
    case IC_SNOW:
      wxDrawCloud(cx, cy - 5, 7);
      for (int i = 0; i < 4; i++) {
        int ph = ((int)(t * 14) + i * 5) % 14;
        int dx = cx - 9 + i * 6, dy = cy + 4 + ph;
        u8g2.drawHLine(dx - 1, dy, 3);
        u8g2.drawVLine(dx, dy - 1, 3);
      }
      break;
    case IC_STORM:
      wxDrawCloud(cx, cy - 5, 7);
      if (fmodf(t, 1.6f) > 0.25f) {
        cline(cx + 1, cy + 3, cx - 4, cy + 10);
        cline(cx - 4, cy + 10, cx + 1, cy + 10);
        cline(cx + 1, cy + 10, cx - 3, cy + 16);
      }
      break;
  }
}

static void faceWeather() {
  char buf[36];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 7, cityLabel());

  if (!wthr.valid) {
    dStrRight(W, 7, WiFi.status() == WL_CONNECTED ? "FETCHING" : "NO LINK");
    dDashed(0, 10, W);
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = cfg.haveLoc ? "NO WEATHER DATA" : "NO LOCATION SET";
    dStr((W - strW(m)) / 2, 34, m);
    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(buf, sizeof(buf), "TRIES %lu  FAILS %lu",
             (unsigned long)wthr.tries, (unsigned long)wthr.fails);
    dStr((W - strW(buf)) / 2, 46, buf);
    return;
  }

  uint32_t ageMin = (millis() - wthr.stamp) / 60000UL;
  if (ageMin == 0) snprintf(buf, sizeof(buf), "NOW");
  else             snprintf(buf, sizeof(buf), "%lum AGO", (unsigned long)ageMin);
  dStrRight(W, 7, buf);
  dDashed(0, 10, W);

  wxDrawIcon(wxIcon(wthr.code), 23, 31, wthr.isDay != 0);

  // temperature, with a hand-drawn degree mark
  snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.tempF));
  u8g2.setFont(u8g2_font_logisoso20_tn);
  dStr(50, 36, buf);
  int tw = strW(buf);
  dCircle(50 + tw + 4, 20, 2);

  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(50, 47, wxLabel(wthr.code));

  u8g2.setFont(u8g2_font_4x6_tr);
  snprintf(buf, sizeof(buf), "H%d L%d", (int)lroundf(wthr.hiF), (int)lroundf(wthr.loF));
  dStr(0, 62, buf);
  snprintf(buf, sizeof(buf), "FEELS %d", (int)lroundf(wthr.feelF));
  dStr(44, 62, buf);
  snprintf(buf, sizeof(buf), "%dMPH %d%%", (int)lroundf(wthr.wind), (int)lroundf(wthr.hum));
  dStrRight(W, 62, buf);
}

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

static void faceForecast() {
  char buf[24];
  u8g2.setFont(u8g2_font_5x7_tf);
  dStr(0, 8, "5-DAY");
  dStrRight(W, 8, cityLabel());
  dDashed(0, 11, W);

  if (!wthr.valid || wthr.fcDays == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *m = !cfg.haveLoc ? "NO LOCATION SET"
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
    const char *d = (i == 0) ? "TODAY" : DOW[(tnow.tm_wday + i) % 7];
    dStr(cx[i] - strW(d) / 2, 20, d);

    wxIconSmall(wxIcon(wthr.fcCode[i]), cx[i], 34);

    u8g2.setFont(u8g2_font_5x7_tf);
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.fcHi[i]));
    dStr(cx[i] - strW(buf) / 2, 53, buf);

    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(wthr.fcLo[i]));
    dStr(cx[i] - strW(buf) / 2, 61, buf);
  }
}

// ------------------------- FACE LABEL ---------------------------------------
static void drawFaceLabel() {
  if (millis() > labelUntil) return;
  u8g2.setFont(u8g2_font_4x6_tr);
  const char *n = FACE_NAMES[face];
  int w = u8g2.getStrWidth(n);
  int x = (W - w) / 2;
  u8g2.setDrawColor(0); u8g2.drawBox(x - 4, 51, w + 8, 11);
  u8g2.setDrawColor(1); u8g2.drawFrame(x - 4, 51, w + 8, 11);
  u8g2.drawStr(x, 59, n);
}

// ------------------------- BUTTON -------------------------------------------
static void nextFace() {
  face = (face + 1) % FACE_COUNT;
  lastFaceSw = millis();
  labelUntil = millis() + 1000;
}
// Button thresholds, all on the one BOOT key:
//   tap            -> next face
//   hold  0.8s     -> toggle 12/24 hour
//   hold  8s       -> erase saved Wi-Fi + location, reboot into setup
static const uint32_t HOLD_FORMAT_MS = 800;
static const uint32_t HOLD_RESET_MS  = 8000;
static const uint32_t HOLD_WARN_MS   = 1800;   // when the warning appears

static bool     btnDown = false;
static uint32_t btnDownAt = 0;

static uint32_t btnHeldMs() {
  return btnDown ? (millis() - btnDownAt) : 0;
}

static void factoryReset() {
  cfgClear();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  const char *a = "CONFIG ERASED";
  const char *b = "RESTARTING";
  u8g2.drawStr((W - u8g2.getStrWidth(a)) / 2, 28, a);
  u8g2.drawStr((W - u8g2.getStrWidth(b)) / 2, 42, b);
  u8g2.sendBuffer();
  delay(1400);
  ESP.restart();
}

static void handleButton() {
  static bool longFired = false;
  bool pressed = (digitalRead(PIN_BTN) == LOW);

  if (pressed && !btnDown) { btnDown = true; btnDownAt = millis(); longFired = false; }
  else if (pressed && btnDown) {
    uint32_t held = millis() - btnDownAt;
    if (!longFired && held > HOLD_FORMAT_MS) { longFired = true; use24h = !use24h; }
    if (held >= HOLD_RESET_MS) factoryReset();      // does not return
  }
  else if (!pressed && btnDown) {
    btnDown = false;
    if (!longFired && millis() - btnDownAt > 40) nextFace();
  }
}

// Countdown overlay so an 8-second hold is never a surprise -- and so a user
// who didn't mean it has six seconds to let go.
static void drawResetOverlay() {
  uint32_t held = btnHeldMs();
  if (held < HOLD_WARN_MS) return;

  float p = (float)(held - HOLD_WARN_MS) / (float)(HOLD_RESET_MS - HOLD_WARN_MS);
  if (p < 0) p = 0; if (p > 1) p = 1;
  int secsLeft = (int)((HOLD_RESET_MS - held + 999) / 1000);
  if (secsLeft < 0) secsLeft = 0;

  u8g2.setDrawColor(0); u8g2.drawBox(4, 16, 120, 32);
  u8g2.setDrawColor(1); u8g2.drawFrame(4, 16, 120, 32);

  u8g2.setFont(u8g2_font_5x7_tf);
  const char *t = "KEEP HOLDING TO RESET";
  u8g2.drawStr((W - u8g2.getStrWidth(t)) / 2, 27, t);

  u8g2.drawFrame(10, 31, 108, 8);
  int fill = (int)(p * 104);
  if (fill > 0) u8g2.drawBox(12, 33, fill, 4);

  char b[24];
  snprintf(b, sizeof(b), "ERASING IN %d", secsLeft);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr((W - u8g2.getStrWidth(b)) / 2, 46, b);
}

// ------------------------- BOOT SEQUENCE ------------------------------------
static void bootLine(const char *s, uint16_t hold) {
  logPush(s);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);
  for (uint8_t i = 0; i < LOG_ROWS; i++)
    if (logBuf[i][0]) u8g2.drawStr(6, 6 + i * 7, logBuf[i]);
  u8g2.drawBox(0, 56, 4, 6);
  u8g2.sendBuffer();
  delay(hold);
}

// ------------------------- SETUP / LOOP -------------------------------------
void setup() {
  Serial.begin(115200);
  bootMillis = millis();
  pinMode(PIN_BTN, INPUT_PULLUP);
  randomSeed(esp_random());

  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  u8g2.setBusClock(8000000);
  u8g2.begin();
  u8g2.setContrast(BRIGHT_DAY);
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);

  starsInit();
  jellyInit();
  rainInit();
  for (uint8_t i = 0; i < LOG_ROWS; i++) logBuf[i][0] = '\0';

  bootLine("WAKE // COLD BOOT", 320);
  bootLine("SSD1309 PANEL .... OK", 260);

  cfgLoad();

  // BOOT held at power-on wipes the stored configuration.
  if (digitalRead(PIN_BTN) == LOW) {
    uint32_t held = millis();
    while (digitalRead(PIN_BTN) == LOW && millis() - held < 2000) delay(20);
    if (millis() - held >= 2000) {
      cfgClear();
      cfgLoad();
      bootLine("CONFIG ERASED", 700);
    }
  }

  if (!cfg.ssid.length()) {
    bootLine("NO CONFIG - SETUP", 700);
    runPortal();                       // never returns
  }

  char b[30];
  snprintf(b, sizeof(b), "JOIN %.18s", cfg.ssid.c_str());
  bootLine(b, 200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);

  if (WiFi.status() == WL_CONNECTED) {
    bootLine("UPLINK ........... OK", 220);

    if (!cfg.haveLoc && cfg.city.length()) {
      bootLine("LOCATING CITY ...", 120);
      bootLine(geocodeCity() ? "GEOCODE .......... OK"
                             : "GEOCODE ........ FAIL", 240);
    }
    String tz = tzResolved("");
    configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
    t0 = millis();
    while (time(nullptr) < 1700000000 && millis() - t0 < 12000) delay(200);
    bootLine(time(nullptr) > 1700000000 ? "NTP HANDSHAKE .... OK"
                                        : "NTP HANDSHAKE .. FAIL", 260);
  } else {
    // Bad password or the network moved: fall back to the setup portal rather
    // than sitting on a dead screen.
    bootLine("UPLINK ......... FAIL", 400);
    bootLine("REOPENING SETUP", 700);
    runPortal();
  }
  bootLine("ALL SYSTEMS NOMINAL", 500);

  // Weather runs on its own task so a slow TLS handshake never stutters
  // the 25fps render loop. Needs a fat stack for mbedtls.
  xTaskCreatePinnedToCore(wxTask, "wx", 12288, nullptr, 1, nullptr, 0);

  timeTick();
}

void loop() {
  handleButton();

  if (FACE_AUTO_MS && millis() - lastFaceSw > FACE_AUTO_MS) nextFace();

  static uint32_t lastNet = 0;
  if (millis() - lastNet > 30000) {
    lastNet = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }

  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  timeTick();

  if (ANTI_BURNIN && millis() - lastDrift > 60000) {
    lastDrift = millis();
    gOfsX = random(-2, 3);
    gOfsY = random(-1, 2);
  }

  static int lastBright = -1;
  int want = (tnow.tm_hour >= 22 || tnow.tm_hour < 7) ? BRIGHT_NIGHT : BRIGHT_DAY;
  if (want != lastBright) { lastBright = want; u8g2.setContrast(want); }

  starsStep();
  u8g2.clearBuffer();

  switch (face) {
    case FACE_NEON:      starsDraw(H);  faceNeon();      break;
    case FACE_DIGITAL:                  faceDigital();   break;
    case FACE_RAIN:                     faceRain();      break;
    case FACE_DECRYPT:                  faceDecrypt();   break;
    case FACE_WEATHER:                  faceWeather();   break;
    case FACE_FORECAST:                 faceForecast();  break;
    case FACE_ORBIT:     starsDraw(H);  faceOrbit();     break;
    case FACE_TERMINAL:                 faceTerminal();  break;
    case FACE_FLYOVER:   starsDraw(20); faceFlyover();   break;
    case FACE_TESSERACT: starsDraw(H);  faceTesseract(); break;
    case FACE_JELLY:                    faceJelly();     break;
    case FACE_BOUNCE:                   faceBounce();    break;
  }

  if (face == FACE_NEON || face == FACE_RAIN || face == FACE_DECRYPT ||
      face == FACE_ORBIT || face == FACE_TERMINAL) applyGlitch();
  drawFaceLabel();
  drawResetOverlay();
  u8g2.sendBuffer();
}
