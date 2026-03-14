#include "PluginProcessor.h"
#include "DSP/EnvelopeData.h"
#include "GUI/LutBaker.h"
#include "ParamIDs.h"
#include "PluginEditor.h"
#include <span>

// ─────────────────────────────────────────────────────────────────────
// APVTS ParameterLayout
// ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
BoomBabyAudioProcessor::createParameterLayout() {
  using FloatParam = juce::AudioParameterFloat;
  using ChoiceParam = juce::AudioParameterChoice;
  using BoolParam = juce::AudioParameterBool;
  using NRange = juce::NormalisableRange<float>;

  // ログスケールのレンジ生成ヘルパー
  auto logRange = [](float min, float max, float mid) {
    auto r = NRange(min, max, 0.01f);
    r.setSkewForCentre(mid);
    return r;
  };
  auto logRangeInt = [](float min, float max, float mid) {
    auto r = NRange(min, max, 1.0f);
    r.setSkewForCentre(mid);
    return r;
  };

  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  // ===================== Sub =====================
  layout.add(
      std::make_unique<ChoiceParam>(ParamIDs::subWaveShape, "Sub Wave",
                                    juce::StringArray{"Tri", "SQR", "SAW"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subLength, "Sub Length",
                                          logRangeInt(10.0f, 2000.0f, 300.0f),
                                          300.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subAmp, "Sub Amp",
                                          NRange(0.0f, 200.0f, 0.1f), 100.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subFreq, "Sub Freq",
                                          logRange(20.0f, 20000.0f, 200.0f),
                                          200.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subMix, "Sub Mix",
                                          NRange(-100.0f, 100.0f, 1.0f), 0.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subSatDrive, "Sub Drive",
                                          NRange(0.0f, 24.0f, 0.1f), 0.0f));
  layout.add(std::make_unique<ChoiceParam>(
      ParamIDs::subSatClipType, "Sub Clip",
      juce::StringArray{"Soft", "Hard", "Tube"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subTone1, "Sub Tone1",
                                          NRange(0.0f, 100.0f, 0.1f), 25.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subTone2, "Sub Tone2",
                                          NRange(0.0f, 100.0f, 0.1f), 25.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subTone3, "Sub Tone3",
                                          NRange(0.0f, 100.0f, 0.1f), 25.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subTone4, "Sub Tone4",
                                          NRange(0.0f, 100.0f, 0.1f), 25.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::subGain, "Sub Gain",
                                          NRange(-60.0f, 12.0f, 0.01f), 0.0f));
  layout.add(std::make_unique<BoolParam>(ParamIDs::subMute, "Sub Mute", false));
  layout.add(std::make_unique<BoolParam>(ParamIDs::subSolo, "Sub Solo", false));

  // ===================== Click =====================
  layout.add(std::make_unique<ChoiceParam>(ParamIDs::clickMode, "Click Mode",
                                           juce::StringArray{"Noise", "Sample"},
                                           0));
  layout.add(
      std::make_unique<FloatParam>(ParamIDs::clickNoiseDecay, "Click Decay",
                                   logRangeInt(1.0f, 2000.0f, 200.0f), 30.0f));
  layout.add(std::make_unique<FloatParam>(
      ParamIDs::clickBpf1Freq, "Click BPF Freq",
      logRangeInt(20.0f, 20000.0f, 1000.0f), 5000.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickBpf1Q, "Click BPF Q",
                                          NRange(0.1f, 18.0f, 0.01f), 0.71f));
  layout.add(
      std::make_unique<ChoiceParam>(ParamIDs::clickBpf1Slope, "Click BPF Slope",
                                    juce::StringArray{"12", "24", "48"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickSamplePitch,
                                          "Click Pitch",
                                          NRange(-24.0f, 24.0f, 1.0f), 0.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickSampleAmp, "Click Amp",
                                          NRange(0.0f, 200.0f, 0.1f), 100.0f));
  layout.add(std::make_unique<FloatParam>(
      ParamIDs::clickSampleDecay, "Click Sample Decay",
      logRangeInt(10.0f, 2000.0f, 300.0f), 300.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickDrive, "Click Drive",
                                          NRange(0.0f, 24.0f, 0.1f), 0.0f));
  layout.add(std::make_unique<ChoiceParam>(
      ParamIDs::clickClipType, "Click Clip",
      juce::StringArray{"Soft", "Hard", "Tube"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickHpfFreq, "Click HPF",
                                          logRangeInt(20.0f, 20000.0f, 1000.0f),
                                          20.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickHpfQ, "Click HPF Q",
                                          NRange(0.1f, 18.0f, 0.01f), 0.71f));
  layout.add(
      std::make_unique<ChoiceParam>(ParamIDs::clickHpfSlope, "Click HPF Slope",
                                    juce::StringArray{"12", "24", "48"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickLpfFreq, "Click LPF",
                                          logRangeInt(20.0f, 20000.0f, 1000.0f),
                                          20000.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickLpfQ, "Click LPF Q",
                                          NRange(0.1f, 18.0f, 0.01f), 0.71f));
  layout.add(
      std::make_unique<ChoiceParam>(ParamIDs::clickLpfSlope, "Click LPF Slope",
                                    juce::StringArray{"12", "24", "48"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::clickGain, "Click Gain",
                                          NRange(-60.0f, 12.0f, 0.01f), 0.0f));
  layout.add(
      std::make_unique<BoolParam>(ParamIDs::clickMute, "Click Mute", false));
  layout.add(
      std::make_unique<BoolParam>(ParamIDs::clickSolo, "Click Solo", false));

  // ===================== Direct =====================
  layout.add(
      std::make_unique<ChoiceParam>(ParamIDs::directMode, "Direct Mode",
                                    juce::StringArray{"Direct", "Sample"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directPitch, "Direct Pitch",
                                          NRange(-24.0f, 24.0f, 1.0f), 0.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directAmp, "Direct Amp",
                                          NRange(0.0f, 200.0f, 0.1f), 100.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directDrive, "Direct Drive",
                                          NRange(0.0f, 24.0f, 0.1f), 0.0f));
  layout.add(std::make_unique<ChoiceParam>(
      ParamIDs::directClipType, "Direct Clip",
      juce::StringArray{"Soft", "Hard", "Tube"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directDecay, "Direct Decay",
                                          logRangeInt(10.0f, 2000.0f, 300.0f),
                                          300.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directHpfFreq, "Direct HPF",
                                          logRangeInt(20.0f, 20000.0f, 1000.0f),
                                          20.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directHpfQ, "Direct HPF Q",
                                          NRange(0.1f, 18.0f, 0.01f), 0.707f));
  layout.add(std::make_unique<ChoiceParam>(
      ParamIDs::directHpfSlope, "Direct HPF Slope",
      juce::StringArray{"12", "24", "48"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directLpfFreq, "Direct LPF",
                                          logRangeInt(20.0f, 20000.0f, 1000.0f),
                                          20000.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directLpfQ, "Direct LPF Q",
                                          NRange(0.1f, 18.0f, 0.01f), 0.707f));
  layout.add(std::make_unique<ChoiceParam>(
      ParamIDs::directLpfSlope, "Direct LPF Slope",
      juce::StringArray{"12", "24", "48"}, 0));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directThreshold,
                                          "Direct Thresh",
                                          NRange(-60.0f, 0.0f, 0.1f), -24.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directHold, "Direct Hold",
                                          NRange(20.0f, 500.0f, 1.0f), 50.0f));
  layout.add(std::make_unique<FloatParam>(ParamIDs::directGain, "Direct Gain",
                                          NRange(-60.0f, 12.0f, 0.01f), 0.0f));
  layout.add(
      std::make_unique<BoolParam>(ParamIDs::directMute, "Direct Mute", false));
  layout.add(
      std::make_unique<BoolParam>(ParamIDs::directSolo, "Direct Solo", false));

  // ===================== Master =====================
  layout.add(std::make_unique<FloatParam>(ParamIDs::masterGain, "Master Gain",
                                          NRange(-60.0f, 12.0f, 0.01f), 0.0f));

  return layout;
}

namespace {
constexpr std::array kAllParamIDs = {
    ParamIDs::subWaveShape,   ParamIDs::subLength,
    ParamIDs::subAmp,         ParamIDs::subFreq,
    ParamIDs::subMix,         ParamIDs::subSatDrive,
    ParamIDs::subSatClipType, ParamIDs::subTone1,
    ParamIDs::subTone2,       ParamIDs::subTone3,
    ParamIDs::subTone4,       ParamIDs::subGain,
    ParamIDs::subMute,        ParamIDs::subSolo,
    ParamIDs::clickMode,      ParamIDs::clickNoiseDecay,
    ParamIDs::clickBpf1Freq,  ParamIDs::clickBpf1Q,
    ParamIDs::clickBpf1Slope, ParamIDs::clickSamplePitch,
    ParamIDs::clickSampleAmp, ParamIDs::clickSampleDecay,
    ParamIDs::clickDrive,     ParamIDs::clickClipType,
    ParamIDs::clickHpfFreq,   ParamIDs::clickHpfQ,
    ParamIDs::clickHpfSlope,  ParamIDs::clickLpfFreq,
    ParamIDs::clickLpfQ,      ParamIDs::clickLpfSlope,
    ParamIDs::clickGain,      ParamIDs::clickMute,
    ParamIDs::clickSolo,      ParamIDs::directMode,
    ParamIDs::directPitch,    ParamIDs::directAmp,
    ParamIDs::directDecay,    ParamIDs::directDrive,
    ParamIDs::directClipType, ParamIDs::directHpfFreq,
    ParamIDs::directHpfQ,     ParamIDs::directHpfSlope,
    ParamIDs::directLpfFreq,  ParamIDs::directLpfQ,
    ParamIDs::directLpfSlope, ParamIDs::directThreshold,
    ParamIDs::directHold,     ParamIDs::directGain,
    ParamIDs::directMute,     ParamIDs::directSolo,
    ParamIDs::masterGain};

/// LUT 駆動パラメータ: DAW Undo/Redo やオートメーション変更時に
/// bakeAllLutsFromState() を再呼出しする必要があるパラメータ群。
constexpr std::array kLutAffectedParamIDs = {
    ParamIDs::subLength,        ParamIDs::subAmp,      ParamIDs::subFreq,
    ParamIDs::subMix,           ParamIDs::subSatDrive, ParamIDs::clickSampleAmp,
    ParamIDs::clickSampleDecay, ParamIDs::directAmp,   ParamIDs::directDecay};
} // namespace

BoomBabyAudioProcessor::BoomBabyAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "BoomBabyState", createParameterLayout()) {
  for (const auto *id : kAllParamIDs)
    apvts_.addParameterListener(id, this);
}

BoomBabyAudioProcessor::~BoomBabyAudioProcessor() {
  // Listener を安全に解除（デストラクタ順序問題を防止）
  for (const auto *id : kAllParamIDs)
    apvts_.removeParameterListener(id, this);
}

// ─────────────────────────────────────────────────────────────────────────
// APVTS パラメータ→DSP 同期
// ─────────────────────────────────────────────────────────────────────────
namespace {
constexpr std::array kSlopes = {12, 24, 48};

void applySubParam(const juce::String &id, float v, int idx, SubEngine &sub,
                   ChannelState &cs) {
  if (id == ParamIDs::subWaveShape)
    sub.oscillator().setWaveShape(static_cast<WaveShape>(idx + 1));
  else if (id == ParamIDs::subLength)
    sub.setLengthMs(v);
  else if (id == ParamIDs::subSatClipType)
    sub.oscillator().setClipType(idx);
  else if (id == ParamIDs::subTone1)
    sub.oscillator().setHarmonicGain(1, v / 100.0f);
  else if (id == ParamIDs::subTone2)
    sub.oscillator().setHarmonicGain(2, v / 100.0f);
  else if (id == ParamIDs::subTone3)
    sub.oscillator().setHarmonicGain(3, v / 100.0f);
  else if (id == ParamIDs::subTone4)
    sub.oscillator().setHarmonicGain(4, v / 100.0f);
  else if (id == ParamIDs::subGain)
    sub.setGainDb(v);
  else if (id == ParamIDs::subMute)
    cs.setMute(ChannelState::Channel::sub, v >= 0.5f);
  else if (id == ParamIDs::subSolo)
    cs.setSolo(ChannelState::Channel::sub, v >= 0.5f);
}

void applyClickParam(const juce::String &id, float v, int idx,
                     ClickEngine &click, ChannelState &cs) {
  if (id == ParamIDs::clickMode)
    click.setMode(idx + 1);
  else if (id == ParamIDs::clickNoiseDecay)
    click.setDecayMs(v);
  else if (id == ParamIDs::clickBpf1Freq)
    click.setFreq1(v);
  else if (id == ParamIDs::clickBpf1Q)
    click.setFocus1(v);
  else if (id == ParamIDs::clickBpf1Slope)
    click.setBpf1Slope(kSlopes[static_cast<std::size_t>(idx)]);
  else if (id == ParamIDs::clickSamplePitch)
    click.setPitchSemitones(v);
  else if (id == ParamIDs::clickSampleAmp)
    click.setSampleAmpLevel(v / 100.0f);
  else if (id == ParamIDs::clickSampleDecay)
    click.setSampleDecayMs(v);
  else if (id == ParamIDs::clickDrive)
    click.setDriveDb(v);
  else if (id == ParamIDs::clickClipType)
    click.setClipType(idx);
  else if (id == ParamIDs::clickHpfFreq)
    click.setHpfFreq(v);
  else if (id == ParamIDs::clickHpfQ)
    click.setHpfQ(v);
  else if (id == ParamIDs::clickHpfSlope)
    click.setHpfSlope(kSlopes[static_cast<std::size_t>(idx)]);
  else if (id == ParamIDs::clickLpfFreq)
    click.setLpfFreq(v);
  else if (id == ParamIDs::clickLpfQ)
    click.setLpfQ(v);
  else if (id == ParamIDs::clickLpfSlope)
    click.setLpfSlope(kSlopes[static_cast<std::size_t>(idx)]);
  else if (id == ParamIDs::clickGain)
    click.setGainDb(v);
  else if (id == ParamIDs::clickMute)
    cs.setMute(ChannelState::Channel::click, v >= 0.5f);
  else if (id == ParamIDs::clickSolo)
    cs.setSolo(ChannelState::Channel::click, v >= 0.5f);
}

void applyDirectParam(const juce::String &id, float v, int idx,
                      DirectEngine &direct, TransientDetector &td,
                      ChannelState &cs) {
  if (id == ParamIDs::directPitch)
    direct.setPitchSemitones(v);
  else if (id == ParamIDs::directDecay)
    direct.setMaxDurationMs(v);
  else if (id == ParamIDs::directDrive)
    direct.setDriveDb(v);
  else if (id == ParamIDs::directClipType)
    direct.setClipType(idx);
  else if (id == ParamIDs::directHpfFreq)
    direct.setHpfFreq(v);
  else if (id == ParamIDs::directHpfQ)
    direct.setHpfQ(v);
  else if (id == ParamIDs::directHpfSlope)
    direct.setHpfSlope(kSlopes[static_cast<std::size_t>(idx)]);
  else if (id == ParamIDs::directLpfFreq)
    direct.setLpfFreq(v);
  else if (id == ParamIDs::directLpfQ)
    direct.setLpfQ(v);
  else if (id == ParamIDs::directLpfSlope)
    direct.setLpfSlope(kSlopes[static_cast<std::size_t>(idx)]);
  else if (id == ParamIDs::directThreshold)
    td.setThresholdDb(v);
  else if (id == ParamIDs::directHold)
    td.setHoldMs(v);
  else if (id == ParamIDs::directGain)
    direct.setGainDb(v);
  else if (id == ParamIDs::directMute)
    cs.setMute(ChannelState::Channel::direct, v >= 0.5f);
  else if (id == ParamIDs::directSolo)
    cs.setSolo(ChannelState::Channel::direct, v >= 0.5f);
}

/// APVTS ValueTree の ENVELOPE ノードから EnvelopeData を復元。
/// 保存データがなければ apvtsDefault で 1 点エンベロープを返す。
EnvelopeData restoreEnvFromState(const juce::ValueTree &state, const char *name,
                                 float apvtsDefault) {
  // 対象の ENVELOPE ノードを検索
  juce::ValueTree child;
  for (int i = 0; i < state.getNumChildren(); ++i) {
    auto c = state.getChild(i);
    if (c.hasType(juce::Identifier{"ENVELOPE"}) &&
        c.getProperty(juce::Identifier{"name"}).toString() ==
            juce::String(name)) {
      child = c;
      break;
    }
  }

  if (!child.isValid()) {
    EnvelopeData env;
    env.setDefaultValue(apvtsDefault);
    env.addPoint(0.0f, apvtsDefault);
    return env;
  }

  EnvelopeData env;
  env.setDefaultValue(static_cast<float>(
      child.getProperty(juce::Identifier{"defaultValue"}, apvtsDefault)));
  env.clearPoints();
  for (int j = 0; j < child.getNumChildren(); ++j) {
    auto pt = child.getChild(j);
    if (!pt.hasType(juce::Identifier{"POINT"}))
      continue;
    const float t = pt.getProperty(juce::Identifier{"timeMs"}, 0.0f);
    const float v = pt.getProperty(juce::Identifier{"value"}, 1.0f);
    const float c = pt.getProperty(juce::Identifier{"curve"}, 0.0f);
    env.addPoint(t, v);
    env.setSegmentCurve(static_cast<int>(env.getPoints().size()) - 1, c);
  }
  if (!env.hasPoints())
    env.addPoint(0.0f, env.getDefaultValue());
  return env;
}

} // namespace

void BoomBabyAudioProcessor::parameterChanged(const juce::String &parameterID,
                                              float newValue) {
  const auto v = newValue;
  const auto idx = static_cast<int>(v);

  if (parameterID.startsWith("sub_"))
    applySubParam(parameterID, v, idx, subEngine_, channelState_);
  else if (parameterID == ParamIDs::directMode)
    setDirectSampleMode(idx == 1);
  else if (parameterID.startsWith("click_"))
    applyClickParam(parameterID, v, idx, clickEngine_, channelState_);
  else if (parameterID.startsWith("direct_"))
    applyDirectParam(parameterID, v, idx, directEngine_,
                     directMode_.transientDetector_, channelState_);
  else if (parameterID == ParamIDs::masterGain)
    master_.setGain(v);

  // LUT 駆動パラメータは DAW Undo/Redo 時にも再ベイクが必要
  for (const auto *p : kLutAffectedParamIDs) {
    if (parameterID == p) {
      bakeAllLutsFromState();
      break;
    }
  }
}

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
  // sampleMode_ の初期値（false = Input モード）に合わせて passthroughMode_
  // を同期。 未同期のまま triggerNote() が呼ばれるとサンプル未ロード判定で
  // early return し active_ が立たず、renderPassthrough が amp=0
  // で無音になるのを防ぐ。
  directEngine_.setPassthroughMode(!directMode_.sampleMode_.load());
  channelState_.resetDetectors();

  directMode_.transientDetector_.prepare(sampleRate);
  directMode_.transientDetector_.setThresholdDb(-24.0f);
  directMode_.transientDetector_.setHoldMs(50.0f);
  monoMixBuffer_.resize(static_cast<std::size_t>(samplesPerBlock));

  inputMonitor_.data_.assign(static_cast<std::size_t>(InputMonitor::kCapacity),
                             0.0f);
  inputMonitor_.fifo_.reset();

  // 全エンジン初期化後に APVTS 値で DSP を復元。
  // setStateInformation が先に呼ばれても prepareToPlay のハードコード値に
  // 上書きされるため、ここで改めて全パラメータを適用する。
  for (const auto *id : kAllParamIDs)
    parameterChanged(id, apvts_.getRawParameterValue(id)->load());

  // prepareToPlay の reset() で白紙になった LUT を再ベイク
  bakeAllLutsFromState();
}

void BoomBabyAudioProcessor::setDirectSampleMode(bool isSample) noexcept {
  directMode_.sampleMode_.store(isSample);
  directEngine_.setPassthroughMode(!isSample);
  // パススルーモード時はトランジェント検出を自動有効化
  directMode_.transientDetector_.setEnabled(!isSample);
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

  if (dm.transientDetector_.isEnabled()) {
    const int pos = dm.transientDetector_.process(
        std::span<const float>(mono, static_cast<std::size_t>(numSamples)));
    if (pos >= 0) {
      eng.sub.triggerNote(pos);
      eng.click.triggerNote(pos);
      eng.direct.triggerNote(pos);
    }
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

/// MIDI ノートオン → 各エンジン triggerNote
void handleMidiEvents(juce::MidiKeyboardState &kbState, SubEngine &sub,
                      ClickEngine &click, DirectEngine &direct,
                      juce::MidiBuffer &midiMessages, int numSamples) {
  kbState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
  for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    if (msg.isNoteOn()) {
      const int offset = metadata.samplePosition;
      sub.triggerNote(offset);
      click.triggerNote(offset);
      direct.triggerNote(offset);
    }
  }
}

} // namespace

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

  handleMidiEvents(keyboardState, subEngine_, clickEngine_, directEngine_,
                   midiMessages, numSamples);
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

void BoomBabyAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = apvts_.copyState();
  auto xml = state.createXml();
  copyXmlToBinary(*xml, destData);
}

void BoomBabyAudioProcessor::setStateInformation(
    const void *data, // NOSONAR: JUCE API
    int sizeInBytes) {
  auto xml = getXmlFromBinary(data, sizeInBytes);
  if (!xml || !xml->hasTagName(apvts_.state.getType()))
    return;

  apvts_.replaceState(juce::ValueTree::fromXml(*xml));

  // replaceState は parameterChanged を発火しないため、
  // 全パラメータの DSP セッターを手動で呼び出す
  for (const auto *id : kAllParamIDs)
    parameterChanged(id, apvts_.getRawParameterValue(id)->load());

  // エンベロープ LUT を保存済み状態から再ベイク
  bakeAllLutsFromState();

  // サンプルファイルを復元（エディタ無しでも音が出るように）
  if (const auto clickPath =
          apvts_.state.getProperty("clickSamplePath").toString();
      clickPath.isNotEmpty()) {
    const juce::File f{clickPath};
    if (f.existsAsFile())
      clickEngine_.sampler().loadSample(f);
  }
  if (const auto directPath =
          apvts_.state.getProperty("directSamplePath").toString();
      directPath.isNotEmpty()) {
    const juce::File f{directPath};
    if (f.existsAsFile())
      directEngine_.sampler().loadSample(f);
  }
}

void BoomBabyAudioProcessor::bakeAllLutsFromState() {
  const float subLenMs =
      apvts_.getRawParameterValue(ParamIDs::subLength)->load();
  const float directDecayMs =
      apvts_.getRawParameterValue(ParamIDs::directDecay)->load();

  auto env = [&](const char *name, float def) {
    return restoreEnvFromState(apvts_.state, name, def);
  };

  const auto load = [&](const char *id) {
    return apvts_.getRawParameterValue(id)->load();
  };

  // LUT → DSP 単位への変換は Editor 側の knob コールバックと対称にする
  // Sub LUT: エンベロープ実効区間に 512 点を集中させる
  const auto ampEnv = env("amp", load(ParamIDs::subAmp) / 100.0f);
  const auto freqEnv = env("freq", load(ParamIDs::subFreq));
  const auto distEnv = env("dist", load(ParamIDs::subSatDrive) / 24.0f);
  const auto mixEnv = env("mix", load(ParamIDs::subMix) / 100.0f);
  bakeLut(ampEnv, subEngine_.envLut(), effectiveLutDuration(ampEnv, subLenMs));
  bakeLut(freqEnv, subEngine_.freqLut(),
          effectiveLutDuration(freqEnv, subLenMs));
  bakeLut(distEnv, subEngine_.distLut(),
          effectiveLutDuration(distEnv, subLenMs));
  bakeLut(mixEnv, subEngine_.mixLut(), effectiveLutDuration(mixEnv, subLenMs));
  const float clickDecayMs =
      apvts_.getRawParameterValue(ParamIDs::clickSampleDecay)->load();
  const auto clickAmpEnv =
      env("clickAmp", load(ParamIDs::clickSampleAmp) / 100.0f);
  bakeLut(clickAmpEnv, clickEngine_.clickAmpLut(),
          effectiveLutDuration(clickAmpEnv, clickDecayMs));
  const auto directAmpEnv =
      env("directAmp", load(ParamIDs::directAmp) / 100.0f);
  bakeLut(directAmpEnv, directEngine_.directAmpLut(),
          effectiveLutDuration(directAmpEnv, directDecayMs));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new BoomBabyAudioProcessor(); // NOSONAR: DAW host takes ownership
}
