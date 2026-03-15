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
#include <juce_dsp/juce_dsp.h>

class BoomBabyAudioProcessor
    : public juce::AudioProcessor,
      private juce::AudioProcessorValueTreeState::Listener {
public:
  BoomBabyAudioProcessor();
  ~BoomBabyAudioProcessor() override;

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

  /// 保存済み APVTS state の ENVELOPE ノードから全 LUT を再ベイク。
  /// setStateInformation / prepareToPlay の両方から呼ぶ。
  void bakeAllLutsFromState();

  /// APVTS アクセサ
  juce::AudioProcessorValueTreeState &getAPVTS() noexcept { return apvts_; }

  /// DAW Undo/Redo 検出用バージョン（setStateInformation 毎にインクリメント）
  int nonParamStateVersion() const noexcept {
    return nonParamStateVersion_.load(std::memory_order_acquire);
  }

  /// GUI鍵盤との共有 MidiKeyboardState
  juce::MidiKeyboardState &getKeyboardState() { return keyboardState; }

  // ── 委譲先エンジン／ヘルパーへのアクセサ ──
  SubEngine &subEngine() noexcept { return subEngine_; }
  ClickEngine &clickEngine() noexcept { return clickEngine_; }
  DirectEngine &directEngine() noexcept { return directEngine_; }
  ChannelState &channelState() noexcept { return channelState_; }

  // ── マスターセクション（ゲイン・レベル計測。将来: limiter / preset
  // 拡張用）──
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

  // ── 入力モニター（FIFO 波形表示。将来: spectrum / input gain 拡張用）──
  struct InputMonitor {
    static constexpr int kCapacity = 192000; ///< ~1秒分 @ 192kHz
    juce::AbstractFifo fifo_{kCapacity};
    std::vector<float> data_;

    juce::AbstractFifo &fifo() noexcept { return fifo_; }
    const std::vector<float> &data() const noexcept { return data_; }
  };
  InputMonitor &inputMonitor() noexcept { return inputMonitor_; }

  // ── Direct モード（パススルー / トランジェント検出 / ルックアヘッド補正）──
  struct DirectMode {
    std::atomic<bool> sampleMode_{false};
    TransientDetector transientDetector_;

    bool isPassthrough() const noexcept { return !sampleMode_.load(); }
    TransientDetector &detector() noexcept { return transientDetector_; }
    /// Sample モード切り替え（UI スレッドから設定）
    void setSampleMode(bool isSample, DirectEngine &engine) noexcept {
      sampleMode_.store(isSample);
      engine.setPassthroughMode(!isSample);
      transientDetector_.setEnabled(!isSample);
    }
  };
  DirectMode &directMode() noexcept { return directMode_; }

private:
  /// APVTS Listener: パラメータ変更を DSP へ反映
  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;

  juce::MidiKeyboardState keyboardState;
  SubEngine subEngine_;
  ClickEngine clickEngine_;
  DirectEngine directEngine_;
  ChannelState channelState_;
  MasterSection master_;
  InputMonitor inputMonitor_;
  DirectMode directMode_;
  std::vector<float> monoMixBuffer_; ///< トランジェント検出用モノ合成バッファ

  /// APVTS（全パラメータの一元管理 + 状態保存/復元）
  static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  juce::AudioProcessorValueTreeState apvts_;

  /// DAW Undo/Redo 検出用: setStateInformation 呼び出し毎にインクリメント
  std::atomic<int> nonParamStateVersion_{0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoomBabyAudioProcessor)
};
