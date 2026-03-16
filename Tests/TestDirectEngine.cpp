#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/DirectEngine.h"
#include <cmath>
#include <memory>
#include <vector>

using namespace Catch::Matchers;

namespace {
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;

/// prepare 済みの DirectEngine をパススルーモードで返すヘルパー
std::unique_ptr<DirectEngine> makePassthroughEngine() {
  auto e = std::make_unique<DirectEngine>();
  e->prepareToPlay(kSampleRate, kBlockSize);
  e->setPassthroughMode(true);
  e->setGainDb(0.0f);
  e->setDriveDb(0.0f);
  e->setClipType(0);       // Soft
  e->setHpfFreq(20.0f);    // bypass
  e->setLpfFreq(20000.0f); // bypass
  e->setMaxDurationMs(300.0f);
  return e;
}

/// scratchBuffer の先頭 numSamples のエネルギー（二乗和）
float energy(const DirectEngine &e, int numSamples) {
  const float *data = e.scratchData();
  float sum = 0.0f;
  for (int i = 0; i < numSamples; ++i)
    sum += data[i] * data[i];
  return sum;
}

/// 一定振幅のモノラル入力を生成
std::vector<float> makeConstInput(int length, float value) {
  return std::vector<float>(static_cast<std::size_t>(length), value);
}

/// DC バイアスの最大絶対値を取得
float maxAbsScratch(const DirectEngine &e, int numSamples) {
  const float *data = e.scratchData();
  float mx = 0.0f;
  for (int i = 0; i < numSamples; ++i)
    mx = std::max(mx, std::abs(data[i]));
  return mx;
}
} // namespace

// ─── 非アクティブ時は render 出力ゼロ ──────────────────────────

// triggerNote() を呼ばずに render しても scratchBuffer・出力バッファ共にゼロであることを確認する
TEST_CASE("DirectEngine: inactive render produces silence", "[direct_engine]") {
  auto engine = std::make_unique<DirectEngine>();
  engine->prepareToPlay(kSampleRate, kBlockSize);
  engine->setPassthroughMode(false);
  engine->setGainDb(0.0f);

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();

  // triggerNote を呼ばずに render
  engine->render(buf, kBlockSize, true, kSampleRate);

  REQUIRE(energy(*engine, kBlockSize) == 0.0f);
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < kBlockSize; ++i)
      REQUIRE(buf.getSample(ch, i) == 0.0f);
}

// ─── 非アクティブ時は renderPassthrough 出力ゼロ ────────────────

// triggerNote() なしで renderPassthrough しても amp=0 のため全出力がゼロであることを確認する
TEST_CASE("DirectEngine: inactive passthrough produces silence",
          "[direct_engine]") {
  auto engine = makePassthroughEngine();

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  auto input = makeConstInput(kBlockSize, 0.5f);

  // triggerNote を呼ばずに renderPassthrough
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

  // active でないので amp=0 → 全出力ゼロ
  REQUIRE(energy(*engine, kBlockSize) == 0.0f);
}

// ─── Tube バイアスリーク回帰テスト（amp==0） ────────────────────

// 非アクティブ（amp==0）のとき Tube ClipType の DC バイアスが出力に漏れないことを確認する
TEST_CASE("DirectEngine: Tube clipType does not leak DC when inactive (amp==0)",
          "[direct_engine]") {
  auto engine = makePassthroughEngine();
  engine->setClipType(2); // Tube
  engine->setDriveDb(12.0f);

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  auto input = makeConstInput(kBlockSize, 0.8f);

  // active でない → amp==0 → Tube パスを通らないはず
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

  REQUIRE(maxAbsScratch(*engine, kBlockSize) == 0.0f);
}

// ─── Tube バイアスリーク回帰テスト（input==0） ──────────────────

// 入力がゼロのとき Tube ClipType の DC バイアスが出力に漏れないことを確認する
TEST_CASE("DirectEngine: Tube clipType does not leak DC when input is zero",
          "[direct_engine]") {
  auto engine = makePassthroughEngine();
  engine->setClipType(2); // Tube
  engine->setDriveDb(12.0f);

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  auto input = makeConstInput(kBlockSize, 0.0f);

  engine->triggerNote(0);
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

  // 入力ゼロなら Tube bias が出力に漏れないこと
  REQUIRE(maxAbsScratch(*engine, kBlockSize) == 0.0f);
}

// ─── パススルーモード: triggerNote 後に入力信号が出力される ──────

