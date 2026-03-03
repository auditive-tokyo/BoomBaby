#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

/// サンプル再生エンジン（Direct チャンネル / Sample モード）
/// - ADSR エンベロープ（Sustain なし: Attack → Hold → Release）
/// - ピッチ: セミトーン単位の線形補間再生
/// - ポスト HPF / LPF カスケードフィルター（ClickEngine と同構造）
class DirectEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分レンダリング。directPass=true のときのみ buffer に加算。
  void render(juce::AudioBuffer<float> &buffer, int numSamples,
              bool directPass, double sampleRate);

  /// サンプルファイルをロード（メッセージスレッドから呼び出すこと）
  void loadSample(const juce::File &file);
  bool isSampleLoaded() const noexcept { return sampleLoaded_.load(); }

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

  /// ロード時に事前計算した波形サムネイルをコピー（メッセージスレッドから）。
  /// outMin/outMax に各ビンの最小値・最大値を格納。サンプル未ロード時は false を返す。
  bool copyWaveformThumbnail(std::vector<float> &outMin,
                             std::vector<float> &outMax) const noexcept;

  /// サンプル長(秒)を返す（未ロード時は 0.0）。
  double getSampleDurationSec() const noexcept { return sampleDurationSec_.load(); }

private:
  static constexpr int kMaxCascade = 4;

  /// フィルター設定後に render ループへ渡す軽量コンテキスト
  struct FilterState {
    bool doHpf;
    bool doLpf;
    int  hpfStg;
    int  lpfStg;
  };

  /// フィルターパラメータをセットし FilterState を返す
  FilterState prepareFilters(float sr);
  /// エンベロープを 1 サンプル進める。停止すべきなら true を返す
  bool advanceEnvelope(float nt, float atkSamples,
                       float decSamples, float relSamples);
  /// 線形補間読み出し + フィルター適用。停止すべきなら done=true
  float readSample(const float *srcData, int srcLen,
                   const FilterState &fs, float gain,
                   double playRate, bool &done);

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

  // サンプルバッファ（SpinLock でガード）
  juce::SpinLock              sampleLock_;
  juce::AudioBuffer<float>    sampleBuffer_;
  std::atomic<bool>           sampleLoaded_{false};
  double                      sampleSampleRate_{44100.0};
  juce::AudioFormatManager    formatManager_;

  // 波形サムネイル（メッセージスレッド専用・ロック不要）
  std::vector<float>          waveformThumbMin_;
  std::vector<float>          waveformThumbMax_;
  std::atomic<double>         sampleDurationSec_{0.0};

  // 再生状態
  std::vector<float>          scratchBuffer_;
  double                      playheadSamples_{0.0};
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
