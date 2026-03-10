#include "DirectEngine.h"
#include "Saturator.h"

#include <cmath>
#include <span>

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
  noteTimeSamples_ = 0.0f;
  active_.store(false);

  sampler_.prepare();
  directAmpLut_.reset();
}

void DirectEngine::triggerNote() {
  // パススルーモード時はサンプル不要でトリガー可能
  if (!passthroughMode_.load() && !sampler_.isLoaded())
    return;

  // サンプルモード時のみプレイヘッドリセット
  if (!passthroughMode_.load())
    sampler_.resetPlayhead();

  noteTimeSamples_ = 0.0f;

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

float DirectEngine::synthesizeSample(const FilterState &fs, double playRate) {
  bool finished = false;
  float s = sampler_.readInterpolated(viewData_, viewLen_, playRate, finished);
  if (finished)
    return 0.0f; // render 側で active を落とす

  // ── Drive + Clip（HPF/LPF 前）──
  s = Saturator::process(s, driveDb_.load(), clipType_.load());
  // ── ポスト HPF / LPF ──
  for (int fi = 0; fs.doHpf && fi < fs.hpfStg; ++fi)
    s = hpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
  for (int fi = 0; fs.doLpf && fi < fs.lpfStg; ++fi)
    s = lpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
  if (fs.doHpf || fs.doLpf)
    s = Saturator::process(s, 0.0f,
                           clipType_.load()); // 共振ピークを ClipType で整形
  return s;
}

float DirectEngine::computeMaxTimeSamples(float sr, double playRate) const {
  const float ampDurMs = directAmpLut_.getDurationMs();
  const float ampDurSamples = ampDurMs * sr / 1000.0f;
  const double dur = sampler_.durationSec();
  const float samplerDurSamples =
      (dur > 0.0 && playRate > 0.0)
          ? static_cast<float>(dur / playRate * static_cast<double>(sr))
          : 1e9f;
  return std::min(ampDurSamples, samplerDurSamples);
}

float DirectEngine::computeSampleAmp(float noteTimeMs) const {
  return EnvelopeLutManager::computeAmp(
      directAmpLut_.getActiveLut(), directAmpLut_.getDurationMs(), noteTimeMs);
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

  const float maxTimeSamples = computeMaxTimeSamples(sr, playRate);
  const FilterState fs = prepareFilters(sr);

  auto view = sampler_.lock();
  if (!view) {
    std::fill_n(scratchBuffer_.data(), static_cast<std::size_t>(numSamples),
                0.0f);
    return;
  }
  // synthesizeSample() からアクセスするためメンバーにキャッシュ
  viewData_ = view.data;
  viewLen_ = view.length;

  const int numCh = buffer.getNumChannels();

  for (int i = 0; i < numSamples; ++i) {
    if (noteTimeSamples_ >= maxTimeSamples) {
      active_.store(false);
      std::fill_n(scratchBuffer_.data() + i,
                  static_cast<std::size_t>(numSamples - i), 0.0f);
      break;
    }

    const float noteTimeMs = noteTimeSamples_ * 1000.0f / sr;
    const float amp = computeSampleAmp(noteTimeMs);
    const float out = synthesizeSample(fs, playRate) * amp * gain;

    scratchBuffer_[static_cast<std::size_t>(i)] = out;
    if (directPass) {
      for (int ch = 0; ch < numCh; ++ch)
        buffer.addSample(ch, i, out);
    }
    noteTimeSamples_ += 1.0f;
  }

  viewData_ = nullptr;
  viewLen_ = 0;
}

// ────────────────────────────────────────────────
// renderPassthrough
// ────────────────────────────────────────────────

void DirectEngine::renderPassthrough(juce::AudioBuffer<float> &buffer,
                                     std::span<const float> inputMono,
                                     int numSamples, double sampleRate) {
  const auto sr = static_cast<float>(sampleRate);
  const float gain = juce::Decibels::decibelsToGain(gainDb_.load());

  // Amp Envelope LUT の最大再生時間（パススルーではサンプル長がないので LUT 期間のみ）
  const float ampDurMs = directAmpLut_.getDurationMs();
  const float maxTimeSamples = ampDurMs * sr / 1000.0f;

  const FilterState fs = prepareFilters(sr);
  const int numCh = buffer.getNumChannels();

  for (int i = 0; i < numSamples; ++i) {
    float amp = 1.0f;

    if (active_.load()) {
      if (noteTimeSamples_ >= maxTimeSamples) {
        active_.store(false);
        amp = 0.0f;
      } else {
        const float noteTimeMs = noteTimeSamples_ * 1000.0f / sr;
        amp = computeSampleAmp(noteTimeMs);
      }
      noteTimeSamples_ += 1.0f;
    } else {
      amp = 0.0f; // Decay 終了後はミュート（Amp Envelope 制御）
    }

    float s = (i < static_cast<int>(inputMono.size())) ? inputMono[static_cast<std::size_t>(i)] : 0.0f;

    // Drive（プリフィルター）
    s = Saturator::process(s, driveDb_.load(), clipType_.load());

    // HPF / LPF
    for (int fi = 0; fs.doHpf && fi < fs.hpfStg; ++fi)
      s = hpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
    for (int fi = 0; fs.doLpf && fi < fs.lpfStg; ++fi)
      s = lpfs_[static_cast<std::size_t>(fi)].processSample(0, s);

    // 共振ピーク整形
    if (fs.doHpf || fs.doLpf)
      s = Saturator::process(s, 0.0f, clipType_.load());

    s *= gain * amp;

    scratchBuffer_[static_cast<std::size_t>(i)] = s;
    for (int ch = 0; ch < numCh; ++ch)
      buffer.addSample(ch, i, s);
  }
}
