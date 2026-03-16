#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/LevelDetector.h"

#include <vector>

using namespace Catch::Matchers;

// ── ピーク検出 ──────────────────────────────────────

// 正のピーク値 (0.5f → -6.02 dB) が正しく検出されることを確認する
TEST_CASE("LevelDetector: detects positive peak", "[level_detector]") {
  LevelDetector det;
  std::vector<float> buf = {0.0f, 0.3f, -0.1f, 0.5f, -0.2f};
  det.process(buf.data(), static_cast<int>(buf.size()));

  // 0.5 → ≈ -6.02 dB
  CHECK_THAT(det.getPeakDb(), WithinAbs(-6.0206, 0.01));
}

// 負のピーク値 (-0.8f) が絶対値として正しく検出されることを確認する (-1.94 dB)
TEST_CASE("LevelDetector: detects negative peak (abs)", "[level_detector]") {
  LevelDetector det;
  std::vector<float> buf = {0.0f, -0.8f, 0.1f};
  det.process(buf.data(), static_cast<int>(buf.size()));

  // 0.8 → ≈ -1.94 dB
  CHECK_THAT(det.getPeakDb(), WithinAbs(-1.9382, 0.01));
}

// 入力値 1.0f (Unity Gain) が 0 dB として読み取れることを確認する
TEST_CASE("LevelDetector: unity gain reads 0 dB", "[level_detector]") {
  LevelDetector det;
  std::vector<float> buf = {1.0f};
  det.process(buf.data(), static_cast<int>(buf.size()));

  CHECK_THAT(det.getPeakDb(), WithinAbs(0.0, 0.01));
}

// 初期状態（入力なし）でピークがフロア値 -100 dB であることを確認する
TEST_CASE("LevelDetector: silence reads -100 dB", "[level_detector]") {
  LevelDetector det;
  CHECK_THAT(det.getPeakDb(), WithinAbs(-100.0, 0.01));
}

// ── nullptr 処理 ────────────────────────────────────

// nullptr を渡したときにクラッシュせず、ピーク値が減衰することを確認する
TEST_CASE("LevelDetector: nullptr data decays without crash",
          "[level_detector]") {
  LevelDetector det;
  // まずピークを入力
  std::vector<float> buf = {0.5f};
  det.process(buf.data(), static_cast<int>(buf.size()));
  const float before = det.getPeakDb();

  // nullptr を渡すと減衰のみ
  det.process(nullptr, 256);
  CHECK(det.getPeakDb() < before);
}

// ── リリース（減衰）特性 ────────────────────────────

// 無音ブロックを繰り返すたびにピーク値が単調減少することを確認する
TEST_CASE("LevelDetector: peak decays over successive silent blocks",
          "[level_detector]") {
  LevelDetector det;
  std::vector<float> pulse = {1.0f};
  det.process(pulse.data(), 1);
  CHECK_THAT(det.getPeakDb(), WithinAbs(0.0, 0.01));

  // 無音ブロックを繰り返すと減衰する
  std::vector<float> silence(512, 0.0f);
  float prev = det.getPeakDb();
  for (int i = 0; i < 10; ++i) {
    det.process(silence.data(), static_cast<int>(silence.size()));
    const float cur = det.getPeakDb();
    CHECK(cur < prev);
    prev = cur;
  }
}

// setDecayPerBlock(0.5f) で 1 ブロックごとに半減することを数値で確認する
TEST_CASE("LevelDetector: custom decay rate", "[level_detector]") {
  LevelDetector det;
  det.setDecayPerBlock(0.5f); // 非常に速い減衰

  std::vector<float> pulse = {1.0f};
  det.process(pulse.data(), 1);

  // 1ブロック無音 → peak = 1.0 * 0.5 = 0.5
  std::vector<float> silence(1, 0.0f);
  det.process(silence.data(), 1);
  CHECK_THAT(det.getPeakDb(),
             WithinAbs(juce::Decibels::gainToDecibels(0.5f, -100.0f), 0.01));

  // もう1ブロック → peak = 0.5 * 0.5 = 0.25
  det.process(silence.data(), 1);
  CHECK_THAT(det.getPeakDb(),
             WithinAbs(juce::Decibels::gainToDecibels(0.25f, -100.0f), 0.01));
}

// ── ピーク保持（大信号後の小信号で即座に落ちない）──

// 大きい入力の直後に小さい入力が来ても、減衰後の値が対小入力より大きければそちらが保持されることを確認する
TEST_CASE("LevelDetector: holds peak when new block is smaller",
          "[level_detector]") {
  LevelDetector det;
  det.setDecayPerBlock(0.99f); // ゆっくり減衰

  std::vector<float> loud = {1.0f};
  det.process(loud.data(), 1);

  // 小さい信号 → ピークは直前の reduced peak と比較して大きい方を採用
  std::vector<float> quiet = {0.01f};
  det.process(quiet.data(), 1);

  // 1.0 * 0.99 = 0.99 > 0.01 → peak ≈ 0.99
  CHECK_THAT(det.getPeakDb(),
             WithinAbs(juce::Decibels::gainToDecibels(0.99f, -100.0f), 0.01));
}

// ── リセット ────────────────────────────────────────

// reset() 後にピーク値がフロア値 -100 dB に戻ることを確認する
TEST_CASE("LevelDetector: reset clears peak", "[level_detector]") {
  LevelDetector det;
  std::vector<float> buf = {1.0f};
  det.process(buf.data(), 1);
  CHECK_THAT(det.getPeakDb(), WithinAbs(0.0, 0.01));

  det.reset();
  CHECK_THAT(det.getPeakDb(), WithinAbs(-100.0, 0.01));
}
