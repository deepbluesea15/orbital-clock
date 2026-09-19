# ORBITAL CLOCK v1.0 — Transparent OLED Desk Clock

A see-through desk clock with 14 switchable faces: digital clocks, live weather
and a 5-day forecast, plus a few things that are just nice to look at — a
wireframe terrain flyover, a rotating 4D hypercube, and drifting jellyfish.

Everything runs on the device. No app, no account, no soldering.

**Code + firmware:** https://github.com/deepbluesea15/orbital-clock

---

## What you need

- ESP32-S3 SuperMini (4 MB flash)
- Waveshare 1.51" Transparent OLED — 128x64, SSD1309
  https://www.waveshare.com/1.51inch-transparent-oled.htm
- 7 female-to-female jumper wires
- USB-C cable
- The printed parts from this model

Total electronics cost is roughly $25-30.

---

## 1. Print

<!-- TODO: fill in from your own slicer profile -->
- Layer height:
- Infill:
- Supports:
- Filament: (black or dark filament makes the transparent panel pop)
- Parts:

---

## 2. Wire it up

Seven wires, no soldering. The display module comes pre-configured for SPI, so
nothing on the board needs changing.

- VCC goes to 3V3 (not 5V)
- GND goes to GND
- DIN goes to GPIO11
- CLK goes to GPIO12
- CS goes to GPIO10
- DC goes to GPIO9
- RST goes to GPIO8

That's it. The button used to switch faces is the BOOT button already on the
ESP32 board.

---

## 3. Flash the firmware

No software to install — this happens in your browser. Chrome or Edge only
(Safari and Firefox can't talk to USB devices).

1. Download `OrbitalClockv1.0.bin` from
   https://github.com/deepbluesea15/orbital-clock/releases/tag/v1.0
2. Plug the ESP32 into your computer with USB-C.
3. Go to https://web.esphome.io
4. Click **Connect**, pick the board from the list.
5. Choose **Install from file**, select the `.bin` you downloaded, confirm.

Takes about a minute. When it finishes, the screen lights up.

**If the board doesn't show up in the list:** unplug it, hold the BOOT button
down, plug it back in, then release. Retry step 4.

---

## 4. Set up Wi-Fi and location

On first boot the clock creates its own Wi-Fi network and shows instructions
right on the screen:

```
      ORBITAL SETUP
  ------------------------
  1  JOIN THIS WI-FI
        ORBITAL-3F7A
  2  OPEN IN BROWSER
        192.168.4.1
     WAITING FOR PHONE
```

1. On your phone, open Wi-Fi settings and join the `ORBITAL-xxxx` network shown
   on the screen. (The last 4 characters are unique to your board.)
2. A setup page should pop up automatically. If it doesn't, open a browser and
   go to `192.168.4.1`.
3. Pick your home Wi-Fi, enter the password, and type your city — for example
   `New York, NY`.
4. Tap **Save and restart**.

The clock reboots, joins your network, gets the time, and looks up your weather.
Done — it never needs setting up again.

Your Wi-Fi password is stored only on the device itself. It isn't in the
firmware file and isn't sent anywhere.

---

## Using it

The BOOT button on the ESP32 does everything:

- **Tap** — next face (it remembers where you left it after a power cut)
- **Hold about 1 second** — switch between 12-hour and 24-hour time
- **Hold 8 seconds** — erase Wi-Fi and location, start setup over (there's an
  on-screen countdown, so let go early and nothing happens)

### The 14 faces

1. NEON — big digits, starfield, occasional glitch
2. DIGITAL — a plain clock with seconds
3. RAIN — falling code with the time in the middle
4. DECRYPT — digits scramble and lock, ringed by live chip temp/heap/uptime
5. WEATHER — current conditions, animated icon, high/low, wind
6. FORECAST — 5-day outlook
7. MOON — phase disc, illumination, age, next full and new moon
8. ORRERY — the solar system at its real positions for today
9. ORBIT — orbital dial, hour/minute/second as planets
10. TERMINAL — scrolling ship's log
11. FLYOVER — wireframe landscape scrolling past
12. TESSERACT — rotating 4D hypercube
13. JELLYFISH — drifting jellies and bubbles
14. BOUNCE — the classic screensaver, counting corner hits

---

## Good to know

- **Weather is free and needs no API key.** It uses Open-Meteo. You never
  register for anything.
- **Screen care.** Transparent OLEDs can retain a faint afterimage from a static
  picture. The firmware nudges the layout a pixel every minute and dims the
  screen overnight between 10pm and 7am automatically.
- **Blank or scrambled screen after flashing?** Double-check the 7 wires, and
  confirm VCC is on 3V3 rather than 5V. If wiring is right, see the
  troubleshooting section on GitHub — some panels use a slightly different
  display init and need a one-word change in the code.
- **Clock shows the wrong time?** Your city probably didn't match. Hold BOOT for
  8 seconds and redo setup, choosing your time zone from the dropdown this time.

---

## Remix / modify

The firmware is a single Arduino sketch, MIT licensed. Pin assignments,
brightness, the night-dimming schedule, and auto-rotating faces are all
settings at the top of the file. Full build instructions are in the repo:

https://github.com/deepbluesea15/orbital-clock

If you make a face of your own, I'd love to see it.
