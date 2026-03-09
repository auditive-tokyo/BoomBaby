#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <span>

BabySquatchAudioProcessor::BabySquatchAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

BabySquatchAudioProcessor::~BabySquatchAudioProcessor() = default;

const juce::String // NOSONAR: JUCE API
BabySquatchAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool BabySquatchAudioProcessor::acceptsMidi() const { return true; }

bool BabySquatchAudioProcessor::producesMidi() const { return false; }

bool BabySquatchAudioProcessor::isMidiEffect() const { return false; }

double BabySquatchAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BabySquatchAudioProcessor::getNumPrograms() { return 1; }

int BabySquatchAudioProcessor::getCurrentProgram() { return 0; }

void BabySquatchAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String // NOSONAR: JUCE API
BabySquatchAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void BabySquatchAudioProcessor::changeProgramName(int index,
                                                  const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

void BabySquatchAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  subEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  clickEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  directEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  channelState_.resetDetectors();

  transientDetector_.prepare(sampleRate);
  transientDetector_.setThresholdDb(-24.0f);
  transientDetector_.setHoldMs(50.0f);
  monoMixBuffer_.resize(static_cast<std::size_t>(samplesPerBlock));

  inputFifoData_.assign(static_cast<std::size_t>(kInputFifoCapacity), 0.0f);
  inputFifo_.reset();
}

void BabySquatchAudioProcessor::releaseResources() {
  // Currently no resources to release - will be populated when adding DSP
  // processing
}

bool BabySquatchAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
    return false;

  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

void BabySquatchAudioProcessor::handleMidiEvents(juce::MidiBuffer &midiMessages,
                                                 int numSamples) {
  // GUI鍵盤のMIDIイベントをバッファにマージ
  keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    if (msg.isNoteOn()) {
      subEngine_.triggerNote();
      clickEngine_.triggerNote();
      directEngine_.triggerNote();
    }
    // NoteOff は無視: One-shot 長さは subLengthMs で決定
  }
}

void BabySquatchAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const auto passes = channelState_.computePasses();
  const int numSamples = buffer.getNumSamples();
  const double sr = getSampleRate();

  // ── モノミックス（Direct パススルーモード時: 波形表示 /
  // トランジェント検出に使用）──
  if (!directSampleMode_.load() && buffer.getNumChannels() > 0) {
    const int numCh = buffer.getNumChannels();
    auto *mono = monoMixBuffer_.data();
    const float *ch0 = buffer.getReadPointer(0);
    if (numCh >= 2) {
      const float *ch1 = buffer.getReadPointer(1);
      for (int i = 0; i < numSamples; ++i)
        mono[i] = (ch0[i] + ch1[i]) * 0.5f;
    } else {
      std::copy_n(ch0, numSamples, mono);
    }

    // Auto トリガー: トランジェント検出
    if (transientDetector_.isEnabled() &&
        transientDetector_.process(std::span<const float>(
            mono, static_cast<std::size_t>(numSamples)))) {
      subEngine_.triggerNote();
      clickEngine_.triggerNote();
      directEngine_.triggerNote();
    }

    // 波形リアルタイム表示用 FIFO へ書き込み
    const int toWrite = juce::jmin(numSamples, inputFifo_.getFreeSpace());
    if (toWrite > 0) {
      int s1;
      int sz1;
      int s2;
      int sz2;
      inputFifo_.prepareToWrite(toWrite, s1, sz1, s2, sz2);
      if (sz1 > 0)
        std::copy_n(mono, sz1, inputFifoData_.data() + s1);
      if (sz2 > 0)
        std::copy_n(mono + sz1, sz2, inputFifoData_.data() + s2);
      inputFifo_.finishedWrite(sz1 + sz2);
    }
  }

  // Direct ミュート: 入力信号ごと消去（Sub はこの後加算するので影響なし）
  // パススルーモード時も renderPassthrough() が addSample するため常にクリア
  buffer.clear();

  handleMidiEvents(midiMessages, numSamples);
  subEngine_.render(buffer, numSamples, passes.sub, sr);
  clickEngine_.render(buffer, numSamples, passes.click, sr);

  // Direct: パススルーモードとサンプルモードで呼び分け
  if (!directSampleMode_.load() && passes.direct) {
    directEngine_.renderPassthrough(
        buffer,
        std::span<const float>(monoMixBuffer_.data(),
                               static_cast<std::size_t>(numSamples)),
        numSamples, sr);
  } else {
    directEngine_.render(buffer, numSamples, passes.direct, sr);
  }

  // マスターゲイン適用
  buffer.applyGain(juce::Decibels::decibelsToGain(masterGainDb_.load()));

  // マスター出力 L/R レベル計測
  for (std::size_t ch = 0; ch < 2; ++ch) {
    const int bufCh =
        juce::jmin(static_cast<int>(ch), buffer.getNumChannels() - 1);
    if (bufCh >= 0)
      masterDetector_[ch].process(buffer.getReadPointer(bufCh), numSamples);
    else
      masterDetector_[ch].process(nullptr, numSamples);
  }

  using enum ChannelState::Channel;

  // Sub レベル計測
  if (passes.sub)
    channelState_.detector(sub).process(subEngine_.scratchData(), numSamples);
  else
    channelState_.detector(sub).process(nullptr, numSamples);

  // Click レベル計測
  if (passes.click)
    channelState_.detector(click).process(clickEngine_.scratchData(),
                                          numSamples);
  else
    channelState_.detector(click).process(nullptr, numSamples);

  // Direct レベル計測（render 後に scratchData を参照）
  if (passes.direct)
    channelState_.detector(direct).process(directEngine_.scratchData(),
                                           numSamples);
  else
    channelState_.detector(direct).process(nullptr, numSamples);
}

bool BabySquatchAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *BabySquatchAudioProcessor::createEditor() {
  // clang-format off
  return new BabySquatchAudioProcessorEditor(*this); // NOSONAR: DAW host takes ownership
  // clang-format on
}

void BabySquatchAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  juce::ignoreUnused(destData);
}

void BabySquatchAudioProcessor::setStateInformation(
    const void *data, // NOSONAR: JUCE API
    int sizeInBytes) {
  juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BabySquatchAudioProcessor(); // NOSONAR: DAW host takes ownership
}
