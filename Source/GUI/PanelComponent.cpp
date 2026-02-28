#include "PanelComponent.h"
#include "UIConstants.h"

// ────────────────────────────────────────────────────
// コンストラクタ
// ────────────────────────────────────────────────────
PanelComponent::PanelComponent(const juce::String &name,
                               juce::Colour accentColour)
    : faderLAF(accentColour) {
  // ── レベルメーター（背景として先に追加） ──
  addAndMakeVisible(levelMeter);

  // ── フェーダー（レベルメーターの上に重ねる） ──
  fader.setSliderStyle(juce::Slider::LinearVertical);
  fader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  fader.setRange(-48.0, 6.0, 0.1);
  fader.setValue(0.0, juce::dontSendNotification);
  fader.setDoubleClickReturnValue(true, 0.0);
  fader.setTextValueSuffix(" dB");
  fader.setLookAndFeel(&faderLAF);
  addAndMakeVisible(fader);

  // ── タイトルラベル ──
  titleLabel.setText(name, juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centred);
  titleLabel.setColour(juce::Label::textColourId, UIConstants::Colours::text);
  addAndMakeVisible(titleLabel);

  // ── 展開ボタン ──
  expandButton.setButtonText(juce::String::charToString(0x25BC)); // ▼
  expandButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colours::transparentBlack);
  expandButton.setColour(juce::TextButton::textColourOffId,
                         UIConstants::Colours::labelText);
  expandButton.onClick = [this] {
    if (onExpandRequested)
      onExpandRequested();
  };
  addAndMakeVisible(expandButton);

  // ── Mute ボタン ──
  muteButton.setClickingTogglesState(true);
  muteButton.setColour(juce::TextButton::buttonColourId,
                       UIConstants::Colours::muteOff);
  muteButton.setColour(juce::TextButton::buttonOnColourId,
                       UIConstants::Colours::muteOn);
  muteButton.setColour(juce::TextButton::textColourOffId,
                       UIConstants::Colours::labelText);
  muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  muteButton.onClick = [this] {
    if (onMuteChanged)
      onMuteChanged(muteButton.getToggleState());
  };
  addAndMakeVisible(muteButton);

  // ── Solo ボタン ──
  soloButton.setClickingTogglesState(true);
  soloButton.setColour(juce::TextButton::buttonColourId,
                       UIConstants::Colours::soloOff);
  soloButton.setColour(juce::TextButton::buttonOnColourId,
                       UIConstants::Colours::soloOn);
  soloButton.setColour(juce::TextButton::textColourOffId,
                       UIConstants::Colours::labelText);
  soloButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  soloButton.onClick = [this] {
    if (onSoloChanged)
      onSoloChanged(soloButton.getToggleState());
  };
  addAndMakeVisible(soloButton);
}

// ────────────────────────────────────────────────────
// デストラクタ
// ────────────────────────────────────────────────────
PanelComponent::~PanelComponent() { fader.setLookAndFeel(nullptr); }

// ────────────────────────────────────────────────────
// paint / resized
// ────────────────────────────────────────────────────
void PanelComponent::paint(juce::Graphics &g) {
  g.setColour(UIConstants::Colours::panelBg);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
}

void PanelComponent::resized() {
  auto area = getLocalBounds().reduced(UIConstants::panelPadding);

  titleLabel.setBounds(area.removeFromTop(UIConstants::labelHeight));

  // 下部ボタン行: M | S | ▼
  auto bottomRow = area.removeFromBottom(UIConstants::expandButtonHeight);
  const int btnW = bottomRow.getWidth() / 3;
  muteButton.setBounds(bottomRow.removeFromLeft(btnW));
  soloButton.setBounds(bottomRow.removeFromLeft(btnW));
  expandButton.setBounds(bottomRow);

  // レベルメーター（左列）+ フェーダーハンドル（その右に隣接）
  constexpr int faderHandleWidth = 12;
  levelMeter.setBounds(area.removeFromLeft(UIConstants::meterWidth));
  fader.setBounds(area.removeFromLeft(faderHandleWidth));
}

// ────────────────────────────────────────────────
// getFader
// ────────────────────────────────────────────────
juce::Slider &PanelComponent::getFader() { return fader; }

// ────────────────────────────────────────────────
// FaderLAF::drawLinearSlider
// トラック: 透明（LevelMeter が背景役）
// サム:     アクセントカラーの水平バー + 右端の ◁
// ────────────────────────────────────────────────
void PanelComponent::FaderLAF::drawLinearSlider(
    juce::Graphics &g, int x, int trackY, int width, int height,
    float sliderPos, float /*minPos*/, float /*maxPos*/,
    juce::Slider::SliderStyle, juce::Slider &slider) {

  // 0 dB 基準ライン（薄いグレー）
  const auto minVal = slider.getMinimum();
  const auto maxVal = slider.getMaximum();
  const auto zeroNorm = static_cast<float>((0.0 - minVal) / (maxVal - minVal));
  const float zeroY =
      static_cast<float>(trackY) +
      (1.0f - zeroNorm) * static_cast<float>(height);
  g.setColour(juce::Colours::white.withAlpha(0.22f));
  g.drawHorizontalLine(static_cast<int>(zeroY), static_cast<float>(x),
                       static_cast<float>(x + width));

  // ◁ 三角形のみ（左向き、先端が左）
  constexpr float triW = 10.0f;
  constexpr float triH = 14.0f;
  g.setColour(accent_.withAlpha(0.92f));
  juce::Path tri;
  tri.addTriangle(
      static_cast<float>(x),          sliderPos,               // 先端（左）
      static_cast<float>(x + width),  sliderPos - triH * 0.5f, // 右上
      static_cast<float>(x + width),  sliderPos + triH * 0.5f  // 右下
  );
  g.fillPath(tri);
  (void)triW;
}

// ────────────────────────────────────────────────
// setOnExpandRequested
// ────────────────────────────────────────────────
void PanelComponent::setOnExpandRequested(std::function<void()> callback) {
  onExpandRequested = std::move(callback);
}

// ────────────────────────────────────────────────
// setExpandIndicator
// ────────────────────────────────────────────────
void PanelComponent::setExpandIndicator(bool isThisPanelExpanded) {
  expandButton.setButtonText(
      juce::String::charToString(isThisPanelExpanded ? 0x25B2 : 0x25BC));
}

// ────────────────────────────────────────────────
// Mute / Solo コールバック + 外部状態更新
// ────────────────────────────────────────────────
void PanelComponent::setOnMuteChanged(std::function<void(bool)> cb) {
  onMuteChanged = std::move(cb);
}

void PanelComponent::setOnSoloChanged(std::function<void(bool)> cb) {
  onSoloChanged = std::move(cb);
}

void PanelComponent::setMuteState(bool muted) {
  muteButton.setToggleState(muted, juce::dontSendNotification);
}

void PanelComponent::setSoloState(bool soloed) {
  soloButton.setToggleState(soloed, juce::dontSendNotification);
}

void PanelComponent::setLevelProvider(std::function<float()> provider) {
  levelMeter.setLevelProvider(std::move(provider));
}
