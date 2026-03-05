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

  // モード変更時にプロバイダーを切り替える（既存 onChange を拡張）
  clickUI.modeCombo.onChange = [this, toneProvider, noiseEnvProvider] {
    const int m = clickUI.modeCombo.getSelectedId();
    processorRef.clickEngine().setMode(m);
    if (m == static_cast<int>(ClickUI::Mode::Noise))
      envelopeCurveEditor.setClickNoiseEnvProvider(noiseEnvProvider);
    else
      envelopeCurveEditor.setClickPreviewProvider(toneProvider);
  };

  // 起動時は Tone モード
  envelopeCurveEditor.setClickPreviewProvider(toneProvider);
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ  |  Decay ラベル + スライダー（同一行）
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);

  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 72;
  constexpr int colGap = 6;
  constexpr int labelW = 36;

  clickUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  clickUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));
  topRow.removeFromLeft(colGap);
  clickUI.decayLabel.setBounds(topRow.removeFromLeft(labelW));
  clickUI.decaySlider.setBounds(topRow);

  // 残りエリア: 8 ノブを 2×4 グリッドで配置
  const std::array<juce::Slider *, 8> knobs = {{
      &clickUI.freq1Slider,
      &clickUI.focus1Slider,
      &clickUI.freq2Slider,
      &clickUI.focus2Slider,
      &clickUI.hpfSlider,
      &clickUI.hpfQSlider,
      &clickUI.lpfSlider,
      &clickUI.lpfQSlider,
  }};
  const std::array<juce::Component *, 8> labels = {{
      &clickUI.freq1Label,
      &clickUI.focus1Label,
      &clickUI.freq2Label,
      &clickUI.focus2Label,
      &clickUI.hpfSlope,
      &clickUI.hpfQLabel,
      &clickUI.lpfSlope,
      &clickUI.lpfQLabel,
  }};
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(row * 4 + col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY() + row * rowH,
                           slotW, rowH);
      labels[idx]->setBounds(slot.removeFromBottom(14));
      knobs[idx]->setBounds(slot);
    }
  }
}
