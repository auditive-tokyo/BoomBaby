#include "ClickEngine.h"
#include <cmath>

void ClickEngine::prepareToPlay(double sampleRate, int samplesPerBlock) {
  sqrOsc_.prepareToPlay(sampleRate);
  sawOsc_.prepareToPlay(sampleRate);
  sqrOsc_.setWaveShape(WaveShape::Square);
  sawOsc_.setWaveShape(WaveShape::Saw);
  // blend = -1.0 → Sine を経由せず wavetable を直接出力
  sqrOsc_.setBlend(-1.0f);
  sawOsc_.setBlend(-1.0f);
  scratchBuffer_.resize(static_cast<size_t>(samplesPerBlock));
  noteTimeSamples_ = 0.0f;
}

void ClickEngine::triggerNote() {
  sqrOsc_.triggerNote();
  sawOsc_.triggerNote();
  noteTimeSamples_ = 0.0f;
}

void ClickEngine::render(juce::AudioBuffer<float> &buffer, int numSamples,
                         bool clickPass, double sampleRate) {
  if (!sqrOsc_.isActive() && !sawOsc_.isActive()) {
    std::fill_n(scratchBuffer_.data(), numSamples, 0.0f);
    return;
  }

  const int numChannels = buffer.getNumChannels();
  const auto sr = static_cast<float>(sampleRate);
  const float clickGain = juce::Decibels::decibelsToGain(gainDb_.load());
  const float decayMs = decayMs_.load();
  const float blend = blend_.load(); // 0=SQR, 1=SAW
  // 5τ 後に停止 (~-87 dB)
  const float maxTimeSamples = decayMs * sr / 1000.0f * 5.0f;

  // 固定ピッチ: 200 Hz（クリックトーン定番）
  constexpr float kClickFreqHz = 200.0f;
  const float sqrGain = 1.0f - blend;
  const float sawGain = blend;

  for (int sample = 0; sample < numSamples; ++sample) {
    if (noteTimeSamples_ >= maxTimeSamples) {
      sqrOsc_.stopNote();
      sawOsc_.stopNote();
      std::fill_n(scratchBuffer_.data() + sample,
                  static_cast<size_t>(numSamples - sample), 0.0f);
      break;
    }

    // 指数減衰エンベロープ
    const float amp =
        std::exp(-noteTimeSamples_ * 1000.0f / (decayMs * sr + 1e-6f));

    sqrOsc_.setFrequencyHz(kClickFreqHz);
    sawOsc_.setFrequencyHz(kClickFreqHz);
    const float oscSample =
        (sqrOsc_.getNextSample() * sqrGain +
         sawOsc_.getNextSample() * sawGain) * amp * clickGain;

    scratchBuffer_[static_cast<size_t>(sample)] = oscSample;
    if (clickPass) {
      for (int ch = 0; ch < numChannels; ++ch)
        buffer.addSample(ch, sample, oscSample);
    }
    noteTimeSamples_ += 1.0f;
  }
}
