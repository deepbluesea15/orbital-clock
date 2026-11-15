# Orbital Clock on ESPHome

This ESPHome configuration builds the original Orbital Clock firmware for the
ESP32-S3 SuperMini and Waveshare 1.51-inch SSD1309 SPI OLED. It preserves the
original U8g2 renderer and all 18 faces in a small ESPHome component. The MIT
license is in the project root.

## Architecture

### Time Synchronization
The clock now uses ESPHome's built-in SNTP time component instead of managing
NTP internally. This provides:
- Reliable automatic time synchronization via ESPHome's time component
- Integration with Home Assistant for timezone support
- Simpler, more maintainable code

### Network Status
Network queries (`WiFi.status()`, `WiFi.RSSI()`, `WiFi.channel()`) have been
consolidated into a `NetworkStatus` adapter class that wraps ESPHome's Wi-Fi
component. This decouples the display faces from Arduino WiFi APIs and makes
testing easier.

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

The refactored codebase compiles successfully to ~1.1MB, using 63.2% of Flash and 
33.7% of RAM on the ESP32-S3. `esphome run` builds and flashes over USB on first 
install, then can use OTA for subsequent updates.

## Refactoring Summary

This refactoring (branch `refactor/esphome-separation`) completes the transition 
from Arduino sketch-based time/Wi-Fi management to ESPHome component integration:

### Completed
- ✅ Removed setup portal (WebServer, DNSServer, and embedded HTML)
- ✅ Removed legacy Wi-Fi management code (WiFi.begin(), reconnect loops)
- ✅ Integrated ESPHome's SNTP time component for reliable synchronization
- ✅ Created NetworkStatus adapter to wrap ESPHome's Wi-Fi component
- ✅ Updated all clock faces to use NetworkStatus and TimeService adapters
- ✅ Consolidated global state (WxData, Cfg) in WeatherService
- ✅ Successfully compiled to firmware without errors or warnings
- ✅ Updated .gitignore to exclude Python temporary folders

### Architecture Improvements
- **Separation of concerns**: Time, weather, and network logic moved to focused helper classes
- **Reduced complexity**: Removed ~500 lines of setup portal code and embedded timezone tables
- **Better testability**: NetworkStatus and TimeService are stateless adapters
- **ESPHome integration**: Leverages built-in components instead of duplicate implementations
- **Consistent state**: All configuration persisted through WeatherService and ESPHome entities