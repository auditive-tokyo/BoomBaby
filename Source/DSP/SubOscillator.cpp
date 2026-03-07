#include "SubOscillator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

// ── 帯域境界（Hz）: 20, 40, 80, … , 10240, 20480 ──
static constexpr std::array<float, SubOscillator::numBands + 1> bandEdges = {
    20.0f,   40.0f,   80.0f,   160.0f,   320.0f,  640.0f,
    1280.0f, 2560.0f, 5120.0f, 10240.0f, 20480.0f};

// ────────────────────────────────────────────────────
// prepareToPlay
// ────────────────────────────────────────────────────
void SubOscillator::prepareToPlay(double newSampleRate) {
  sampleRate = newSampleRate;
  currentIndex = 0.0f;
  buildAllTables();
  // デフォルトポインタを Sine band-0 に設定
  activeSineTable = tables[0][0].data();
  activeShapeTable = activeSineTable;
}

// ── 各波形の1サンプル計算ヘルパー（file-scope static） ──

static double computeTriSample(double phase, int maxHarmonic) {
  double sum = 0.0;
  for (int n = 1; n <= maxHarmonic; n += 2) {
    const double sign = ((n / 2) % 2 == 0) ? 1.0 : -1.0;
    sum += sign * std::sin(static_cast<double>(n) * phase) /
           static_cast<double>(n * n);
  }
  return sum * (8.0 / (juce::MathConstants<double>::pi *
                       juce::MathConstants<double>::pi));
}

static double computeSquareSample(double phase, int maxHarmonic) {
  double sum = 0.0;
  for (int n = 1; n <= maxHarmonic; n += 2)
    sum += std::sin(static_cast<double>(n) * phase) / static_cast<double>(n);
  return sum * (4.0 / juce::MathConstants<double>::pi);
}

static double computeSawSample(double phase, int maxHarmonic) {
  double sum = 0.0;
  for (int n = 1; n <= maxHarmonic; ++n) {
    const double sign = (n % 2 == 1) ? 1.0 : -1.0;
    sum += sign * std::sin(static_cast<double>(n) * phase) /
           static_cast<double>(n);
  }
  return sum * (2.0 / juce::MathConstants<double>::pi);
}

/// 1波形 × 1帯域のテーブルを埋める
static void buildShapeBandTable(std::vector<float> &tbl, WaveShape ws,
                                int maxHarmonic) {
  constexpr auto twoPi = juce::MathConstants<double>::twoPi;
  for (int i = 0; i < SubOscillator::tableSize; ++i) {
    const double phase = twoPi * i / SubOscillator::tableSize;
    double sample = 0.0;
    switch (ws) {
    using enum WaveShape;
    case Sine:   sample = std::sin(phase); break;
    case Tri:    sample = computeTriSample(phase, maxHarmonic); break;
    case Square: sample = computeSquareSample(phase, maxHarmonic); break;
    case Saw:    sample = computeSawSample(phase, maxHarmonic); break;
    }
    tbl[static_cast<size_t>(i)] = static_cast<float>(sample);
  }
  tbl[static_cast<size_t>(SubOscillator::tableSize)] = tbl[0]; // wrap用
}

// ────────────────────────────────────────────────────
// buildAllTables — 4波形 × 10帯域を事前計算
// ────────────────────────────────────────────────────
void SubOscillator::buildAllTables() {
  const auto nyquist = static_cast<float>(sampleRate * 0.5);

  for (int shape = 0; shape < numShapes; ++shape) {
    const auto ws = static_cast<WaveShape>(shape);
    for (int band = 0; band < numBands; ++band) {
      auto &tbl = tables[static_cast<size_t>(shape)][static_cast<size_t>(band)];
      tbl.assign(static_cast<size_t>(tableSize + 1), 0.0f);

      const float bandTop = bandEdges[static_cast<size_t>(band + 1)];
      const int maxHarmonic =
          std::max(1, static_cast<int>(std::floor(nyquist / bandTop)));

      buildShapeBandTable(tbl, ws, maxHarmonic);
    }
  }
}

// ────────────────────────────────────────────────────
// bandIndexForFreq — 周波数 → 帯域インデックス (0〜9)
// ────────────────────────────────────────────────────
int SubOscillator::bandIndexForFreq(float hz) {
  // 帯域は対数スケール（倍々）: 20,40,80,...,10240,20480
  // log2(hz/20) でオクターブ番号を得て clamp
  if (hz <= bandEdges[0])
    return 0;
  const auto band = static_cast<int>(std::floor(std::log2(hz / bandEdges[0])));
  return std::clamp(band, 0, numBands - 1);
}

// ────────────────────────────────────────────────────
// triggerNote / stopNote
// ────────────────────────────────────────────────────
void SubOscillator::triggerNote() {
  active = true;
  currentIndex = 0.0f;
  // Tone1〜Tone4 位相リセット
  for (auto& h : harmonics)
    h.phase = 0.0f;
}

