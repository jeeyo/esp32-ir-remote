# ESPHome Beep Detector — Claude Session Context

A custom ESPHome component (`beep_detector`) that listens on any `microphone:` platform and fires a trigger when it hears a tone-like "beep" at a configurable frequency/amplitude/duration, with a runtime calibration sweep. The component is the deliverable; `samples/` holds worked examples of using it, each a self-contained ESPHome config built independently by CI.

## Files

- `components/beep_detector/` — the component: `__init__.py` (schema/codegen), `beep_detector.h`, `beep_detector.cpp` (Goertzel detector, calibration sweep)
- `samples/basic-beep-detector/basic-beep-detector.yaml` — minimal generic-ESP32 usage: PDM mic → `beep_detector` → binary sensor
- `samples/gree-ac-remote/ac-remote.yaml` — full M5StickC-Plus build: Gree AC IR control (`climate: platform: gree`) using `beep_detector` as passive command confirmation. `samples/gree-ac-remote/fonts/roboto.ttf` is its display font; `samples/gree-ac-remote/README.md` has the hardware/HA-specific docs (BOM, wiring, entity reference, troubleshooting)
- `.github/workflows/build.yml` — CI builds both sample YAMLs on push/PR via `esphome/workflows`
- `.github/workflows/release.yml` — builds + publishes the `gree-ac-remote` sample firmware to a GH release on `v*` tag
- `secrets.yaml` — WiFi/API/OTA credentials for local sample builds (gitignored)

## Non-obvious constraints

- **The component doesn't know about any specific appliance.** `beep_detector` only reports beeps; it has no concept of "AC" or "confirmation". Self-triggered/paused flags (`set_self_triggered`, `is_self_triggered`, `set_paused`, `is_paused`) are generic hooks a sample's own YAML/automation gives meaning to — don't add appliance-specific logic into the component itself. New use cases belong in `samples/`.
- **`beep_detector_source` substitution** — each sample's `external_components` source is `${beep_detector_source}`, defaulting to `../../components` (local path relative to the sample's own directory) so cloned builds and CI work as-is. No-clone users override the substitution to `github://jeeyo/esp32-ir-ac-thermostat@<tag>` from a wrapper YAML.
- **I2S PDM** — only `i2s_lrclk_pin` (the clock) plus `i2s_din_pin` (the data). Don't add `i2s_bclk_pin` — it triggers a "pin used twice" error in ESPHome 2025.11+.
- **Amplitude window (min+max)** — rejects beeps from other sources at the wrong volume (e.g. an adjacent-room AC unit). Calibration finds the right window for a given install.

### `samples/gree-ac-remote` specifics

- **`climate.gree` is the AC control surface** — `climate.ac` (id `ac_climate`) is a standard ESPHome `climate_ir` platform entity (mode, target temperature, fan) exposed directly to HA. There is no separate custom switch/thermostat layer; HA drives `climate.ac` directly.
- **`gree_model` substitution picks the IR dialect** — Trane/Airlux/Electrolux "YT1F" remotes cover several rebadged Gree protocol variants (`generic`, `yan`, `yaa`, `yac`, `yac1fb9`, `yx1ff`, `yag`). Default is `generic`; if the AC doesn't respond, try the others and watch `binary_sensor.ac_beep_confirmed` for feedback.
- **IR TX uses the M5StickC-Plus's built-in IR LED (GPIO9), not the Grove port** — no `remote_receiver`/`receiver_id` exists in this sample: the built-in LED is transmit-only, so `climate.gree` is transmit-only too (no physical-remote-to-HA state sync).
- **Two I2C buses** — the internal HAT-port bus (GPIO21/22) carries only the AXP192 PMU; the Grove connector (GPIO32/33, bus id `grove_i2c`) carries the ENV III Unit (`sht3xd` + `qmp6988`, both set `i2c_id: grove_i2c`). Keep them separate — don't merge the ENV sensors back onto the default bus or `i2c_id` needs updating on both.
- **Beep detector is passive-only, not a retry loop** — `climate_ir`'s `transmit_state()` runs synchronously inside the platform's `control()`, with no per-command retry/gate hook exposed to YAML. `on_control` (fires *before* `control()`, i.e. before the IR transmit) arms `beep_det.set_self_triggered(true)`; the `confirm_ac_command` script then waits 3s and just reports confirmed/unconfirmed via `last_action_text` — it never resends or disables anything.
- **AXP192 comes from `makerwolf/esphome-axp192`** — newer ESPHome no longer has the airy10 top-level `axp192:` schema; it's now a `sensor: platform: axp192` block. Required on M5StickC-Plus or the LCD backlight stays off.
- **No `!secret` references in `ac-remote.yaml`** — keeps CI green and lets pre-built firmware be configured via captive portal. Users add secrets locally via a wrapper config or direct edit. Do not re-introduce `!secret` without also adding CI handling.
- **Modes (`calibrating` global, bool)** — `false` = normal (default), `true` = calibrate (Button A long press 3s).
- **Hardware pins (M5StickC-Plus)** — IR TX GPIO9 (built-in LED), PDM Mic CLK GPIO0 / DATA GPIO34, Display SPI CLK=13/MOSI=15/CS=5/DC=23/RST=18, AXP192 I2C (HAT bus) SDA=21/SCL=22, ENV III Unit I2C (Grove bus) SDA=32/SCL=33, Button A GPIO37 (inverted), Button B GPIO39 (inverted, unused).

## Build

```bash
esphome run samples/basic-beep-detector/basic-beep-detector.yaml
esphome run samples/gree-ac-remote/ac-remote.yaml
```
