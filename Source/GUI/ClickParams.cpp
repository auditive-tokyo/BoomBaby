// ClickParams.cpp
// Click panel UI setup / layout
#include "../DSP/Saturator.h"
#include "../ParamIDs.h"
#include "../PluginEditor.h"
#include "LutBaker.h"
#include "WaveformUtils.h"

namespace {

void styleKnobLabel(juce::Label &label, const juce::String &text,
                    const juce::Font &font) {
  label.setText(text, juce::dontSendNotification);
  label.setFont(font);
  label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  label.setJustificationType(juce::Justification::centred);
}
void styleClickKnob(juce::Slider &s, ColouredSliderLAF &laf) {
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 1, 1);
  s.setLookAndFeel(&laf);
}

float computeNoiseEnv(float decayMs, float timeSec) {
  return (timeSec * 1000.0f < decayMs)
             ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
             : 0.0f;
}

} // namespace

void BoomBabyAudioProcessorEditor::clickRepaintOrRefresh() {
  if (clickUI.modeCombo.getSelectedId() ==
      std::to_underlying(ClickUI::Mode::Sample))
    refreshClickSampleProvider();
  else
    envelopeCurveEditor.repaint();
}

void BoomBabyAudioProcessorEditor::setupClickParams() {
  // ── Mode label ──
  const auto smallFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));
  clickUI.modeLabel.setText("mode:", juce::dontSendNotification);
  clickUI.modeLabel.setFont(smallFont);
  clickUI.modeLabel.setColour(juce::Label::textColourId,
                              UIConstants::Colours::labelText);
  clickUI.modeLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(clickUI.modeLabel);

  // ── Mode combo ──
  clickUI.modeCombo.addItem("Noise", std::to_underlying(ClickUI::Mode::Noise));
  clickUI.modeCombo.addItem("Sample",
                            std::to_underlying(ClickUI::Mode::Sample));
  clickUI.modeCombo.setSelectedId(std::to_underlying(ClickUI::Mode::Noise),
                                  juce::dontSendNotification);
  clickUI.modeCombo.setLookAndFeel(&darkComboLAF);
  addAndMakeVisible(clickUI.modeCombo);

  // ── Filter param labels ──
  const auto tinyFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));
  styleKnobLabel(clickUI.noise.decayLabel, "Decay", tinyFont);
  clickUI.noise.bpf1.slopeSelector.setOnChange([this](int dboct) {
    processorRef.clickEngine().setBpf1Slope(dboct);
    constexpr std::array kSlopes = {12, 24, 48};
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == dboct)
        syncParam(ParamIDs::clickBpf1Slope, static_cast<float>(i));
    envelopeCurveEditor.repaint();
  });
  styleKnobLabel(clickUI.noise.bpf1.qLabel, "Q", tinyFont);
  // ClipTypeセレクターはノブ上部ラベルを兼ねるため別途KnobLabel設定不要
  styleKnobLabel(clickUI.hpf.qLabel, "Q", tinyFont);
  styleKnobLabel(clickUI.lpf.qLabel, "Q", tinyFont);
  addAndMakeVisible(clickUI.noise.decayLabel);
  addAndMakeVisible(clickUI.noise.bpf1.slopeSelector);
  addAndMakeVisible(clickUI.noise.bpf1.qLabel);
  addAndMakeVisible(clickUI.hpf.slope);
  addAndMakeVisible(clickUI.hpf.qLabel);
  addAndMakeVisible(clickUI.lpf.slope);
  addAndMakeVisible(clickUI.lpf.qLabel);

  // HPF slope selector
  clickUI.hpf.slope.setOnChange([this](int dboct) {
    processorRef.clickEngine().setHpfSlope(dboct);
    constexpr std::array kSlopes = {12, 24, 48};
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == dboct)
        syncParam(ParamIDs::clickHpfSlope, static_cast<float>(i));
    clickRepaintOrRefresh();
  });
  // LPF slope selector
  clickUI.lpf.slope.setOnChange([this](int dboct) {
    processorRef.clickEngine().setLpfSlope(dboct);
    constexpr std::array kSlopes = {12, 24, 48};
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == dboct)
        syncParam(ParamIDs::clickLpfSlope, static_cast<float>(i));
    clickRepaintOrRefresh();
  });

  // ── Sliders ──
  // Decay  1–2000 ms ロータリーノブ
  styleClickKnob(clickUI.noise.decaySlider, clickKnobLAF);
  clickUI.noise.decaySlider.setRange(1.0, 2000.0, 1.0);
  clickUI.noise.decaySlider.setSkewFactorFromMidPoint(200.0);
  clickUI.noise.decaySlider.setDoubleClickReturnValue(true, 30.0);
  clickUI.noise.decaySlider.setValue(30.0, juce::dontSendNotification);
  clickUI.noise.decaySlider.textFromValueFunction = [](double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " ms"
                      : juce::String(v / 1000.0, 2) + " s";
  };
  clickUI.noise.decaySlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.noise.decaySlider.onValueChange = [this] {
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.noise.decaySlider.getValue()));
    syncParam(ParamIDs::clickNoiseDecay,
              static_cast<float>(clickUI.noise.decaySlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.noise.decaySlider);

  // Note (BPF1 freq)  20–20000 Hz  log
  styleClickKnob(clickUI.noise.bpf1.freqSlider, clickKnobLAF);
  clickUI.noise.bpf1.freqSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.noise.bpf1.freqSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.noise.bpf1.freqSlider.setTextValueSuffix(" Hz");
  clickUI.noise.bpf1.freqSlider.setValue(5000.0, juce::dontSendNotification);
  clickUI.noise.bpf1.freqSlider.setDoubleClickReturnValue(true, 5000.0);
  clickUI.noise.bpf1.freqSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.noise.bpf1.freqSlider.onValueChange = [this] {
    processorRef.clickEngine().setFreq1(
        static_cast<float>(clickUI.noise.bpf1.freqSlider.getValue()));
    syncParam(ParamIDs::clickBpf1Freq,
              static_cast<float>(clickUI.noise.bpf1.freqSlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.noise.bpf1.freqSlider);

  // Q (BPF1 Q)  0.1–18
  styleClickKnob(clickUI.noise.bpf1.qSlider, clickKnobLAF);
  clickUI.noise.bpf1.qSlider.setRange(0.1, 18.0, 0.01);
  clickUI.noise.bpf1.qSlider.setValue(0.71, juce::dontSendNotification);
  clickUI.noise.bpf1.qSlider.setDoubleClickReturnValue(true, 0.71);
  clickUI.noise.bpf1.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  clickUI.noise.bpf1.qSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.noise.bpf1.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setFocus1(
        static_cast<float>(clickUI.noise.bpf1.qSlider.getValue()));
    syncParam(ParamIDs::clickBpf1Q,
              static_cast<float>(clickUI.noise.bpf1.qSlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.noise.bpf1.qSlider);

  // Drive  0–24 dB
  styleClickKnob(clickUI.noise.saturator.driveSlider, clickKnobLAF);
  clickUI.noise.saturator.driveSlider.setRange(0.0, 24.0, 0.1);
  clickUI.noise.saturator.driveSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.noise.saturator.driveSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.noise.saturator.driveSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 1) + " dB";
  };
  clickUI.noise.saturator.driveSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.noise.saturator.driveSlider.onValueChange = [this] {
    processorRef.clickEngine().setDriveDb(
        static_cast<float>(clickUI.noise.saturator.driveSlider.getValue()));
    syncParam(
        ParamIDs::clickDrive,
        static_cast<float>(clickUI.noise.saturator.driveSlider.getValue()));
    clickRepaintOrRefresh();
  };
  addAndMakeVisible(clickUI.noise.saturator.driveSlider);

  // ClipType セレクター（Soft / Hard / Tube）— Drive ノブ上部ラベルを兼ねる
  clickUI.noise.saturator.clipType.setOnChange([this](int t) {
    processorRef.clickEngine().setClipType(t);
    syncParam(ParamIDs::clickClipType, static_cast<float>(t));
    clickRepaintOrRefresh();
  });
  addAndMakeVisible(clickUI.noise.saturator.clipType);

  // HPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.hpf.slider, clickKnobLAF);
  clickUI.hpf.slider.setRange(20.0, 20000.0, 1.0);
  clickUI.hpf.slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.hpf.slider.setTextValueSuffix(" Hz");
  clickUI.hpf.slider.setValue(20.0, juce::dontSendNotification);
  clickUI.hpf.slider.setDoubleClickReturnValue(true, 20.0); // 20Hz = バイパス
  clickUI.hpf.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.hpf.slider.onValueChange = [this] {
    processorRef.clickEngine().setHpfFreq(
        static_cast<float>(clickUI.hpf.slider.getValue()));
    syncParam(ParamIDs::clickHpfFreq,
              static_cast<float>(clickUI.hpf.slider.getValue()));
    clickRepaintOrRefresh();
  };
  addAndMakeVisible(clickUI.hpf.slider);

  // HPF Q  0.1–18
  styleClickKnob(clickUI.hpf.qSlider, clickKnobLAF);
  clickUI.hpf.qSlider.setRange(0.1, 18.0, 0.01);
  clickUI.hpf.qSlider.setValue(0.71, juce::dontSendNotification);
  clickUI.hpf.qSlider.setDoubleClickReturnValue(true, 0.71);
  clickUI.hpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  clickUI.hpf.qSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.hpf.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfQ(
        static_cast<float>(clickUI.hpf.qSlider.getValue()));
    syncParam(ParamIDs::clickHpfQ,
              static_cast<float>(clickUI.hpf.qSlider.getValue()));
    clickRepaintOrRefresh();
  };
  addAndMakeVisible(clickUI.hpf.qSlider);

  // LPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.lpf.slider, clickKnobLAF);
  clickUI.lpf.slider.setRange(20.0, 20000.0, 1.0);
  clickUI.lpf.slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.lpf.slider.setTextValueSuffix(" Hz");
  clickUI.lpf.slider.setValue(20000.0, juce::dontSendNotification);
  clickUI.lpf.slider.setDoubleClickReturnValue(true,
                                               20000.0); // 20000Hz = バイパス
  clickUI.lpf.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.lpf.slider.onValueChange = [this] {
    processorRef.clickEngine().setLpfFreq(
        static_cast<float>(clickUI.lpf.slider.getValue()));
    syncParam(ParamIDs::clickLpfFreq,
              static_cast<float>(clickUI.lpf.slider.getValue()));
    clickRepaintOrRefresh();
  };
  addAndMakeVisible(clickUI.lpf.slider);

  // LPF Q  0.1–18
  styleClickKnob(clickUI.lpf.qSlider, clickKnobLAF);
  clickUI.lpf.qSlider.setRange(0.1, 18.0, 0.01);
  clickUI.lpf.qSlider.setValue(0.71, juce::dontSendNotification);
  clickUI.lpf.qSlider.setDoubleClickReturnValue(true, 0.71);
  clickUI.lpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  clickUI.lpf.qSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.lpf.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setLpfQ(
        static_cast<float>(clickUI.lpf.qSlider.getValue()));
    syncParam(ParamIDs::clickLpfQ,
              static_cast<float>(clickUI.lpf.qSlider.getValue()));
    clickRepaintOrRefresh();
  };
  addAndMakeVisible(clickUI.lpf.qSlider);

  // ── Sample モード専用: Pitch / A / D / R ──
  // Pitch: -24 〜 +24 半音
  styleClickKnob(clickUI.sample.pitch.slider, clickKnobLAF);
  clickUI.sample.pitch.slider.setRange(-24.0, 24.0, 1.0);
  clickUI.sample.pitch.slider.setDoubleClickReturnValue(true, 0.0);
  clickUI.sample.pitch.slider.setValue(0.0, juce::dontSendNotification);
  clickUI.sample.pitch.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.sample.pitch.slider.onValueChange = [this] {
    processorRef.clickEngine().setPitchSemitones(
        static_cast<float>(clickUI.sample.pitch.slider.getValue()));
    syncParam(ParamIDs::clickSamplePitch,
              static_cast<float>(clickUI.sample.pitch.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.sample.pitch.label, "Pitch", tinyFont);
  clickUI.sample.pitch.slider.setVisible(false);
  clickUI.sample.pitch.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.pitch.slider);
  addAndMakeVisible(clickUI.sample.pitch.label);

  // Amp: 0–200%（Sub Amp と同等）
  styleClickKnob(clickUI.sample.amp.slider, clickKnobLAF);
  clickUI.sample.amp.slider.setRange(0.0, 200.0, 0.1);
  clickUI.sample.amp.slider.setDoubleClickReturnValue(true, 100.0);
  clickUI.sample.amp.slider.setValue(100.0, juce::dontSendNotification);
  clickUI.sample.amp.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::clickAmp);
  };
  clickUI.sample.amp.slider.onValueChange = [this] {
    const float v =
        static_cast<float>(clickUI.sample.amp.slider.getValue()) / 100.0f;
    syncParam(ParamIDs::clickSampleAmp,
              static_cast<float>(clickUI.sample.amp.slider.getValue()));
    envDatas.clickAmp.setDefaultValue(v);
    if (!envDatas.clickAmp.isEnvelopeControlled())
      envDatas.clickAmp.setPointValue(0, v);
    bakeLut(envDatas.clickAmp, processorRef.clickEngine().clickAmpLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.repaint();
  };
  // 初期デフォルトポイント（1点：ノブ制御状態）
  envDatas.clickAmp.addPoint(0.0f, envDatas.clickAmp.getDefaultValue());
  {
    const bool controlled = envDatas.clickAmp.isEnvelopeControlled();
    clickUI.sample.amp.slider.setEnabled(!controlled);
    clickUI.sample.amp.slider.setTooltip(
        controlled ? "Click on Amp label to edit envelope" : "");
  }
  styleKnobLabel(clickUI.sample.amp.label, "Amp", tinyFont);
  clickUI.sample.amp.label.addMouseListener(this, false);
  clickUI.sample.amp.slider.setVisible(false);
  clickUI.sample.amp.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.amp.slider);
  addAndMakeVisible(clickUI.sample.amp.label);

  // Sample Decay: エンベロープ LUT の期間を制御（Noiseモードの Decay
  // とは完全別） 初期値 = Sub lengthのデフォルト 300 ms
  styleClickKnob(clickUI.sample.decay.slider, clickKnobLAF);
  clickUI.sample.decay.slider.setRange(10.0, 2000.0, 1.0);
  clickUI.sample.decay.slider.setSkewFactorFromMidPoint(300.0);
  clickUI.sample.decay.slider.setDoubleClickReturnValue(true, 300.0);
  clickUI.sample.decay.slider.setValue(300.0, juce::dontSendNotification);
  clickUI.sample.decay.slider.textFromValueFunction = [](double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " ms"
                      : juce::String(v / 1000.0, 2) + " s";
  };
  clickUI.sample.decay.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  clickUI.sample.decay.slider.onValueChange = [this] {
    const auto durMs =
        static_cast<float>(clickUI.sample.decay.slider.getValue());
    syncParam(ParamIDs::clickSampleDecay, durMs);
    bakeLut(envDatas.clickAmp, processorRef.clickEngine().clickAmpLut(), durMs);
    envelopeCurveEditor.setClickDecayMs(durMs);
  };
  // ※ LUT は syncUIFromState() → onEnvelopeChanged() で正しい値にベイクされる
  styleKnobLabel(clickUI.sample.decay.label, "Decay", tinyFont);
  clickUI.sample.decay.slider.setVisible(false);
  clickUI.sample.decay.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.decay.slider);
  addAndMakeVisible(clickUI.sample.decay.label);

  // Sample ロードボタン
  clickUI.sample.loadButton.setColour(
      juce::TextButton::buttonColourId,
      UIConstants::Colours::panelBg.brighter(0.15f));
  clickUI.sample.loadButton.setColour(juce::TextButton::textColourOffId,
                                      UIConstants::Colours::labelText);
  clickUI.sample.loadButton.setVisible(false);
  clickUI.sample.loadButton.onClick = [this] { onClickSampleLoadClicked(); };
  clickUI.sample.loadButton.setOnFileDropped(
      [this](const juce::File &file) { onClickSampleFileChosen(file); });
  clickUI.sample.loadButton.setOnClear([this] {
    processorRef.clickEngine().sampler().unloadSample();
    clickUI.sample.loadedFilePath.clear();
    clickUI.sample.thumbMin.clear();
    clickUI.sample.thumbMax.clear();
    clickUI.sample.thumbDurSec = 0.0;
    clickUI.sample.loadButton.setButtonText("Drop or Click to Load");
    clickUI.sample.loadButton.setTooltip({});
    clickUI.sample.loadButton.setHasFile(false);
    processorRef.getAPVTS().state.setProperty("clickSamplePath", "", nullptr);
    envelopeCurveEditor.setClickPreviewProvider(nullptr);
  });
  addAndMakeVisible(clickUI.sample.loadButton);

  // ※ DSP デフォルト値の設定は不要 — プロセッサの setStateInformation で
  // 正しい値が既に適用されており、エディタ構築直後の syncUIFromState() で
  // ウィジェット＋DSP が正確に復元される。ここでリセットすると再生中に
  // 一瞬デフォルト値が適用されノイズの原因になる。

  // プレビュープロバイダーを clickUI に保持
  // DSP = BPF(constant-skirt) × Decayエンベロープ なので形状 × 振幅スケーリング
  clickUI.noiseProvider = [this](float timeSec) {
    const auto decayMs =
        static_cast<float>(clickUI.noise.decaySlider.getValue());
    return computeNoiseEnv(decayMs, timeSec) * computeNoiseAmplitudeScale();
  };

  // モード変更時にコントロール表示切替 + プロバイダー更新
  clickUI.modeCombo.onChange = [this] {
    const int m = clickUI.modeCombo.getSelectedId();
    processorRef.clickEngine().setMode(m);
    syncParam(ParamIDs::clickMode, static_cast<float>(m - 1));
    applyClickMode(m);
    resized();
  };

  // 起動時は Noise モード
  envelopeCurveEditor.setClickNoiseEnvProvider(clickUI.noiseProvider);
}

