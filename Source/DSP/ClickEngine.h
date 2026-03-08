#pragma once

#include "SamplePlayer.h"

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

/// Click チャンネルの DSP エンジン。
///   mode=1 (Noise)  : ホワイトノイズ → BPF 励起型（セルフレゾナント）
///   mode=2 (Sample) : SamplePlayer から読み出し
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
  /// Noise: BPF1 中心周波数
  void setFreq1(float hz) { freq1_.store(hz); }
  /// Noise: BPF1 Q（高いほどリング）
  void setFocus1(float q) { focus1_.store(q); }
  /// HPF カットオフ周波数 (20Hz 以下でバイパス)
  void setHpfFreq(float hz) { hpfParams_.freq.store(hz); }
  void setHpfQ(float q) { hpfParams_.q.store(q); }
  /// HPF スロープ選択（12/24/48 dB/oct → 1/2/4 カスケード段数）
  /// BPF1 スロープ選択（12/24/48 dB/oct → 1/2/4 カスケード段数）
  void setBpf1Slope(int dboct) {
    int stages = 1;
    if (dboct >= 48)
      stages = 4;
    else if (dboct >= 24)
      stages = 2;
    bpf1Stages_.store(stages);
  }
  /// Drive 量（0〜24 dB; 内部: pow(10, dB/20)）
  void setDriveDb(float db) { driveDb_.store(db); }
  /// ClipType: 0=Soft(tanh), 1=Hard, 2=Tube
  void setClipType(int t) { clipType_.store(t); }
  void setHpfSlope(int dboct) {
    int stages = 1;
    if (dboct >= 48)
      stages = 4;
    else if (dboct >= 24)
      stages = 2;
    hpfParams_.stages.store(stages);
  }
  /// LPF カットオフ周波数 (20000Hz 以上でバイパス)
  void setLpfFreq(float hz) { lpfParams_.freq.store(hz); }
  void setLpfQ(float q) { lpfParams_.q.store(q); }
  /// LPF スロープ選択（12/24/48 dB/oct → 1/2/4 カスケード段数）
  void setLpfSlope(int dboct) {
    int stages = 1;
    if (dboct >= 48)
      stages = 4;
    else if (dboct >= 24)
      stages = 2;
    lpfParams_.stages.store(stages);
  }

  // Sample モード用
  void setPitchSemitones(float st) { sampleParams_.pitchSemitones.store(st); }
  void setAttackMs(float ms) { sampleParams_.attackMs.store(ms); }
  void setReleaseMs(float ms) { sampleParams_.releaseMs.store(ms); }

  /// レベル計測用 scratchBuffer の先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

  /// 内部 SamplePlayer への参照（UI からのロード用）
  SamplePlayer &sampler() noexcept { return sampler_; }
  const SamplePlayer &sampler() const noexcept { return sampler_; }

private:
  static constexpr int kMaxCascade = 4;

  /// HPF/LPF のパラメーター（freq / Q / カスケード段数）をまとめた構造体
  struct FilterParams {
    std::atomic<float> freq{0.0f};
    std::atomic<float> q{0.0f};
    std::atomic<int> stages{1};
  };

  // ── render() の分割ヘルパー ──
  struct FilterFlags {
    bool bpf1;
    bool hpf;
    bool lpf;
    int bpf1Stages; ///< 1=12dB/oct, 2=24dB/oct, 4=48dB/oct
    int hpfStages;
    int lpfStages;
  };
  FilterFlags setupFilters(float sr);
  float synthesizeSample(int mode, const FilterFlags &flags, double playRate);

  // ── BPF（bpf1 はカスケード最大4段） ──
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade>
      bpf1s_; // freq1 / focus1
  // ── ポスト HPF / LPF（カスケード最大4段）──
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> hpfs_;
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> lpfs_;

  juce::Random random_;  // Noise モード用 RNG
  SamplePlayer sampler_; // Sample モード用

  std::vector<float> scratchBuffer_;
  float noteTimeSamples_{0.0f};
  std::atomic<bool> active_{false};

  /// Sample モード用パラメーターをまとめた構造体（A/D/R + ピッチ）
  struct SampleModeParams {
    std::atomic<float> pitchSemitones{0.0f};
    std::atomic<float> attackMs{1.0f};
    std::atomic<float> releaseMs{50.0f};
  };

  std::atomic<int> bpf1Stages_{1}; // BPF1 cascade stages
  std::atomic<int> mode_{1};       // 1=Noise, 2=Sample
  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> decayMs_{30.0f};
  // Sample モード用パラメーター
  SampleModeParams sampleParams_;
  // Noise 用 BPF パラメーター
  std::atomic<float> freq1_{5000.0f}; // BPF1 中心周波数
  std::atomic<float> focus1_{0.71f};  // BPF1 Q
  std::atomic<float> driveDb_{0.0f};  // Drive (dB)
  std::atomic<int> clipType_{0};      // 0=Soft, 1=Hard, 2=Bit
  FilterParams hpfParams_{20.0f, 0.71f, 1}; // デフォルト: バイパス(20Hz), Q=0.71
  FilterParams lpfParams_{20000.0f, 0.71f,
                          1}; // デフォルト: バイパス(20kHz), Q=0.71
};