void SubOscillator::stopNote() {
  active = false;
  tableDelta = 0.0f;
  currentIndex = 0.0f;
}

// ────────────────────────────────────────────────────
// setFrequencyHz — tableDelta 更新 + 帯域選択
// ────────────────────────────────────────────────────
void SubOscillator::setFrequencyHz(float hz) {
  tableDelta = static_cast<float>(hz * tableSize / sampleRate);

  const int band = bandIndexForFreq(hz);
  activeBand = band;

  const auto bandIdx = static_cast<size_t>(band);
  // Sine テーブルは常に更新（Mix で参照するため）
  activeSineTable = tables[0][bandIdx].data();
  // 選択波形テーブルも更新
  const auto shapeIdx = static_cast<size_t>(currentShape.load());
  activeShapeTable = tables[shapeIdx][bandIdx].data();
}

// ────────────────────────────────────────────────────
// setWaveShape / getWaveShape
// ────────────────────────────────────────────────────
void SubOscillator::setWaveShape(WaveShape shape) {
  currentShape.store(static_cast<int>(shape));
}

WaveShape SubOscillator::getWaveShape() const {
  return static_cast<WaveShape>(currentShape.load());
}

// ────────────────────────────────────────────────────
// setBlend — Mix 値設定（-1.0〜+1.0）
// ────────────────────────────────────────────────────
void SubOscillator::setBlend(float blend) {
  blend_.store(std::clamp(blend, -1.0f, 1.0f));
}

// ────────────────────────────────────────────────────
// setDist — Saturate drive 量設定（0.0〜1.0）
// ────────────────────────────────────────────────────
void SubOscillator::setDist(float drive01) {
  dist_.store(std::clamp(drive01, 0.0f, 1.0f));
}

// ────────────────────────────────────────────────────
// setHarmonicGain — Tone1〜Tone4 倍音ゲイン設定
// ────────────────────────────────────────────────────
void SubOscillator::setHarmonicGain(int n, float gain) {
  if (n >= 1 && n <= numHarmonics)
    harmonicGains[static_cast<size_t>(n - 1)].store(gain);
}

// ────────────────────────────────────────────────────
// readTable — テーブルから線形補間で1サンプル
// ────────────────────────────────────────────────────
float SubOscillator::readTable(const float *table) const {
  const auto index0 = static_cast<size_t>(currentIndex);
  const auto index1 = index0 + 1;
  const float frac = currentIndex - static_cast<float>(index0);
  return table[index0] + frac * (table[index1] - table[index0]);
}

// ────────────────────────────────────────────────────
// getNextSample — Mix クロスフェード + Tone1〜Tone4 加算合成
//   b ≤ 0: lerp(sine, wavetable, -b)
//   b > 0: lerp(sine, additive,   b)
// ────────────────────────────────────────────────────
float SubOscillator::getNextSample() {
  if (!active)
    return 0.0f;

  // Sine テーブル読み出し
  const float sineSample = readTable(activeSineTable);

  // 選択波形（Tri/Square/Saw）テーブル読み出し
  const float shapeSample = readTable(activeShapeTable);

  // Tone1〜Tone4 加算合成（gain=0 のハーモニクスはスキップ）
  float additiveSample = 0.0f;
  const float phaseDelta =
      tableDelta * (juce::MathConstants<float>::twoPi /
                    static_cast<float>(tableSize));
  for (int n = 0; n < numHarmonics; ++n) {
    const float g = harmonicGains[static_cast<size_t>(n)].load();
    if (g > 0.0f) {
      additiveSample += g * std::sin(harmonics[static_cast<size_t>(n)].phase);
      harmonics[static_cast<size_t>(n)].phase +=
          phaseDelta * static_cast<float>(n + 1);
      if (harmonics[static_cast<size_t>(n)].phase >=
          juce::MathConstants<float>::twoPi)
        harmonics[static_cast<size_t>(n)].phase -=
            juce::MathConstants<float>::twoPi;
    }
  }

  // Mix クロスフェード
  const float b = blend_.load();
  float sample;
  if (b <= 0.0f) {
    // 左側: Sine ← → Wavetable（Tri/Sqr/Saw）
    const float t = -b; // 0.0〜1.0
    sample = std::lerp(sineSample, shapeSample, t);
  } else {
    // 右側: Sine ← → Additive（Tone1〜Tone4）
    sample = std::lerp(sineSample, additiveSample, b);
  }

  // Distortion（tanh soft-clip — 常時適用、Saturate=0 でも軽いサチュレーション）
  const float d = dist_.load();
  const float drive = std::pow(10.0f, d * 24.0f / 20.0f); // 0〜24 dB → 1.0〜15.85
  sample = std::tanh(drive * sample) / std::tanh(drive);

  currentIndex += tableDelta;
  if (currentIndex >= static_cast<float>(tableSize))
    currentIndex -= static_cast<float>(tableSize);

  return sample;
}