float BoomBabyAudioProcessorEditor::computeNoiseAmplitudeScale() const {
  // ── BPF 振幅: constant-skirt SVF-TPT, 出力 ∝ √(Q × fc) ──
  const auto q = static_cast<float>(clickUI.noise.bpf1.qSlider.getValue());
  const auto freq =
      static_cast<float>(clickUI.noise.bpf1.freqSlider.getValue());
  const int slope = clickUI.noise.bpf1.slopeSelector.getSlope();
  float stages = 1.0f;
  if (slope >= 48)
    stages = 4.0f;
  else if (slope >= 24)
    stages = 2.0f;
  constexpr float kDefaultQxFc = 0.71f * 5000.0f;
  const float bpfScale = std::pow(q * freq / kDefaultQxFc, stages * 0.5f);

  // ── Saturator: DSP では BPF 直後・Decay 前に適用 ──
  // 非線形なので bpfScale ごと Saturator に通す。
  // デフォルト値 (drive=0dB, Soft, bpfScale=1.0) で 1.0 に正規化。
  const auto driveDb =
      static_cast<float>(clickUI.noise.saturator.driveSlider.getValue());
  const int clipType = clickUI.noise.saturator.clipType.getSelected();
  constexpr float kDefaultSat = 0.7615942f; // tanh(1.0)
  const float afterSat =
      Saturator::process(bpfScale, driveDb, clipType) / kDefaultSat;

  // ── HPF / LPF: BPF 中心周波数での SVF-TPT マグニチュード応答 ──
  // Noise のエネルギーは BPF fc 付近に集中しているので、
  // HPF/LPF の fc_bpf でのゲインを掛けるだけで精確な近似。
  const double rawSr = processorRef.getSampleRate();
  const float kSr = rawSr > 0.0 ? static_cast<float>(rawSr) : 44100.0f;
  float filterScale = 1.0f;

  if (const auto hpfFreq = static_cast<float>(clickUI.hpf.slider.getValue());
      hpfFreq > 20.5f) {
    const auto hpfQ = static_cast<float>(clickUI.hpf.qSlider.getValue());
    const int hpfSlope = clickUI.hpf.slope.getSlope();
    float hpfStages = 1.0f;
    if (hpfSlope >= 48)
      hpfStages = 4.0f;
    else if (hpfSlope >= 24)
      hpfStages = 2.0f;
    // SVF-TPT HPF: |H_hp(f)| at f=freq (BPF center)
    const float g = std::tan(juce::MathConstants<float>::pi * hpfFreq / kSr);
    const float gEval = std::tan(juce::MathConstants<float>::pi * freq / kSr);
    const float R = 1.0f / (2.0f * hpfQ);
    // H_hp(z) のマグニチュード: 1 / (1 + 2Rg/gEval + g²/gEval²)
    const float ratio = g / gEval;
    const float mag = 1.0f / (1.0f + 2.0f * R * ratio + ratio * ratio);
    filterScale *= std::pow(mag, hpfStages);
  }

  if (const auto lpfFreq = static_cast<float>(clickUI.lpf.slider.getValue());
      lpfFreq < 19999.5f) {
    const auto lpfQ = static_cast<float>(clickUI.lpf.qSlider.getValue());
    const int lpfSlope = clickUI.lpf.slope.getSlope();
    float lpfStages = 1.0f;
    if (lpfSlope >= 48)
      lpfStages = 4.0f;
    else if (lpfSlope >= 24)
      lpfStages = 2.0f;
    // SVF-TPT LPF: |H_lp(f)| at f=freq (BPF center)
    const float g = std::tan(juce::MathConstants<float>::pi * lpfFreq / kSr);
    const float gEval = std::tan(juce::MathConstants<float>::pi * freq / kSr);
    const float R = 1.0f / (2.0f * lpfQ);
    // H_lp(z) のマグニチュード: g²/gEval² / (1 + 2Rg/gEval + g²/gEval²)
    // → 1 / (gEval²/g² + 2R·gEval/g + 1)
    const float ratio = gEval / g;
    const float mag = 1.0f / (1.0f + 2.0f * R * ratio + ratio * ratio);
    filterScale *= std::pow(mag, lpfStages);
  }

  return afterSat * filterScale;
}

void BoomBabyAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ  |  Sample ロードボタン or (Decay ラベル +
  // スライダー)
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);

  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 72;
  constexpr int colGap = 6;

  clickUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  clickUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));
  topRow.removeFromLeft(colGap);

  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;

  // 上段4ノブ: Sample → Pitch/Amp/Drive/Decay, Noise → BP/Q/Drive/ClipType
  // Noiseモード: Decayは上段ノブ行のスロット3に移動済み—トップ行は何もなし
  if (const bool isSample = (clickUI.modeCombo.getSelectedId() ==
                              std::to_underlying(ClickUI::Mode::Sample));
      isSample) {
    clickUI.sample.loadButton.setBounds(topRow);
    // スロット 0=Pitch, 1=Amp, 2=Drive/ClipType（Noiseと共用）, 3=Decay
    const std::array<juce::Slider *, 4> topKnobs = {
        {&clickUI.sample.pitch.slider, &clickUI.sample.amp.slider,
         &clickUI.noise.saturator.driveSlider, &clickUI.sample.decay.slider}};
    const std::array<juce::Component *, 4> topLabels = {
        {&clickUI.sample.pitch.label, &clickUI.sample.amp.label,
         &clickUI.noise.saturator.clipType, &clickUI.sample.decay.label}};
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY(), slotW, rowH);
      topLabels[idx]->setBounds(slot.removeFromBottom(14));
      topKnobs[idx]->setBounds(slot);
    }
  } else {
    // Noise: スロット 0-2 はノブ+ラベル、スロット 3 は Decay
    const std::array<juce::Slider *, 4> topKnobs = {
        {&clickUI.noise.bpf1.freqSlider, &clickUI.noise.bpf1.qSlider,
         &clickUI.noise.saturator.driveSlider, &clickUI.noise.decaySlider}};
    const std::array<juce::Component *, 4> topLabels = {
        {&clickUI.noise.bpf1.slopeSelector, &clickUI.noise.bpf1.qLabel,
         &clickUI.noise.saturator.clipType,
         &clickUI.noise.decayLabel}}; // clipType が Drive のラベルを兼ねる
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY(), slotW, rowH);
      topLabels[idx]->setBounds(slot.removeFromBottom(14));
      topKnobs[idx]->setBounds(slot);
    }
  }

  // 下段4ノブ: HP(slope) / HPQ / LP(slope) / LPQ（常時）
  const std::array<juce::Slider *, 4> botKnobs = {{
      &clickUI.hpf.slider,
      &clickUI.hpf.qSlider,
      &clickUI.lpf.slider,
      &clickUI.lpf.qSlider,
  }};
  const std::array<juce::Component *, 4> botLabels = {{
      &clickUI.hpf.slope,
      &clickUI.hpf.qLabel,
      &clickUI.lpf.slope,
      &clickUI.lpf.qLabel,
  }};
  for (int col = 0; col < 4; ++col) {
    const auto idx = static_cast<size_t>(col);
    juce::Rectangle slot(area.getX() + col * slotW, area.getY() + rowH, slotW,
                         rowH);
    botLabels[idx]->setBounds(slot.removeFromBottom(14));
    botKnobs[idx]->setBounds(slot);
  }
}
void BoomBabyAudioProcessorEditor::onClickSampleLoadClicked() {
  clickUI.sample.fileChooser = std::make_unique<juce::FileChooser>(
      "Load Sample",
      juce::File::getSpecialLocation(juce::File::userMusicDirectory),
      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  clickUI.sample.fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser &fc) {
        const auto result = fc.getResult();
        if (result.existsAsFile())
          onClickSampleFileChosen(result);
      });
}

