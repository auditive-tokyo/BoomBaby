#pragma once

#include "SamplePlayer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

/// サンプル再生エンジン（Direct チャンネル / Sample モード）
/// - AHR エンベロープ（Attack → Hold → Release）
/// - ピッチ: セミトーン単位の線形補間再生
/// - ポスト HPF / LPF カスケードフィルター + tanh ソフトクリップ
class DirectEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分レンダリング。directPass=true のときのみ buffer に加算。
  void render(juce::AudioBuffer<float> &buffer, int numSamples,
              bool directPass, double sampleRate);

  /// 内部 SamplePlayer への参照（UI からのロード / サムネイル取得用）
  SamplePlayer       &sampler()       noexcept { return sampler_; }
  const SamplePlayer &sampler() const noexcept { return sampler_; }

  // ── UI→DSP setter（スレッドセーフ） ──
  void setGainDb(float db)          { gainDb_.store(db); }
  void setPitchSemitones(float st)  { pitchSemitones_.store(st); }
  void setAttackMs(float ms)        { envParams_.attackMs.store(juce::jmax(0.1f, ms)); }
  void setDecayMs(float ms)          { envParams_.decayMs.store(juce::jmax(0.1f, ms)); }
  void setReleaseMs(float ms)        { envParams_.releaseMs.store(juce::jmax(0.1f, ms)); }

  void setHpfFreq(float hz) { hpfParams_.freq.store(hz); }
  void setHpfQ(float q)     { hpfParams_.q.store(q); }
  void setHpfSlope(int dboct) {
    int s = 1;
    if (dboct >= 48)      { s = 4; }
    else if (dboct >= 24) { s = 2; }
    hpfParams_.stages.store(s);
  }
  void setLpfFreq(float hz) { lpfParams_.freq.store(hz); }
  void setLpfQ(float q)     { lpfParams_.q.store(q); }
  void setLpfSlope(int dboct) {
    int s = 1;
    if (dboct >= 48)      { s = 4; }
    else if (dboct >= 24) { s = 2; }
    lpfParams_.stages.store(s);
  }

  /// レベル計測用スクラッチバッファの先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

private:
  static constexpr int kMaxCascade = 4;

  struct FilterState {
    bool doHpf;
    bool doLpf;
    int  hpfStg;
    int  lpfStg;
  };

  FilterState prepareFilters(float sr);
  bool advanceEnvelope(float nt, float atkSamples,
                       float decSamples, float relSamples);
  /// フィルター適用 + tanh ソフトクリップ
  float applyFilters(float s, const FilterState &fs);

  struct FilterParams {
    std::atomic<float> freq{0.0f};
    std::atomic<float> q{0.0f};
    std::atomic<int>   stages{1};
  };

  struct EnvParams {
    std::atomic<float> attackMs{1.0f};
    std::atomic<float> decayMs{200.0f};
    std::atomic<float> releaseMs{50.0f};
  };

  enum class EnvPhase { Idle, Attack, Hold, Release };

  SamplePlayer sampler_;

  // 再生状態
  std::vector<float>          scratchBuffer_;
  std::atomic<bool>           active_{false};
  EnvPhase                    envPhase_{EnvPhase::Idle};
  float                       envLevel_{0.0f};

  // フィルター
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> hpfs_;
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> lpfs_;
  FilterParams hpfParams_{};
  FilterParams lpfParams_{};

  // パラメータ
  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> pitchSemitones_{0.0f};
  EnvParams          envParams_{};
};
