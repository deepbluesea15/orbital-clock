# Orbital Clock on ESPHome

This ESPHome configuration builds the original Orbital Clock firmware for the
ESP32-S3 SuperMini and Waveshare 1.51-inch SSD1309 SPI OLED. It preserves the
original U8g2 renderer and all 18 faces in a small ESPHome component. The MIT
license is in the project root.

## Configure

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
& .\.venv\Scripts\esphome.exe run esphome/orbital-clock.yaml
```

`esphome run` builds and flashes over USB on first install, then can use OTA for
subsequent updates. The original Arduino sketch remains in `../OrbitalClock/`.