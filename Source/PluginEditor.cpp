#include "PluginEditor.h"
#include "PluginProcessor.h"

// ────────────────────────────────────────────────────
// LUT ベイクメンバー関数
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::bakeAmpLut() {
  constexpr int lutSize = EnvelopeLutManager::lutSize;
  std::array<float, lutSize> lut{};
  const float durationMs = envelopeCurveEditor.getDisplayDurationMs();
  for (int i = 0; i < lutSize; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lutSize - 1) * durationMs;
    lut[static_cast<size_t>(i)] = ampEnvData.evaluate(t);
  }
  processorRef.subEngine().envLut().setDurationMs(durationMs);
  processorRef.subEngine().envLut().bake(lut.data(), lutSize);
}

void BabySquatchAudioProcessorEditor::bakePitchLut() {
  constexpr int lutSize = EnvelopeLutManager::lutSize;
  std::array<float, lutSize> lut{};
  const float durationMs = envelopeCurveEditor.getDisplayDurationMs();
  for (int i = 0; i < lutSize; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lutSize - 1) * durationMs;
    lut[static_cast<size_t>(i)] = pitchEnvData.evaluate(t);
  }
  processorRef.subEngine().pitchLut().setDurationMs(durationMs);
  processorRef.subEngine().pitchLut().bake(lut.data(), lutSize);
}

void BabySquatchAudioProcessorEditor::bakeDistLut() {
  constexpr int lutSize = EnvelopeLutManager::lutSize;
  std::array<float, lutSize> lut{};
  const float durationMs = envelopeCurveEditor.getDisplayDurationMs();
  for (int i = 0; i < lutSize; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lutSize - 1) * durationMs;
    lut[static_cast<size_t>(i)] = distEnvData.evaluate(t);
  }
  processorRef.subEngine().distLut().setDurationMs(durationMs);
  processorRef.subEngine().distLut().bake(lut.data(), lutSize);
}

void BabySquatchAudioProcessorEditor::bakeBlendLut() {
  constexpr int lutSize = EnvelopeLutManager::lutSize;
  std::array<float, lutSize> lut{};
  const float durationMs = envelopeCurveEditor.getDisplayDurationMs();
  for (int i = 0; i < lutSize; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lutSize - 1) * durationMs;
    lut[static_cast<size_t>(i)] = blendEnvData.evaluate(t);
  }
  processorRef.subEngine().blendLut().setDurationMs(durationMs);
  processorRef.subEngine().blendLut().bake(lut.data(), lutSize);
}

// ────────────────────────────────────────────────────
// パネルルーティング（Mute/Solo/レベルメーター）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupPanelRouting(
    BabySquatchAudioProcessor &p) {
  using enum ChannelState::Channel;
  subPanel.setOnMuteChanged([&p](bool m) { p.channelState().setMute(sub, m); });
  subPanel.setOnSoloChanged([&p](bool s) { p.channelState().setSolo(sub, s); });
  clickPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(click, m); });
  clickPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(click, s); });
  directPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(direct, m); });
  directPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(direct, s); });

  subPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(sub); });
  clickPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(click); });
  directPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(direct); });

  // Sub フェーダー → ゲインコントロール
  subPanel.getFader().onValueChange = [this] {
    processorRef.subEngine().setGainDb(
        static_cast<float>(subPanel.getFader().getValue()));
  };
  // Click フェーダー → ゲインコントロール
  clickPanel.getFader().onValueChange = [this] {
    processorRef.clickEngine().setGainDb(
        static_cast<float>(clickPanel.getFader().getValue()));
  };
  // TODO: direct の gain setter 実装後に配線追加
}

