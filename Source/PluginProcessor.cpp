#include "PluginProcessor.h"
#include "PluginEditor.h"

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

  // ── トランジェント検出（入力信号をクリアする前に解析）──
  // Sample モード時は入力不要のため検出もスキップ
  if (transientDetector_.isEnabled() && !directSampleMode_.load() && buffer.getNumChannels() > 0) {
    const int numCh = buffer.getNumChannels();
    auto *mono = monoMixBuffer_.data();
    // ステレオ→モノミックス（L+R 平均の絶対値で検出）
    const float *ch0 = buffer.getReadPointer(0);
    if (numCh >= 2) {
      const float *ch1 = buffer.getReadPointer(1);
      for (int i = 0; i < numSamples; ++i)
        mono[i] = (ch0[i] + ch1[i]) * 0.5f;
    } else {
      std::copy_n(ch0, numSamples, mono);
    }
    if (transientDetector_.process(std::span<const float>(mono, static_cast<std::size_t>(numSamples)))) {
      subEngine_.triggerNote();
      clickEngine_.triggerNote();
      directEngine_.triggerNote();
    }
  }

  // Direct ミュート: 入力信号ごと消去（Sub はこの後加算するので影響なし）
  // Sample モード時も入力をクリア（サンプル再生は render() で加算する）
  if (!passes.direct || directSampleMode_.load())
    buffer.clear();

  handleMidiEvents(midiMessages, numSamples);
  subEngine_.render(buffer, numSamples, passes.sub, sr);
  clickEngine_.render(buffer, numSamples, passes.click, sr);
  directEngine_.render(buffer, numSamples, passes.direct, sr);

  // マスターゲイン適用
  buffer.applyGain(juce::Decibels::decibelsToGain(masterGainDb_.load()));

  // マスター出力 L/R レベル計測
  for (std::size_t ch = 0; ch < 2; ++ch) {
    const int bufCh = juce::jmin(static_cast<int>(ch), buffer.getNumChannels() - 1);
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
