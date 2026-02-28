#pragma once

#include "SubOscillator.h"
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/// Click チャンネル（Tone モード）の DSP を一括管理するエンジン。
/// SQR / SAW 2 本のオシレーターを線形ブレンドし、指数減衰エンベロープを適用。
class ClickEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分をレンダリングし、buffer に加算する。
  void render(juce::AudioBuffer<float> &buffer, int numSamples,
              bool clickPass, double sampleRate);

  // ── UI→DSP setter（スレッドセーフ） ──
  void setGainDb(float db) { gainDb_.store(db); }
  void setBlend(float blend01) { blend_.store(blend01); }
  void setDecayMs(float ms) { decayMs_.store(ms); }

  /// レベル計測用 scratchBuffer の先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

private:
  SubOscillator sqrOsc_;
  SubOscillator sawOsc_;

  std::vector<float> scratchBuffer_;
  float noteTimeSamples_{0.0f};

  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> decayMs_{50.0f};
  std::atomic<float> blend_{0.5f}; // 0=SQR, 1=SAW
};
