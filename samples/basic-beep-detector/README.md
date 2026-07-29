# Sample: Basic Beep Detector

The smallest useful wiring of the [`beep_detector`](../../) component: a generic ESP32 dev board, a PDM microphone, and a binary sensor that turns on for half a second whenever a beep is heard. No display, no IR, no appliance-specific logic — a starting point for wiring the component into your own project.

## Hardware

Any ESP32 board plus any I2S PDM microphone breakout. `basic-beep-detector.yaml` uses `esp32dev` with placeholder mic pins (`GPIO32` clock, `GPIO33` data) — change `esp32.board` and the `i2s_audio`/`microphone` block to match your hardware. `beep_detector` itself is not tied to I2S/PDM; swap in any ESPHome [`microphone:`](https://esphome.io/components/microphone/) platform.

## Build

Clone the repo and build directly — `beep_detector_source` defaults to the local `../../components` path:

```bash
esphome run samples/basic-beep-detector/basic-beep-detector.yaml
```

To build without cloning, point `beep_detector_source` at GitHub instead:

```yaml
substitutions:
  beep_detector_source: github://jeeyo/esp32-ir-ac-thermostat@main
```

## What it does

1. `microphone: platform: i2s_audio` streams PCM samples to `beep_detector`
2. `beep_detector` scores each chunk against `target_frequency: 4000.0` (±200 Hz) with amplitude window 500–5000
3. On a valid beep, `on_beep_detected` logs a line and pulses `binary_sensor.beep_detected` on for 500 ms

Adjust `target_frequency`, `amplitude_min`/`amplitude_max`, and the duration window to match whatever you're listening for — see the [component README](../../README.md#calibration) for how to calibrate these against a real beep source instead of guessing.
