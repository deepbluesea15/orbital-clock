# ORBITAL

A twelve-face display toy for the **Waveshare 1.51" transparent OLED** and an
**ESP32-S3 SuperMini**. Clocks, weather, and a few things that are just nice to
look at through a see-through panel.

No credentials in the source. On first boot the clock opens its own Wi-Fi
network and walks you through setup from your phone.

## Faces

Tap the BOOT button to cycle.

| # | Face | What it is |
|---|------|-----------|
| 1 | NEON | Big digits with a blinking colon, drifting starfield, 60-second bar, occasional glitch |
| 2 | DIGITAL | A plain, undecorated clock with seconds. Readable across the room |
| 3 | RAIN | Falling glyph columns with bright heads, time punched through the middle |
| 4 | DECRYPT | Digits scramble through random glyphs and lock, like a cipher resolving |
| 5 | WEATHER | Current conditions with an animated icon, high/low, feels-like, wind, humidity |
| 6 | FORECAST | Five-day strip, one column per day |
| 7 | ORBIT | Orbital dial — hour, minute and second bodies circling a ringed sun |
| 8 | TERMINAL | Scrolling ship's log with a live clock and blinking cursor |
| 9 | FLYOVER | Wireframe terrain scrolling toward you under a crescent moon |
| 10 | TESSERACT | A rotating 4D hypercube, projected 4D→3D→2D |
| 11 | JELLYFISH | Drifting jellies with pulsing bells and rising bubbles |
| 12 | BOUNCE | The screensaver. Counts bounces, and separately corner hits |

## Hardware

- [Waveshare 1.51" Transparent OLED](https://www.waveshare.com/1.51inch-transparent-oled.htm) — 128×64, SSD1309, 4-wire SPI
- ESP32-S3 SuperMini (4 MB flash, no PSRAM needed)
- Seven jumper wires

The display module ships strapped for 4-wire SPI, so the resistors on the back
need no changes.

## Wiring

| OLED (7-pin cable) | SuperMini | Note |
|---|---|---|
| VCC | 3V3 | not 5V |
| GND | GND | |
| DIN | GPIO11 | SPI MOSI |
| CLK | GPIO12 | SPI SCK |
| CS | GPIO10 | |
| DC | GPIO9 | |
| RST | GPIO8 | |

GPIO0 is the onboard BOOT button and needs no wiring.

These pins are the S3's default FSPI group, they sit together on the header, and
none is a strapping pin. If you change them, edit the `PIN_*` defines at the top
of the sketch. Avoid GPIO19/20 (USB), GPIO43/44 (UART0) and GPIO46.

## Flashing

### Option A — prebuilt binary, nothing to install

1. Grab `orbital_clock_esp32s3_4mb.bin` from the
   [Releases](../../releases) page.
2. Open [web.esphome.io](https://web.esphome.io/) or
   [esptool-js](https://espressif.github.io/esptool-js/) in Chrome or Edge.
3. Connect, pick the board's serial port, choose **Install from file**, select
   the `.bin`.

It's a full flash image with bootloader and partition table, so it writes at
offset 0. If the board doesn't appear in the port list, hold BOOT while plugging
in the USB-C cable.

> Flashing this image blanks the settings partition, so every flash returns the
> clock to first-boot setup.

### Option B — build it yourself

Arduino IDE:

1. Install the **esp32** boards package by Espressif (Boards Manager).
2. Install **U8g2** by oliver (Library Manager).
3. Open `orbital_clock/orbital_clock.ino`.
4. Board: **ESP32S3 Dev Module**. Set **USB CDC On Boot: Enabled**.
5. Upload.

Or with `arduino-cli`:

```sh
arduino-cli core install esp32:esp32
arduino-cli lib install U8g2
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --output-dir ./build orbital_clock
arduino-cli upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --input-dir ./build orbital_clock
```

`build/orbital_clock.ino.merged.bin` is the single flash-at-zero image.

## First-time setup

On first boot the screen shows:

```
      ORBITAL SETUP
  ------------------------
  1  JOIN THIS WI-FI
        ORBITAL-3F7A
  2  OPEN IN BROWSER
        192.168.4.1
     WAITING FOR PHONE
```

Join that network from a phone. The setup page usually opens by itself; if not,
browse to `192.168.4.1`. Enter your Wi-Fi network, its password, and a city, then
save. The clock restarts and joins your network.

The network name ends in the last two bytes of your board's MAC, so it's unique
to your device.

**The setup network is open — no password.** It is only up during setup, and
nothing sensitive is exposed, but anyone in range can reach that page while it
is running.

## Buttons

| Action | Effect |
|---|---|
| Tap BOOT | Next face |
| Hold 0.8s | Toggle 12/24 hour |
| Hold 8s | Erase Wi-Fi and location, reboot into setup |
| Hold while powering on | Same erase, for when the firmware is wedged |

The 8-second hold shows a progress bar and countdown from 1.8 seconds onward.
Let go before it reaches zero and nothing changes.

## Time and location

- **Time** comes from NTP (`pool.ntp.org`).
- **Weather** comes from [Open-Meteo](https://open-meteo.com/), which needs no
  API key. Nothing secret ends up in the firmware.
- The city you type is geocoded *after* the clock joins your network, returning
  coordinates plus an IANA time zone name.
- The ESP has no time zone database, so the sketch carries a table of 37 POSIX
  rule strings and maps the IANA name onto it. DST transitions then happen on
  their own rather than being frozen at the offset that was true on setup day.
  If your city is ambiguous, pick the zone from the dropdown instead.

Settings live in NVS on the device, never in source.

## Notes on the panel

Waveshare warns that a static image on this display causes afterimages. The
sketch shifts the whole layout by a pixel or two every minute and drops contrast
overnight between 22:00 and 07:00. Both are on by default — `ANTI_BURNIN` and
`BRIGHT_NIGHT` near the top of the sketch.

Transparent OLEDs look best with sparse bright pixels on black, which is why
most faces are line art rather than filled shapes.

## Implementation notes

A few things that were less obvious than they look:

- **U8g2 coordinates are unsigned.** A negative x wraps to ~65500 and the shape
  vanishes instead of clipping. FLYOVER, TESSERACT and the jellyfish tentacles
  all draw off-edge constantly, so those lines go through a Cohen–Sutherland
  clipper first.
- **Open-Meteo emits `current_units` before `current`,** where every key's value
  is a unit *string*. Parsing from the top of the document reads the temperature
  as 0 and the weather code as 0 — a sunny zero-degree day. The parser anchors to
  the `current` and `daily` objects.
- **Weather runs on its own FreeRTOS task** pinned to core 0 with a 12 KB stack.
  A TLS handshake takes a second or two and would otherwise visibly hitch the
  25 fps render loop.
- **The glitch effect** manipulates U8g2's framebuffer directly, rotating bytes
  within a tile row. It's gated to the cyberpunk faces — a corrupted jellyfish
  just looks like a bug.

## Troubleshooting

| Symptom | Try |
|---|---|
| Blank or garbled display | Swap `NONAME0` for `NONAME2` in the U8g2 constructor. The SSD1309 has two common init variants |
| Screen works, wrong offset | Same fix as above |
| Board not in serial port list | Hold BOOT while plugging in USB |
| Clock shows UTC | The city didn't geocode. Pick a time zone from the dropdown |
| Weather says NO LOCATION SET | No city was entered, or geocoding failed. Re-run setup |
| Setup network never appears | Hold BOOT 8 seconds to force a reset |

## License

MIT — see [LICENSE](LICENSE). Put your name in the copyright line.
