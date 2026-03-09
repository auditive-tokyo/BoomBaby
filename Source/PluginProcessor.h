#pragma once

#include "DSP/ChannelState.h"
#include "DSP/ClickEngine.h"
#include "DSP/DirectEngine.h"
#include "DSP/LevelDetector.h"
#include "DSP/SubEngine.h"
#include "DSP/TransientDetector.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class BabySquatchAudioProcessor : public juce::AudioProcessor {
public:
  BabySquatchAudioProcessor();
  ~BabySquatchAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  using juce::AudioProcessor::processBlock;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override; // NOSONAR: JUCE API signature
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String
  getProgramName(int index) override; // NOSONAR: JUCE API signature
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  /// GUI鍵盤との共有 MidiKeyboardState
  juce::MidiKeyboardState &getKeyboardState() { return keyboardState; }

  // ── 委譲先エンジン／ヘルパーへのアクセサ ──
  SubEngine    &subEngine()    noexcept { return subEngine_; }
  ClickEngine  &clickEngine()  noexcept { return clickEngine_; }
  DirectEngine &directEngine() noexcept { return directEngine_; }
  ChannelState &channelState() noexcept { return channelState_; }
  const ChannelState &channelState() const noexcept { return channelState_; }

  void setMasterGainDb(float db) { masterGainDb_.store(db); }
  float getMasterGainDb() const  { return masterGainDb_.load(); }

  /// トランジェント検出器へのアクセサ
  TransientDetector &transientDetector() noexcept { return transientDetector_; }

  /// Direct Sample モード時は入力をミュート（UI スレッドから設定）
  void setDirectSampleMode(bool isSample) noexcept { directSampleMode_.store(isSample); }
  /// UIスレッドからマスター出力ドアを取得 (ch: 0=L, 1=R)
  float getMasterLevelDb(int ch) const { return masterDetector_[static_cast<std::size_t>(ch & 1)].getPeakDb(); }

private:
  void handleMidiEvents(juce::MidiBuffer &midiMessages, int numSamples);

  juce::MidiKeyboardState keyboardState;
  SubEngine    subEngine_;
  ClickEngine  clickEngine_;
  DirectEngine directEngine_;
  ChannelState channelState_;
  std::atomic<float> masterGainDb_{0.0f};
  mutable std::array<LevelDetector, 2> masterDetector_; // 0=L, 1=R
  TransientDetector transientDetector_;
  std::vector<float> monoMixBuffer_; // トランジェント検出用モノ合成バッファ
  std::atomic<bool> directSampleMode_{false}; // Direct が Sample モードのとき入力を消去

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessor)
};
