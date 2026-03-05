// ClickParams.cpp
// Click panel UI setup / layout
#include "../PluginEditor.h"

namespace {
void styleFilterSlider(juce::Slider &s) {
  s.setSliderStyle(juce::Slider::LinearHorizontal);
  s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 16);
  s.setColour(juce::Slider::backgroundColourId,
              UIConstants::Colours::waveformBg);
  s.setColour(juce::Slider::trackColourId,
              UIConstants::Colours::clickArc.withAlpha(0.45f));
  s.setColour(juce::Slider::thumbColourId, UIConstants::Colours::clickArc);
  s.setColour(juce::Slider::textBoxTextColourId,
              juce::Colours::white.withAlpha(0.90f));
  s.setColour(juce::Slider::textBoxBackgroundColourId,
              UIConstants::Colours::waveformBg);
  s.setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
}

void styleFilterLabel(juce::Label &label, const juce::String &text,
                      const juce::Font &font) {
  label.setText(text, juce::dontSendNotification);
  label.setFont(font);
  label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  label.setJustificationType(juce::Justification::centredRight);
}
void styleKnobLabel(juce::Label &label, const juce::String &text,
                    const juce::Font &font) {
  label.setText(text, juce::dontSendNotification);
  label.setFont(font);
  label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  label.setJustificationType(juce::Justification::centred);
}
void styleClickKnob(juce::Slider &s, ColouredSliderLAF &laf) {
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  s.setLookAndFeel(&laf);
}
} // namespace

