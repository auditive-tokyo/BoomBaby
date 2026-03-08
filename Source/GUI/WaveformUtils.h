#pragma once

#include <cmath>
#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

// ────────────────────────────────────────────────────────────────
// WaveformUtils
//   サムネイル波形プレビュー描画用ヘルパー。
//   ClickParams.cpp / DirectParams.cpp 共通で使用。
// ────────────────────────────────────────────────────────────────
namespace WaveformUtils {

/// 時刻 timeSec における波形サムネイルの振幅 (min, max) を
/// A → Hold → R エンベロープでスケールして返す。
/// 範囲外または durSec <= 0 の場合は {0, 0} を返す。
inline std::pair<float, float>
computePreview(const std::vector<float> &thumbMin,
               const std::vector<float> &thumbMax, double durSec,
               float attackSec, float holdSec, float releaseSec,
               float timeSec) {

  // A → Hold → R エンベロープ
  float env = 0.0f;
  if (timeSec < attackSec) {
    env = (attackSec > 0.0f) ? timeSec / attackSec : 1.0f;
  } else if (timeSec < attackSec + holdSec) {
    env = 1.0f;
  } else {
    const float rel = timeSec - attackSec - holdSec;
    env = (releaseSec > 0.0f)
              ? juce::jlimit(0.0f, 1.0f, 1.0f - rel / releaseSec)
              : 0.0f;
  }

  // サムネイル参照
  if (durSec <= 0.0 || timeSec < 0.0f || timeSec >= static_cast<float>(durSec))
    return {0.0f, 0.0f};

  const float t = timeSec / static_cast<float>(durSec);
  const auto n = static_cast<int>(thumbMin.size());
  const int idx =
      juce::jlimit(0, n - 1, static_cast<int>(t * static_cast<float>(n)));

  return {thumbMin[static_cast<std::size_t>(idx)] * env,
          thumbMax[static_cast<std::size_t>(idx)] * env};
}

/// LUT ベースのエンベロープ・プレビュー（Direct / Click Sample 共通）。
/// ampDurMs がエンベロープ再生期間、ampScale が振幅スケール。
/// 再生時間は min(サンプル長, エンベロープ期間) でクリップする。
inline std::pair<float, float>
computeLutPreview(const std::vector<float> &thumbMin,
                  const std::vector<float> &thumbMax, double durSec,
                  float ampDurMs, float ampScale, float timeSec) {
  const float ampDurSec = ampDurMs / 1000.0f;

  if (const float maxDur = std::min(static_cast<float>(durSec), ampDurSec);
      maxDur <= 0.0f || timeSec < 0.0f || timeSec >= maxDur)
    return {0.0f, 0.0f};

  // 末尾 5ms half-cosine フェードアウト（DSP の computeSampleAmp と同一）
  constexpr float fadeOutMs = 5.0f;
  float fade = 1.0f;
  if (const float fadeStartMs = std::max(0.0f, ampDurMs - fadeOutMs);
      timeSec * 1000.0f > fadeStartMs) {
    const float fadeT = (timeSec * 1000.0f - fadeStartMs) / fadeOutMs;
    fade = 0.5f * (1.0f + std::cos(fadeT * juce::MathConstants<float>::pi));
  }
  const float scale = ampScale * fade;

  const float thumbT = timeSec / static_cast<float>(durSec);
  const auto n = static_cast<int>(thumbMin.size());
  const int idx =
      juce::jlimit(0, n - 1, static_cast<int>(thumbT * static_cast<float>(n)));

  return {thumbMin[static_cast<std::size_t>(idx)] * scale,
          thumbMax[static_cast<std::size_t>(idx)] * scale};
}

} // namespace WaveformUtils
