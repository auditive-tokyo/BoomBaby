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
  SubEngine &subEngine() noexcept { return subEngine_; }
  ClickEngine &clickEngine() noexcept { return clickEngine_; }
  DirectEngine &directEngine() noexcept { return directEngine_; }
  ChannelState &channelState() noexcept { return channelState_; }
  const ChannelState &channelState() const noexcept { return channelState_; }

  // ── マスターセクション（ゲイン・レベル計測。将来: limiter / preset 拡張用）──
  struct MasterSection {
    std::atomic<float> gainDb_{0.0f};
    mutable std::array<LevelDetector, 2> detector_; ///< 0=L, 1=R

    void setGain(float db) noexcept { gainDb_.store(db); }
    float getGain() const noexcept { return gainDb_.load(); }
    float getLevelDb(int ch) const noexcept {
      return detector_[static_cast<std::size_t>(ch & 1)].getPeakDb();
    }
  };
  MasterSection &master() noexcept { return master_; }
  const MasterSection &master() const noexcept { return master_; }

  // ── 入力モニター（FIFO 波形表示。将来: spectrum / input gain 拡張用）──
  struct InputMonitor {
    static constexpr int kCapacity = 48000; ///< ~1秒分 @ 48kHz
    juce::AbstractFifo fifo_{kCapacity};
    std::vector<float> data_;

    juce::AbstractFifo &fifo() noexcept { return fifo_; }
    const std::vector<float> &data() const noexcept { return data_; }
  };
  InputMonitor &inputMonitor() noexcept { return inputMonitor_; }

  // ── Direct モード（パススルー / トランジェント検出。将来: latency comp 拡張用）──
  struct DirectMode {
    std::atomic<bool> sampleMode_{false};
    TransientDetector transientDetector_;

    bool isPassthrough() const noexcept { return !sampleMode_.load(); }
    TransientDetector &detector() noexcept { return transientDetector_; }
  };
  /// Direct Sample モード切り替え（UI スレッドから設定）
  void setDirectSampleMode(bool isSample) noexcept;
  DirectMode &directMode() noexcept { return directMode_; }

private:
  void handleMidiEvents(juce::MidiBuffer &midiMessages, int numSamples);

  juce::MidiKeyboardState keyboardState;
  SubEngine subEngine_;
  ClickEngine clickEngine_;
  DirectEngine directEngine_;
  ChannelState channelState_;
  MasterSection master_;
  InputMonitor inputMonitor_;
  DirectMode directMode_;
  std::vector<float> monoMixBuffer_; ///< トランジェント検出用モノ合成バッファ

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessor)
};
