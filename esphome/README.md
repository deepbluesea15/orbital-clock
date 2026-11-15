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
timezone choices and geocoding results it supports. Automatic timezone uses
UTC until a location with a supported timezone has been resolved.
The Solar face uses ESPHome's `sun` component for sunrise and sunset, with
coordinates updated from the cached or geocoded weather location. Its countdown
uses the next actual event, even when the selected timezone differs from the
weather location. Near polar transitions it shows the current horizon state
when a complete daylight period is unavailable.

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
state is protected while it is read or updated. A Wi-Fi connection wakes the
weather task immediately instead of waiting for its next scheduled retry.
Location-cache writes, Sun coordinates, and automatic timezone updates run on
the ESPHome main loop alongside other preferences and time services.
Failed weather or location requests retain the last successful conditions and
retry with jittered exponential backoff, starting around one minute and
capping at 15 minutes. Changing settings or reconnecting Wi-Fi wakes the task
immediately.
The forecast shows the dates supplied for the weather location, independently
of the clock's selected time zone. Location lookup failures are shown on the
weather, forecast, and Solar faces and recorded in the logs.

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
Wi-Fi or Home Assistant outages do not cause automatic reboots, so a clock
that has already synchronized can continue running offline.

## Home Assistant

After the device joins Wi-Fi, add it to Home Assistant through the ESPHome
integration. These controls are exposed and saved by ESPHome across reboots:

- **Clock face** selects any of the 18 display faces.
- **Clock format** selects 12-hour or 24-hour time.
- **Weather units** selects Celsius or Fahrenheit.
- **Time zone** uses the weather location automatically or selects a supported
	IANA time zone directly.
- **Weather location** accepts a city or place name; changing it triggers a
	fresh geocode and weather fetch. The entered name is retained for the
	location cache.
- **Day brightness** and **Night brightness** set OLED contrast from 0% to
  100%. The defaults are 100% and 12%; 0% is minimum contrast, not display off.
- **Night dimming starts** and **Night dimming ends** set the schedule in the
  clock's selected local time zone (default 22:00–07:00). Equal times disable
  the night period.
- **Face rotation interval** sets minutes between faces (1–1440); zero disables
  rotation. Selecting a face or changing the interval restarts the countdown.
- **Reset clock settings** restores the face, format, units, timezone, and
	location defaults, as well as brightness, schedule, and rotation defaults.

The BOOT button still cycles faces and toggles the time format; those changes
are saved to flash within approximately six seconds and reflected back into the
Home Assistant entities. Holding BOOT for eight seconds resets the same clock
settings as the Home Assistant button.
At startup, the display shows a short terminal sequence with the current
network and time synchronization status before switching to the clock.
On the BOUNCE face, each non-corner hit has a 1-in-25 chance of aiming the
clock toward a corner; the display counts both hits and corner hits.

## Code structure

`OrbitalClock` is the ESPHome lifecycle adapter. `ClockSettings` owns restored
entity values and keeps Home Assistant in sync. `ClockRuntime` coordinates
startup and frame updates, `ButtonController` owns BOOT-button behavior,
`WeatherService` owns geocoding and weather fetching, and `ClockFaces` dispatches
the selected renderer. Each face implements the abstract `ClockFace::draw()`
interface with a matching `.h` declaration and `.cpp` implementation under
`custom_components/orbital_clock_faces/` (`NeonFace`, `DigitalFace`, `AnalogFace`,
and the other face classes). Animated state such as rain columns, flip tiles,
bounce counters, and jellyfish positions is held by the owning face class.
The face sources are packaged as the `orbital_clock_faces` ESPHome external
component so they can be fetched alongside `orbital_clock`.
`DisplayEngine` and `WeatherService` are
interfaces at the ESPHome boundary; `SketchDisplayEngine` and
`SketchWeatherService` adapt the runtime to them. The original Arduino sketch
remains separately in `../OrbitalClock/OrbitalClock.ino`.

## Build

### ESPHome Dashboard

Create a device YAML in the ESPHome Dashboard:

```yaml
packages:
  orbital_clock:
    url: https://github.com/arikalish/orbital-clock-esphome.git
    ref: main
    files: [esphome/orbital-clock.yaml]
    refresh: always

esphome:
  name: orbital-clock
  friendly_name: Orbital Clock
```

Add the same secrets listed in `secrets.yaml.example` to the Dashboard's
`secrets.yaml`. ESPHome downloads both the package and its external components
from GitHub; the Dashboard does not need a local copy of the source. The
`esphome.name` and `friendly_name` entries can be changed per device. For
the first OTA upload after a hostname change, use the device's old address.
The package and its external components are refreshed on each build.

### Local build

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

The YAML fetches the `main` branch from GitHub for its external components;
merge and push component changes before testing them with this configuration.
`esphome run` builds and flashes over USB on first install, then can use OTA for
subsequent updates.
Serial logs are enabled on the ESP32-S3 USB Serial/JTAG interface at 115200 baud.

## Refactoring Summary

This refactoring completes the transition
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