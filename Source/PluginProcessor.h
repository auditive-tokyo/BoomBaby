#pragma once

#include "DSP/ChannelState.h"
#include "DSP/EnvelopeLutManager.h"
#include "DSP/OomphOscillator.h"
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

class BabySquatchAudioProcessor : public juce::AudioProcessor
{
public:
    BabySquatchAudioProcessor();
    ~BabySquatchAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override; // NOSONAR: JUCE API signature
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override; // NOSONAR: JUCE API signature
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /// GUI鍵盤との共有 MidiKeyboardState
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }

    /// Oomph出力ゲイン（dB）— UIノブから書き込み、processBlockで適用
    void setOomphGainDb(float db) { oomphGainDb.store(db); }

    /// One-shot 長さ（ms）— Length ボックスから書き込み、renderOomph で自動停止に使用
    void setOomphLengthMs(float ms) { oomphLengthMs.store(ms); }

    // ── 委譲先ヘルパーへのアクセサ ──
    ChannelState&       channelState()       noexcept { return channelState_; }
    const ChannelState& channelState() const noexcept { return channelState_; }
    EnvelopeLutManager& envLut()             noexcept { return envLut_; }
    EnvelopeLutManager& pitchLut()            noexcept { return pitchLut_; }
    EnvelopeLutManager& distLut()             noexcept { return distLut_; }
    EnvelopeLutManager& blendLut()            noexcept { return blendLut_; }
    OomphOscillator&    oomphOscillator()     noexcept { return oomphOsc; }

private:
    void handleMidiEvents(juce::MidiBuffer& midiMessages, int numSamples);
    void renderOomph(juce::AudioBuffer<float>& buffer, int numSamples, bool oomphPass);

    juce::MidiKeyboardState keyboardState;
    OomphOscillator oomphOsc;
    std::atomic<float> oomphGainDb{0.0f};

    ChannelState channelState_;
    EnvelopeLutManager envLut_;
    EnvelopeLutManager pitchLut_;
    EnvelopeLutManager distLut_;
    EnvelopeLutManager blendLut_;

    std::vector<float> oomphScratchBuffer;
    float noteTimeSamples{0.0f};
    std::atomic<float> oomphLengthMs{300.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabySquatchAudioProcessor)
};