void BoomBabyAudioProcessorEditor::setClickModeVisible(bool isSample) {
  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.noise.decayLabel),
        static_cast<juce::Component *>(&clickUI.noise.decaySlider),
        static_cast<juce::Component *>(&clickUI.noise.bpf1.slopeSelector),
        static_cast<juce::Component *>(&clickUI.noise.bpf1.freqSlider),
        static_cast<juce::Component *>(&clickUI.noise.bpf1.qLabel),
        static_cast<juce::Component *>(&clickUI.noise.bpf1.qSlider)})
    c->setVisible(!isSample);

  // Saturator（Drive + ClipType）は両モード共通で常時表示
  clickUI.noise.saturator.driveSlider.setVisible(true);
  clickUI.noise.saturator.clipType.setVisible(true);

  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.sample.loadButton),
        static_cast<juce::Component *>(&clickUI.sample.pitch.slider),
        static_cast<juce::Component *>(&clickUI.sample.pitch.label),
        static_cast<juce::Component *>(&clickUI.sample.amp.slider),
        static_cast<juce::Component *>(&clickUI.sample.amp.label),
        static_cast<juce::Component *>(&clickUI.sample.decay.slider),
        static_cast<juce::Component *>(&clickUI.sample.decay.label)})
    c->setVisible(isSample);
}

