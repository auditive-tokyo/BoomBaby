#pragma once

#include "SamplePlayer.h"

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

/// Click チャンネルの DSP エンジン。
///   mode=1 (Tone)  : インパルス → BPF1(freq1/focus1) → BPF2(freq2/focus2)
///   mode=2 (Noise) : ホワイトノイズ → BPF 励起型（セルフレゾナント）
///   HPF/LPF は両モード共通のポスト EQ
class ClickEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分をレンダリングし、buffer に加算する。
  void render(juce::AudioBuffer<float> &buffer, int numSamples, bool clickPass,
              double sampleRate);

  // ── UI→DSP setter（スレッドセーフ） ──
  void setMode(int m) { mode_.store(m); }
  void setGainDb(float db) { gainDb_.store(db); }
  void setDecayMs(float ms) { decayMs_.store(ms); }
  /// Tone:  Osc 開始周波数 + BPF1 中心 /  Noise: BPF1 中心周波数
  void setFreq1(float hz) { freq1_.store(hz); }
  /// Tone:  BPF1 Q（0=バイパス）       /  Noise: BPF1 Q（高いほどリング）
  void setFocus1(float q) { focus1_.store(q); }
  void setFreq2(float hz) { freq2_.store(hz); }
  void setFocus2(float q) { focus2_.store(q); }
  /// HPF カットオフ周波数 (Q=0 でバイパス)
  void setHpfFreq(float hz) { hpfParams_.freq.store(hz); }
  void setHpfQ(float q) { hpfParams_.q.store(q); }
  /// HPF スロープ選択（12/24/48 dB/oct → 1/2/4 カスケード段数）
  void setHpfSlope(int dboct) {
    int stages = 1;
    if (dboct >= 48)      stages = 4;
    else if (dboct >= 24) stages = 2;
    hpfParams_.stages.store(stages);
  }
  /// LPF カットオフ周波数 (Q=0 でバイパス)
  void setLpfFreq(float hz) { lpfParams_.freq.store(hz); }
  void setLpfQ(float q) { lpfParams_.q.store(q); }
  /// LPF スロープ選択（12/24/48 dB/oct → 1/2/4 カスケード段数）
  void setLpfSlope(int dboct) {
    int stages = 1;
    if (dboct >= 48)      stages = 4;
    else if (dboct >= 24) stages = 2;
    lpfParams_.stages.store(stages);
  }

  /// レベル計測用 scratchBuffer の先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

  /// 内部 SamplePlayer への参照（UI からのロード用）
  SamplePlayer       &sampler()       noexcept { return sampler_; }
  const SamplePlayer &sampler() const noexcept { return sampler_; }

private:
  static constexpr int kMaxCascade = 4;

  /// HPF/LPF のパラメーター（freq / Q / カスケード段数）をまとめた構造体
  struct FilterParams {
    std::atomic<float> freq{0.0f};
    std::atomic<float> q{0.0f};
    std::atomic<int>   stages{1};
  };

  // ── render() の分割ヘルパー ──
  struct FilterFlags {
    bool bpf1;
    bool bpf2;
    bool hpf;
    bool lpf;
    int  hpfStages; ///< 1=12dB/oct, 2=24dB/oct, 4=48dB/oct
    int  lpfStages;
  };
  FilterFlags setupFilters(float sr);
  float synthesizeSample(int mode, const FilterFlags &flags);

  // ── 2 段 BPF ──
  juce::dsp::StateVariableTPTFilter<float> bpf1_; // freq1 / focus1
  juce::dsp::StateVariableTPTFilter<float> bpf2_; // freq2 / focus2
  // ── ポスト HPF / LPF（カスケード最大4段）──
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> hpfs_;
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> lpfs_;

  juce::Random random_; // Noise モード用 RNG
  SamplePlayer sampler_; // Sample モード用

  std::vector<float> scratchBuffer_;
  float noteTimeSamples_{0.0f};
  std::atomic<bool> active_{false};

  std::atomic<int> mode_{1}; // 1=Tone, 2=Noise, 3=Sample
  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> decayMs_{50.0f};
  std::atomic<float> freq1_{5000.0f};   // BPF1 中心周波数
  std::atomic<float> focus1_{0.71f};    // BPF1 Q
  std::atomic<float> freq2_{10000.0f};  // BPF2 中心周波数
  std::atomic<float> focus2_{0.0f};     // BPF2 Q (0=bypass)
  FilterParams hpfParams_{200.0f, 0.0f, 1};
  FilterParams lpfParams_{8000.0f, 0.0f, 1};
};
