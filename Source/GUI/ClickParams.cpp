// ClickParams.cpp
// Click panel UI setup / layout
#include "../PluginEditor.h"

namespace {

std::pair<float, float> computeClickPreview(const std::vector<float> &thumbMin,
                                            const std::vector<float> &thumbMax,
                                            double durSec, float attackSec,
                                            float holdSec, float releaseSec,
                                            float timeSec) {
  // A → Hold → R エンベロープ
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
  if (durSec <= 0.0 || timeSec < 0.0f || timeSec >= static_cast<float>(durSec))
    return {0.0f, 0.0f};
  const float t = timeSec / static_cast<float>(durSec);
  const auto n = static_cast<int>(thumbMin.size());
  const int idx =
      juce::jlimit(0, n - 1, static_cast<int>(t * static_cast<float>(n)));
  return {thumbMin[static_cast<std::size_t>(idx)] * env,
          thumbMax[static_cast<std::size_t>(idx)] * env};
}

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
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 1, 1);
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
  styleFilterLabel(clickUI.toneNoise.decayLabel, "Decay:", tinyFont);
  styleKnobLabel(clickUI.toneNoise.bpf1.freqLabel, "Freq", tinyFont);
  styleKnobLabel(clickUI.toneNoise.bpf1.qLabel, "Focus", tinyFont);
  styleKnobLabel(clickUI.toneNoise.bpf2.freqLabel, "Air", tinyFont);
  styleKnobLabel(clickUI.toneNoise.bpf2.qLabel, "Focus", tinyFont);
  styleKnobLabel(clickUI.hpf.qLabel, "Reso", tinyFont);
  styleKnobLabel(clickUI.lpf.qLabel, "Reso", tinyFont);
  addAndMakeVisible(clickUI.toneNoise.decayLabel);
  addAndMakeVisible(clickUI.toneNoise.bpf1.freqLabel);
  addAndMakeVisible(clickUI.toneNoise.bpf1.qLabel);
  addAndMakeVisible(clickUI.toneNoise.bpf2.freqLabel);
  addAndMakeVisible(clickUI.toneNoise.bpf2.qLabel);
  addAndMakeVisible(clickUI.hpf.slope);
  addAndMakeVisible(clickUI.hpf.qLabel);
  addAndMakeVisible(clickUI.lpf.slope);
  addAndMakeVisible(clickUI.lpf.qLabel);

  // HPF slope selector
  clickUI.hpf.slope.setOnChange(
      [this](int dboct) { processorRef.clickEngine().setHpfSlope(dboct); });
  // LPF slope selector
  clickUI.lpf.slope.setOnChange(
      [this](int dboct) { processorRef.clickEngine().setLpfSlope(dboct); });

  // ── Sliders ──
  // Decay  1–2000 ms
  styleFilterSlider(clickUI.toneNoise.decaySlider);
  clickUI.toneNoise.decaySlider.setRange(1.0, 2000.0, 1.0);
  clickUI.toneNoise.decaySlider.setTextValueSuffix(" ms");
  clickUI.toneNoise.decaySlider.setValue(50.0, juce::dontSendNotification);
  clickUI.toneNoise.decaySlider.onValueChange = [this] {
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.toneNoise.decaySlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.toneNoise.decaySlider);

  // Note (BPF1 freq)  20–20000 Hz  log
  styleClickKnob(clickUI.toneNoise.bpf1.freqSlider, clickKnobLAF);
  clickUI.toneNoise.bpf1.freqSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.toneNoise.bpf1.freqSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.toneNoise.bpf1.freqSlider.setTextValueSuffix(" Hz");
  clickUI.toneNoise.bpf1.freqSlider.setValue(5000.0,
                                             juce::dontSendNotification);
  clickUI.toneNoise.bpf1.freqSlider.setDoubleClickReturnValue(true, 5000.0);
  clickUI.toneNoise.bpf1.freqSlider.onValueChange = [this] {
    processorRef.clickEngine().setFreq1(
        static_cast<float>(clickUI.toneNoise.bpf1.freqSlider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.toneNoise.bpf1.freqSlider);

  // Focus (BPF1 Q)  0–12
  styleClickKnob(clickUI.toneNoise.bpf1.qSlider, clickKnobLAF);
  clickUI.toneNoise.bpf1.qSlider.setRange(0.0, 12.0, 0.01);
  clickUI.toneNoise.bpf1.qSlider.setValue(0.71, juce::dontSendNotification);
  clickUI.toneNoise.bpf1.qSlider.setDoubleClickReturnValue(true, 0.71);
  clickUI.toneNoise.bpf1.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setFocus1(
        static_cast<float>(clickUI.toneNoise.bpf1.qSlider.getValue()));
  };
  addAndMakeVisible(clickUI.toneNoise.bpf1.qSlider);

  // Air (BPF2 freq)  20–20000 Hz  log
  styleClickKnob(clickUI.toneNoise.bpf2.freqSlider, clickKnobLAF);
  clickUI.toneNoise.bpf2.freqSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.toneNoise.bpf2.freqSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.toneNoise.bpf2.freqSlider.setTextValueSuffix(" Hz");
  clickUI.toneNoise.bpf2.freqSlider.setValue(10000.0,
                                             juce::dontSendNotification);
  clickUI.toneNoise.bpf2.freqSlider.setDoubleClickReturnValue(true, 10000.0);
  clickUI.toneNoise.bpf2.freqSlider.onValueChange = [this] {
    processorRef.clickEngine().setFreq2(
        static_cast<float>(clickUI.toneNoise.bpf2.freqSlider.getValue()));
  };
  addAndMakeVisible(clickUI.toneNoise.bpf2.freqSlider);

  // Focus (BPF2 Q)  0–12
  styleClickKnob(clickUI.toneNoise.bpf2.qSlider, clickKnobLAF);
  clickUI.toneNoise.bpf2.qSlider.setRange(0.0, 12.0, 0.01);
  clickUI.toneNoise.bpf2.qSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.toneNoise.bpf2.qSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.toneNoise.bpf2.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setFocus2(
        static_cast<float>(clickUI.toneNoise.bpf2.qSlider.getValue()));
  };
  addAndMakeVisible(clickUI.toneNoise.bpf2.qSlider);

  // HPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.hpf.slider, clickKnobLAF);
  clickUI.hpf.slider.setRange(20.0, 20000.0, 1.0);
  clickUI.hpf.slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.hpf.slider.setTextValueSuffix(" Hz");
  clickUI.hpf.slider.setValue(1100.0, juce::dontSendNotification);
  clickUI.hpf.slider.setDoubleClickReturnValue(true, 1100.0);
  clickUI.hpf.slider.onValueChange = [this] {
    processorRef.clickEngine().setHpfFreq(
        static_cast<float>(clickUI.hpf.slider.getValue()));
  };
  addAndMakeVisible(clickUI.hpf.slider);

  // HPF Reso  0–12
  styleClickKnob(clickUI.hpf.qSlider, clickKnobLAF);
  clickUI.hpf.qSlider.setRange(0.0, 12.0, 0.01);
  clickUI.hpf.qSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.hpf.qSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.hpf.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfQ(
        static_cast<float>(clickUI.hpf.qSlider.getValue()));
  };
  addAndMakeVisible(clickUI.hpf.qSlider);

  // LPF freq  20–20000 Hz  log
  styleClickKnob(clickUI.lpf.slider, clickKnobLAF);
  clickUI.lpf.slider.setRange(20.0, 20000.0, 1.0);
  clickUI.lpf.slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.lpf.slider.setTextValueSuffix(" Hz");
  clickUI.lpf.slider.setValue(8000.0, juce::dontSendNotification);
  clickUI.lpf.slider.setDoubleClickReturnValue(true, 8000.0);
  clickUI.lpf.slider.onValueChange = [this] {
    processorRef.clickEngine().setLpfFreq(
        static_cast<float>(clickUI.lpf.slider.getValue()));
  };
  addAndMakeVisible(clickUI.lpf.slider);

  // LPF Reso  0–12
  styleClickKnob(clickUI.lpf.qSlider, clickKnobLAF);
  clickUI.lpf.qSlider.setRange(0.0, 12.0, 0.01);
  clickUI.lpf.qSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.lpf.qSlider.setDoubleClickReturnValue(true, 0.0);
  clickUI.lpf.qSlider.onValueChange = [this] {
    processorRef.clickEngine().setLpfQ(
        static_cast<float>(clickUI.lpf.qSlider.getValue()));
  };
  addAndMakeVisible(clickUI.lpf.qSlider);

  // ── Sample モード専用: Pitch / A / D / R ──
  // Pitch: -24 〜 +24 半音
  styleClickKnob(clickUI.sample.pitch.slider, clickKnobLAF);
  clickUI.sample.pitch.slider.setRange(-24.0, 24.0, 1.0);
  clickUI.sample.pitch.slider.setDoubleClickReturnValue(true, 0.0);
  clickUI.sample.pitch.slider.setValue(0.0, juce::dontSendNotification);
  clickUI.sample.pitch.slider.onValueChange = [this] {
    processorRef.clickEngine().setPitchSemitones(
        static_cast<float>(clickUI.sample.pitch.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.sample.pitch.label, "Pitch", tinyFont);
  clickUI.sample.pitch.slider.setVisible(false);
  clickUI.sample.pitch.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.pitch.slider);
  addAndMakeVisible(clickUI.sample.pitch.label);

  // Attack: 0.1–500 ms
  styleClickKnob(clickUI.sample.attack.slider, clickKnobLAF);
  clickUI.sample.attack.slider.setRange(0.1, 500.0, 0.1);
  clickUI.sample.attack.slider.setSkewFactorFromMidPoint(20.0);
  clickUI.sample.attack.slider.setDoubleClickReturnValue(true, 1.0);
  clickUI.sample.attack.slider.setValue(1.0, juce::dontSendNotification);
  clickUI.sample.attack.slider.onValueChange = [this] {
    processorRef.clickEngine().setAttackMs(
        static_cast<float>(clickUI.sample.attack.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.sample.attack.label, "A", tinyFont);
  clickUI.sample.attack.slider.setVisible(false);
  clickUI.sample.attack.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.attack.slider);
  addAndMakeVisible(clickUI.sample.attack.label);

  // Hold (D): 1–2000 ms（ラベルは "D"）
  styleClickKnob(clickUI.sample.hold.slider, clickKnobLAF);
  clickUI.sample.hold.slider.setRange(1.0, 2000.0, 1.0);
  clickUI.sample.hold.slider.setSkewFactorFromMidPoint(200.0);
  clickUI.sample.hold.slider.setDoubleClickReturnValue(true, 200.0);
  clickUI.sample.hold.slider.setValue(200.0, juce::dontSendNotification);
  clickUI.sample.hold.slider.onValueChange = [this] {
    processorRef.clickEngine().setDecayMs(
        static_cast<float>(clickUI.sample.hold.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.sample.hold.label, "D", tinyFont);
  clickUI.sample.hold.slider.setVisible(false);
  clickUI.sample.hold.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.hold.slider);
  addAndMakeVisible(clickUI.sample.hold.label);

  // Release: 1–1000 ms
  styleClickKnob(clickUI.sample.release.slider, clickKnobLAF);
  clickUI.sample.release.slider.setRange(1.0, 1000.0, 1.0);
  clickUI.sample.release.slider.setSkewFactorFromMidPoint(100.0);
  clickUI.sample.release.slider.setDoubleClickReturnValue(true, 50.0);
  clickUI.sample.release.slider.setValue(50.0, juce::dontSendNotification);
  clickUI.sample.release.slider.onValueChange = [this] {
    processorRef.clickEngine().setReleaseMs(
        static_cast<float>(clickUI.sample.release.slider.getValue()));
    refreshClickSampleProvider();
  };
  styleKnobLabel(clickUI.sample.release.label, "R", tinyFont);
  clickUI.sample.release.slider.setVisible(false);
  clickUI.sample.release.label.setVisible(false);
  addAndMakeVisible(clickUI.sample.release.slider);
  addAndMakeVisible(clickUI.sample.release.label);

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
  addAndMakeVisible(clickUI.sample.loadButton);

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

  // プレビュープロバイダーを clickUI に保持
  clickUI.toneProvider = [this](float timeSec) {
    const auto decayMs =
        static_cast<float>(clickUI.toneNoise.decaySlider.getValue());
    const float env = (timeSec * 1000.0f < decayMs)
                          ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
                          : 0.0f;
    const auto freq1 =
        static_cast<float>(clickUI.toneNoise.bpf1.freqSlider.getValue());
    return std::sin(juce::MathConstants<float>::twoPi * freq1 * timeSec) * env;
  };
  clickUI.noiseProvider = [this](float timeSec) {
    const auto decayMs =
        static_cast<float>(clickUI.toneNoise.decaySlider.getValue());
    return (timeSec * 1000.0f < decayMs)
               ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
               : 0.0f;
  };

  // モード変更時にコントロール表示切替 + プロバイダー更新
  clickUI.modeCombo.onChange = [this] {
    const int m = clickUI.modeCombo.getSelectedId();
    processorRef.clickEngine().setMode(m);
    if (m == static_cast<int>(ClickUI::Mode::Sample))
      applyClickSampleMode();
    else
      applyClickToneNoiseMode(m);
    resized();
  };

  // 起動時は Tone モード
  envelopeCurveEditor.setClickPreviewProvider(clickUI.toneProvider);
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ  |  Sample ロードボタン or (Decay ラベル +
  // スライダー)
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
    clickUI.sample.loadButton.setBounds(topRow);
  } else {
    clickUI.toneNoise.decayLabel.setBounds(topRow.removeFromLeft(labelW));
    clickUI.toneNoise.decaySlider.setBounds(topRow);
  }

  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;

  // 上段4ノブ: Sample → Pitch/A/D/R, Tone/Noise → Freq/Focus/Air/Focus
  const std::array<juce::Slider *, 4> topKnobs =
      isSample
          ? std::array<juce::Slider *, 4>{{&clickUI.sample.pitch.slider,
                                           &clickUI.sample.attack.slider,
                                           &clickUI.sample.hold.slider,
                                           &clickUI.sample.release.slider}}
          : std::array<juce::Slider *, 4>{{&clickUI.toneNoise.bpf1.freqSlider,
                                           &clickUI.toneNoise.bpf1.qSlider,
                                           &clickUI.toneNoise.bpf2.freqSlider,
                                           &clickUI.toneNoise.bpf2.qSlider}};
  const std::array<juce::Component *, 4> topLabels =
      isSample
          ? std::array<juce::Component *, 4>{{&clickUI.sample.pitch.label,
                                              &clickUI.sample.attack.label,
                                              &clickUI.sample.hold.label,
                                              &clickUI.sample.release.label}}
          : std::array<juce::Component *, 4>{{&clickUI.toneNoise.bpf1.freqLabel,
                                              &clickUI.toneNoise.bpf1.qLabel,
                                              &clickUI.toneNoise.bpf2.freqLabel,
                                              &clickUI.toneNoise.bpf2.qLabel}};
  for (int col = 0; col < 4; ++col) {
    const auto idx = static_cast<size_t>(col);
    juce::Rectangle slot(area.getX() + col * slotW, area.getY(), slotW, rowH);
    topLabels[idx]->setBounds(slot.removeFromBottom(14));
    topKnobs[idx]->setBounds(slot);
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
void BabySquatchAudioProcessorEditor::onClickSampleLoadClicked() {
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

void BabySquatchAudioProcessorEditor::applyClickSampleMode() {
  // Tone/Noise 専用コントロールを隠す
  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.toneNoise.decayLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.decaySlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.freqLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.freqSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.qLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.qSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.freqLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.freqSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.qLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.qSlider)})
    c->setVisible(false);

  // Sample 専用コントロールを表示
  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.sample.loadButton),
        static_cast<juce::Component *>(&clickUI.sample.pitch.slider),
        static_cast<juce::Component *>(&clickUI.sample.pitch.label),
        static_cast<juce::Component *>(&clickUI.sample.attack.slider),
        static_cast<juce::Component *>(&clickUI.sample.attack.label),
        static_cast<juce::Component *>(&clickUI.sample.hold.slider),
        static_cast<juce::Component *>(&clickUI.sample.hold.label),
        static_cast<juce::Component *>(&clickUI.sample.release.slider),
        static_cast<juce::Component *>(&clickUI.sample.release.label)})
    c->setVisible(true);

  // HPF/LPF Q を "Q" / 0.1–6.0 に変更
  clickUI.hpf.qSlider.setRange(0.1, 6.0, 0.01);
  clickUI.hpf.qSlider.setValue(0.707, juce::dontSendNotification);
  clickUI.hpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  clickUI.hpf.qLabel.setText("Q", juce::dontSendNotification);
  clickUI.lpf.qSlider.setRange(0.1, 6.0, 0.01);
  clickUI.lpf.qSlider.setValue(0.707, juce::dontSendNotification);
  clickUI.lpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  clickUI.lpf.qLabel.setText("Q", juce::dontSendNotification);

  // HPF/LPF 周波数を Direct Sample モードの初期値へ
  clickUI.hpf.slider.setValue(20.0, juce::dontSendNotification);
  clickUI.hpf.slider.setDoubleClickReturnValue(true, 20.0);
  clickUI.lpf.slider.setValue(20000.0, juce::dontSendNotification);
  clickUI.lpf.slider.setDoubleClickReturnValue(true, 20000.0);
  processorRef.clickEngine().setHpfFreq(20.0f);
  processorRef.clickEngine().setHpfQ(0.707f);
  processorRef.clickEngine().setLpfFreq(20000.0f);
  processorRef.clickEngine().setLpfQ(0.707f);

  // Sample モードの A/D/R + Pitch を DSP へ
  processorRef.clickEngine().setAttackMs(
      static_cast<float>(clickUI.sample.attack.slider.getValue()));
  processorRef.clickEngine().setDecayMs(
      static_cast<float>(clickUI.sample.hold.slider.getValue()));
  processorRef.clickEngine().setReleaseMs(
      static_cast<float>(clickUI.sample.release.slider.getValue()));
  processorRef.clickEngine().setPitchSemitones(
      static_cast<float>(clickUI.sample.pitch.slider.getValue()));
  // サンプル未ロード時は波形をクリア、ロード済みなら更新
  envelopeCurveEditor.setClickPreviewProvider(nullptr);
  refreshClickSampleProvider();
}

