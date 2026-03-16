#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/SubEngine.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace Catch::Matchers;

namespace {
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr float kDefaultFreqHz = 200.0f;

/// freqLut に定数周波数を設定するヘルパー
void setConstantFreqLut(SubEngine &eng, float hz = kDefaultFreqHz) {
  std::array<float, EnvelopeLutManager::lutSize> buf;
  buf.fill(hz);
  eng.freqLut().bake(buf.data(), EnvelopeLutManager::lutSize);
  eng.freqLut().setDurationMs(300.0f);
}

/// prepare → freq LUT 設定 済みのエンジンを返すヘルパー
std::unique_ptr<SubEngine> makeEngine(float lengthMs = 300.0f,
                                      float gainDb = 0.0f) {
  auto eng = std::make_unique<SubEngine>();
  eng->prepareToPlay(kSampleRate, kBlockSize);
  setConstantFreqLut(*eng);

  // dist/mix LUT はリセット後 1.0f 埋め → drive 24dB / mix=additive になるので
  // テスト用にゼロ（クリーン／Sine）へ上書きする
  std::array<float, EnvelopeLutManager::lutSize> zeros{};
  eng->distLut().bake(zeros.data(), EnvelopeLutManager::lutSize);
  eng->distLut().setDurationMs(300.0f);
  eng->mixLut().bake(zeros.data(), EnvelopeLutManager::lutSize);
  eng->mixLut().setDurationMs(300.0f);

  eng->setLengthMs(lengthMs);
  eng->setGainDb(gainDb);
  return eng;
}

/// render 1 block のヘルパー
std::vector<float> renderOneBlock(SubEngine &eng, int numSamples = kBlockSize,
                                  bool subPass = true) {
  juce::AudioBuffer<float> buf(1, numSamples);
  buf.clear();
  eng.render(buf, numSamples, subPass, kSampleRate);
  auto *ch = buf.getReadPointer(0);
  return {ch, ch + numSamples};
}

float absPeak(const std::vector<float> &v) {
  float p = 0.0f;
  for (float s : v)
    p = std::max(p, std::abs(s));
  return p;
}
} // namespace

// ── prepareToPlay ───────────────────────────────────

TEST_CASE("SubEngine: prepareToPlay does not crash", "[sub_engine]") {
  SubEngine eng;
  eng.prepareToPlay(kSampleRate, kBlockSize);
  SUCCEED();
}

// ── triggerNote + render ────────────────────────────

TEST_CASE("SubEngine: render produces silence before trigger", "[sub_engine]") {
  auto eng = makeEngine();

  auto samples = renderOneBlock(*eng);
  CHECK(absPeak(samples) < 1e-6f);
}

TEST_CASE("SubEngine: render produces output after trigger", "[sub_engine]") {
  auto eng = makeEngine();
  eng->triggerNote();

  auto samples = renderOneBlock(*eng);
  CHECK(absPeak(samples) > 1e-6f);
}

// ── 出力値域 ────────────────────────────────────────

TEST_CASE("SubEngine: output stays within reasonable range", "[sub_engine]") {
  auto eng = makeEngine(500.0f);
  eng->triggerNote();

  for (int blk = 0; blk < 20; ++blk) {
    auto samples = renderOneBlock(*eng);
    for (float s : samples)
      CHECK(std::abs(s) <= 2.0f);
  }
}

// ── gainDb ──────────────────────────────────────────

TEST_CASE("SubEngine: setGainDb affects output level", "[sub_engine]") {
  auto peakOf = [](float gainDb) {
    auto eng = makeEngine(300.0f, gainDb);
    eng->triggerNote();

    float peak = 0.0f;
    for (int blk = 0; blk < 5; ++blk) {
      auto samples = renderOneBlock(*eng);
      peak = std::max(peak, absPeak(samples));
    }
    return peak;
  };

  CHECK(peakOf(0.0f) > peakOf(-12.0f));
}

// ── lengthMs（One-shot 長さ）────────────────────────

TEST_CASE("SubEngine: stops after lengthMs", "[sub_engine]") {
  auto eng = makeEngine(10.0f);
  eng->triggerNote();

  for (int blk = 0; blk < 10; ++blk)
    renderOneBlock(*eng);

  auto tail = renderOneBlock(*eng);
  CHECK(absPeak(tail) < 1e-6f);
}

// ── subPass=false（Mute 時: buffer に加算しない）────

TEST_CASE("SubEngine: subPass=false writes scratchBuffer only",
          "[sub_engine]") {
  auto eng = makeEngine();
  eng->triggerNote();

  juce::AudioBuffer<float> buf(1, kBlockSize);
  buf.clear();
  eng->render(buf, kBlockSize, false, kSampleRate);

  // buffer は無音のまま
  const float *ch = buf.getReadPointer(0);
  float bufPeak = 0.0f;
  for (int i = 0; i < kBlockSize; ++i)
    bufPeak = std::max(bufPeak, std::abs(ch[i]));
  CHECK(bufPeak < 1e-6f);

  // scratchBuffer にはデータがある
  const float *scratch = eng->scratchData();
  float scrPeak = 0.0f;
  for (int i = 0; i < kBlockSize; ++i)
    scrPeak = std::max(scrPeak, std::abs(scratch[i]));
  CHECK(scrPeak > 1e-6f);
}

// ── triggerNote offset ──────────────────────────────

TEST_CASE("SubEngine: triggerNote offset delays start", "[sub_engine]") {
  auto eng = makeEngine();
  eng->triggerNote(100); // 100 サンプル遅延

  auto samples = renderOneBlock(*eng);

  // 先頭 100 サンプルは無音
  for (int i = 0; i < 100; ++i)
    CHECK(std::abs(samples[static_cast<std::size_t>(i)]) < 1e-6f);

  // 100 以降にはデータがある
  float tail = 0.0f;
  for (std::size_t i = 100; i < samples.size(); ++i)
    tail = std::max(tail, std::abs(samples[i]));
  CHECK(tail > 1e-6f);
}

// ── stereo render ───────────────────────────────────

TEST_CASE("SubEngine: renders identical L/R in stereo", "[sub_engine]") {
  auto eng = makeEngine();
  eng->triggerNote();

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  eng->render(buf, kBlockSize, true, kSampleRate);

  const float *left = buf.getReadPointer(0);
  const float *right = buf.getReadPointer(1);
  for (int i = 0; i < kBlockSize; ++i)
    CHECK_THAT(static_cast<double>(left[i]),
               WithinAbs(static_cast<double>(right[i]), 1e-6));
}
