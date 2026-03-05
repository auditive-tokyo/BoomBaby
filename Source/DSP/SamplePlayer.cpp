#include "SamplePlayer.h"
#include <algorithm>

// ────────────────────────────────────────────────────
// lifecycle
// ────────────────────────────────────────────────────

void SamplePlayer::prepare() {
  formatManager_.registerBasicFormats(); // WAV / AIFF
  playheadSamples_ = 0.0;
}

// ────────────────────────────────────────────────────
// サンプルロード（メッセージスレッドから）
// ────────────────────────────────────────────────────

void SamplePlayer::loadSample(const juce::File &file) {
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

  // 波形サムネイル事前計算
  {
    constexpr int kThumbBins = 512;
    const int total = mono.getNumSamples();
    const float *src = mono.getReadPointer(0);
    thumbMin_.resize(static_cast<std::size_t>(kThumbBins));
    thumbMax_.resize(static_cast<std::size_t>(kThumbBins));
    for (int bin = 0; bin < kThumbBins; ++bin) {
      const int s = (bin * total) / kThumbBins;
      const int e = ((bin + 1) * total) / kThumbBins;
      float mn = 0.0f;
      float mx = 0.0f;
      for (int j = s; j < e; ++j) {
        mn = std::min(mn, src[j]);
        mx = std::max(mx, src[j]);
      }
      thumbMin_[static_cast<std::size_t>(bin)] = mn;
      thumbMax_[static_cast<std::size_t>(bin)] = mx;
    }
    durationSec_.store(static_cast<double>(total) / fileSr);
  }

  // スピンロックで保護しながらスワップ
  {
    const juce::SpinLock::ScopedLockType lk(sampleLock_);
    buffer_ = std::move(mono);
    sampleSampleRate_ = fileSr;
  }

  loaded_.store(true);
}

// ────────────────────────────────────────────────────
// メタ情報
// ────────────────────────────────────────────────────

bool SamplePlayer::copyThumbnail(std::vector<float> &outMin,
                                 std::vector<float> &outMax) const noexcept {
  if (!loaded_.load())
    return false;
  outMin = thumbMin_;
  outMax = thumbMax_;
  return true;
}

// ────────────────────────────────────────────────────
// 読み出し
// ────────────────────────────────────────────────────

SamplePlayer::LockedView SamplePlayer::lock() noexcept {
  LockedView v;
  v.lock = std::make_unique<juce::SpinLock::ScopedTryLockType>(sampleLock_);
  if (v.lock->isLocked() && loaded_.load()) {
    v.data = buffer_.getReadPointer(0);
    v.length = buffer_.getNumSamples();
  }
  return v;
}

float SamplePlayer::readInterpolated(const float *srcData, int srcLen,
                                     double playRate, bool &finished) {
  if (playheadSamples_ >= static_cast<double>(srcLen - 1)) {
    finished = true;
    return 0.0f;
  }
  const auto i0 = static_cast<int>(playheadSamples_);
  const int i1 = juce::jmin(i0 + 1, srcLen - 1);
  const auto frac =
      static_cast<float>(playheadSamples_ - static_cast<double>(i0));
  const float s = srcData[i0] * (1.0f - frac) + srcData[i1] * frac;
  playheadSamples_ += playRate;
  return s;
}

float SamplePlayer::readNext(double playRate, bool &finished) {
  auto view = lock();
  if (!view) {
    finished = true;
    return 0.0f;
  }
  return readInterpolated(view.data, view.length, playRate, finished);
}
