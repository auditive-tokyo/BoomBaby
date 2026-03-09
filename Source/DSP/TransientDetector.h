#pragma once

#include <atomic>
#include <cmath>
#include <span>
#include <juce_audio_basics/juce_audio_basics.h>

/// 入力信号のトランジェント（過渡成分）を検出し、Sub/Click の
/// 自動トリガーに使用する軽量デテクター（ヘッダオンリー）。
///
/// アルゴリズム: 二重エンベロープフォロワー差分方式
///   1. Fast envelope（attack 0.2ms / release 10ms）→ ピーク即追従
///   2. Slow envelope（attack 10ms / release 200ms）→ 背景レベル追従
///   3. onset = envFast − envSlow  が threshold を超えたら TRIGGER
///   4. ヒステリシス: onset が threshold×30% を下回ったら再アーム
///   5. Hold time（デフォルト 50ms）で多重発火防止
class TransientDetector {
public:
  /// オーディオスレッド開始前に呼ぶ
  void prepare(double sampleRate) noexcept {
    sr_ = sampleRate;
    updateCoeffs();
    reset();
  }

  void reset() noexcept {
    envFast_ = 0.0f;
    envSlow_ = 0.0f;
    holdCounter_ = 0;
    armed_ = true;
  }

  /// 検出閾値を dBFS で設定（例: -24.0f）
  void setThresholdDb(float db) noexcept {
    threshLin_ = juce::Decibels::decibelsToGain(db);
  }

  /// トリガー後の最小間隔（ms）
  void setHoldMs(float ms) noexcept {
    holdSamples_ = static_cast<int>(ms * 0.001f * static_cast<float>(sr_));
  }

  /// 有効／無効
  void setEnabled(bool on) noexcept { enabled_.store(on); }
  bool isEnabled() const noexcept { return enabled_.load(); }

  /// 1 ブロック分を解析し、トランジェントが見つかったら true を返す。
  /// @param input  モノ入力（呼び出し側でステレオから合成しておく）
  bool process(std::span<const float> input) noexcept {
    if (!enabled_.load())
      return false;

    bool triggered = false;

    for (const float x_raw : input) {
      const float x = std::abs(x_raw);

      // ── Fast envelope（attack 0.2ms / release 10ms） ──
      const float cfA = (x > envFast_) ? attackFast_ : releaseFast_;
      envFast_ += cfA * (x - envFast_);

      // ── Slow envelope（attack 10ms / release 200ms） ──
      const float csA = (x > envSlow_) ? attackSlow_ : releaseSlow_;
      envSlow_ += csA * (x - envSlow_);

      // ── onset = 差分 ──
      const float onset = envFast_ - envSlow_;

      if (onset > threshLin_ && armed_ && holdCounter_ <= 0) {
        triggered = true;
        armed_ = false;
        holdCounter_ = holdSamples_;
      }

      // ヒステリシス: onset が閾値の 30% 以下まで落ちたら再アーム
      if (onset < threshLin_ * kHysteresisRatio)
        armed_ = true;

      if (holdCounter_ > 0)
        --holdCounter_;
    }

    return triggered;
  }

private:
  void updateCoeffs() noexcept {
    const auto fs = static_cast<float>(sr_);
    // alpha = 1 − exp(−1 / (tau × fs))
    attackFast_ = 1.0f - std::exp(-1.0f / (0.0002f * fs)); // 0.2ms
    releaseFast_ = 1.0f - std::exp(-1.0f / (0.010f * fs)); // 10ms
    attackSlow_ = 1.0f - std::exp(-1.0f / (0.010f * fs));  // 10ms
    releaseSlow_ = 1.0f - std::exp(-1.0f / (0.200f * fs)); // 200ms
  }

  static constexpr float kHysteresisRatio = 0.3f;

  double sr_ = 44100.0;
  float envFast_ = 0.0f;
  float envSlow_ = 0.0f;
  float attackFast_ = 0.0f;
  float releaseFast_ = 0.0f;
  float attackSlow_ = 0.0f;
  float releaseSlow_ = 0.0f;
  float threshLin_ = 0.063f; // ≈ −24 dBFS
  int holdSamples_ = 2205;   // ≈ 50ms @ 44.1kHz
  int holdCounter_ = 0;
  bool armed_ = true;
  std::atomic<bool> enabled_{false};
};
