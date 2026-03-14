#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <vector>

/// WAV/AIFF サンプルのロード・再生を管理する共通クラス。
/// Direct / Click の Sample モードで共有する。
/// フィルターやエンベロープは呼び出し側が担当する。
class SamplePlayer {
public:
  // ── lifecycle ──
  void prepare();

  /// サンプルファイルをロード（メッセージスレッドから呼ぶこと）。
  void loadSample(const juce::File &file);

  /// ロード済みサンプルを解放する（メッセージスレッドから呼ぶこと）。
  void unloadSample();

  bool isLoaded() const noexcept { return loaded_.load(); }

  /// NoteOn 時にプレイヘッドをリセット。
  void resetPlayhead() noexcept { playheadSamples_ = 0.0; }

  /// 線形補間で 1 サンプル読み出し、プレイヘッドを playRate だけ進める。
  /// サンプル末端に達したら 0 を返し finished=true をセットする。
  /// ロック取得に失敗した場合も 0 / finished=true。
  float readNext(double playRate, bool &finished);

  /// ロック取得を render ループの先頭で 1 回だけ行う版。
  /// srcData/srcLen が有効な間、readInterpolated を直接呼べる。
  struct LockedView {
    const float *data = nullptr;
    int          length = 0;
    std::unique_ptr<juce::SpinLock::ScopedTryLockType> lock;
    explicit operator bool() const noexcept { return data != nullptr; }
  };
  LockedView lock() noexcept;

  /// ロック済みバッファに対する線形補間読み出し。プレイヘッドを進める。
  float readInterpolated(const float *srcData, int srcLen,
                         double playRate, bool &finished);

  // ── メタ情報 ──
  double sampleRate() const noexcept { return sampleSampleRate_; }
  double durationSec() const noexcept { return durationSec_.load(); }
  bool copyThumbnail(std::vector<float> &outMin,
                     std::vector<float> &outMax) const noexcept;

private:
  juce::AudioFormatManager  formatManager_;
  juce::SpinLock            sampleLock_;
  juce::AudioBuffer<float>  buffer_;
  std::atomic<bool>         loaded_{false};
  double                    sampleSampleRate_{44100.0};
  double                    playheadSamples_{0.0};

  // 波形サムネイル（メッセージスレッド専用）
  std::vector<float>        thumbMin_;
  std::vector<float>        thumbMax_;
  std::atomic<double>       durationSec_{0.0};
};
