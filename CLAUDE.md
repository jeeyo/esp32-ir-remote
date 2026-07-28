# ESP32 IR Remote — Claude Session Context

ESPHome firmware for M5StickC-Plus acting as an IR remote transmitter for a Gree-protocol AC (built for a Trane/Airlux/Electrolux "YT1F" rebadged Gree unit), using ESPHome's built-in `climate: platform: gree` (`climate_ir`). Acoustic beep detection provides a passive, best-effort confirmation that a command was received — not a retry mechanism.

## Files

- `ac-remote.yaml` — entire ESPHome config (board, display, mic, IR, Gree climate, sensors, scripts, automations)
- `secrets.yaml` — WiFi/API/OTA credentials (gitignored)
- `components/beep_detector/` — custom ESPHome component: `__init__.py` (schema/codegen), `beep_detector.h`, `beep_detector.cpp` (Goertzel detector, calibration sweep)
- `fonts/roboto.ttf` — display font
- `.github/workflows/build.yml` — CI build on push/PR via `esphome/workflows`
- `.github/workflows/release.yml` — build + publish firmware to GH release on `v*` tag

## Hardware Pins (M5StickC-Plus)

| Function | Pin |
|---|---|
| IR LED TX | GPIO32 (Grove pin 1) |
| IR Receiver | GPIO33 (Grove pin 2) |
| PDM Mic CLK | GPIO0 |
| PDM Mic DATA | GPIO34 |
| Display SPI | CLK=13, MOSI=15, CS=5, DC=23, RST=18 |
| AXP192 / ENV HAT I2C | SDA=21, SCL=22 |
| Button A | GPIO37 (inverted) |
| Button B | GPIO39 (inverted, unused — no longer needed now that IR learn mode is gone) |

## Modes (`calibrating` global, bool)

| Value | Mode | Entry |
|---|---|---|
| false | Normal | default |
| true | Calibrate | Button A long press 3s |

## Non-obvious constraints

- **`climate.gree` is the AC control surface** — `climate.ac` (id `ac_climate`) is a standard ESPHome `climate_ir` platform entity (mode, target temperature, fan) exposed directly to HA. There is no separate custom switch/thermostat layer; HA drives `climate.ac` directly (schedule it there if desired).
- **`gree_model` substitution picks the IR dialect** — Trane/Airlux/Electrolux "YT1F" remotes cover several rebadged Gree protocol variants (`generic`, `yan`, `yaa`, `yac`, `yac1fb9`, `yx1ff`, `yag`). Default is `generic`; if the AC doesn't respond, try the others and watch `binary_sensor.ac_beep_confirmed` for feedback.
- **`receiver_id: ir_receiver` on `climate.gree`** — the same `remote_receiver` used for transmit-side diagnostics also feeds `climate_ir`'s built-in listener, so a physical-remote button press is decoded and syncs HA state automatically. This replaces the old "beep toggles state" hack — no application code needed.
- **Beep detector is passive-only, not a retry loop** — `climate_ir`'s `transmit_state()` runs synchronously inside the platform's `control()`, with no per-command retry/gate hook exposed to YAML. `on_control` (fires *before* `control()`, i.e. before the IR transmit) arms `beep_det.set_self_triggered(true)`; the `confirm_ac_command` script then waits 3s and just reports confirmed/unconfirmed via `last_action_text` — it never resends or disables anything.
- **I2S PDM** — only `i2s_lrclk_pin: GPIO0` (the clock) plus `i2s_din_pin: GPIO34` (the data). Don't add `i2s_bclk_pin` — it triggers a "pin used twice" error in ESPHome 2025.11+.
- **Amplitude window (min+max)** — rejects beeps from adjacent-room AC units at lower volume. Calibration finds the right window for this install.
- **`beep_detector_source` substitution** — `external_components` source for `beep_detector` is `${beep_detector_source}`, defaulting to `components` (local path) so cloned builds and CI work as-is. No-clone users override the substitution to `github://jeeyo/esp32-ir-remote@<tag>` from a wrapper YAML. Same pattern for `ota_password` (default empty, override with `!secret`).
- **AXP192 comes from `makerwolf/esphome-axp192`** — newer ESPHome no longer has the airy10 top-level `axp192:` schema; it's now a `sensor: platform: axp192` block. Required on M5StickC-Plus or the LCD backlight stays off.
- **No `!secret` references in `ac-remote.yaml`** — keeps CI green and lets pre-built firmware be configured via captive portal. Users add secrets locally via a wrapper config or direct edit (see README § "Adding Your Secrets"). Do not re-introduce `!secret` without also adding CI handling.
- **AC internal setpoint vs. `climate.ac`'s target_temperature** — `climate.gree` sends the full Gree protocol frame (mode + temperature + fan) on every command, so target temperature is no longer independent of the AC like the old ON/OFF-only thermostat was; the AC should track whatever `climate.ac` last sent.

## Build

```bash
esphome run ac-remote.yaml
```
