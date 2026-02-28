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
  channelState_.resetDetectors();
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
    }
    // NoteOff は無視: One-shot 長さは subLengthMs で決定
  }
}

void BabySquatchAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const auto passes = channelState_.computePasses();

  // Direct ミュート: 入力信号ごと消去（Sub はこの後加算するので影響なし）
  if (!passes.direct)
    buffer.clear();

  const int numSamples = buffer.getNumSamples();
  const double sr = getSampleRate();

  // Direct レベル計測（レンダリング前の純粋入力信号）
  using enum ChannelState::Channel;
  if (passes.direct)
    channelState_.detector(direct).process(buffer.getReadPointer(0), numSamples);
  else
    channelState_.detector(direct).process(nullptr, numSamples);

  handleMidiEvents(midiMessages, numSamples);
  subEngine_.render(buffer, numSamples, passes.sub, sr);
  clickEngine_.render(buffer, numSamples, passes.click, sr);

  // Sub レベル計測
  if (passes.sub)
    channelState_.detector(sub).process(subEngine_.scratchData(), numSamples);
  else
    channelState_.detector(sub).process(nullptr, numSamples);

  // Click レベル計測
  if (passes.click)
    channelState_.detector(click).process(clickEngine_.scratchData(), numSamples);
  else
    channelState_.detector(click).process(nullptr, numSamples);
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
