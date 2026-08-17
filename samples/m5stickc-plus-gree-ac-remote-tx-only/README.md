# Sample: Gree AC Remote (M5StickC-Plus, internal IR TX only)

ESPHome firmware for M5StickC-Plus that acts as an IR remote transmitter for a Gree-protocol air conditioner (built for a Trane/Airlux/Electrolux "YT1F" universal remote unit), using ESPHome's built-in [`climate: platform: gree`](https://esphome.io/components/climate/gree/) component. The [`beep_detector`](../../) component gives passive, best-effort confirmation that the AC actually received a command. No cloud, no subscription — exposed as a full `climate` entity in Home Assistant.

This variant transmits IR via the M5StickC-Plus's **built-in IR LED** — no external IR hardware needed, but there's no IR receiver, so `climate.ac` is transmit-only (no physical-remote-to-HA state sync). The Grove connector is free and used here for a Grove-connected ENV III Unit. If you need physical-remote sync, see [`samples/m5stickc-plus-gree-ac-remote-tx-rx`](../m5stickc-plus-gree-ac-remote-tx-rx) instead, which uses an external Grove IR TX+RX module.

This is one worked example of using `beep_detector`; see the [repo README](../../README.md) for the component itself, and [`samples/basic-beep-detector`](../basic-beep-detector) for a minimal, hardware-agnostic starting point.

## Features

- **Gree Protocol Climate Entity** — full mode/temperature/fan control via ESPHome's built-in `climate_ir` Gree platform; no manual IR code learning required, transmitted via the M5StickC-Plus's built-in IR LED (no external IR hardware needed)
- **Acoustic Confirmation** — listens for the AC's confirmation beep after each command and reports confirmed/unconfirmed (best-effort, no retries)
- **Beep Calibration** — sweep 1–8 kHz to find your AC's exact beep frequency and amplitude
- **Temperature / Humidity / Pressure** — ENV III Unit sensors (Grove I2C) exposed to Home Assistant
- **On-device Display** — shows AC mode, target temperature, beep confirmation, and status

## Hardware

### Bill of Materials

| Item | SKU / Notes |
|------|-------------|
| M5StickC-Plus | ESP32-PICO, built-in display + PDM mic + built-in IR LED |
| M5Stack ENV III Unit | SHT30 temp/humidity + QMP6988 pressure, Grove I2C |

IR is sent via the M5StickC-Plus's built-in IR LED — no external IR module needed. The ENV III Unit connects via the Grove port (I2C).

### Pins

| Function | Pin |
|---|---|
| IR LED TX (built-in) | GPIO9 |
| PDM Mic CLK | GPIO0 |
| PDM Mic DATA | GPIO34 |
| Display SPI | CLK=13, MOSI=15, CS=5, DC=23, RST=18 |
| AXP192 I2C (internal HAT bus) | SDA=21, SCL=22 |
| ENV III Unit I2C (Grove) | SDA=GPIO32, SCL=GPIO33 |
| Button A | GPIO37 (inverted) |
| Button B | GPIO39 (inverted, unused) |

> **No physical-remote sync:** the M5StickC-Plus's built-in IR LED is transmit-only (there's no built-in IR receiver), so `climate.ac` no longer decodes button presses from the AC's own remote — this sample is IR-transmit-only.

### Wiring: M5Stack ENV III Unit → Grove Port

```
ENV III Unit (Grove)   M5StickC-Plus
─────────────────────  ─────────────
  Yellow (SDA) ──────── GPIO32 (Grove pin 1)
  White  (SCL) ──────── GPIO33 (Grove pin 2)
  Red    (5V)  ──────── 5V
  Black  (GND) ──────── GND
```

---

## Quick Start — Flash Pre-built Firmware

No toolchain needed. Download and flash in 2 minutes.

