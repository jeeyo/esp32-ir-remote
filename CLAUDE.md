# ESPHome Beep Detector — Claude Session Context

A custom ESPHome component (`beep_detector`) that listens on any `microphone:` platform and fires a trigger when it hears a tone-like "beep" at a configurable frequency/amplitude/duration, with a runtime calibration sweep. The component is the deliverable; `samples/` holds worked examples of using it, each a self-contained ESPHome config built independently by CI.

## Files

- `components/beep_detector/` — the component: `__init__.py` (schema/codegen), `beep_detector.h`, `beep_detector.cpp` (Goertzel detector, calibration sweep)
- `samples/basic-beep-detector/basic-beep-detector.yaml` — minimal generic-ESP32 usage: PDM mic → `beep_detector` → binary sensor
- `samples/m5stickc-plus-gree-ac-remote-tx-only/ac-remote.yaml` — M5StickC-Plus build using the **built-in IR LED** (GPIO9, transmit-only) for Gree AC IR control (`climate: platform: gree`), using `beep_detector` as passive command confirmation. Grove port free; ENV III Unit wired there. `samples/m5stickc-plus-gree-ac-remote-tx-only/fonts/roboto.ttf` is its display font; its `README.md` has the hardware/HA-specific docs (BOM, wiring, entity reference, troubleshooting)
- `samples/m5stickc-plus-gree-ac-remote-tx-rx/ac-remote.yaml` — same build, but using an external Grove IR TX+RX module instead of the built-in LED, adding physical-remote-to-HA state sync; ENV III HAT lives on the internal HAT-port I2C bus instead since Grove is occupied by the IR module. Same file layout (`fonts/roboto.ttf`, `README.md`) as the tx-only sample.
- `.github/workflows/build.yml` — CI builds all three sample YAMLs on push/PR via `esphome/workflows`
- `.github/workflows/release.yml` — builds + publishes both `gree-ac-remote` sample variants (`ac-remote-tx-only`, `ac-remote-tx-rx`) to a GH release on `v*` tag, as separate parallel jobs
- `secrets.yaml` — WiFi/API/OTA credentials for local sample builds (gitignored)

## Non-obvious constraints