void BabySquatchAudioProcessorEditor::applyClickToneNoiseMode(int m) {
  // Tone/Noise 専用コントロールを表示
  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.toneNoise.decayLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.decaySlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.freqLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.freqSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.qLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf1.qSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.freqLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.freqSlider),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.qLabel),
        static_cast<juce::Component *>(&clickUI.toneNoise.bpf2.qSlider)})
    c->setVisible(true);

  // Sample 専用コントロールを隠す
  for (juce::Component *c :
       {static_cast<juce::Component *>(&clickUI.sample.loadButton),
        static_cast<juce::Component *>(&clickUI.sample.pitch.slider),
        static_cast<juce::Component *>(&clickUI.sample.pitch.label),
        static_cast<juce::Component *>(&clickUI.sample.attack.slider),
        static_cast<juce::Component *>(&clickUI.sample.attack.label),
        static_cast<juce::Component *>(&clickUI.sample.hold.slider),
        static_cast<juce::Component *>(&clickUI.sample.hold.label),
        static_cast<juce::Component *>(&clickUI.sample.release.slider),
        static_cast<juce::Component *>(&clickUI.sample.release.label)})
    c->setVisible(false);

  // HPF/LPF Q を "Reso" / 0–12 に戻す
  // Sample モードから来た場合（range下限が0.1）のみ値もリセットする
  const bool fromSample = (clickUI.hpf.qSlider.getRange().getStart() > 0.0);
  clickUI.hpf.qSlider.setRange(0.0, 12.0, 0.01);
  if (fromSample) clickUI.hpf.qSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.hpf.qSlider.textFromValueFunction = nullptr;
  clickUI.hpf.qLabel.setText("Reso", juce::dontSendNotification);
  clickUI.lpf.qSlider.setRange(0.0, 12.0, 0.01);
  if (fromSample) clickUI.lpf.qSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.lpf.qSlider.textFromValueFunction = nullptr;
  clickUI.lpf.qLabel.setText("Reso", juce::dontSendNotification);

  // Tone/Noise モードの HPF/LPF 初期値へ戻す（Sample からの遷移時のみ）
  if (fromSample) {
    clickUI.hpf.slider.setValue(1100.0, juce::dontSendNotification);
    clickUI.lpf.slider.setValue(8000.0, juce::dontSendNotification);
  }
  clickUI.hpf.slider.setDoubleClickReturnValue(true, 1100.0);
  clickUI.lpf.slider.setDoubleClickReturnValue(true, 8000.0);
  if (fromSample) {
    processorRef.clickEngine().setHpfFreq(200.0f);
    processorRef.clickEngine().setHpfQ(0.0f);
    processorRef.clickEngine().setLpfFreq(8000.0f);
    processorRef.clickEngine().setLpfQ(0.0f);
  }

  // Tone/Noise の Decay を DSP へ戻す
  processorRef.clickEngine().setDecayMs(
      static_cast<float>(clickUI.toneNoise.decaySlider.getValue()));

  if (m == static_cast<int>(ClickUI::Mode::Noise))
    envelopeCurveEditor.setClickNoiseEnvProvider(clickUI.noiseProvider);
  else
    envelopeCurveEditor.setClickPreviewProvider(clickUI.toneProvider);
}