// ── モード切替: 共有ウィジェットの状態を保存 ──
void BoomBabyAudioProcessorEditor::ClickUI::saveModeState(
    ModeState &dst) const {
  dst.hpfFreq = hpf.slider.getValue();
  dst.hpfQ = hpf.qSlider.getValue();
  dst.hpfSlope = hpf.slope.getSlope();
  dst.lpfFreq = lpf.slider.getValue();
  dst.lpfQ = lpf.qSlider.getValue();
  dst.lpfSlope = lpf.slope.getSlope();
  dst.drive = noise.saturator.driveSlider.getValue();
  dst.clipType = noise.saturator.clipType.getSelected();
}

// ── モード切替: 共有ウィジェットへ状態を復元し DSP へ反映 ──
void BoomBabyAudioProcessorEditor::ClickUI::restoreModeState(
    const ModeState &src, ClickEngine &eng) {
  hpf.slider.setValue(src.hpfFreq, juce::dontSendNotification);
  hpf.qSlider.setValue(src.hpfQ, juce::dontSendNotification);
  hpf.slope.setSlope(src.hpfSlope);
  lpf.slider.setValue(src.lpfFreq, juce::dontSendNotification);
  lpf.qSlider.setValue(src.lpfQ, juce::dontSendNotification);
  lpf.slope.setSlope(src.lpfSlope);
  noise.saturator.driveSlider.setValue(src.drive, juce::dontSendNotification);
  noise.saturator.clipType.setSelected(src.clipType);

  eng.setHpfFreq(static_cast<float>(src.hpfFreq));
  eng.setHpfQ(static_cast<float>(src.hpfQ));
  eng.setHpfSlope(src.hpfSlope);
  eng.setLpfFreq(static_cast<float>(src.lpfFreq));
  eng.setLpfQ(static_cast<float>(src.lpfQ));
  eng.setLpfSlope(src.lpfSlope);
  eng.setDriveDb(static_cast<float>(src.drive));
  eng.setClipType(src.clipType);
}