- **The component doesn't know about any specific appliance.** `beep_detector` only reports beeps; it has no concept of "AC" or "confirmation". Self-triggered/paused flags (`set_self_triggered`, `is_self_triggered`, `set_paused`, `is_paused`) are generic hooks a sample's own YAML/automation gives meaning to — don't add appliance-specific logic into the component itself. New use cases belong in `samples/`.
- **`beep_detector_source` substitution** — each sample's `external_components` source is `${beep_detector_source}`, defaulting to `../../components` (local path relative to the sample's own directory) so cloned builds and CI work as-is. No-clone users override the substitution to `github://jeeyo/esphome-beep-detector@<tag>` from a wrapper YAML.
- **I2S PDM** — only `i2s_lrclk_pin` (the clock) plus `i2s_din_pin` (the data). Don't add `i2s_bclk_pin` — it triggers a "pin used twice" error in ESPHome 2025.11+.
- **Amplitude window (min+max)** — rejects beeps from other sources at the wrong volume (e.g. an adjacent-room AC unit). Calibration finds the right window for a given install.
- **`paused` must not block calibration** — `on_audio_data()` only early-returns on `paused_` when `!calibrating_`; a sample pausing detection before `start_calibration()` (to suppress spurious `on_beep_detected` triggers) would otherwise starve the calibration sweep of audio and always report 0 Hz / 0.0 amplitude. Keep this invariant if touching `on_audio_data()`.
- **Cooldown must not block calibration either** — `process_audio()` checks `calibrating_` before the `cooldown_ms_` gate, not after. A normal beep detected shortly before `start_calibration()` (e.g. testing the AC, then long-pressing to calibrate) sets `last_beep_time_`; if the cooldown check ran first it would silently eat the first `cooldown_ms` (default 5s) of the 10s calibration window, same 0 Hz / 0.0 amplitude symptom as the `paused` issue above. Keep the calibration branch's early-return ahead of the cooldown check.
- **`finish_calibration()` only measures — it never applies the result.** It returns the peak frequency/amplitude seen during the sweep but leaves `target_frequency_`/`amplitude_min_`/`amplitude_max_` untouched. A caller that just logs the "suggested" values (and expects the user to hand-edit `beep_detector:` and reflash) leaves detection running against the old window indefinitely — "calibration works, but nothing ever gets detected afterwards". Callers that want calibration to take effect must call `set_target_frequency()` / `set_amplitude_min()` / `set_amplitude_max()` themselves (both gree-ac-remote samples' `start_calibration` script does this immediately after `finish_calibration()`), and must guard against a zero-amplitude result (nothing heard in the window) since applying it would zero out the detection window instead of leaving the previous one intact.

### `samples/m5stickc-plus-gree-ac-remote-*` specifics (both variants)

- **`climate.gree` is the AC control surface** — `climate.ac` (id `ac_climate`) is a standard ESPHome `climate_ir` platform entity (mode, target temperature, fan) exposed directly to HA. There is no separate custom switch/thermostat layer; HA drives `climate.ac` directly.
- **`gree_model` substitution picks the IR dialect** — Trane/Airlux/Electrolux "YT1F" remotes cover several rebadged Gree protocol variants (`generic`, `yan`, `yaa`, `yac`, `yac1fb9`, `yx1ff`, `yag`). Default is `generic`; if the AC doesn't respond, try the others and watch `binary_sensor.ac_beep_confirmed` for feedback.
- **Beep detector is passive-only, not a retry loop** — `climate_ir`'s `transmit_state()` runs synchronously inside the platform's `control()`, with no per-command retry/gate hook exposed to YAML. `on_control` (fires *before* `control()`, i.e. before the IR transmit) arms `beep_det.set_self_triggered(true)`; the `confirm_ac_command` script then waits 3s and just reports confirmed/unconfirmed via `last_action_text` — it never resends or disables anything.
- **AXP192 comes from `makerwolf/esphome-axp192`** — newer ESPHome no longer has the airy10 top-level `axp192:` schema; it's now a `sensor: platform: axp192` block. Required on M5StickC-Plus or the LCD backlight stays off.
- **AXP192 must be the first entry under `sensor:`** — it returns `setup_priority::DATA`, the same as `sht3xd`/`qmp6988` (neither overrides `PollingComponent`'s default). ESPHome's `Application::setup()` breaks equal-priority ties by registration order, i.e. YAML declaration order within the section. If AXP192 is declared after the ENV III sensors, they probe the still-unpowered Grove/HAT rail during their own `setup()`, get no I2C ACK, `mark_failed()`, and never report temperature/humidity/pressure again for the rest of the boot — regardless of correct wiring. Keep AXP192 first if reordering `sensor:`.
- **No `!secret` references in `ac-remote.yaml`** — keeps CI green and lets pre-built firmware be configured via captive portal. Users add secrets locally via a wrapper config or direct edit. Do not re-introduce `!secret` without also adding CI handling.
- **Modes (`calibrating` global, bool)** — `false` = normal (default), `true` = calibrate (Button A long press 3s).
- **`on_boot` must nest under the top-level `esphome:` block, not stand alone.** It's part of that block's own config schema (`esphome/core/config.py`), not a registered component — a bare top-level `on_boot:` key fails config validation with "Component not found: on_boot", so the whole build breaks (verify with `esphome config <file>` if touching it). Both samples use it to kick off the `wake_screen` screen-timeout script once setup() completes.
- **Keep the two variants in sync** — changes to shared logic (calibration script, on_control confirmation flow, display layout, button handling) should generally be applied to both `tx-only` and `tx-rx` unless the change is specific to the IR/sensor wiring difference.

#### `m5stickc-plus-gree-ac-remote-tx-only`

- **IR TX uses the M5StickC-Plus's built-in IR LED (GPIO9), not the Grove port** — no `remote_receiver`/`receiver_id` exists: the built-in LED is transmit-only, so `climate.gree` is transmit-only too (no physical-remote-to-HA state sync).
- **Two I2C buses, both explicitly `id`'d** — the internal HAT-port bus (`hat_i2c`, GPIO21/22) carries only the AXP192 PMU (`i2c_id: hat_i2c`); the Grove connector (`grove_i2c`, GPIO32/33) carries the ENV III Unit (`sht3xd` + `qmp6988`, both `i2c_id: grove_i2c`). ESPHome requires every bus to have an `id` once more than one `i2c:` bus is declared — an unnamed first bus fails CI. Keep them separate and keep `i2c_id` set on every consumer.
- **Hardware pins (M5StickC-Plus)** — IR TX GPIO9 (built-in LED), PDM Mic CLK GPIO0 / DATA GPIO34, Display SPI CLK=13/MOSI=15/CS=5/DC=23/RST=18, AXP192 I2C (HAT bus) SDA=21/SCL=22, ENV III Unit I2C (Grove bus) SDA=32/SCL=33, Button A GPIO37 (inverted), Button B GPIO39 (inverted, unused).

#### `m5stickc-plus-gree-ac-remote-tx-rx`

- **IR TX+RX via external M5Stack IR Unit on the Grove port** — `remote_transmitter` on GPIO32, `remote_receiver` (id `ir_receiver`) on GPIO33, bound into `climate.gree` via `receiver_id`, so physical-remote button presses decode and sync `climate.ac`'s HA state automatically. `dump: raw` is left on the receiver to help diagnose the Gree dialect.
- **Single I2C bus** — GPIO21/22 carries both the AXP192 PMU and the ENV III HAT (stacked on the HAT port; no wiring), since the Grove port is occupied by the IR Unit.
- **Hardware pins (M5StickC-Plus)** — IR TX GPIO32 (Grove pin 1), IR RX GPIO33 (Grove pin 2), PDM Mic CLK GPIO0 / DATA GPIO34, Display SPI CLK=13/MOSI=15/CS=5/DC=23/RST=18, AXP192/ENV HAT I2C SDA=21/SCL=22, Button A GPIO37 (inverted), Button B GPIO39 (inverted, unused).

## Build

```bash
esphome run samples/basic-beep-detector/basic-beep-detector.yaml
esphome run samples/m5stickc-plus-gree-ac-remote-tx-only/ac-remote.yaml
esphome run samples/m5stickc-plus-gree-ac-remote-tx-rx/ac-remote.yaml
```
