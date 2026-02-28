// ClickParams.cpp
// Click panel UI setup / layout
#include "../PluginEditor.h"

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
  clickUI.modeCombo.addItem("Tone",   static_cast<int>(ClickUI::Mode::Tone));
  clickUI.modeCombo.addItem("Noise",  static_cast<int>(ClickUI::Mode::Noise));
  clickUI.modeCombo.addItem("Sample", static_cast<int>(ClickUI::Mode::Sample));
  clickUI.modeCombo.setSelectedId(static_cast<int>(ClickUI::Mode::Tone),
                                  juce::dontSendNotification);
  clickUI.modeCombo.setLookAndFeel(&darkComboLAF);
  clickUI.modeCombo.onChange = [this] {
    const bool isTone = (clickUI.modeCombo.getSelectedId() ==
                         static_cast<int>(ClickUI::Mode::Tone));
    clickUI.xyPad.setVisible(isTone);
  };
  addAndMakeVisible(clickUI.modeCombo);

  // ── XY Pad ──
  // blend: 0=SQR, 1=SAW → clickToneOsc の波形選択
  // decay: 0=LONG, 1=SHORT → 指数減衰の時定数 (200ms〜5ms)
  clickUI.xyPad.setOnChanged([this](float blend, float decay) {
    processorRef.clickEngine().setBlend(blend);
    const float decayMs = juce::jmap(decay, 0.0f, 1.0f, 200.0f, 5.0f);
    processorRef.clickEngine().setDecayMs(decayMs);
  });
  addAndMakeVisible(clickUI.xyPad);

  // 起動時の初期値を DSP へ反映（コールバックは dragg 時のみ呼ばれるため）
  processorRef.clickEngine().setBlend(clickUI.xyPad.getBlend());
  processorRef.clickEngine().setDecayMs(
      juce::jmap(clickUI.xyPad.getDecay(), 0.0f, 1.0f, 200.0f, 5.0f));
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);
  constexpr int modeLabelW = 38;
  clickUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  clickUI.modeCombo.setBounds(topRow);

  // 残りエリア: XYPad（Tone モード時のみ表示）
  clickUI.xyPad.setBounds(area);
}
