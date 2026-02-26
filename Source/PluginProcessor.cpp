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
  subOsc.prepareToPlay(sampleRate);
  subScratchBuffer.resize(static_cast<size_t>(samplesPerBlock));
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

  // MIDIバッファを走査 → SubOscillator に通知
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    if (msg.isNoteOn()) {
      subOsc.triggerNote();
      noteTimeSamples = 0.0f; // エンベロープリセット
    }
    // NoteOff は無視: One-shot 長さは subLengthMs で決定
  }
}

void BabySquatchAudioProcessor::renderSub(juce::AudioBuffer<float> &buffer,
                                            int numSamples, bool subPass) {
  if (!subOsc.isActive()) {
    std::fill_n(subScratchBuffer.data(), numSamples, 0.0f);
    return;
  }

  const float gain = juce::Decibels::decibelsToGain(subGainDb.load());
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

  const float lengthMs = subLengthMs.load();
  constexpr float fadeOutMs = 5.0f;
  const float fadeStartMs = std::max(0.0f, lengthMs - fadeOutMs);

  for (int sample = 0; sample < numSamples; ++sample) {
    const float noteTimeMs = noteTimeSamples * 1000.0f / sr;

    // One-shot: Length 到達で自動停止
    if (noteTimeMs >= lengthMs) {
      subOsc.stopNote();
      std::fill_n(subScratchBuffer.data() + sample,
                  static_cast<size_t>(numSamples - sample), 0.0f);
      break;
    }

    // 末尾 fadeout: half-cosine ゲイン (1.0 → 0.0)
    float fadeGain = 1.0f;
    if (noteTimeMs > fadeStartMs && fadeOutMs > 0.0f) {
      const float t = (noteTimeMs - fadeStartMs) / fadeOutMs; // 0..1
      fadeGain = 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
    }

    // Pitch LUT → Hz
    const float pitchLutPos =
        (pitchDurMs > 0.0f)
            ? (noteTimeMs / pitchDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto pitchLutIdx =
        std::min(static_cast<int>(pitchLutPos), EnvelopeLutManager::lutSize - 1);
    const float pitchHz = pLut[static_cast<size_t>(pitchLutIdx)];
    subOsc.setFrequencyHz(pitchHz);

    // DIST LUT → drive01 (0.0～1.0)
    const float distLutPos =
        (distDurMs > 0.0f)
            ? (noteTimeMs / distDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto distLutIdx =
        std::min(static_cast<int>(distLutPos), EnvelopeLutManager::lutSize - 1);
    subOsc.setDist(dLut[static_cast<size_t>(distLutIdx)]);

    // BLEND LUT → blend (-1.0～+1.0)
    const float blendLutPos =
        (blendDurMs > 0.0f)
            ? (noteTimeMs / blendDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto blendLutIdx =
        std::min(static_cast<int>(blendLutPos), EnvelopeLutManager::lutSize - 1);
    subOsc.setBlend(bLut[static_cast<size_t>(blendLutIdx)]);

    // AMP LUT → エンベロープゲイン
    const float lutPos =
        (ampDurMs > 0.0f)
            ? (noteTimeMs / ampDurMs) *
                  static_cast<float>(EnvelopeLutManager::lutSize - 1)
            : 0.0f;
    const auto lutIndex =
        std::min(static_cast<int>(lutPos), EnvelopeLutManager::lutSize - 1);
    const float envGain = ampLut[static_cast<size_t>(lutIndex)];

    const float oscSample = subOsc.getNextSample() * gain * envGain * fadeGain;
    subScratchBuffer[static_cast<size_t>(sample)] = oscSample;
    if (subPass) {
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

  // Direct ミュート: 入力信号ごと消去（Sub はこの後加算するので影響なし）
  if (!passes.direct)
    buffer.clear();

  const int numSamples = buffer.getNumSamples();

  // Direct レベル計測（レンダリング前の純粋入力信号）
  using enum ChannelState::Channel;
  if (passes.direct)
    channelState_.detector(direct).process(buffer.getReadPointer(0), numSamples);
  else
    channelState_.detector(direct).process(nullptr, numSamples);

  handleMidiEvents(midiMessages, numSamples);
  renderSub(buffer, numSamples, passes.sub);

  // Sub レベル計測（renderSub が書いた scratchBuffer から）
  if (passes.sub)
    channelState_.detector(sub).process(subScratchBuffer.data(),
                                          numSamples);
  else
    channelState_.detector(sub).process(nullptr, numSamples);

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
