#include "DirectEngine.h"
#include <cmath>

// ────────────────────────────────────────────────────
// lifecycle
// ────────────────────────────────────────────────────

void DirectEngine::prepareToPlay(double /*sampleRate*/, int samplesPerBlock) {
  using enum juce::dsp::StateVariableTPTFilterType;
  for (auto &f : hpfs_) {
    f.reset();
    f.setType(highpass);
  }
  for (auto &f : lpfs_) {
    f.reset();
    f.setType(lowpass);
  }

  scratchBuffer_.resize(static_cast<std::size_t>(samplesPerBlock));
  envPhase_ = EnvPhase::Idle;
  envLevel_ = 0.0f;
  active_.store(false);

  sampler_.prepare();
}

void DirectEngine::triggerNote() {
  if (!sampler_.isLoaded())
    return;

  sampler_.resetPlayhead();
  envPhase_ = EnvPhase::Attack;
  envLevel_ = 0.0f;

  for (auto &f : hpfs_)
    f.reset();
  for (auto &f : lpfs_)
    f.reset();

  active_.store(true);
}

// ────────────────────────────────────────────────────
// プライベートヘルパー
// ────────────────────────────────────────────────────

auto DirectEngine::prepareFilters(float sr) -> FilterState {
  const float hpfF = juce::jlimit(20.0f, sr * 0.49f, hpfParams_.freq.load());
  const float hpfQv = hpfParams_.q.load();
  const int hpfStg = hpfParams_.stages.load();
  const float lpfF = juce::jlimit(20.0f, sr * 0.49f, lpfParams_.freq.load());
  const float lpfQv = lpfParams_.q.load();
  const int lpfStg = lpfParams_.stages.load();
  const bool doHpf = hpfF > 20.5f;
  const bool doLpf = lpfF < 19999.0f;

  if (doHpf) {
    const float qH = juce::jlimit(0.1f, 6.0f, hpfQv > 0.001f ? hpfQv : 0.707f);
    for (int i = 0; i < hpfStg; ++i) {
      hpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(hpfF);
      hpfs_[static_cast<std::size_t>(i)].setResonance(qH);
    }
  }
  if (doLpf) {
    const float qL = juce::jlimit(0.1f, 6.0f, lpfQv > 0.001f ? lpfQv : 0.707f);
    for (int i = 0; i < lpfStg; ++i) {
      lpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(lpfF);
      lpfs_[static_cast<std::size_t>(i)].setResonance(qL);
    }
  }
  return {doHpf, doLpf, hpfStg, lpfStg};
}

float DirectEngine::applyFilters(float s, const FilterState &fs) {
  for (int fi = 0; fs.doHpf && fi < fs.hpfStg; ++fi)
    s = hpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
  for (int fi = 0; fs.doLpf && fi < fs.lpfStg; ++fi)
    s = lpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
  if (fs.doHpf || fs.doLpf)
    s = std::tanh(s);  // 共振ピークのソフトクリップ
  return s;
}

bool DirectEngine::advanceEnvelope(float nt, float atkSamples, float decSamples,
                                   float relSamples) {
  using enum EnvPhase;
  if (envPhase_ == Attack) {
    if (atkSamples < 1.0f) {
      envLevel_ = 1.0f;
      envPhase_ = Hold;
    } else {
      envLevel_ = juce::jmin(1.0f, nt / atkSamples);
      if (envLevel_ >= 1.0f)
        envPhase_ = Hold;
    }
  } else if (envPhase_ == Hold) {
    envLevel_ = 1.0f;
    if (nt >= atkSamples + decSamples)
      envPhase_ = Release;
  } else if (envPhase_ == Release) {
    const float relStart = atkSamples + decSamples;
    const float t = (relSamples > 0.0f) ? (nt - relStart) / relSamples : 1.0f;
    envLevel_ = juce::jmax(0.0f, 1.0f - t);
    if (envLevel_ <= 0.0f) {
      envPhase_ = Idle;
      active_.store(false);
      return true;
    }
  }
  return false;
}

// ────────────────────────────────────────────────────
// render
// ────────────────────────────────────────────────────

void DirectEngine::render(juce::AudioBuffer<float> &buffer, int numSamples,
                          bool directPass, double sampleRate) {
  if (!active_.load()) {
    std::fill_n(scratchBuffer_.data(), static_cast<std::size_t>(numSamples),
                0.0f);
    return;
  }

  const auto sr = static_cast<float>(sampleRate);
  const float gain = juce::Decibels::decibelsToGain(gainDb_.load());
  const double playRate =
      static_cast<double>(std::pow(2.0f, pitchSemitones_.load() / 12.0f)) *
      sampler_.sampleRate() / sampleRate;
  const float atkSamples = envParams_.attackMs.load() * sr / 1000.0f;
  const float decSamples = envParams_.decayMs.load() * sr / 1000.0f;
  const float relSamples = envParams_.releaseMs.load() * sr / 1000.0f;

  const FilterState fs = prepareFilters(sr);

  auto view = sampler_.lock();
  if (!view) {
    std::fill_n(scratchBuffer_.data(), static_cast<std::size_t>(numSamples),
                0.0f);
    return;
  }

  const int numCh = buffer.getNumChannels();
  // playRate 基準で noteTime を推定
  // （SamplePlayer の playheadSamples_ は直接見えないため近似）
  float noteTimeSamples = 0.0f;
  // 初回ブロック向けに、ブロック先頭の noteTime を保持しない簡易実装
  // → advanceEnvelope は相対サンプル位置で換算

  for (int i = 0; i < numSamples; ++i) {
    // エンベロープ: ブロック先頭からの出力サンプル数で近似
    noteTimeSamples += 1.0f;
    bool done = advanceEnvelope(noteTimeSamples, atkSamples, decSamples, relSamples);

    float out = 0.0f;
    if (!done) {
      bool finished = false;
      float s = sampler_.readInterpolated(view.data, view.length,
                                          playRate, finished);
      if (finished) {
        envPhase_ = EnvPhase::Idle;
        active_.store(false);
        done = true;
      } else {
        out = applyFilters(s, fs) * envLevel_ * gain;
      }
    }

    scratchBuffer_[static_cast<std::size_t>(i)] = out;
    if (directPass) {
      for (int ch = 0; ch < numCh; ++ch)
        buffer.addSample(ch, i, out);
    }
    if (done) {
      std::fill_n(scratchBuffer_.data() + i + 1,
                  static_cast<std::size_t>(numSamples - i - 1), 0.0f);
      break;
    }
  }
}
