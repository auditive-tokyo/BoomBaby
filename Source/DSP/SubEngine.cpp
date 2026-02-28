#include "SubEngine.h"
#include <cmath>

void SubEngine::prepareToPlay(double sampleRate, int samplesPerBlock) {
  osc_.prepareToPlay(sampleRate);
  scratchBuffer_.resize(static_cast<size_t>(samplesPerBlock));
  noteTimeSamples_ = 0.0f;
  envLut_.reset();
}

void SubEngine::triggerNote() {
  osc_.triggerNote();
  noteTimeSamples_ = 0.0f;
}

void SubEngine::render(juce::AudioBuffer<float> &buffer, int numSamples,
                       bool subPass, double sampleRate) {
  if (!osc_.isActive()) {
    std::fill_n(scratchBuffer_.data(), numSamples, 0.0f);
    return;
  }

  const float gain = juce::Decibels::decibelsToGain(gainDb_.load());
  const int numChannels = buffer.getNumChannels();
  const auto &ampLut = envLut_.getActiveLut();
  const float ampDurMs = envLut_.getDurationMs();
  const auto &pLut = pitchLut_.getActiveLut();
  const float pitchDurMs = pitchLut_.getDurationMs();
  const auto &dLut = distLut_.getActiveLut();
  const float distDurMs = distLut_.getDurationMs();
  const auto &bLut = blendLut_.getActiveLut();
  const float blendDurMs = blendLut_.getDurationMs();
  const auto sr = static_cast<float>(sampleRate);

  const float lengthMs = lengthMs_.load();
  constexpr float fadeOutMs = 5.0f;
  const float fadeStartMs = std::max(0.0f, lengthMs - fadeOutMs);

  for (int sample = 0; sample < numSamples; ++sample) {
    const float noteTimeMs = noteTimeSamples_ * 1000.0f / sr;

    // One-shot: Length 到達で自動停止
    if (noteTimeMs >= lengthMs) {
      osc_.stopNote();
      std::fill_n(scratchBuffer_.data() + sample,
                  static_cast<size_t>(numSamples - sample), 0.0f);
      break;
    }

    // 末尾 fadeout: half-cosine ゲイン (1.0 → 0.0)
    float fadeGain = 1.0f;
    if (noteTimeMs > fadeStartMs && fadeOutMs > 0.0f) {
      const float t = (noteTimeMs - fadeStartMs) / fadeOutMs;
      fadeGain = 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
    }

    // Freq LUT → Hz
    const float pitchLutPos =
        (pitchDurMs > 0.0f)
            ? (noteTimeMs / pitchDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto pitchLutIdx =
        std::min(static_cast<int>(pitchLutPos), EnvelopeLutManager::lutSize - 1);
    const float pitchHz = pLut[static_cast<size_t>(pitchLutIdx)];
    osc_.setFrequencyHz(pitchHz);

    // Saturate LUT → drive01
    const float distLutPos =
        (distDurMs > 0.0f)
            ? (noteTimeMs / distDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto distLutIdx =
        std::min(static_cast<int>(distLutPos), EnvelopeLutManager::lutSize - 1);
    osc_.setDist(dLut[static_cast<size_t>(distLutIdx)]);

    // Mix LUT → blend
    const float blendLutPos =
        (blendDurMs > 0.0f)
            ? (noteTimeMs / blendDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto blendLutIdx =
        std::min(static_cast<int>(blendLutPos), EnvelopeLutManager::lutSize - 1);
    osc_.setBlend(bLut[static_cast<size_t>(blendLutIdx)]);

    // Gain LUT → エンベロープゲイン
    const float lutPos =
        (ampDurMs > 0.0f)
            ? (noteTimeMs / ampDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto lutIndex =
        std::min(static_cast<int>(lutPos), EnvelopeLutManager::lutSize - 1);
    const float envGain = ampLut[static_cast<size_t>(lutIndex)];

    const float oscSample = osc_.getNextSample() * gain * envGain * fadeGain;
    scratchBuffer_[static_cast<size_t>(sample)] = oscSample;
    if (subPass) {
      for (int ch = 0; ch < numChannels; ++ch)
        buffer.addSample(ch, sample, oscSample);
    }
    noteTimeSamples_ += 1.0f;
  }
}
