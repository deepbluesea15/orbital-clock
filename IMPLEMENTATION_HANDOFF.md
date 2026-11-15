# Orbital Clock ESPHome Refactor Handoff

## Goal

Finish the user's request to do all four follow-ups from the review:

1. Remove the sketch's obsolete Wi-Fi setup portal and Wi-Fi management paths.
2. Separate remaining concerns into focused classes/files.
3. Use ESPHome's Wi-Fi component as the network-state authority.
4. Use ESPHome's SNTP/time component for time synchronization.

Preserve the current 18 faces, button behavior, Home Assistant controls, weather
fetching, and Arduino sketch compatibility unless the implementation clearly
targets the ESPHome component only.

## Important first step

Inspect `git status` and reread the current versions of files before editing.
The prior session made partial, unvalidated changes. In particular,
`clock_runtime.cpp` was edited to add helper includes and remove part of its
configuration/persistence code, while the old portal/timezone/weather/button
implementation may still be present. Do not assume it currently builds or that
the new helpers are integrated. Preserve unrelated worktree changes.

The following new helper files were started in
`esphome/custom_components/orbital_clock/` and need review before reuse:

- `button_controller.{h,cpp}`
- `network_status.{h,cpp}`
- `starfield.{h,cpp}`
- `time_service.{h,cpp}`
- `timezone_resolver.{h,cpp}`
- `weather_service.{h,cpp}`

Some may duplicate code that remains in `clock_runtime.cpp`, and some draft
interfaces/implementations may need correction or removal. Avoid leaving both
old and new definitions active.

## Implementation sequence

### 1. Reconcile and complete runtime separation

- Read `clock_runtime.cpp`, `clock_runtime.h`, `clock_face_context.h`,
  `clock_settings.*`, `orbital_clock.*`, `sketch_adapters.*`, all affected face
  files, and `orbital-clock.yaml`.
- Remove the dead setup portal (`WebServer`, `DNSServer`, HTML, scanning,
  `runPortal`, and its OLED UI) from the ESPHome component.
- Remove the legacy sketch Wi-Fi connect/reconnect flow, boot-time credentials,
  and obsolete `ORBITAL_ESPHOME` branches from this component.
- Move button state/handling, starfield state/animation, timezone resolution,
  weather parsing/fetching/state, and timekeeping out of the runtime into their
  own classes. Integrate the classes rather than merely adding files.
- Keep `ClockRuntime` as the display/render coordinator. Keep public adapter
  entry points consistent with `SketchDisplayEngine` and
  `SketchWeatherService`, or replace them consistently across all call sites.
- Keep ESPHome select/text entities as the persistent source of user settings.
  Remove obsolete `Preferences` writes for settings already restored by those
  entities; retain only data that genuinely needs independent persistence, if
  any.

### 2. Make ESPHome own Wi-Fi

- Keep the `wifi:` and `captive_portal:` configuration in
  `esphome/orbital-clock.yaml`; ESPHome already manages station connection,
  retry, fallback AP, and captive portal.
- Remove `WiFi.begin`, `WiFi.mode`, `WiFi.disconnect`, connection retry loops,
  legacy AP code, and direct `WiFi.status`/RSSI/channel reads from component
  runtime and face code.
- Use a small network-status adapter backed by
  `esphome::wifi::global_wifi_component` for connected/RSSI/channel queries.
- Update display faces (currently including decrypt, terminal, and weather)
  to use that adapter. Handle a missing component or disconnected state safely.
- Remove now-unused Arduino Wi-Fi, portal, and preferences libraries/configuration
  from YAML only after confirming no remaining component dependency needs them.
  Keep TLS/HTTP libraries if weather/geocoding still uses `HTTPClient`.

### 3. Use ESPHome time/SNTP

- Add an ESPHome `time:` entry using the `sntp` platform and make the component
  depend on the appropriate time/network components.
- Pass the configured ESPHome `RealTimeClock` source into the clock component
  or its `TimeService`; consume its UTC epoch rather than calling
  `configTzTime` or configuring SNTP from the runtime.
- Preserve the Home Assistant timezone selector, including its Automatic
  behavior and the listed supported zones. Resolve geocoded IANA zones through
  one mapping/helper and apply timezone changes consistently to rendered local
  time. Do not assume setting libc `TZ` also updates ESPHome's parsed timezone
  state; inspect ESPHome's configured APIs and choose one authoritative
  conversion path.
- Handle time before the first SNTP sync as invalid without displaying bogus
  calendar data. Ensure late geocoding in Automatic mode updates the effective
  timezone but does not start a second SNTP client.

### 4. Configuration/docs and verification

- Remove stale Wi-Fi/portal/SNTP wording, unused libraries, and obsolete
  framework settings only when no longer necessary.
- Update `esphome/README.md` to reflect the actual class boundaries and explain
  that ESPHome owns Wi-Fi and SNTP.
- Check all face includes/call sites and generated-component dependencies for
  consistency.

## Validation

From the repository root, first identify the already-installed ESPHome command
within this project or its known Python environment; do not enumerate unrelated
workspaces or caches. The README documents the project-local command:

```powershell
& .\.venv\Scripts\esphome.exe config esphome/orbital-clock.yaml
& .\.venv\Scripts\esphome.exe compile esphome/orbital-clock.yaml
```

If that executable is absent, use the Python environment where ESPHome is
already installed rather than installing dependencies without a demonstrated
need. Run the smallest relevant checks first, then a complete ESPHome compile.
Resolve all failures caused by the refactor. Finally inspect `git diff` and
`git status` to ensure no unrelated changes were reverted or included.

## Constraints and known limitation

The previous session's environment denied further edits to
`esphome/custom_components/orbital_clock/clock_runtime.cpp` and explicitly
instructed not to retry that effect in that session. The user later said
permissions were re-enabled, but the prior session could not verify a changed
tool authorization state. A new session should inspect its current permissions
and proceed only if edits are allowed; do not attempt to bypass a denial.
