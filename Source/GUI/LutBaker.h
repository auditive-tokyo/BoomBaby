#pragma once

#include "../DSP/EnvelopeData.h"
#include "../DSP/EnvelopeLutManager.h"

#include <array>

/// エンベロープの実効区間（最終ポイントの timeMs）を LUT 期間として返す。
/// 1 点以下（＝ノブ制御 or 未設定）の場合は fallbackMs を使う。
/// これにより LUT の 512 点をエンベロープ区間に集中させ、
/// Length 変更時の解像度劣化を防ぐ。
inline float effectiveLutDuration(const EnvelopeData &envData,
                                  float fallbackMs) {
  if (const auto &pts = envData.getPoints(); pts.size() > 1)
    if (const float lastT = pts.back().timeMs; lastT > 0.0f)
      return lastT;
  return fallbackMs;
}

/// EnvelopeData を評価して EnvelopeLutManager に焼き込むユーティリティ。
/// BoomBabyAudioProcessorEditor のメンバー関数を分割した複数の翻訳単位から
/// 呼び出せるよう inline free function として提供する。
inline void bakeLut(const EnvelopeData &envData, EnvelopeLutManager &lut,
                    float durationMs) {
  constexpr int lutSize = EnvelopeLutManager::lutSize;
  std::array<float, lutSize> buf{};
  for (int i = 0; i < lutSize; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lutSize - 1) * durationMs;
    buf[static_cast<size_t>(i)] = envData.evaluate(t);
  }
  lut.setDurationMs(durationMs);
  lut.bake(buf.data(), lutSize);
}
