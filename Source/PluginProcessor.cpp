#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <span>

BoomBabyAudioProcessor::BoomBabyAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

BoomBabyAudioProcessor::~BoomBabyAudioProcessor() = default;

const juce::String // NOSONAR: JUCE API
BoomBabyAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool BoomBabyAudioProcessor::acceptsMidi() const { return true; }

bool BoomBabyAudioProcessor::producesMidi() const { return false; }

bool BoomBabyAudioProcessor::isMidiEffect() const { return false; }

double BoomBabyAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BoomBabyAudioProcessor::getNumPrograms() { return 1; }

int BoomBabyAudioProcessor::getCurrentProgram() { return 0; }

void BoomBabyAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String // NOSONAR: JUCE API
BoomBabyAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void BoomBabyAudioProcessor::changeProgramName(int index,
                                                  const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

void BoomBabyAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  subEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  clickEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  directEngine_.prepareToPlay(sampleRate, samplesPerBlock);
  channelState_.resetDetectors();

  directMode_.transientDetector_.prepare(sampleRate);
  directMode_.transientDetector_.setThresholdDb(-24.0f);
  directMode_.transientDetector_.setHoldMs(50.0f);
  monoMixBuffer_.resize(static_cast<std::size_t>(samplesPerBlock));

  inputMonitor_.data_.assign(static_cast<std::size_t>(InputMonitor::kCapacity),
                             0.0f);
  inputMonitor_.fifo_.reset();
}

void BoomBabyAudioProcessor::setDirectSampleMode(bool isSample) noexcept {
  directMode_.sampleMode_.store(isSample);
  directEngine_.setPassthroughMode(!isSample);
}

void BoomBabyAudioProcessor::releaseResources() {
  // Currently no resources to release - will be populated when adding DSP
  // processing
}

bool BoomBabyAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
    return false;

  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

// ─────────────────────────────────────────────────────────────────────────
// processBlock ヘルパー（フリー関数: クラスメソッドカウントに含まれない）
// 将来の拡張（input gain, FFT, latency comp, LUFS 等）は各関数へ追記する
// ─────────────────────────────────────────────────────────────────────────
namespace {

/// 3エンジンをまとめる集約（パラメータ数削減のため）
struct EngineRefs {
  SubEngine &sub;
  ClickEngine &click;
  DirectEngine &direct;
};

/// パススルーモード時: モノミックス → トランジェント検出 → FIFO 供給
void processPassthroughMonitor(BoomBabyAudioProcessor::DirectMode &dm,
                               BoomBabyAudioProcessor::InputMonitor &im,
                               EngineRefs eng,
                               const juce::AudioBuffer<float> &buffer,
                               std::vector<float> &monoMixBuf, int numSamples) {
  if (dm.sampleMode_.load() || buffer.getNumChannels() == 0)
    return;

  auto *mono = monoMixBuf.data();
  const float *ch0 = buffer.getReadPointer(0);
  if (buffer.getNumChannels() >= 2) {
    const float *ch1 = buffer.getReadPointer(1);
    for (int i = 0; i < numSamples; ++i)
      mono[i] = (ch0[i] + ch1[i]) * 0.5f;
  } else {
    std::copy_n(ch0, numSamples, mono);
  }

  if (dm.transientDetector_.isEnabled() &&
      dm.transientDetector_.process(
          std::span<const float>(mono, static_cast<std::size_t>(numSamples)))) {
    eng.sub.triggerNote();
    eng.click.triggerNote();
    eng.direct.triggerNote();
  }

  const int toWrite = juce::jmin(numSamples, im.fifo_.getFreeSpace());
  if (toWrite > 0) {
    int s1;
    int sz1;
    int s2;
    int sz2;
    im.fifo_.prepareToWrite(toWrite, s1, sz1, s2, sz2);
    if (sz1 > 0)
      std::copy_n(mono, sz1, im.data_.data() + s1);
    if (sz2 > 0)
      std::copy_n(mono + sz1, sz2, im.data_.data() + s2);
    im.fifo_.finishedWrite(sz1 + sz2);
  }
}

/// Direct エンジンのパススルー vs サンプルモード呼び分け
void renderDirectEngine(const BoomBabyAudioProcessor::DirectMode &dm,
                        const ChannelState::Passes &passes,
                        DirectEngine &directEng,
                        const std::vector<float> &monoMixBuf,
                        juce::AudioBuffer<float> &buffer, int numSamples,
                        double sr) {
  if (!dm.sampleMode_.load() && passes.direct) {
    directEng.renderPassthrough(
        buffer,
        std::span<const float>(monoMixBuf.data(),
                               static_cast<std::size_t>(numSamples)),
        numSamples, sr);
  } else {
    directEng.render(buffer, numSamples, passes.direct, sr);
  }
}

/// マスター L/R + 各チャンネルのレベル計測
void measureChannelLevels(const ChannelState::Passes &passes,
                          BoomBabyAudioProcessor::MasterSection &master,
                          ChannelState &channelState, EngineRefs eng,
                          const juce::AudioBuffer<float> &buffer,
                          int numSamples) {
  for (std::size_t ch = 0; ch < 2; ++ch) {
    const int bufCh =
        juce::jmin(static_cast<int>(ch), buffer.getNumChannels() - 1);
    master.detector_[ch].process(
        bufCh >= 0 ? buffer.getReadPointer(bufCh) : nullptr, numSamples);
  }

  using enum ChannelState::Channel;
  channelState.detector(sub).process(
      passes.sub ? eng.sub.scratchData() : nullptr, numSamples);
  channelState.detector(click).process(
      passes.click ? eng.click.scratchData() : nullptr, numSamples);
  channelState.detector(direct).process(
      passes.direct ? eng.direct.scratchData() : nullptr, numSamples);
}

} // namespace

void BoomBabyAudioProcessor::handleMidiEvents(juce::MidiBuffer &midiMessages,
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

void BoomBabyAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const auto passes = channelState_.computePasses();
  const int numSamples = buffer.getNumSamples();
  const double sr = getSampleRate();

  // パススルーモード: モノミックス / トランジェント検出 / FIFO 供給
  processPassthroughMonitor(directMode_, inputMonitor_,
                            {subEngine_, clickEngine_, directEngine_}, buffer,
                            monoMixBuffer_, numSamples);

  // Direct ミュート: 入力信号を消去（Sub はこの後加算。renderPassthrough も
  // addSample）
  buffer.clear();

  handleMidiEvents(midiMessages, numSamples);
  subEngine_.render(buffer, numSamples, passes.sub, sr);
  clickEngine_.render(buffer, numSamples, passes.click, sr);
  renderDirectEngine(directMode_, passes, directEngine_, monoMixBuffer_, buffer,
                     numSamples, sr);

  // マスターゲイン適用
  buffer.applyGain(juce::Decibels::decibelsToGain(master_.gainDb_.load()));

  measureChannelLevels(passes, master_, channelState_,
                       {subEngine_, clickEngine_, directEngine_}, buffer,
                       numSamples);
}

bool BoomBabyAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *BoomBabyAudioProcessor::createEditor() {
  // clang-format off
  return new BoomBabyAudioProcessorEditor(*this); // NOSONAR: DAW host takes ownership
  // clang-format on
}

void BoomBabyAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  juce::ignoreUnused(destData);
}

void BoomBabyAudioProcessor::setStateInformation(
    const void *data, // NOSONAR: JUCE API
    int sizeInBytes) {
  juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BoomBabyAudioProcessor(); // NOSONAR: DAW host takes ownership
}
