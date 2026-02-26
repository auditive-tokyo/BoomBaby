#pragma once

#include <array>
#include <atomic>
#include <vector>

/// 波形選択 enum（Sine / Tri / Square / Saw）
enum class WaveShape { Sine = 0, Tri = 1, Square = 2, Saw = 3 };

/// Sub用 Band-limited Wavetable オシレーター（オーディオスレッドで使用）
/// 4波形 × 10帯域のテーブルを prepareToPlay で事前生成し、
/// 再生周波数に応じてバンドを自動選択、線形補間で読み出す。
/// triggerNote() で発音開始、setFrequencyHz() で毎サンプル周波数更新
class SubOscillator {
public:
  SubOscillator() = default;
  ~SubOscillator() = default;

  /// processBlock 前に呼び出し（サンプルレート設定 + 全テーブル構築）
  void prepareToPlay(double sampleRate);

  /// 発音開始（トリガーのみ、ピッチは setFrequencyHz で制御）
  void triggerNote();

  /// 発音停止
  void stopNote();

  /// 周波数を設定（Hz、毎サンプル呼び出し可）
  void setFrequencyHz(float hz);

  /// 次のサンプルを生成（オーディオスレッドから呼び出し）
  float getNextSample();

  /// 発音中かどうか
  bool isActive() const { return active; }

  /// 波形選択（UIスレッドから呼び出し可）
  void setWaveShape(WaveShape shape);

  /// 現在の波形を取得
  WaveShape getWaveShape() const;

  /// BLEND 値を設定（UIスレッドから呼び出し可）
  /// @param blend -1.0〜+1.0（0=Sine, -1=Wavetable, +1=Additive）
  void setBlend(float blend);

  /// H1〜H4 倍音ゲイン設定（UIスレッドから呼び出し可）
  /// @param n 倍音番号 1〜4（H1=基底音×1, H2=×2, H3=×3, H4=×4）
  /// @param gain 0.0〜1.0
  void setHarmonicGain(int n, float gain);

  /// Distortion drive 量を設定（UIスレッドから呼び出し可）
  /// @param drive01 0.0〜1.0（0=クリーン、1=フルドライブ）
  void setDist(float drive01);

  // ── 定数（外部から参照可能にするため public）──
  static constexpr int tableSize  = 2048;
  static constexpr int numBands   = 10;   // 20Hz〜20480Hz を 10 オクターブ分割
  static constexpr int numShapes  = 4;    // Sine / Tri / Square / Saw

private:

  /// [shape][band] → tableSize+1 要素（wrap用に +1）
  std::array<std::array<std::vector<float>, numBands>, numShapes> tables;

  // ── 再生状態 ──
  bool active = false;
  double sampleRate = 44100.0;
  float currentIndex = 0.0f;
  float tableDelta   = 0.0f;  // phaseIncrement に相当

  /// setFrequencyHz() が算出した帯域テーブルへのポインタ（Sine 用）
  const float* activeSineTable = nullptr;
  /// setFrequencyHz() が算出した帯域テーブルへのポインタ（選択波形用）
  const float* activeShapeTable = nullptr;

  std::atomic<int> currentShape{0};  // WaveShape の int 値
  int activeBand = 0;

  // ── BLEND ──
  std::atomic<float> blend_{0.0f};  // -1.0〜+1.0

  // ── DIST ──
  std::atomic<float> dist_{0.0f};  // 0.0〜1.0（UI値そのまま）

  // ── H1〜H4 加算合成 ──
  static constexpr int numHarmonics = 4;
  struct HarmonicOsc {
    float phase = 0.0f;
  };
  std::array<HarmonicOsc, numHarmonics> harmonics{};
  std::array<std::atomic<float>, numHarmonics> harmonicGains{0.0f, 0.0f, 0.0f, 0.0f};

  // ── テーブル構築 ──
  void buildAllTables();
  static int bandIndexForFreq(float hz);

  /// テーブルから線形補間で1サンプル読み出す
  float readTable(const float* table) const;
};
