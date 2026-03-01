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
  clickUI.modeCombo.onChange = [this] {
    processorRef.clickEngine().setMode(clickUI.modeCombo.getSelectedId());
    envelopeCurveEditor.repaint();
  };
  processorRef.clickEngine().setMode(static_cast<int>(ClickUI::Mode::Tone));
  addAndMakeVisible(clickUI.modeCombo);

  // ── Filter param labels ──
  const auto tinyFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));
  styleFilterLabel(clickUI.decayLabel, "Decay:", tinyFont);
  styleFilterLabel(clickUI.freq1Label, "Freq:", tinyFont);
  styleFilterLabel(clickUI.focus1Label, "Focus:", tinyFont);
  styleFilterLabel(clickUI.freq2Label, "Air:", tinyFont);
  styleFilterLabel(clickUI.focus2Label, "Focus:", tinyFont);
  styleFilterLabel(clickUI.hpfLabel, "HPF:", tinyFont);
  styleFilterLabel(clickUI.hpfQLabel, "Reso:", tinyFont);
  styleFilterLabel(clickUI.lpfLabel, "LPF:", tinyFont);
  styleFilterLabel(clickUI.lpfQLabel, "Reso:", tinyFont);
  addAndMakeVisible(clickUI.decayLabel);
  addAndMakeVisible(clickUI.freq1Label);
  addAndMakeVisible(clickUI.focus1Label);
  addAndMakeVisible(clickUI.freq2Label);
  addAndMakeVisible(clickUI.focus2Label);
  addAndMakeVisible(clickUI.hpfLabel);
  addAndMakeVisible(clickUI.hpfQLabel);
  addAndMakeVisible(clickUI.lpfLabel);
  addAndMakeVisible(clickUI.lpfQLabel);

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
  styleFilterSlider(clickUI.freq1Slider);
  clickUI.freq1Slider.setRange(20.0, 20000.0, 1.0);
  clickUI.freq1Slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.freq1Slider.setTextValueSuffix(" Hz");
  clickUI.freq1Slider.setValue(5000.0, juce::dontSendNotification);
  clickUI.freq1Slider.onValueChange = [this] {
    processorRef.clickEngine().setFreq1(
        static_cast<float>(clickUI.freq1Slider.getValue()));
    envelopeCurveEditor.repaint();
  };
  addAndMakeVisible(clickUI.freq1Slider);

  // Focus (BPF1 Q)  0–12
  styleFilterSlider(clickUI.focus1Slider);
  clickUI.focus1Slider.setRange(0.0, 12.0, 0.01);
  clickUI.focus1Slider.setValue(0.71, juce::dontSendNotification);
  clickUI.focus1Slider.onValueChange = [this] {
    processorRef.clickEngine().setFocus1(
        static_cast<float>(clickUI.focus1Slider.getValue()));
  };
  addAndMakeVisible(clickUI.focus1Slider);

  // Air (BPF2 freq)  20–20000 Hz  log
  styleFilterSlider(clickUI.freq2Slider);
  clickUI.freq2Slider.setRange(20.0, 20000.0, 1.0);
  clickUI.freq2Slider.setSkewFactorFromMidPoint(1000.0);
  clickUI.freq2Slider.setTextValueSuffix(" Hz");
  clickUI.freq2Slider.setValue(10000.0, juce::dontSendNotification);
  clickUI.freq2Slider.onValueChange = [this] {
    processorRef.clickEngine().setFreq2(
        static_cast<float>(clickUI.freq2Slider.getValue()));
  };
  addAndMakeVisible(clickUI.freq2Slider);

  // Focus (BPF2 Q)  0–12
  styleFilterSlider(clickUI.focus2Slider);
  clickUI.focus2Slider.setRange(0.0, 12.0, 0.01);
  clickUI.focus2Slider.setValue(0.0, juce::dontSendNotification);
  clickUI.focus2Slider.onValueChange = [this] {
    processorRef.clickEngine().setFocus2(
        static_cast<float>(clickUI.focus2Slider.getValue()));
  };
  addAndMakeVisible(clickUI.focus2Slider);

  // HPF freq  20–20000 Hz  log
  styleFilterSlider(clickUI.hpfSlider);
  clickUI.hpfSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.hpfSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.hpfSlider.setTextValueSuffix(" Hz");
  clickUI.hpfSlider.setValue(200.0, juce::dontSendNotification);
  clickUI.hpfSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfFreq(
        static_cast<float>(clickUI.hpfSlider.getValue()));
  };
  addAndMakeVisible(clickUI.hpfSlider);

  // HPF Reso  0–12
  styleFilterSlider(clickUI.hpfQSlider);
  clickUI.hpfQSlider.setRange(0.0, 12.0, 0.01);
  clickUI.hpfQSlider.setValue(0.0, juce::dontSendNotification);
  clickUI.hpfQSlider.onValueChange = [this] {
    processorRef.clickEngine().setHpfQ(
        static_cast<float>(clickUI.hpfQSlider.getValue()));
  };
  addAndMakeVisible(clickUI.hpfQSlider);

  // LPF freq  20–20000 Hz  log
  styleFilterSlider(clickUI.lpfSlider);
  clickUI.lpfSlider.setRange(20.0, 20000.0, 1.0);
  clickUI.lpfSlider.setSkewFactorFromMidPoint(1000.0);
  clickUI.lpfSlider.setTextValueSuffix(" Hz");
  clickUI.lpfSlider.setValue(8000.0, juce::dontSendNotification);
  clickUI.lpfSlider.onValueChange = [this] {
    processorRef.clickEngine().setLpfFreq(
        static_cast<float>(clickUI.lpfSlider.getValue()));
  };
  addAndMakeVisible(clickUI.lpfSlider);

  // LPF Reso  0–12
  styleFilterSlider(clickUI.lpfQSlider);
  clickUI.lpfQSlider.setRange(0.0, 12.0, 0.01);
  clickUI.lpfQSlider.setValue(0.0, juce::dontSendNotification);
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
  // Tone: BPF インパルス応答の近似（減衰サイン波）
  // Noise: 指数減衰エンベロープの輪郭（±env）
  envelopeCurveEditor.setClickPreviewProvider([this](float timeSec) {
    const auto decayMs = static_cast<float>(clickUI.decaySlider.getValue());
    // decayMs = 全体の減衰時間 → τ=decayMs/5。decayMs以降はゼロ。
    const float env = (timeSec * 1000.0f < decayMs)
                          ? std::exp(-timeSec * 5000.0f / (decayMs + 1e-6f))
                          : 0.0f;

    if (const int mode = clickUI.modeCombo.getSelectedId();
        mode == static_cast<int>(ClickUI::Mode::Noise)) {
      return env;
    }

    // Tone: freq1 をそのまま使用（高周波では波形が密になるが変化は見える）
    // ただし decay 期間内に最大 30 サイクルに収まるよう上限をかけて視認性を確保
    const auto freq1 = static_cast<float>(clickUI.freq1Slider.getValue());
    const float maxVisibleFreq = 30000.0f / (decayMs + 1e-6f);
    const float usedFreq = std::min(freq1, maxVisibleFreq);
    return std::sin(juce::MathConstants<float>::twoPi * usedFreq * timeSec) *
           env;
  });
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);
  constexpr int modeLabelW = 38;
  clickUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  clickUI.modeCombo.setBounds(topRow);

  // 残りエリア: 全幅をフィルターパネルとして使用（9 行）
  constexpr int rowGap = 3;
  constexpr int labelW = 36;
  auto &filterPanel = area;

  constexpr int numRows = 9;
  const int rowH = (filterPanel.getHeight() - rowGap * (numRows - 1)) / numRows;
  const std::array<std::pair<juce::Label *, juce::Slider *>, numRows> rows = {{
      {&clickUI.decayLabel, &clickUI.decaySlider},
      {&clickUI.freq1Label, &clickUI.freq1Slider},
      {&clickUI.focus1Label, &clickUI.focus1Slider},
      {&clickUI.freq2Label, &clickUI.freq2Slider},
      {&clickUI.focus2Label, &clickUI.focus2Slider},
      {&clickUI.hpfLabel, &clickUI.hpfSlider},
      {&clickUI.hpfQLabel, &clickUI.hpfQSlider},
      {&clickUI.lpfLabel, &clickUI.lpfSlider},
      {&clickUI.lpfQLabel, &clickUI.lpfQSlider},
  }};
  for (auto [label, slider] : rows) {
    auto row = filterPanel.removeFromTop(rowH);
    filterPanel.removeFromTop(rowGap);
    label->setBounds(row.removeFromLeft(labelW));
    slider->setBounds(row);
  }
}
