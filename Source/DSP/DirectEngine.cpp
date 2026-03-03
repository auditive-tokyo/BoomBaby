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
  playheadSamples_ = 0.0;
  envPhase_ = EnvPhase::Idle;
  envLevel_ = 0.0f;
  active_.store(false);

  formatManager_.registerBasicFormats(); // WAV / AIFF
}

void DirectEngine::triggerNote() {
  if (!sampleLoaded_.load())
    return;

  playheadSamples_ = 0.0;
  envPhase_ = EnvPhase::Attack;
  envLevel_ = 0.0f;

  for (auto &f : hpfs_)
    f.reset();
  for (auto &f : lpfs_)
    f.reset();

  active_.store(true);
}

// ────────────────────────────────────────────────────
// サンプルロード（メッセージスレッドから）
// ────────────────────────────────────────────────────

void DirectEngine::loadSample(const juce::File &file) {
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager_.createReaderFor(file));
  if (reader == nullptr)
    return;

  // デコード（最大 30 秒）
  const auto maxSamples = static_cast<int>(
      std::min(reader->lengthInSamples,
               static_cast<juce::int64>(reader->sampleRate * 30.0)));
  juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels),
                               maxSamples);
  reader->read(&buf, 0, maxSamples, 0, true, true);

  // モノ化（複数チャンネルの場合は平均）
  juce::AudioBuffer<float> mono(1, maxSamples);
  mono.clear();
  for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    mono.addFrom(0, 0, buf, ch, 0, maxSamples,
                 1.0f / static_cast<float>(buf.getNumChannels()));

  const double fileSr = reader->sampleRate;

  // スピンロックで保護しながらスワップ
  {
    const juce::SpinLock::ScopedLockType lock(sampleLock_);
    sampleBuffer_ = std::move(mono);
    sampleSampleRate_ = fileSr;
  }

  active_.store(false); // 再生中なら停止
  sampleLoaded_.store(true);
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
  const bool doHpf = hpfQv > 0.001f;
  const bool doLpf = lpfQv > 0.001f;

  if (doHpf) {
    const float qH = juce::jlimit(0.01f, 30.0f, hpfQv);
    for (int i = 0; i < hpfStg; ++i) {
      hpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(hpfF);
      hpfs_[static_cast<std::size_t>(i)].setResonance(qH);
    }
  }
  if (doLpf) {
    const float qL = juce::jlimit(0.01f, 30.0f, lpfQv);
    for (int i = 0; i < lpfStg; ++i) {
      lpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(lpfF);
      lpfs_[static_cast<std::size_t>(i)].setResonance(qL);
    }
  }
  return {doHpf, doLpf, hpfStg, lpfStg};
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

float DirectEngine::readSample(const float *srcData, int srcLen,
                               const FilterState &fs, float gain,
                               double playRate, bool &done) {
  using enum EnvPhase;
  if (playheadSamples_ >= static_cast<double>(srcLen - 1)) {
    envPhase_ = Idle;
    active_.store(false);
    done = true;
    return 0.0f;
  }

  const auto i0 = static_cast<int>(playheadSamples_);
  const int i1 = juce::jmin(i0 + 1, srcLen - 1);
  const auto frac =
      static_cast<float>(playheadSamples_ - static_cast<double>(i0));
  float s = srcData[i0] * (1.0f - frac) + srcData[i1] * frac;

  for (int fi = 0; fs.doHpf && fi < fs.hpfStg; ++fi)
    s = hpfs_[static_cast<std::size_t>(fi)].processSample(0, s);
  for (int fi = 0; fs.doLpf && fi < fs.lpfStg; ++fi)
    s = lpfs_[static_cast<std::size_t>(fi)].processSample(0, s);

  playheadSamples_ += playRate;
  return s * envLevel_ * gain;
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
      sampleSampleRate_ / sampleRate;
  const float atkSamples = attackMs_.load() * sr / 1000.0f;
  const float decSamples = decayMs_.load() * sr / 1000.0f;
  const float relSamples = releaseMs_.load() * sr / 1000.0f;

  const FilterState fs = prepareFilters(sr);

  if (const juce::SpinLock::ScopedTryLockType lock(sampleLock_);
      !lock.isLocked()) {
    std::fill_n(scratchBuffer_.data(), static_cast<std::size_t>(numSamples),
                0.0f);
    return;
  }

  const int srcLen = sampleBuffer_.getNumSamples();
  const float *srcData = sampleBuffer_.getReadPointer(0);
  const int numCh = buffer.getNumChannels();
  const double noteTimeAtBlock = playheadSamples_ / playRate;

  for (int i = 0; i < numSamples; ++i) {
    const auto nt =
        static_cast<float>(noteTimeAtBlock + static_cast<double>(i));
    bool done = advanceEnvelope(nt, atkSamples, decSamples, relSamples);
    const float out =
        done ? 0.0f : readSample(srcData, srcLen, fs, gain, playRate, done);

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