// triggerNote() 後に renderPassthrough すると入力信号がそのまま出力されることを確認する
TEST_CASE("DirectEngine: passthrough outputs signal after trigger",
          "[direct_engine]") {
  auto engine = makePassthroughEngine();

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  auto input = makeConstInput(kBlockSize, 0.5f);

  engine->triggerNote(0);
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

  // エネルギーが非ゼロ＝信号が出力されている
  REQUIRE(energy(*engine, kBlockSize) > 0.0f);
}

// ─── パススルー: ゼロ入力は全ClipTypeで出力ゼロ ─────────────────

// Soft/Hard/Tube の全 ClipType で入力ゼロなら出力もゼロであることを確認する
TEST_CASE("DirectEngine: zero input passthrough is silent for all clip types",
          "[direct_engine]") {
  for (int ct = 0; ct <= 2; ++ct) {
    SECTION("ClipType " + std::to_string(ct)) {
      auto engine = makePassthroughEngine();
      engine->setClipType(ct);
      engine->setDriveDb(6.0f);

      juce::AudioBuffer<float> buf(2, kBlockSize);
      buf.clear();
      auto input = makeConstInput(kBlockSize, 0.0f);

      engine->triggerNote(0);
      engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

      REQUIRE(maxAbsScratch(*engine, kBlockSize) == 0.0f);
    }
  }
}

// ─── triggerNote のフィルターリセット ────────────────────────────

// triggerNote() でフィルター状態がリセットされ、同じ入力に対して 2 回目も同一出力になることを確認する
TEST_CASE("DirectEngine: triggerNote resets filters", "[direct_engine]") {
  auto engine = makePassthroughEngine();
  engine->setHpfFreq(1000.0f);
  engine->setHpfSlope(12);

  juce::AudioBuffer<float> buf(2, kBlockSize);
  auto input = makeConstInput(kBlockSize, 0.5f);

  // 1st trigger + render → フィルター状態が蓄積
  engine->triggerNote(0);
  buf.clear();
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);
  float e1 = energy(*engine, kBlockSize);

  // 2nd trigger → フィルターリセットされるので同一入力で同一出力
  engine->triggerNote(0);
  buf.clear();
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);
  float e2 = energy(*engine, kBlockSize);

  REQUIRE_THAT(static_cast<double>(e2),
               WithinRel(static_cast<double>(e1), 1e-4));
}

// ─── Drive がゲインを増加させる ─────────────────────────────────

// Drive 18dB の出力エネルギーが Drive 0dB より大きくなることを確認する
TEST_CASE("DirectEngine: drive increases output level", "[direct_engine]") {
  // Drive = 0 dB
  auto engineA = makePassthroughEngine();
  engineA->setDriveDb(0.0f);
  juce::AudioBuffer<float> bufA(2, kBlockSize);
  bufA.clear();
  auto input = makeConstInput(kBlockSize, 0.3f);
  engineA->triggerNote(0);
  engineA->renderPassthrough(bufA, input, kBlockSize, kSampleRate);
  float eA = energy(*engineA, kBlockSize);

  // Drive = 18 dB
  auto engineB = makePassthroughEngine();
  engineB->setDriveDb(18.0f);
  juce::AudioBuffer<float> bufB(2, kBlockSize);
  bufB.clear();
  engineB->triggerNote(0);
  engineB->renderPassthrough(bufB, input, kBlockSize, kSampleRate);
  float eB = energy(*engineB, kBlockSize);

  REQUIRE(eB > eA);
}

// ─── maxDuration 超過で出力停止 ─────────────────────────────────

// setMaxDurationMs(5ms) を超えたブロック後半で scratchBuffer がゼロに戻ることを確認する
TEST_CASE("DirectEngine: passthrough stops after maxDuration",
          "[direct_engine]") {
  auto engine = makePassthroughEngine();
  // 5ms = ~220 samples @ 44100
  engine->setMaxDurationMs(5.0f);

  juce::AudioBuffer<float> buf(2, kBlockSize);
  buf.clear();
  auto input = makeConstInput(kBlockSize, 0.5f);

  engine->triggerNote(0);
  engine->renderPassthrough(buf, input, kBlockSize, kSampleRate);

  // ブロック後半（5ms 以降）は出力ゼロのはず
  const float *scratch = engine->scratchData();
  int silentCount = 0;
  for (int i = kBlockSize / 2; i < kBlockSize; ++i) {
    if (scratch[i] == 0.0f)
      ++silentCount;
  }
  // 後半 256 サンプルのうち大半がゼロ（5ms ≈ 220 samples なのでほぼ全部ゼロ）
  REQUIRE(silentCount > (kBlockSize / 2) - 10);
}