void BabySquatchAudioProcessorEditor::setupClickParams() {
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
  clickUI.modeCombo.addItem("Tone", static_cast<int>(ClickUI::Mode::Tone));
  clickUI.modeCombo.addItem("Noise", static_cast<int>(ClickUI::Mode::Noise));
  clickUI.modeCombo.addItem("Sample", static_cast<int>(ClickUI::Mode::Sample));
  clickUI.modeCombo.setSelectedId(static_cast<int>(ClickUI::Mode::Tone),
                                  juce::dontSendNotification);
  clickUI.modeCombo.setLookAndFeel(&darkComboLAF);
  processorRef.clickEngine().setMode(static_cast<int>(ClickUI::Mode::Tone));
  addAndMakeVisible(clickUI.modeCombo);

  // ── Filter param labels ──
  const auto tinyFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));
  styleFilterLabel(clickUI.decayLabel, "Decay:", tinyFont);
  styleKnobLabel(clickUI.freq1Label, "Freq", tinyFont);
  styleKnobLabel(clickUI.focus1Label, "Focus", tinyFont);
  styleKnobLabel(clickUI.freq2Label, "Air", tinyFont);
  styleKnobLabel(clickUI.focus2Label, "Focus", tinyFont);
  styleKnobLabel(clickUI.hpfQLabel, "Reso", tinyFont);
  styleKnobLabel(clickUI.lpfQLabel, "Reso", tinyFont);
  addAndMakeVisible(clickUI.decayLabel);
  addAndMakeVisible(clickUI.freq1Label);
  addAndMakeVisible(clickUI.focus1Label);
  addAndMakeVisible(clickUI.freq2Label);
  addAndMakeVisible(clickUI.focus2Label);
  addAndMakeVisible(clickUI.hpfSlope);
  addAndMakeVisible(clickUI.hpfQLabel);
  addAndMakeVisible(clickUI.lpfSlope);
  addAndMakeVisible(clickUI.lpfQLabel);

  // HPF slope selector
  clickUI.hpfSlope.setOnChange(
      [this](int dboct) { processorRef.clickEngine().setHpfSlope(dboct); });
  // LPF slope selector
  clickUI.lpfSlope.setOnChange(
      [this](int dboct) { processorRef.clickEngine().setLpfSlope(dboct); });

  // ── Sliders ──
  // Decay  1–2000 ms
  styleFilterSlider(clickUI.decaySlider);
  clickUI.decaySlider.setRange(1.0, 2000.0, 1.0);
  clickUI.decaySlider.setTextValueSuffix(" ms");
  clickUI.decaySlider.setValue(50.0, juce::dontSendNotification);
  clickUI.decaySlider.onValueChange = [this] {
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.decaySlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.decaySlider);

  // Note (BPF1 freq)  20–20000 Hz  log
  styleClickKnob(clickUI.freq1Slider, clickKnobLAF);
  clickUI.freq1Slider.setRange(20.0, 20000.0, 1.0);
  clickUI.freq1Slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.freq1Slider.setTextValueSuffix(" Hz");
  clickUI.freq1Slider.setValue(5000.0, juce::dontSendNotification);
  clickUI.freq1Slider.setDoubleClickReturnValue(true, 5000.0);
  clickUI.freq1Slider.onValueChange = [this] {
    processorRef.clickEngine().setFreq1(
        static_cast<float>(clickUI.freq1Slider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.freq1Slider);

  // Focus (BPF1 Q)  0–12
  styleClickKnob(clickUI.focus1Slider, clickKnobLAF);
  clickUI.focus1Slider.setRange(0.0, 12.0, 0.01);
  clickUI.focus1Slider.setValue(0.71, juce::dontSendNotification);
  clickUI.focus1Slider.setDoubleClickReturnValue(true, 0.71);
  clickUI.focus1Slider.onValueChange = [this] {
    processorRef.clickEngine().setFocus1(
        static_cast<float>(clickUI.focus1Slider.getValue()));
  };
  addAndMakeVisible(clickUI.focus1Slider);

  // Air (BPF2 freq)  20–20000 Hz  log
  styleClickKnob(clickUI.freq2Slider, clickKnobLAF);
  clickUI.freq2Slider.setRange(20.0, 20000.0, 1.0);
  clickUI.freq2Slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.freq2Slider.setTextValueSuffix(" Hz");
  clickUI.freq2Slider.setValue(10000.0, juce::dontSendNotification);
  clickUI.freq2Slider.setDoubleClickReturnValue(true, 10000.0);
  clickUI.freq2Slider.onValueChange = [this] {
    processorRef.clickEngine().setFreq2(
        static_cast<float>(clickUI.freq2Slider.getValue()));
  };
  addAndMakeVisible(clickUI.freq2Slider);

  // Focus (BPF2 Q)  0–12
  styleClickKnob(clickUI.focus2Slider, clickKnobLAF);
  clickUI.focus2Slider.setRange(0.0, 12.0, 0.01);
  clickUI.focus2Slider.setValue(0.0, juce::dontSendNotification);
  clickUI.focus2Slider.setDoubleClickReturnValue(true, 0.0);
  clickUI.focus2Slider.onValueChange = [this] {
    processorRef.clickEngine().setFocus2(
        static_cast<float>(clickUI.focus2Slider.getValue()));
  };
  addAndMakeVisible(clickUI.focus2Slider);

  // HPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.hpfSlider, clickKnobLAF);
  clickUI.hpfSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.hpfSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.hpfSlider.setTextValueSuffix(" Hz");
  clickUI.hpfSlider.setValue(1100.0, juce::dontSendNotification);
  clickUI.hpfSlider.setDoubleClickReturnValue(true, 1100.0);
  clickUI.hpfSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfFreq(
        static_cast<float>(clickUI.hpfSlider.getValue()));
  };
  addAndMakeVisible(clickUI.hpfSlider);

  // HPF Reso  0–12
  styleClickKnob(clickUI.hpfQSlider, clickKnobLAF);
  clickUI.hpfQSlider.setRange(0.0, 12.0, 0.01);
  clickUI.hpfQSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.hpfQSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.hpfQSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfQ(
        static_cast<float>(clickUI.hpfQSlider.getValue()));
  };
  addAndMakeVisible(clickUI.hpfQSlider);

  // LPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.lpfSlider, clickKnobLAF);
  clickUI.lpfSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.lpfSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.lpfSlider.setTextValueSuffix(" Hz");
  clickUI.lpfSlider.setValue(8000.0, juce::dontSendNotification);
  clickUI.lpfSlider.setDoubleClickReturnValue(true, 8000.0);
  clickUI.lpfSlider.onValueChange = [this] {
    processorRef.clickEngine().setLpfFreq(
        static_cast<float>(clickUI.lpfSlider.getValue()));
  };
  addAndMakeVisible(clickUI.lpfSlider);

  // LPF Reso  0–12
  styleClickKnob(clickUI.lpfQSlider, clickKnobLAF);
  clickUI.lpfQSlider.setRange(0.0, 12.0, 0.01);
  clickUI.lpfQSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.lpfQSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.lpfQSlider.onValueChange = [this] {
    processorRef.clickEngine().setLpfQ(
        static_cast<float>(clickUI.lpfQSlider.getValue()));
  };
  addAndMakeVisible(clickUI.lpfQSlider);

  // ── Sample モード専用: Pitch / A / D / R ──
  // Pitch: -24 〜 +24 半音
  styleClickKnob(clickUI.pitch.slider, clickKnobLAF);
  clickUI.pitch.slider.setRange(-24.0, 24.0, 1.0);
  clickUI.pitch.slider.setDoubleClickReturnValue(true, 0.0);
  clickUI.pitch.slider.setValue(0.0, juce::dontSendNotification);
  clickUI.pitch.slider.onValueChange = [this] {
    processorRef.clickEngine().setPitchSemitones(
        static_cast<float>(clickUI.pitch.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.pitch.label, "Pitch", tinyFont);
  clickUI.pitch.slider.setVisible(false);
  clickUI.pitch.label.setVisible(false);
  addAndMakeVisible(clickUI.pitch.slider);
  addAndMakeVisible(clickUI.pitch.label);

  // Attack: 0.1–500 ms
  styleClickKnob(clickUI.attack.slider, clickKnobLAF);
  clickUI.attack.slider.setRange(0.1, 500.0, 0.1);
  clickUI.attack.slider.setSkewFactorFromMidPoint(20.0);
  clickUI.attack.slider.setDoubleClickReturnValue(true, 1.0);
  clickUI.attack.slider.setValue(1.0, juce::dontSendNotification);
  clickUI.attack.slider.onValueChange = [this] {
    processorRef.clickEngine().setAttackMs(
        static_cast<float>(clickUI.attack.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.attack.label, "A", tinyFont);
  clickUI.attack.slider.setVisible(false);
  clickUI.attack.label.setVisible(false);
  addAndMakeVisible(clickUI.attack.slider);
  addAndMakeVisible(clickUI.attack.label);

  // Hold (D): 1–2000 ms（ラベルは "D"）
  styleClickKnob(clickUI.hold.slider, clickKnobLAF);
  clickUI.hold.slider.setRange(1.0, 2000.0, 1.0);
  clickUI.hold.slider.setSkewFactorFromMidPoint(200.0);
  clickUI.hold.slider.setDoubleClickReturnValue(true, 200.0);
  clickUI.hold.slider.setValue(200.0, juce::dontSendNotification);
  clickUI.hold.slider.onValueChange = [this] {
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.hold.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.hold.label, "D", tinyFont);
  clickUI.hold.slider.setVisible(false);
  clickUI.hold.label.setVisible(false);
  addAndMakeVisible(clickUI.hold.slider);
  addAndMakeVisible(clickUI.hold.label);

  // Release: 1–1000 ms
  styleClickKnob(clickUI.release.slider, clickKnobLAF);
  clickUI.release.slider.setRange(1.0, 1000.0, 1.0);
  clickUI.release.slider.setSkewFactorFromMidPoint(100.0);
  clickUI.release.slider.setDoubleClickReturnValue(true, 50.0);
  clickUI.release.slider.setValue(50.0, juce::dontSendNotification);
  clickUI.release.slider.onValueChange = [this] {
    processorRef.clickEngine().setReleaseMs(
        static_cast<float>(clickUI.release.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.release.label, "R", tinyFont);
  clickUI.release.slider.setVisible(false);
  clickUI.release.label.setVisible(false);
  addAndMakeVisible(clickUI.release.slider);
  addAndMakeVisible(clickUI.release.label);

  // Sample ロードボタン
  clickUI.sampleLoadButton.setColour(juce::TextButton::buttonColourId,
                                     UIConstants::Colours::panelBg.brighter(0.15f));
  clickUI.sampleLoadButton.setColour(juce::TextButton::textColourOffId,
                                     UIConstants::Colours::labelText);
  clickUI.sampleLoadButton.setVisible(false);
  clickUI.sampleLoadButton.onClick = [this] { onClickSampleLoadClicked(); };
  clickUI.sampleLoadButton.setOnFileDropped(
      [this](const juce::File &file) { onClickSampleFileChosen(file); });
  addAndMakeVisible(clickUI.sampleLoadButton);

  // 起動時デフォルト値を DSP へ反映
  processorRef.clickEngine().setDecayMs(50.0f);
  processorRef.clickEngine().setFreq1(5000.0f);
  processorRef.clickEngine().setFocus1(0.71f);
  processorRef.clickEngine().setFreq2(10000.0f);
  processorRef.clickEngine().setFocus2(0.0f);
  processorRef.clickEngine().setHpfFreq(200.0f);
  processorRef.clickEngine().setHpfQ(0.0f);
  processorRef.clickEngine().setLpfFreq(8000.0f);
  processorRef.clickEngine().setLpfQ(0.0f);
  processorRef.clickEngine().setPitchSemitones(0.0f);
  processorRef.clickEngine().setAttackMs(1.0f);
  processorRef.clickEngine().setReleaseMs(50.0f);

  // ── Click 波形プレビュープロバイダーを EnvelopeCurveEditor に登録 ──
  // モードに応じて Tone（減衰サイン波）/ Noise（±env 帯）を切り替える
  auto toneProvider = [this](float timeSec) {
    const auto decayMs = static_cast<float>(clickUI.decaySlider.getValue());
    const float env = (timeSec * 1000.0f < decayMs)
                          ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
                          : 0.0f;
    const auto freq1 = static_cast<float>(clickUI.freq1Slider.getValue());
    return std::sin(juce::MathConstants<float>::twoPi * freq1 * timeSec) * env;
  };
  auto noiseEnvProvider = [this](float timeSec) {
    const auto decayMs = static_cast<float>(clickUI.decaySlider.getValue());
    return (timeSec * 1000.0f < decayMs)
               ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
               : 0.0f;
  };

  // モード変更時にコントロール表示切替 + プロバイダー更新
  clickUI.modeCombo.onChange = [this, toneProvider, noiseEnvProvider] {
    const int m = clickUI.modeCombo.getSelectedId();
    processorRef.clickEngine().setMode(m);
    const bool isSample = (m == static_cast<int>(ClickUI::Mode::Sample));

    // Tone/Noise 専用コントロールの表示切替
    for (juce::Component *c : {static_cast<juce::Component *>(&clickUI.decayLabel),
                    static_cast<juce::Component *>(&clickUI.decaySlider),
                    static_cast<juce::Component *>(&clickUI.freq1Label),
                    static_cast<juce::Component *>(&clickUI.freq1Slider),
                    static_cast<juce::Component *>(&clickUI.focus1Label),
                    static_cast<juce::Component *>(&clickUI.focus1Slider),
                    static_cast<juce::Component *>(&clickUI.freq2Label),
                    static_cast<juce::Component *>(&clickUI.freq2Slider),
                    static_cast<juce::Component *>(&clickUI.focus2Label),
                    static_cast<juce::Component *>(&clickUI.focus2Slider)})
      c->setVisible(!isSample);

    // Sample 専用コントロールの表示切替
    for (juce::Component *c : {static_cast<juce::Component *>(&clickUI.sampleLoadButton),
                    static_cast<juce::Component *>(&clickUI.pitch.slider),
                    static_cast<juce::Component *>(&clickUI.pitch.label),
                    static_cast<juce::Component *>(&clickUI.attack.slider),
                    static_cast<juce::Component *>(&clickUI.attack.label),
                    static_cast<juce::Component *>(&clickUI.hold.slider),
                    static_cast<juce::Component *>(&clickUI.hold.label),
                    static_cast<juce::Component *>(&clickUI.release.slider),
                    static_cast<juce::Component *>(&clickUI.release.label)})
      c->setVisible(isSample);

    // HPF/LPF Q スライダーの範囲・ラベルを切り替え
    if (isSample) {
      clickUI.hpfQSlider.setRange(0.1, 6.0, 0.01);
      clickUI.hpfQSlider.setValue(0.707, juce::dontSendNotification);
      clickUI.hpfQSlider.textFromValueFunction = [](double v) { return juce::String(v, 2); };
      clickUI.hpfQLabel.setText("Q", juce::dontSendNotification);
      clickUI.lpfQSlider.setRange(0.1, 6.0, 0.01);
      clickUI.lpfQSlider.setValue(0.707, juce::dontSendNotification);
      clickUI.lpfQSlider.textFromValueFunction = [](double v) { return juce::String(v, 2); };
      clickUI.lpfQLabel.setText("Q", juce::dontSendNotification);
      // HPF/LPF 周波数も Direct Sample モードの初期値へ
      clickUI.hpfSlider.setValue(20.0, juce::dontSendNotification);
      clickUI.lpfSlider.setValue(20000.0, juce::dontSendNotification);
      processorRef.clickEngine().setHpfFreq(20.0f);
      processorRef.clickEngine().setHpfQ(0.707f);
      processorRef.clickEngine().setLpfFreq(20000.0f);
      processorRef.clickEngine().setLpfQ(0.707f);
      // Sample モードの A/D/R を DSP へ
      processorRef.clickEngine().setAttackMs(
          static_cast<float>(clickUI.attack.slider.getValue()));
      processorRef.clickEngine().setDecayMs(
          static_cast<float>(clickUI.hold.slider.getValue()));
      processorRef.clickEngine().setReleaseMs(
          static_cast<float>(clickUI.release.slider.getValue()));
      processorRef.clickEngine().setPitchSemitones(
          static_cast<float>(clickUI.pitch.slider.getValue()));
      refreshClickSampleProvider();
    } else {
      clickUI.hpfQSlider.setRange(0.0, 12.0, 0.01);
      clickUI.hpfQSlider.setValue(0.0, juce::dontSendNotification);
      clickUI.hpfQSlider.textFromValueFunction = nullptr;
      clickUI.hpfQLabel.setText("Reso", juce::dontSendNotification);
      clickUI.lpfQSlider.setRange(0.0, 12.0, 0.01);
      clickUI.lpfQSlider.setValue(0.0, juce::dontSendNotification);
      clickUI.lpfQSlider.textFromValueFunction = nullptr;
      clickUI.lpfQLabel.setText("Reso", juce::dontSendNotification);
      // Tone/Noise モードの HPF/LPF 初期値へ戻す
      clickUI.hpfSlider.setValue(1100.0, juce::dontSendNotification);
      clickUI.lpfSlider.setValue(8000.0, juce::dontSendNotification);
      processorRef.clickEngine().setHpfFreq(200.0f);
      processorRef.clickEngine().setHpfQ(0.0f);
      processorRef.clickEngine().setLpfFreq(8000.0f);
      processorRef.clickEngine().setLpfQ(0.0f);
      // Tone/Noise の Decay を DSP へ戻す
      processorRef.clickEngine().setDecayMs(
          static_cast<float>(clickUI.decaySlider.getValue()));
      if (m == static_cast<int>(ClickUI::Mode::Noise))
        envelopeCurveEditor.setClickNoiseEnvProvider(noiseEnvProvider);
      else
        envelopeCurveEditor.setClickPreviewProvider(toneProvider);
    }
    resized();
  };

  // 起動時は Tone モード
  envelopeCurveEditor.setClickPreviewProvider(toneProvider);
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ  |  Sample ロードボタン or (Decay ラベル + スライダー)
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);

  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 72;
  constexpr int colGap = 6;
  constexpr int labelW = 36;

  clickUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  clickUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));
  topRow.removeFromLeft(colGap);

  const bool isSample = (clickUI.modeCombo.getSelectedId() ==
                          static_cast<int>(ClickUI::Mode::Sample));
  if (isSample) {
    clickUI.sampleLoadButton.setBounds(topRow);
  } else {
    clickUI.decayLabel.setBounds(topRow.removeFromLeft(labelW));
    clickUI.decaySlider.setBounds(topRow);
  }

  const int slotW = area.getWidth() / 4;
  const int rowH  = area.getHeight() / 2;

  // 上段4ノブ: Sample → Pitch/A/D/R, Tone/Noise → Freq/Focus/Air/Focus
  const std::array<juce::Slider *, 4> topKnobs =
      isSample ? std::array<juce::Slider *, 4>{{&clickUI.pitch.slider,
                                                 &clickUI.attack.slider,
                                                 &clickUI.hold.slider,
                                                 &clickUI.release.slider}}
               : std::array<juce::Slider *, 4>{{&clickUI.freq1Slider,
                                                 &clickUI.focus1Slider,
                                                 &clickUI.freq2Slider,
                                                 &clickUI.focus2Slider}};
  const std::array<juce::Component *, 4> topLabels =
      isSample ? std::array<juce::Component *, 4>{{&clickUI.pitch.label,
                                                    &clickUI.attack.label,
                                                    &clickUI.hold.label,
                                                    &clickUI.release.label}}
               : std::array<juce::Component *, 4>{{&clickUI.freq1Label,
                                                    &clickUI.focus1Label,
                                                    &clickUI.freq2Label,
                                                    &clickUI.focus2Label}};
  for (int col = 0; col < 4; ++col) {
    const auto idx = static_cast<size_t>(col);
    juce::Rectangle slot(area.getX() + col * slotW, area.getY(), slotW, rowH);
    topLabels[idx]->setBounds(slot.removeFromBottom(14));
    topKnobs[idx]->setBounds(slot);
  }

  // 下段4ノブ: HP(slope) / HPQ / LP(slope) / LPQ（常時）
  const std::array<juce::Slider *, 4> botKnobs = {{
      &clickUI.hpfSlider, &clickUI.hpfQSlider,
      &clickUI.lpfSlider, &clickUI.lpfQSlider,
  }};
  const std::array<juce::Component *, 4> botLabels = {{
      &clickUI.hpfSlope, &clickUI.hpfQLabel,
      &clickUI.lpfSlope, &clickUI.lpfQLabel,
  }};
  for (int col = 0; col < 4; ++col) {
    const auto idx = static_cast<size_t>(col);
    juce::Rectangle slot(area.getX() + col * slotW, area.getY() + rowH, slotW, rowH);
    botLabels[idx]->setBounds(slot.removeFromBottom(14));
    botKnobs[idx]->setBounds(slot);
  }
}
void BabySquatchAudioProcessorEditor::onClickSampleLoadClicked() {
  clickUI.fileChooser = std::make_unique<juce::FileChooser>(
      "Load Sample",
      juce::File::getSpecialLocation(juce::File::userMusicDirectory),
      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  clickUI.fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser &fc) {
        const auto result = fc.getResult();
        if (result.existsAsFile())
          onClickSampleFileChosen(result);
      });
}

void BabySquatchAudioProcessorEditor::onClickSampleFileChosen(
    const juce::File &file) {
  clickUI.loadedFilePath = file.getFullPathName();
  clickUI.sampleLoadButton.setButtonText(file.getFileNameWithoutExtension());
  clickUI.sampleLoadButton.setTooltip(clickUI.loadedFilePath);
  processorRef.clickEngine().sampler().loadSample(file);

  if (!processorRef.clickEngine().sampler().copyThumbnail(clickUI.thumbMin,
                                                          clickUI.thumbMax))
    return;
  clickUI.thumbDurSec = processorRef.clickEngine().sampler().durationSec();
  refreshClickSampleProvider();
}

void BabySquatchAudioProcessorEditor::refreshClickSampleProvider() {
  if (clickUI.thumbMin.empty() || clickUI.thumbDurSec <= 0.0)
    return;

  const auto semitones = static_cast<float>(clickUI.pitch.slider.getValue());
  const float speedRatio = std::pow(2.0f, semitones / 12.0f);
  const double durSec = clickUI.thumbDurSec / static_cast<double>(speedRatio);

  const float attackSec  = static_cast<float>(clickUI.attack.slider.getValue())  / 1000.0f;
  const float holdSec    = static_cast<float>(clickUI.hold.slider.getValue())    / 1000.0f;
  const float releaseSec = static_cast<float>(clickUI.release.slider.getValue()) / 1000.0f;

  auto minPtr = std::make_shared<std::vector<float>>(clickUI.thumbMin);
  auto maxPtr = std::make_shared<std::vector<float>>(clickUI.thumbMax);

  envelopeCurveEditor.setClickPreviewProvider(
      [minPtr, maxPtr, durSec, attackSec, holdSec, releaseSec](float timeSec) {
        float env = 0.0f;
        if (timeSec < attackSec) {
          env = (attackSec > 0.0f) ? timeSec / attackSec : 1.0f;
        } else if (timeSec < attackSec + holdSec) {
          env = 1.0f;
        } else {
          const float rel = timeSec - attackSec - holdSec;
          env = (releaseSec > 0.0f)
                    ? juce::jlimit(0.0f, 1.0f, 1.0f - rel / releaseSec)
                    : 0.0f;
        }
        if (durSec <= 0.0 || timeSec < 0.0f ||
            timeSec >= static_cast<float>(durSec))
          return 0.0f;
        const float t = timeSec / static_cast<float>(durSec);
        const auto n = static_cast<int>(minPtr->size());
        const int idx = juce::jlimit(0, n - 1,
                                     static_cast<int>(t * static_cast<float>(n)));
        return ((*minPtr)[static_cast<std::size_t>(idx)] +
                (*maxPtr)[static_cast<std::size_t>(idx)]) *
               0.5f * env;
      });
}