void BabySquatchAudioProcessorEditor::onClickSampleFileChosen(
    const juce::File &file) {
  clickUI.sample.loadedFilePath = file.getFullPathName();
  clickUI.sample.loadButton.setButtonText(file.getFileNameWithoutExtension());
  clickUI.sample.loadButton.setTooltip(clickUI.sample.loadedFilePath);
  processorRef.clickEngine().sampler().loadSample(file);

  if (!processorRef.clickEngine().sampler().copyThumbnail(
          clickUI.sample.thumbMin, clickUI.sample.thumbMax))
    return;
  clickUI.sample.thumbDurSec =
      processorRef.clickEngine().sampler().durationSec();
  refreshClickSampleProvider();
}

void BabySquatchAudioProcessorEditor::refreshClickSampleProvider() {
  if (clickUI.sample.thumbMin.empty() || clickUI.sample.thumbDurSec <= 0.0)
    return;

  const auto semitones =
      static_cast<float>(clickUI.sample.pitch.slider.getValue());
  const float speedRatio = std::pow(2.0f, semitones / 12.0f);
  const double durSec =
      clickUI.sample.thumbDurSec / static_cast<double>(speedRatio);

  const float attackSec =
      static_cast<float>(clickUI.sample.attack.slider.getValue()) / 1000.0f;
  const float holdSec =
      static_cast<float>(clickUI.sample.hold.slider.getValue()) / 1000.0f;
  const float releaseSec =
      static_cast<float>(clickUI.sample.release.slider.getValue()) / 1000.0f;

  auto minPtr = std::make_shared<std::vector<float>>(clickUI.sample.thumbMin);
  auto maxPtr = std::make_shared<std::vector<float>>(clickUI.sample.thumbMax);

  envelopeCurveEditor.setClickPreviewProvider(
      [minPtr, maxPtr, durSec, attackSec, holdSec, releaseSec](float timeSec) {
        const auto [lo, hi] = computeClickPreview(
            *minPtr, *maxPtr, durSec, attackSec, holdSec, releaseSec, timeSec);
        return (lo + hi) * 0.5f;
      });
}