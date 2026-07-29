#include "beep_detector.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace beep_detector {

static const char *TAG = "beep_detector";

void BeepDetector::setup() {
  ESP_LOGI(TAG, "Setting up BeepDetector");
  ESP_LOGI(TAG, "  Target frequency: %.0f Hz (±%.0f Hz)", this->target_frequency_, this->frequency_tolerance_);
  ESP_LOGI(TAG, "  Amplitude window: %.0f - %.0f", this->amplitude_min_, this->amplitude_max_);
  ESP_LOGI(TAG, "  Duration window: %d - %d ms", this->min_duration_ms_, this->max_duration_ms_);

  if (this->mic_ != nullptr) {
    this->mic_->add_data_callback([this](const std::vector<uint8_t> &data) {
      this->on_audio_data(data);
    });
    this->mic_->start();
  }
}

void BeepDetector::loop() {
  // YAML triggers must fire on the main loop, never the audio task.
  if (this->beep_detected_pending_.exchange(false)) {
    this->beep_detected_callback_.call();
  }
}

void BeepDetector::on_audio_data(const std::vector<uint8_t> &data) {
  if (this->paused_ || data.empty()) {
    return;
  }
  const int16_t *samples = reinterpret_cast<const int16_t *>(data.data());
  int num_samples = static_cast<int>(data.size() / sizeof(int16_t));
  if (num_samples > 0) {
    this->process_audio(samples, num_samples);
  }
}

float BeepDetector::goertzel_magnitude(const int16_t *samples, int num_samples, float frequency) {
  // Goertzel algorithm — efficient single-frequency DFT bin
  // O(N) per frequency, much cheaper than FFT for single-tone detection
  float k = roundf((float)num_samples * frequency / (float)this->sample_rate_);
  float omega = 2.0f * M_PI * k / (float)num_samples;
  float coeff = 2.0f * cosf(omega);

  float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;

  for (int i = 0; i < num_samples; i++) {
    s0 = (float)samples[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }

  // Magnitude squared (avoid sqrt for efficiency, compare squared thresholds)
  float magnitude_sq = s1 * s1 + s2 * s2 - coeff * s1 * s2;
  // Normalize by number of samples
  float magnitude = sqrtf(fabsf(magnitude_sq)) / (float)num_samples;

  return magnitude;
}

void BeepDetector::process_audio(const int16_t *data, int num_samples) {
  uint32_t now = millis();

  // Cooldown check — ignore beeps too soon after the last one
  if (this->last_beep_time_ > 0 && (now - this->last_beep_time_) < (uint32_t)this->cooldown_ms_) {
    return;
  }

  if (this->calibrating_) {
    // Calibration mode: sweep frequencies and track peak
    for (int freq = CAL_FREQ_START; freq <= CAL_FREQ_END; freq += CAL_FREQ_STEP) {
      float mag = this->goertzel_magnitude(data, num_samples, (float)freq);
      if (mag > this->cal_peak_amplitude_) {
        this->cal_peak_amplitude_ = mag;
        this->cal_peak_frequency_ = (float)freq;
      }
    }
    return;
  }

  // Normal detection mode: check target frequency and neighbors for robustness
  float mag_center = this->goertzel_magnitude(data, num_samples, this->target_frequency_);
  float mag_low = this->goertzel_magnitude(data, num_samples, this->target_frequency_ - this->frequency_tolerance_);
  float mag_high = this->goertzel_magnitude(data, num_samples, this->target_frequency_ + this->frequency_tolerance_);

  // Use the maximum of the three bins to handle slight frequency drift
  float magnitude = std::max({mag_center, mag_low, mag_high});

  bool in_amplitude_window = (magnitude >= this->amplitude_min_) && (magnitude <= this->amplitude_max_);

  if (in_amplitude_window) {
    if (!this->beep_active_) {
      // Beep onset
      this->beep_active_ = true;
      this->beep_start_time_ = now;
      ESP_LOGD(TAG, "Beep onset detected (amplitude: %.1f)", magnitude);
    }
  } else {
    if (this->beep_active_) {
      // Beep offset — check duration
      uint32_t duration = now - this->beep_start_time_;
      this->beep_active_ = false;

      ESP_LOGD(TAG, "Beep offset (duration: %u ms, amplitude was: %.1f)", (unsigned) duration, magnitude);

      if ((int)duration >= this->min_duration_ms_ && (int)duration <= this->max_duration_ms_) {
        // Valid beep detected — defer trigger fire to main loop.
        ESP_LOGI(TAG, "Valid beep detected! Duration: %u ms", (unsigned) duration);
        this->last_beep_time_ = now;
        this->beep_detected_pending_ = true;
      } else if ((int)duration > this->max_duration_ms_) {
        ESP_LOGD(TAG, "Beep too long (%u ms > %d ms), ignoring", (unsigned) duration, this->max_duration_ms_);
      } else {
        ESP_LOGD(TAG, "Beep too short (%u ms < %d ms), ignoring", (unsigned) duration, this->min_duration_ms_);
      }
    }
  }
}

void BeepDetector::start_calibration() {
  ESP_LOGI(TAG, "Starting calibration — trigger AC beep within 10 seconds");
  this->calibrating_ = true;
  this->cal_peak_frequency_ = 0.0f;
  this->cal_peak_amplitude_ = 0.0f;
}

CalibrationResult BeepDetector::finish_calibration() {
  this->calibrating_ = false;

  CalibrationResult result;
  result.frequency = this->cal_peak_frequency_;
  result.amplitude = this->cal_peak_amplitude_;

  ESP_LOGI(TAG, "Calibration complete:");
  ESP_LOGI(TAG, "  Peak frequency: %.0f Hz", result.frequency);
  ESP_LOGI(TAG, "  Peak amplitude: %.0f", result.amplitude);
  ESP_LOGI(TAG, "  Suggested amplitude_min: %.0f", result.amplitude * 0.7f);
  ESP_LOGI(TAG, "  Suggested amplitude_max: %.0f", result.amplitude * 1.3f);

  return result;
}

}  // namespace beep_detector
}  // namespace esphome
