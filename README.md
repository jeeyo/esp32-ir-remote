# ESPHome Beep Detector

A custom [ESPHome](https://esphome.io/) component that listens on any `microphone:` platform and fires a trigger when it hears a tone-like "beep" — a sustained signal at a target frequency, within an amplitude window and duration window you configure. Runtime calibration sweeps 1–8 kHz to find the frequency and amplitude of a real-world beep source so you don't have to guess.

It's a passive listener, not a controller: `beep_detector` only reports what it hears. What you *do* with that (confirm a command, count events, trigger an automation) is up to your YAML — see the [samples](#samples) below for two different ways to use it.

## Why

Lots of appliances (air conditioners, microwaves, door locks, washing machines) beep to acknowledge a command from their own remote/keypad, but give you no other feedback channel. If you're driving one of these over IR, RF, or a relay from ESPHome, `beep_detector` lets you listen for that acknowledgement acoustically instead of trusting a fire-and-forget transmit.

## Installation

Reference the component from `external_components:` in your ESPHome YAML. Pull it straight from GitHub — no need to clone this repo:

```yaml
external_components:
  - source: github://jeeyo/esphome-beep-detector@main
    components: [beep_detector]
```

Pin to a release tag instead of `@main` for a stable build. If you *have* cloned this repo (e.g. to build one of the samples), point `source:` at the local `components/` directory instead.

`beep_detector` depends on a configured [`microphone:`](https://esphome.io/components/microphone/) platform — any platform works (I2S/PDM, ADC, etc.), it just needs to produce 16-bit PCM samples.

## Configuration

```yaml
microphone:
  - platform: i2s_audio
    id: my_mic
    # ...

beep_detector:
  id: beep_det
  microphone_id: my_mic
  target_frequency: 4000.0
  frequency_tolerance: 200.0
  amplitude_min: 500.0
  amplitude_max: 5000.0
  min_duration_ms: 50
  max_duration_ms: 500
  sample_rate: 16000
  cooldown_ms: 5000
  on_beep_detected:
    - logger.log: "Beep!"
```

| Option | Type | Default | Description |
|---|---|---|---|
| `microphone_id` | ID | *required* | The `microphone:` component to listen on |
| `target_frequency` | float (Hz) | `4000.0` | Frequency to detect |
| `frequency_tolerance` | float (Hz) | `200.0` | ± window checked alongside the center frequency, to tolerate drift |
| `amplitude_min` / `amplitude_max` | float | `500.0` / `5000.0` | Goertzel magnitude window a beep's amplitude must fall inside — rejects both silence and beeps that are too loud (e.g. a closer, different source) |
| `min_duration_ms` / `max_duration_ms` | int (ms) | `50` / `500` | How long the tone must sustain to count as a beep, not a click or a drone |
| `sample_rate` | int (Hz) | `16000` | Must match the microphone's configured sample rate |
| `cooldown_ms` | int (ms) | `5000` | Minimum time between two detected beeps |
| `on_beep_detected` | automation | — | Fires each time a beep passes all of the above checks |

### Detection algorithm

Each audio chunk is scored with a [Goertzel filter](https://en.wikipedia.org/wiki/Goertzel_algorithm) — an efficient single-bin DFT — evaluated at `target_frequency` and at `target_frequency ± frequency_tolerance`, taking the max of the three. That's cheaper than a full FFT when you only care about one tone, which matters on an ESP32's audio task. A beep is reported once its amplitude has stayed inside the window for between `min_duration_ms` and `max_duration_ms`, and `cooldown_ms` has elapsed since the last one.

### Lambda API

Beyond the YAML schema, `beep_detector` exposes a few methods for use in `lambda:` blocks or scripts:

| Method | Purpose |
|---|---|
| `id(beep_det).start_calibration()` | Begin a calibration sweep (see below) |
| `id(beep_det).finish_calibration()` | End the sweep, returns a `CalibrationResult{ float frequency; float amplitude; }` with the loudest frequency seen |
| `id(beep_det).set_paused(bool)` / `is_paused()` | Suspend/resume detection (e.g. while calibrating, or during a known-noisy period) |
| `id(beep_det).set_self_triggered(bool)` / `is_self_triggered()` | Free-form flag with no built-in behavior — useful for distinguishing "a beep I caused" from "a beep from somewhere else" in your own automations |

### Calibration

Rather than guessing `target_frequency`/`amplitude_min`/`amplitude_max` for a specific beep source, run a calibration sweep against it:

1. Call `id(beep_det).start_calibration()` (e.g. from a button press or a script)
2. Trigger the real-world beep within the next ~10 seconds
3. Call `id(beep_det).finish_calibration()` — logs the peak frequency/amplitude seen during the sweep, plus a suggested `amplitude_min`/`amplitude_max` (±30% of peak)
4. Copy those values into your `beep_detector:` config and reflash

The [`samples/m5stickc-plus-gree-ac-remote-tx-only`](samples/m5stickc-plus-gree-ac-remote-tx-only) sample wires this up to a physical button and 10-second timer if you want a working reference.

## Samples

| Sample | What it shows |
|---|---|
| [`samples/basic-beep-detector`](samples/basic-beep-detector) | Bare-minimum usage on a generic ESP32 board: a PDM mic feeding `beep_detector`, toggling a binary sensor on each beep. Start here if you're integrating the component into your own project. |
| [`samples/m5stickc-plus-gree-ac-remote-tx-only`](samples/m5stickc-plus-gree-ac-remote-tx-only) | Full real-world build: an M5StickC-Plus driving a Gree-protocol air conditioner over IR (ESPHome's built-in `climate: platform: gree`) via its **built-in IR LED** (transmit-only, no physical-remote sync), using `beep_detector` as passive, best-effort confirmation that each command was received. Grove port is free for a Grove ENV III Unit. |
| [`samples/m5stickc-plus-gree-ac-remote-tx-rx`](samples/m5stickc-plus-gree-ac-remote-tx-rx) | Same build, but using an external Grove IR TX+RX module instead of the built-in LED, adding physical-remote-to-HA state sync at the cost of the Grove port (ENV III sensors live on the internal HAT port instead). |

Each sample is a self-contained ESPHome config; see its own README for build/flash instructions specific to that hardware. All are built on every push/PR by [`.github/workflows/build.yml`](.github/workflows/build.yml).

## Repository layout

- `components/beep_detector/` — the component: `__init__.py` (schema/codegen), `beep_detector.h`/`.cpp` (Goertzel detector, calibration sweep)
- `samples/` — example ESPHome configs built on the component (see above)
- `.github/workflows/build.yml` — CI build of all samples on push/PR
- `.github/workflows/release.yml` — builds and publishes both `gree-ac-remote` sample variants (`tx-only` and `tx-rx`) to a GitHub release on `v*` tags

## Contributing

Issues and PRs welcome — in particular, extra samples for other beep-acknowledging appliances are a great way to prove out the component against real hardware.