void BoomBabyAudioProcessorEditor::applyClickMode(int modeId) {
  if (const bool isSample = (modeId == std::to_underlying(ClickUI::Mode::Sample));
      isSample) {
    // 旧モード（Noise）の共有パラメーターを保存
    clickUI.saveModeState(clickUI.noiseState);
    setClickModeVisible(true);
    // Sample モードの保存値を復元
    clickUI.restoreModeState(clickUI.sampleState, processorRef.clickEngine());
    // Sample 専用パラメーターを DSP へ
    processorRef.clickEngine().setPitchSemitones(
        static_cast<float>(clickUI.sample.pitch.slider.getValue()));
    processorRef.clickEngine().setSampleAmpLevel(
        static_cast<float>(clickUI.sample.amp.slider.getValue()) / 100.0f);
    envelopeCurveEditor.setClickPreviewProvider(nullptr);
    refreshClickSampleProvider();
  } else {
    // 旧モード（Sample）の共有パラメーターを保存
    clickUI.saveModeState(clickUI.sampleState);
    setClickModeVisible(false);
    // Noise モードの保存値を復元
    clickUI.restoreModeState(clickUI.noiseState, processorRef.clickEngine());
    // Noise 専用パラメーターを DSP へ
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.noise.decaySlider.getValue()));
    processorRef.clickEngine().setFreq1(
        static_cast<float>(clickUI.noise.bpf1.freqSlider.getValue()));
    processorRef.clickEngine().setFocus1(
        static_cast<float>(clickUI.noise.bpf1.qSlider.getValue()));
    processorRef.clickEngine().setBpf1Slope(
        clickUI.noise.bpf1.slopeSelector.getSlope());
    if (modeId == std::to_underlying(ClickUI::Mode::Noise))
      envelopeCurveEditor.setClickNoiseEnvProvider(clickUI.noiseProvider);
  }

  // 復元した共有パラメーターを APVTS に反映（DAW
  // 保存時に正しい値が取得されるように）
  syncParam(ParamIDs::clickHpfFreq,
            static_cast<float>(clickUI.hpf.slider.getValue()));
  syncParam(ParamIDs::clickHpfQ,
            static_cast<float>(clickUI.hpf.qSlider.getValue()));
  {
    constexpr std::array kSlopes = {12, 24, 48};
    int idx = 0;
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == clickUI.hpf.slope.getSlope())
        idx = i;
    syncParam(ParamIDs::clickHpfSlope, static_cast<float>(idx));
  }
  syncParam(ParamIDs::clickLpfFreq,
            static_cast<float>(clickUI.lpf.slider.getValue()));
  syncParam(ParamIDs::clickLpfQ,
            static_cast<float>(clickUI.lpf.qSlider.getValue()));
  {
    constexpr std::array kSlopes = {12, 24, 48};
    int idx = 0;
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == clickUI.lpf.slope.getSlope())
        idx = i;
    syncParam(ParamIDs::clickLpfSlope, static_cast<float>(idx));
  }
  syncParam(ParamIDs::clickDrive,
            static_cast<float>(clickUI.noise.saturator.driveSlider.getValue()));
  syncParam(ParamIDs::clickClipType,
            static_cast<float>(clickUI.noise.saturator.clipType.getSelected()));

  // ModeState を ValueTree に永続化
  saveEnvelopesToState();
}

