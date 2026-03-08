#pragma once

#include "EnvelopeLutManager.h"
#include "SamplePlayer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

/// サンプル再生エンジン（Direct チャンネル / Sample モード）
/// - Click sampler と同一の LUT エンベロープ方式
/// - ピッチ: セミトーン単位の線形補間再生
/// - Saturator (Soft/Hard/Tube) + ポスト HPF / LPF カスケードフィルター
class DirectEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分レンダリング。directPass=true のときのみ buffer に加算。
  void render(juce::AudioBuffer<float> &buffer, int numSamples, bool directPass,
              double sampleRate);

  /// 内部 SamplePlayer への参照（UI からのロード / サムネイル取得用）
  SamplePlayer &sampler() noexcept { return sampler_; }
  const SamplePlayer &sampler() const noexcept { return sampler_; }

  /// Direct Amp 用 LUT（UI から bakeLut で書き込み）
  EnvelopeLutManager &directAmpLut() noexcept { return directAmpLut_; }

  // ── UI→DSP setter（スレッドセーフ） ──
  void setGainDb(float db) { gainDb_.store(db); }
  void setPitchSemitones(float st) { pitchSemitones_.store(st); }
  void setDriveDb(float db) { driveDb_.store(db); }
  void setClipType(int t) { clipType_.store(t); }

  void setHpfFreq(float hz) { hpfParams_.freq.store(hz); }
  void setHpfQ(float q) { hpfParams_.q.store(q); }
  void setHpfSlope(int dboct) {
    int s = 1;
    if (dboct >= 48) {
      s = 4;
    } else if (dboct >= 24) {
      s = 2;
    }
    hpfParams_.stages.store(s);
  }
  void setLpfFreq(float hz) { lpfParams_.freq.store(hz); }
  void setLpfQ(float q) { lpfParams_.q.store(q); }
  void setLpfSlope(int dboct) {
    int s = 1;
    if (dboct >= 48) {
      s = 4;
    } else if (dboct >= 24) {
      s = 2;
    }
    lpfParams_.stages.store(s);
  }

  /// レベル計測用スクラッチバッファの先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

private:
  static constexpr int kMaxCascade = 4;

  struct FilterState {
    bool doHpf;
    bool doLpf;
    int hpfStg;
    int lpfStg;
  };

  FilterState prepareFilters(float sr);
  /// サンプル合成（読み出し→Saturator→フィルター→共振整形）
  float synthesizeSample(const FilterState &fs, double playRate);
  /// LUT エンベロープ振幅（末尾 half-cosine フェード付き）
  float computeSampleAmp(float noteTimeMs) const;
  /// 停止判定用最大再生時間（サンプル数）
  float computeMaxTimeSamples(float sr, double playRate) const;

  struct FilterParams {
    std::atomic<float> freq{0.0f};
    std::atomic<float> q{0.0f};
    std::atomic<int> stages{1};
  };

  SamplePlayer sampler_;

  // synthesizeSample() 用ロックビューキャッシュ（render 中のみ有効）
  const float *viewData_{nullptr};
  int viewLen_{0};

  // 再生状態
  std::vector<float> scratchBuffer_;
  std::atomic<bool> active_{false};
  float noteTimeSamples_{0.0f};

  // フィルター
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> hpfs_;
  std::array<juce::dsp::StateVariableTPTFilter<float>, kMaxCascade> lpfs_;
  FilterParams hpfParams_{};
  FilterParams lpfParams_{};

  // パラメータ
  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> pitchSemitones_{0.0f};
  std::atomic<float> driveDb_{0.0f};
  std::atomic<int> clipType_{0};
  EnvelopeLutManager directAmpLut_; // Direct Amp エンベロープ LUT
};
