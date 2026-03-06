#include "MasterFader.h"
#include "UIConstants.h"

// ────────────────────────────────────────────────────
// コンストラクタ
// ────────────────────────────────────────────────────
MasterFader::MasterFader() {
  // ── ラベル ──
  label.setText("MASTER", juce::dontSendNotification);
  label.setFont(juce::Font(juce::FontOptions(UIConstants::fontSizeSmall)));
  label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  label.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(label);

  // ── フェーダー ──
  fader.setSliderStyle(juce::Slider::LinearHorizontal);
  fader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  fader.setRange(minDb, maxDb, 0.1);
  fader.setValue(0.0, juce::dontSendNotification);
  fader.setDoubleClickReturnValue(true, 0.0);
  fader.setTextValueSuffix(" dB");

  // トラック色・サム色
  fader.setColour(juce::Slider::trackColourId,
                  UIConstants::Colours::text.withAlpha(0.3f));
  fader.setColour(juce::Slider::thumbColourId, UIConstants::Colours::text);

  fader.onValueChange = [this] {
    if (onValueChange)
      onValueChange(static_cast<float>(fader.getValue()));
  };
  addAndMakeVisible(fader);
}

void MasterFader::setOnValueChange(std::function<void(float)> cb) {
  onValueChange = std::move(cb);
}

// ────────────────────────────────────────────────────
// resized
// ────────────────────────────────────────────────────
void MasterFader::resized() {
  auto area = getLocalBounds();
  label.setBounds(area.removeFromTop(labelHeight));
  fader.setBounds(area.reduced(0, 4));
}