// ────────────────────────────────────────────────────
// エンベロープ変更時のノブ同期
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::onEnvelopeChanged() {
  bakeAmpLut();
  bakePitchLut();
  bakeDistLut();
  bakeBlendLut();
  // 1点=ノブ制御（有効化＋ポイント値をノブに反映）、2点以上=エンベロープ制御（無効化）

  // Gain
  const bool ampCtrl = ampEnvData.isEnvelopeControlled();
  subKnobs[0].setEnabled(!ampCtrl);
  subKnobs[0].setTooltip(ampCtrl ? "Value is controlled by envelope" : "");
  if (!ampCtrl && ampEnvData.hasPoints()) {
    const float v = ampEnvData.getPoints()[0].value;
    ampEnvData.setDefaultValue(v);
    subKnobs[0].setValue(v * 100.0, juce::dontSendNotification);
  }

  // Freq
  const bool pitchCtrl = pitchEnvData.isEnvelopeControlled();
  subKnobs[1].setEnabled(!pitchCtrl);
  subKnobs[1].setTooltip(pitchCtrl ? "Value is controlled by envelope" : "");
  if (!pitchCtrl && pitchEnvData.hasPoints()) {
    const float hz = pitchEnvData.getPoints()[0].value;
    pitchEnvData.setDefaultValue(hz);
    subKnobs[1].setValue(hz, juce::dontSendNotification);
    envelopeCurveEditor.setDisplayCycles(
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  }

  // Saturate
  const bool distCtrl = distEnvData.isEnvelopeControlled();
  subKnobs[3].setEnabled(!distCtrl);
  subKnobs[3].setTooltip(distCtrl ? "Value is controlled by envelope" : "");
  if (!distCtrl && distEnvData.hasPoints()) {
    const float v = distEnvData.getPoints()[0].value;
    distEnvData.setDefaultValue(v);
    subKnobs[3].setValue(v * 100.0, juce::dontSendNotification);
  }

  // Mix
  const bool blendCtrl = blendEnvData.isEnvelopeControlled();
  subKnobs[2].setEnabled(!blendCtrl);
  subKnobs[2].setTooltip(blendCtrl ? "Value is controlled by envelope" : "");
  if (!blendCtrl && blendEnvData.hasPoints()) {
    const float v = blendEnvData.getPoints()[0].value;
    blendEnvData.setDefaultValue(v);
    subKnobs[2].setValue(v * 100.0, juce::dontSendNotification);
    envelopeCurveEditor.setPreviewBlend(v);
  }
}

// ────────────────────────────────────────────────────
// エンベロープカーブエディタ内部配線
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupEnvelopeCurveEditor() {
  envelopeCurveEditor.setOnChange([this] { onEnvelopeChanged(); });
  // 初期状態: Gain が選択済み（ラベル色を反映）
  switchEditTarget(EnvelopeCurveEditor::EditTarget::gain);
}

void BabySquatchAudioProcessorEditor::mouseDown(
    const juce::MouseEvent &e) {
  using enum EnvelopeCurveEditor::EditTarget;
  if (e.eventComponent == &subKnobLabels[0])
    switchEditTarget(gain);
  else if (e.eventComponent == &subKnobLabels[1])
    switchEditTarget(freq);
  else if (e.eventComponent == &subKnobLabels[2])
    switchEditTarget(mix);
  else if (e.eventComponent == &subKnobLabels[3])
    switchEditTarget(saturate);
}

void BabySquatchAudioProcessorEditor::switchEditTarget(
    EnvelopeCurveEditor::EditTarget t) {
  envelopeCurveEditor.setEditTarget(t);
  using enum EnvelopeCurveEditor::EditTarget;
  const auto kLabel = UIConstants::Colours::labelText;
  subKnobLabels[0].setColour(juce::Label::textColourId,
                             t == gain ? UIConstants::Colours::subArc : kLabel);
  subKnobLabels[1].setColour(juce::Label::textColourId,
                             t == freq ? juce::Colours::cyan : kLabel);
  subKnobLabels[3].setColour(juce::Label::textColourId,
                             t == saturate ? juce::Colour(0xFFFF9500) : kLabel);
  subKnobLabels[2].setColour(juce::Label::textColourId,
                             t == mix ? juce::Colour(0xFF4CAF50) : kLabel);
}

// ────────────────────────────────────────────────────
// コンストラクター
// ────────────────────────────────────────────────────
BabySquatchAudioProcessorEditor::BabySquatchAudioProcessorEditor(
    BabySquatchAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState()) {
  addAndMakeVisible(subPanel);
  addAndMakeVisible(clickPanel);
  addAndMakeVisible(directPanel);
  addAndMakeVisible(keyboard);
  addAndMakeVisible(envelopeCurveEditor);

  pitchEnvData.setDefaultValue(200.0f);

  setupPanelRouting(p);
  setupEnvelopeCurveEditor();
  setupClickParams();
  setupSubKnobsRow();
  setupWaveShapeCombo();
  setupLengthBox();
  setupPitchKnob();
  setupAmpKnob();
  setupBlendKnob();
  setupDistKnob();
  setupHarmonicKnobs();

  setSize(UIConstants::windowWidth, UIConstants::windowHeight +
                                        UIConstants::expandedAreaHeight +
                                        UIConstants::panelGap);
}

BabySquatchAudioProcessorEditor::~BabySquatchAudioProcessorEditor() {
  subWave.combo.setLookAndFeel(nullptr);
  clickUI.modeCombo.setLookAndFeel(nullptr);
}

void BabySquatchAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(UIConstants::Colours::background);

  // 展開エリアの背景
  g.setColour(UIConstants::Colours::panelBg);
  g.fillRoundedRectangle(envelopeCurveEditor.getBounds().toFloat(), 6.0f);
}

void BabySquatchAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(UIConstants::panelPadding);

  // 常時表示の展開エリア（下部）
  {
    auto expandArea = area.removeFromBottom(UIConstants::expandedAreaHeight);
    keyboard.setBounds(
        expandArea.removeFromBottom(UIConstants::keyboardHeight));
    envelopeCurveEditor.setBounds(expandArea);
    area.removeFromBottom(UIConstants::panelGap);
  }

  // 残りを3パネルで均等分割
  const int panelWidth = (area.getWidth() - UIConstants::panelGap * 2) / 3;

  subPanel.setBounds(area.removeFromLeft(panelWidth));
  area.removeFromLeft(UIConstants::panelGap);

  clickPanel.setBounds(area.removeFromLeft(panelWidth));
  area.removeFromLeft(UIConstants::panelGap);

  directPanel.setBounds(area);

  // SUB ノブをパネル内コンテンツエリアに常時配置
  {
    const auto b = subPanel.getBounds();
    constexpr int faderHandleWidth = 12;
    constexpr int contentLeft =
        UIConstants::panelPadding + UIConstants::meterWidth + faderHandleWidth;
    constexpr int contentTop =
        UIConstants::panelPadding + UIConstants::labelHeight;
    constexpr int contentBot =
        UIConstants::panelPadding + UIConstants::expandButtonHeight;
    juce::Rectangle contentArea{b.getX() + contentLeft, b.getY() + contentTop,
                                b.getWidth() - contentLeft -
                                    UIConstants::panelPadding,
                                b.getHeight() - contentTop - contentBot};
    // 底行: [Length ボックス | subWave.combo]
    auto bottomRow = contentArea.removeFromBottom(22);
    contentArea.removeFromBottom(4);                   // ギャップ
    constexpr int lengthTotalW = 44 + 34 + 18 + 2 * 2; // 100px
    layoutLengthBox(bottomRow.removeFromLeft(lengthTotalW));
    constexpr int waveLabelW = 38;
    subWave.label.setBounds(bottomRow.removeFromLeft(waveLabelW));
    subWave.combo.setBounds(bottomRow);
    layoutSubKnobsRow(contentArea);
  }

  // CLICK コンテンツをパネル内に常時配置
  {
    const auto b = clickPanel.getBounds();
    constexpr int faderHandleWidth = 12;
    constexpr int contentLeft =
        UIConstants::panelPadding + UIConstants::meterWidth + faderHandleWidth;
    constexpr int contentTop =
        UIConstants::panelPadding + UIConstants::labelHeight;
    constexpr int contentBot =
        UIConstants::panelPadding + UIConstants::expandButtonHeight;
    juce::Rectangle contentArea{b.getX() + contentLeft, b.getY() + contentTop,
                                b.getWidth() - contentLeft -
                                    UIConstants::panelPadding,
                                b.getHeight() - contentTop - contentBot};
    layoutClickParams(contentArea);
  }
}
