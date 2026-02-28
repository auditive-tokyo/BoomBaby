#pragma once

#include "EnvelopeLutManager.h"
#include "SubOscillator.h"
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/// Sub チャンネルの DSP を一括管理するエンジン。
/// PluginProcessor から renderSub / 関連フィールドを分離し、
/// メソッド数を削減するためのヘルパークラス。
class SubEngine {
public:
  // ── lifecycle ──
  void prepareToPlay(double sampleRate, int samplesPerBlock);

  /// MIDI NoteOn 時のトリガー
  void triggerNote();

  /// 1 ブロック分をレンダリングし、buffer に加算する。
  /// @param subPass  false のときは scratchBuffer のみ書き込み（Mute 時）
  void render(juce::AudioBuffer<float> &buffer, int numSamples,
              bool subPass, double sampleRate);

  // ── UI→DSP setter（スレッドセーフ） ──
  void setGainDb(float db) { gainDb_.store(db); }
  void setLengthMs(float ms) { lengthMs_.store(ms); }

  // ── 委譲先アクセサ ──
  EnvelopeLutManager &envLut() noexcept { return envLut_; }
  EnvelopeLutManager &pitchLut() noexcept { return pitchLut_; }
  EnvelopeLutManager &distLut() noexcept { return distLut_; }
  EnvelopeLutManager &blendLut() noexcept { return blendLut_; }
  SubOscillator &oscillator() noexcept { return osc_; }

  /// レベル計測用 scratchBuffer の先頭ポインタ
  const float *scratchData() const noexcept { return scratchBuffer_.data(); }

private:
  SubOscillator osc_;
  EnvelopeLutManager envLut_;
  EnvelopeLutManager pitchLut_;
  EnvelopeLutManager distLut_;
  EnvelopeLutManager blendLut_;

  std::vector<float> scratchBuffer_;
  float noteTimeSamples_{0.0f};

  std::atomic<float> gainDb_{0.0f};
  std::atomic<float> lengthMs_{300.0f};
};
