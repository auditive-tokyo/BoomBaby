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
  oomphOsc.prepareToPlay(sampleRate);
  oomphScratchBuffer.resize(static_cast<size_t>(samplesPerBlock));
  noteTimeSamples = 0.0f;

  envLut_.reset();
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

  // MIDIバッファを走査 → OomphOscillator に通知
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    if (msg.isNoteOn()) {
      oomphOsc.triggerNote();
      noteTimeSamples = 0.0f; // エンベロープリセット
    } else if (msg.isNoteOff()) {
      oomphOsc.stopNote();
    }
  }
}

void BabySquatchAudioProcessor::renderOomph(juce::AudioBuffer<float> &buffer,
                                            int numSamples, bool oomphPass) {
  if (!oomphOsc.isActive()) {
    std::fill_n(oomphScratchBuffer.data(), numSamples, 0.0f);
    return;
  }

  const float gain = juce::Decibels::decibelsToGain(oomphGainDb.load());
  const int numChannels = buffer.getNumChannels();
  const auto &ampLut = envLut_.getActiveLut();
  const float ampDurMs = envLut_.getDurationMs();
  const auto &pLut = pitchLut_.getActiveLut();
  const float pitchDurMs = pitchLut_.getDurationMs();
  const auto &dLut = distLut_.getActiveLut();
  const float distDurMs = distLut_.getDurationMs();
  const auto &bLut = blendLut_.getActiveLut();
  const float blendDurMs = blendLut_.getDurationMs();
  const auto sr = static_cast<float>(getSampleRate());

  for (int sample = 0; sample < numSamples; ++sample) {
    const float noteTimeMs = noteTimeSamples * 1000.0f / sr;

    // Pitch LUT → Hz
    const float pitchLutPos =
        (pitchDurMs > 0.0f)
            ? (noteTimeMs / pitchDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto pitchLutIdx =
        std::min(static_cast<int>(pitchLutPos), EnvelopeLutManager::lutSize - 1);
    const float pitchHz = pLut[static_cast<size_t>(pitchLutIdx)];
    oomphOsc.setFrequencyHz(pitchHz);

    // DIST LUT → drive01 (0.0～1.0)
    const float distLutPos =
        (distDurMs > 0.0f)
            ? (noteTimeMs / distDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto distLutIdx =
        std::min(static_cast<int>(distLutPos), EnvelopeLutManager::lutSize - 1);
    oomphOsc.setDist(dLut[static_cast<size_t>(distLutIdx)]);

    // BLEND LUT → blend (-1.0～+1.0)
    const float blendLutPos =
        (blendDurMs > 0.0f)
            ? (noteTimeMs / blendDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto blendLutIdx =
        std::min(static_cast<int>(blendLutPos), EnvelopeLutManager::lutSize - 1);
    oomphOsc.setBlend(bLut[static_cast<size_t>(blendLutIdx)]);

    // AMP LUT → エンベロープゲイン
    const float lutPos =
        (ampDurMs > 0.0f)
            ? (noteTimeMs / ampDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto lutIndex =
        std::min(static_cast<int>(lutPos), EnvelopeLutManager::lutSize - 1);
    const float envGain = ampLut[static_cast<size_t>(lutIndex)];

    const float oscSample = oomphOsc.getNextSample() * gain * envGain;
    oomphScratchBuffer[static_cast<size_t>(sample)] = oscSample;
    if (oomphPass) {
      for (int ch = 0; ch < numChannels; ++ch)
        buffer.addSample(ch, sample, oscSample);
    }
    noteTimeSamples += 1.0f;
  }
}

void BabySquatchAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const auto passes = channelState_.computePasses();

  // Dry ミュート: 入力信号ごと消去（Oomph はこの後加算するので影響なし）
  if (!passes.dry)
    buffer.clear();

  const int numSamples = buffer.getNumSamples();

  // Dry レベル計測（レンダリング前の純粋入力信号）
  using enum ChannelState::Channel;
  if (passes.dry)
    channelState_.detector(dry).process(buffer.getReadPointer(0), numSamples);
  else
    channelState_.detector(dry).process(nullptr, numSamples);

  handleMidiEvents(midiMessages, numSamples);
  renderOomph(buffer, numSamples, passes.oomph);

  // Oomph レベル計測（renderOomph が書いた scratchBuffer から）
  if (passes.oomph)
    channelState_.detector(oomph).process(oomphScratchBuffer.data(),
                                          numSamples);
  else
    channelState_.detector(oomph).process(nullptr, numSamples);

  // Click: 未実装（無音）
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
