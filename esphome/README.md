# Orbital Clock on ESPHome

This ESPHome configuration builds the original Orbital Clock firmware for the
ESP32-S3 SuperMini and Waveshare 1.51-inch SSD1309 SPI OLED. It preserves the
original U8g2 renderer and all 18 faces in a small ESPHome component. The MIT
license is in the project root.

## Architecture

### Time Synchronization
The clock now uses ESPHome's built-in SNTP time component instead of managing
NTP internally. `TimeService` reads ESPHome's local `ESPTime`, and location-based
timezone changes use ESPHome's POSIX timezone rules. ESPHome does not include a
runtime IANA timezone database, so the component retains mappings for the
timezone choices and geocoding results it supports.
The Solar face uses ESPHome's `sun` component for sunrise and sunset, with
coordinates updated from the cached or geocoded weather location.

### Network Status
Network queries (`WiFi.status()`, `WiFi.RSSI()`, `WiFi.channel()`) have been
consolidated into a `NetworkStatus` adapter class that wraps ESPHome's Wi-Fi
component. This decouples the display faces from Arduino WiFi APIs and makes
testing easier.

### Runtime and Button
ESPHome schedules the display frames and settings synchronization instead of
polling both on every component loop. The BOOT pin is an ESPHome GPIO binary
sensor; ESPHome automations handle the hold delays and restart action, while
`ButtonController` retains tap/hold state and the reset-progress overlay.

### Weather
Weather requests use ESPHome's `http_request` component, JSON utilities, and
global preferences for the location cache. Network I/O stays on a background
task so slow requests do not block display updates; shared weather and location
state is protected while it is read or updated.

### Configuration

On a fresh checkout, create the ignored secrets file first:

```powershell
Copy-Item esphome/secrets.yaml.example esphome/secrets.yaml
```

Edit `secrets.yaml` with the Wi-Fi credentials, fallback access-point password,
and a private API encryption key. Generate a 32-byte key with:

```sh
python -c "import base64, secrets; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

`secrets.yaml` is ignored by Git. `secrets.yaml.example` shows the required
fields. Wi-Fi credentials are provided to ESPHome; if the network is
unavailable, the device starts the `ORBITAL-SETUP` fallback access point.

## Home Assistant

After the device joins Wi-Fi, add it to Home Assistant through the ESPHome
integration. These controls are exposed and saved by ESPHome across reboots:

- **Clock face** selects any of the 18 display faces.
- **Clock format** selects 12-hour or 24-hour time.
- **Weather units** selects Celsius or Fahrenheit.
- **Time zone** uses the weather location automatically or selects a supported
	IANA time zone directly.
- **Weather location** accepts a city or place name; changing it triggers a
	fresh geocode and weather fetch.
- **Reset clock settings** restores the face, format, units, timezone, and
	location defaults.

The BOOT button still cycles faces and toggles the time format; those changes
are reflected back into the Home Assistant entities. Holding BOOT for eight
seconds resets the same clock settings as the Home Assistant button.

## Code structure

`OrbitalClock` is the ESPHome lifecycle adapter. `ClockSettings` owns restored
entity values and keeps Home Assistant in sync. `ClockRuntime` coordinates
startup and frame updates, `ButtonController` owns BOOT-button behavior,
`WeatherService` owns geocoding and weather fetching, and `ClockFaces` dispatches
the selected renderer. Each face implements the abstract `ClockFace::draw()`
interface with a matching `.h` declaration and `.cpp` implementation under
`custom_components/orbital_clock/faces/` (`NeonFace`, `DigitalFace`, `AnalogFace`,
and the other face classes). Animated state such as rain columns, flip tiles,
bounce counters, and jellyfish positions is held by the owning face class.
`DisplayEngine` and `WeatherService` are
interfaces at the ESPHome boundary; `SketchDisplayEngine` and
`SketchWeatherService` adapt the runtime to them. The original Arduino sketch
remains separately in `../OrbitalClock/OrbitalClock.ino`.

## Build

From the project root, create a local ESPHome environment and set `esphome/secrets.yaml`:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install esphome
Copy-Item esphome/secrets.yaml.example esphome/secrets.yaml
```

Fill in the secrets file, then validate and build/flash:

```powershell
& .\.venv\Scripts\esphome.exe config esphome/orbital-clock.yaml
& .\.venv\Scripts\esphome.exe compile esphome/orbital-clock.yaml
& .\.venv\Scripts\esphome.exe run esphome/orbital-clock.yaml
```

`esphome run` builds and flashes over USB on first install, then can use OTA for
subsequent updates.
Serial logs are enabled on the ESP32-S3 USB Serial/JTAG interface at 115200 baud.

## Refactoring Summary

This refactoring (branch `refactor/esphome-separation`) completes the transition 
from Arduino sketch-based time/Wi-Fi management to ESPHome component integration:

### Completed
- ✅ Removed setup portal (WebServer, DNSServer, and embedded HTML)
- ✅ Removed legacy Wi-Fi management code (WiFi.begin(), reconnect loops)
- ✅ Integrated ESPHome's SNTP time component for reliable synchronization
- ✅ Created NetworkStatus adapter to wrap ESPHome's Wi-Fi component
- ✅ Updated all clock faces to use NetworkStatus and TimeService adapters
- ✅ Uses ESPHome HTTP, JSON, preferences, SNTP, and Wi-Fi functionality
- ✅ Protects weather data shared between rendering and background refresh
- ✅ Successfully compiles to firmware
- ✅ Updated .gitignore to exclude Python temporary folders

### Architecture Improvements
- **Separation of concerns**: Time, weather, and network logic moved to focused helper classes
- **Reduced complexity**: Removed ~500 lines of setup portal code; centralized supported timezone mappings
- **Better testability**: NetworkStatus and TimeService are stateless adapters
- **ESPHome integration**: Leverages built-in components instead of duplicate implementations
- **Consistent state**: Settings restore through ESPHome entities; the location cache uses ESPHome preferences