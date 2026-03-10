#pragma once

#include "../DSP/EnvelopeData.h"
#include "../DSP/EnvelopeLutManager.h"

#include <array>

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
