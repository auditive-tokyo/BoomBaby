#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/ClickEngine.h"
#include <cmath>
#include <memory>

using namespace Catch::Matchers;

namespace {
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;

/// prepare → パラメータ設定済みの ClickEngine を返すヘルパー
std::unique_ptr<ClickEngine> makeEngine() {
  auto e = std::make_unique<ClickEngine>();
  e->prepareToPlay(kSampleRate, kBlockSize);
  e->setMode(1); // Noise
  e->setGainDb(0.0f);
  e->setDecayMs(30.0f);
  e->setFreq1(5000.0f);
  e->setFocus1(0.71f);
  e->setDriveDb(0.0f);
  e->setClipType(0);       // Soft
  e->setHpfFreq(20.0f);    // bypass
  e->setLpfFreq(20000.0f); // bypass
  return e;
}

/// scratchBuffer の先頭 numSamples を合計二乗する（エネルギー計測）
float energy(const ClickEngine &e, int numSamples) {
  const float *data = e.scratchData();
  float sum = 0.0f;
  for (int i = 0; i < numSamples; ++i)
    sum += data[i] * data[i];
  return sum;
}
} // namespace

// ─── 非アクティブ時は出力ゼロ ─────────────────────────────────

TEST_CASE("ClickEngine: inactive produces silence", "[click_engine]") {
  auto engine = makeEngine();
  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  // triggerNote を呼ばずに render
  engine->render(buf, kBlockSize, true, kSampleRate);

  REQUIRE(energy(*engine, kBlockSize) == 0.0f);
  // buffer にも何も加算されていない
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < kBlockSize; ++i)
      REQUIRE(buf.getSample(ch, i) == 0.0f);
}

// ─── triggerNote 後は出力が非ゼロ ──────────────────────────────

TEST_CASE("ClickEngine: Noise mode produces output after trigger",
          "[click_engine]") {
  auto engine = makeEngine();
  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  engine->triggerNote(0);
  engine->render(buf, kBlockSize, true, kSampleRate);

  // エネルギーがゼロでないこと（Noise が出ている）
  REQUIRE(energy(*engine, kBlockSize) > 0.0f);
}

// ─── 出力値が有限（NaN / Inf でない）──────────────────────────

TEST_CASE("ClickEngine: output is finite", "[click_engine]") {
  auto engine = makeEngine();
  engine->setDriveDb(24.0f); // 最大 drive
  engine->setClipType(2);    // Tube
  engine->setFocus1(20.0f);  // 高 Q → 共振強め
  engine->setBpf1Slope(48);  // 48dB/oct

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  engine->triggerNote(0);
  engine->render(buf, kBlockSize, true, kSampleRate);

  const float *data = engine->scratchData();
  for (int i = 0; i < kBlockSize; ++i)
    REQUIRE(std::isfinite(data[i]));
}

// ─── decay 後にエンジンが非アクティブになる ────────────────────

TEST_CASE("ClickEngine: deactivates after decay", "[click_engine]") {
  auto engine = makeEngine();
  engine->setDecayMs(5.0f); // 5ms → ~220 samples

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  engine->triggerNote(0);

  // 複数ブロック回して decay を超過させる
  for (int block = 0; block < 10; ++block) {
    buf.clear();
    engine->render(buf, kBlockSize, true, kSampleRate);
  }

  // 最後のブロックの scratchBuffer はゼロのはず
  REQUIRE(energy(*engine, kBlockSize) == 0.0f);
}

// ─── triggerNote でフィルター状態がリセットされる ───────────────

TEST_CASE("ClickEngine: triggerNote resets state (deterministic output)",
          "[click_engine]") {
  auto engine = makeEngine();
  juce::AudioBuffer<float> buf1(2, kBlockSize);
  juce::AudioBuffer<float> buf2(2, kBlockSize);

  // 1回目のトリガー → render
  engine->triggerNote(0);
  buf1.clear();
  engine->render(buf1, kBlockSize, true, kSampleRate);

  // 2回目のトリガー → render（RNG + フィルターがリセットされているはず）
  engine->triggerNote(0);
  buf2.clear();
  engine->render(buf2, kBlockSize, true, kSampleRate);

  // 決定論的: 同じ seed + リセット済みフィルター → 同じ出力
  const float *d1 = buf1.getReadPointer(0);
  const float *d2 = buf2.getReadPointer(0);
  for (int i = 0; i < kBlockSize; ++i)
    REQUIRE(d1[i] == d2[i]);
}

// ─── clickPass=false だと buffer に加算されない ────────────────

TEST_CASE("ClickEngine: clickPass=false does not add to buffer",
          "[click_engine]") {
  auto engine = makeEngine();
  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  engine->triggerNote(0);
  engine->render(buf, kBlockSize, false, kSampleRate);

  // scratchBuffer にはデータがあるが、buffer はゼロのまま
  REQUIRE(energy(*engine, kBlockSize) > 0.0f);
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < kBlockSize; ++i)
      REQUIRE(buf.getSample(ch, i) == 0.0f);
}

// ─── HPF/LPF スロープ設定がクラッシュしない ────────────────────

TEST_CASE("ClickEngine: all filter slopes render without crash",
          "[click_engine]") {
  for (int slope : {12, 24, 48}) {
    auto engine = makeEngine();
    engine->setHpfFreq(200.0f);
    engine->setLpfFreq(8000.0f);
    engine->setHpfSlope(slope);
    engine->setLpfSlope(slope);
    engine->setBpf1Slope(slope);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();

    engine->triggerNote(0);
    engine->render(buf, kBlockSize, true, kSampleRate);

    const float *data = engine->scratchData();
    for (int i = 0; i < kBlockSize; ++i)
      REQUIRE(std::isfinite(data[i]));
  }
}

// ─── 全 ClipType でクラッシュしない ────────────────────────────

TEST_CASE("ClickEngine: all ClipTypes produce finite output",
          "[click_engine]") {
  for (int clipType : {0, 1, 2}) {
    auto engine = makeEngine();
    engine->setDriveDb(12.0f);
    engine->setClipType(clipType);

    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();

    engine->triggerNote(0);
    engine->render(buf, kBlockSize, true, kSampleRate);

    const float *data = engine->scratchData();
    for (int i = 0; i < kBlockSize; ++i)
      REQUIRE(std::isfinite(data[i]));
  }
}