void BoomBabyAudioProcessorEditor::onClickSampleFileChosen(
    const juce::File &file) {
  clickUI.sample.loadedFilePath = file.getFullPathName();
  clickUI.sample.loadButton.setButtonText(file.getFileNameWithoutExtension());
  clickUI.sample.loadButton.setTooltip(clickUI.sample.loadedFilePath);
  clickUI.sample.loadButton.setHasFile(true);
  processorRef.getAPVTS().state.setProperty(
      "clickSamplePath", clickUI.sample.loadedFilePath, nullptr);
  processorRef.clickEngine().sampler().loadSample(file);

  if (!processorRef.clickEngine().sampler().copyThumbnail(
          clickUI.sample.thumbMin, clickUI.sample.thumbMax))
    return;
  clickUI.sample.thumbDurSec =
      processorRef.clickEngine().sampler().durationSec();
  refreshClickSampleProvider();
}

void BoomBabyAudioProcessorEditor::refreshClickSampleProvider() {
  if (clickUI.sample.thumbMin.empty() || clickUI.sample.thumbDurSec <= 0.0)
    return;

  const auto semitones =
      static_cast<float>(clickUI.sample.pitch.slider.getValue());
  const float speedRatio = std::pow(2.0f, semitones / 12.0f);
  const double durSec =
      clickUI.sample.thumbDurSec / static_cast<double>(speedRatio);

  // Drive + ClipType を thumb min/max に適用（Saturator は単調関数なので
  // min/max への直接適用で精確）
  const auto driveDb =
      static_cast<float>(clickUI.noise.saturator.driveSlider.getValue());
  const int clipType = clickUI.noise.saturator.clipType.getSelected();
  const std::size_t n = clickUI.sample.thumbMin.size();
  auto minPtr = std::make_shared<std::vector<float>>(n);
  auto maxPtr = std::make_shared<std::vector<float>>(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*minPtr)[i] =
        Saturator::process(clickUI.sample.thumbMin[i], driveDb, clipType);
    (*maxPtr)[i] =
        Saturator::process(clickUI.sample.thumbMax[i], driveDb, clipType);
  }

  // HPF / LPF を thumb データに適用（DSP: Saturator → HPF → LPF の順）
  const double rawSr = processorRef.getSampleRate();
  const float sr = rawSr > 0.0 ? static_cast<float>(rawSr) : 44100.0f;
  if (const auto hpfFreq = static_cast<float>(clickUI.hpf.slider.getValue());
      hpfFreq > 20.5f) {
    const auto hpfQ = static_cast<float>(clickUI.hpf.qSlider.getValue());
    const int hpfSlope = clickUI.hpf.slope.getSlope();
    int hpfStages = 1;
    if (hpfSlope >= 48)
      hpfStages = 4;
    else if (hpfSlope >= 24)
      hpfStages = 2;
    SvfPassUtils::applySvfPass(*minPtr, hpfFreq, hpfQ, hpfStages, 0, sr);
    SvfPassUtils::applySvfPass(*maxPtr, hpfFreq, hpfQ, hpfStages, 0, sr);
  }
  if (const auto lpfFreq = static_cast<float>(clickUI.lpf.slider.getValue());
      lpfFreq < 19999.5f) {
    const auto lpfQ = static_cast<float>(clickUI.lpf.qSlider.getValue());
    const int lpfSlope = clickUI.lpf.slope.getSlope();
    int lpfStages = 1;
    if (lpfSlope >= 48)
      lpfStages = 4;
    else if (lpfSlope >= 24)
      lpfStages = 2;
    SvfPassUtils::applySvfPass(*minPtr, lpfFreq, lpfQ, lpfStages, 1, sr);
    SvfPassUtils::applySvfPass(*maxPtr, lpfFreq, lpfQ, lpfStages, 1, sr);
  }

  envelopeCurveEditor.setClickPreviewProvider(
      [minPtr, maxPtr, durSec](float timeSec) {
        // A/D/R なし: サンプル全体の実ピーク幅をそのまま返す
        return WaveformUtils::computePreview(*minPtr, *maxPtr, durSec, 0.0f,
                                             static_cast<float>(durSec) + 1.0f,
                                             0.0f, timeSec);
      });
  envelopeCurveEditor.repaint();
}