1. Download the latest `ac-remote-tx-only.bin` from [GitHub Releases](https://github.com/jeeyo/esphome-beep-detector/releases/latest)
2. Open [ESPHome Web Installer](https://web.esphome.io/) in Chrome or Edge
3. Click **Install** → select the `.bin` file → connect your M5StickC-Plus via USB-C
4. On first boot, the device exposes a WiFi network named **AC-Remote-Fallback** (password `fallback123`). Connect to it with your phone; a captive portal opens where you enter your home WiFi credentials
5. Once the device joins your network, open Home Assistant → **Settings → Devices & Services → Add Integration → ESPHome**
6. Enter the device IP or hostname `ac-remote.local`. On first adoption, HA generates an API encryption key and stores it

> The default Gree protocol dialect is `generic` — if your AC doesn't respond, see [Finding Your Gree Model](#finding-your-gree-model) below.
>
> The pre-built firmware does not hard-code WiFi, API encryption, or OTA credentials — those are configured post-install via the captive portal and Home Assistant. If you want to bake them in at build time, see [Setup from Source](#setup-from-source).

---

## Setup from Source

Build your own firmware with WiFi / API / OTA credentials baked in. **You do not need to clone this repo** — a small wrapper YAML pulls everything from GitHub.

You only create two files: `secrets.yaml` and `my-ac-remote.yaml`.

### 1. Install ESPHome

```bash
pip install esphome
```

### 2. Pick a release tag

Browse [Releases](https://github.com/jeeyo/esphome-beep-detector/releases) and pick the version you want to build (e.g. `v0.1.0`). Use the same tag in **both** places below so the upstream YAML and the bundled `beep_detector` component come from the same commit.

### 3. Create `secrets.yaml`

```yaml
wifi_ssid: "YourWiFi"
wifi_password: "YourWiFiPassword"
api_encryption_key: "base64-32-bytes"
ota_password: "your-ota-password"
```

Generate the API key:

```bash
python3 -c "import secrets, base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

`secrets.yaml` should stay out of version control — add it to `.gitignore` if you keep your wrapper config in a repo.

### 4. Create `my-ac-remote.yaml`

This is the only config file you build with. It pulls `ac-remote.yaml` and the `beep_detector` component straight from GitHub at the tag you picked, and layers your secrets on top.

```yaml
substitutions:
  # Tell the upstream config to fetch beep_detector from GitHub at the same tag.
  beep_detector_source: github://jeeyo/esphome-beep-detector@v0.1.0
  ota_password: !secret ota_password
  # Gree IR dialect — see "Finding Your Gree Model" below.
  gree_model: generic

packages:
  upstream:
    url: https://github.com/jeeyo/esphome-beep-detector
    ref: v0.1.0
    files: [samples/m5stickc-plus-gree-ac-remote-tx-only/ac-remote.yaml]
    refresh: 1d

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_encryption_key
```

The wrapper:

- **`packages.upstream`** pulls `ac-remote.yaml` from GitHub at the chosen tag.
- **`substitutions.beep_detector_source`** redirects the upstream's local-path component reference to GitHub, so no clone is needed.
- **`substitutions.ota_password`** fills in the OTA password (the upstream defaults to empty).
- **`substitutions.gree_model`** picks the Gree protocol dialect (the upstream defaults to `generic`).
- **`wifi`** / **`api`** are dict-merged with the upstream blocks: your station credentials and API encryption key get added without removing the captive-portal AP fallback.

> Bump the tag in **both** `beep_detector_source` and `packages.upstream.ref` together when you want to upgrade.

### 5. Build and flash

USB:

```bash
esphome run my-ac-remote.yaml
```

OTA (subsequent updates, no cable needed):

```bash
esphome run my-ac-remote.yaml --device ac-remote.local
```

ESPHome downloads the upstream YAML and the `beep_detector` source on first build and caches them; pass `--cache off` if you ever need to force a re-fetch.

### Developer / contributor build

If you're modifying this repo itself, clone it and build `samples/m5stickc-plus-gree-ac-remote-tx-only/ac-remote.yaml` directly:

```bash
esphome run samples/m5stickc-plus-gree-ac-remote-tx-only/ac-remote.yaml
```

The substitutions default to the local `../../components` path, so no wrapper is needed. Drop `wifi:` / `api:` / `ota_password:` overrides into a sibling `secrets.yaml` and patch the YAML, or create a wrapper that points `packages.upstream: !include ac-remote.yaml` at the local file.

---

## Finding Your Gree Model

"YT1F" is a universal remote family used across several rebadged Gree units (Trane, Airlux, Electrolux, Tadiran, Sharp, McQuay, Aermec...), and different units under that umbrella speak slightly different IR dialects. `ac-remote.yaml` exposes the dialect as the `gree_model` substitution (default `generic`), matching ESPHome's [`climate.gree`](https://esphome.io/components/climate/gree/) model options:

```
generic, yan, yaa, yac, yac1fb9, yx1ff, yag
```

To find the right one for your unit:

1. Set `gree_model` in your wrapper YAML (or edit `ac-remote.yaml` directly if building from source) and reflash
2. From Home Assistant (or the on-device Button A short-press), send a command to `climate.ac`
3. Watch `binary_sensor.ac_beep_confirmed` and the on-device display — if the AC beeps back within ~3 seconds, that model is correct
4. If it stays unconfirmed, try the next model in the list

The beep detector needs to be calibrated first (see below) for this to be reliable — an uncalibrated detector may never confirm regardless of whether the IR command is correct.

---

## Calibrate Beep Detection

If commands never confirm (check `binary_sensor.ac_beep_confirmed`), the beep detector needs calibrating for your specific AC unit and room. This wraps the generic calibration flow described in the [component README](../../README.md#calibration) with a physical button and on-device display feedback:

1. Long-press **Button A** for 3 seconds — display shows `CALIBRATE`
2. Use the physical remote to trigger your AC so it beeps
3. Wait for the 10-second calibration window to complete
4. Open logs (`esphome logs ac-remote.yaml`) and note:
   - `Peak frequency` → update `target_frequency` in `beep_detector:` block
   - `Suggested amplitude_min` / `amplitude_max` → update those fields
5. Reflash with updated values

---

## Adopting in Home Assistant

### Step 1 — Add the ESPHome Integration

1. **Settings → Devices & Services → Add Integration → ESPHome**
2. Enter `ac-remote.local` (or device IP)
3. Enter the API encryption key from your `secrets.yaml`
4. The device appears with entities across sensors, binary sensors, text sensor, and a climate entity

### Step 2 — Add a Thermostat Card to Lovelace

Paste this into a dashboard card (YAML mode):

```yaml
type: thermostat
entity: climate.ac
name: AC
```

Or use the visual card editor: **Add Card → Thermostat → Entity: climate.ac**.

The card lets you set target temperature and switch modes (off/cool/heat/dry/fan, depending on what your unit and `gree_model` support).

### Step 3 — Example Automations

**Away mode — raise setpoint when nobody is home:**

```yaml
automation:
  - alias: "AC away mode"
    trigger:
      - platform: state
        entity_id: group.household
        to: not_home
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.ac
        data:
          temperature: 28
          hvac_mode: cool
```

**Night schedule — lower setpoint at bedtime:**

```yaml
automation:
  - alias: "AC night cooling"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.ac
        data:
          temperature: 22
          hvac_mode: cool

  - alias: "AC morning off"
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: climate.set_hvac_mode
        target:
          entity_id: climate.ac
        data:
          hvac_mode: "off"
```

**Alert on missing beep confirmation:**

```yaml
automation:
  - alias: "AC command unconfirmed alert"
    trigger:
      - platform: state
        entity_id: text_sensor.last_action
        to: "No beep detected — check AC / IR alignment"
    action:
      - service: notify.mobile_app
        data:
          message: "AC command sent but no beep heard back — check IR alignment"
```

### Step 4 — Full Entity Reference

| Entity | Type | Description |
|--------|------|-------------|
| `climate.ac` | Climate | Gree AC control — mode, target temperature, fan |
| `sensor.temperature` | Sensor | Room temperature from ENV III Unit (°C) — also fed to `climate.ac` as current temperature |
| `sensor.humidity` | Sensor | Relative humidity (%) |
| `sensor.pressure` | Sensor | Barometric pressure (hPa) |
| `sensor.battery_level` | Sensor | M5StickC-Plus battery charge (%) from AXP192 |
| `binary_sensor.ac_beep_confirmed` | Binary Sensor | Last command got an acoustic beep back |
| `sensor.beep_frequency` | Sensor | Calibration: detected peak frequency (Hz) |
| `sensor.beep_amplitude` | Sensor | Calibration: detected peak amplitude |
| `text_sensor.last_action` | Text Sensor | Human-readable status of last action |

---

## Button Controls

| Button | Action | Function |
|--------|--------|----------|
| Button A | Short press | Toggle AC off/cool (normal mode only) |
| Button A | Long press 3s | Enter beep calibration mode |

Mode, target temperature, and fan speed are otherwise set via Home Assistant. Button B has no assigned function.

---

## How It Works

### Command Flow

1. HA (or Button A) sends a Gree climate command
2. `on_control` fires *before* the transmit, arming the beep detector's self-triggered flag and clearing the previous confirmation
3. `climate_ir` builds and sends the full Gree IR frame via the M5StickC-Plus's built-in IR LED (GPIO9) — this happens synchronously, with no built-in retry
4. The PDM microphone listens for a confirmation beep for a 3-second window
5. Beep detected within the amplitude window → `binary_sensor.ac_beep_confirmed` turns on, `text_sensor.last_action` reports "confirmed"
6. No beep → `text_sensor.last_action` reports "No beep detected" — nothing is resent or disabled automatically; check placement/dialect and retry manually

`climate_ir`'s `transmit_state()` runs synchronously inside the platform's `control()`, with no per-command retry/gate hook exposed to YAML — `beep_detector` here is strictly a passive confirmation signal, not a retry mechanism.

This sample is transmit-only: the M5StickC-Plus's built-in IR LED has no matching receiver, so there is no physical-remote-to-HA state sync — `climate.ac` only changes when HA (or Button A) sends a command.

### Amplitude Window

Multiple identical AC units in adjacent rooms produce the same beep frequency. The `amplitude_min` / `amplitude_max` window ensures only the nearby unit's beep (at expected volume) triggers detection. Calibration finds the right window for your installation.

---

## Troubleshooting

**AC not responding to commands:**
- Try other `gree_model` values (see [Finding Your Gree Model](#finding-your-gree-model))
- Verify with a phone camera that the IR LED flashes when a command is sent
- Position the device closer to / with clear line of sight to the AC unit's receiver

**No beep detected / always unconfirmed:**
- Run calibration (Button A long press) to find the correct frequency
- Check amplitude values in logs — may need a wider `amplitude_min`/`amplitude_max` window

**False triggers from adjacent rooms:**
- Narrow the amplitude window (tighter `amplitude_min` / `amplitude_max`)
- Position the device closer to your target AC unit

**Temperature reading seems off:**
- The SHT30 on the ENV III Unit can still read slightly high if placed close to the M5 body — give it some cable slack away from the device
- Apply a fixed offset in the YAML under `env_temperature` sensor: add `filters: - offset: -1.5`
- Calibrate against a reference thermometer after running the device for 30 minutes

**Display not working:**
- AXP192 must initialise before the display — check I2C connection at GPIO21/22

**ENV III Unit not detected:**
- Check the Grove cable is fully seated and wired SDA→GPIO32 / SCL→GPIO33
- Verify the `grove_i2c` bus (GPIO32/33) isn't shared with another Grove peripheral at a conflicting address
