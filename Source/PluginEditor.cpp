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
  processorRef.envLut().setDurationMs(durationMs);
  processorRef.envLut().bake(lut.data(), lutSize);
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
  processorRef.pitchLut().setDurationMs(durationMs);
  processorRef.pitchLut().bake(lut.data(), lutSize);
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
  processorRef.distLut().setDurationMs(durationMs);
  processorRef.distLut().bake(lut.data(), lutSize);
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
  processorRef.blendLut().setDurationMs(durationMs);
  processorRef.blendLut().bake(lut.data(), lutSize);
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
    processorRef.setSubGainDb(
        static_cast<float>(subPanel.getFader().getValue()));
  };
  // TODO: click/direct の gain setter 実装後に配線追加
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

  // AMP
  const bool ampCtrl = ampEnvData.isEnvelopeControlled();
  subKnobs[1].setEnabled(!ampCtrl);
  subKnobs[1].setTooltip(ampCtrl ? "Value is controlled by envelope" : "");
  if (!ampCtrl && ampEnvData.hasPoints()) {
    const float v = ampEnvData.getPoints()[0].value;
    ampEnvData.setDefaultValue(v);
    subKnobs[1].setValue(v * 100.0, juce::dontSendNotification);
  }

  // PITCH
  const bool pitchCtrl = pitchEnvData.isEnvelopeControlled();
  subKnobs[0].setEnabled(!pitchCtrl);
  subKnobs[0].setTooltip(pitchCtrl ? "Value is controlled by envelope" : "");
  if (!pitchCtrl && pitchEnvData.hasPoints()) {
    const float hz = pitchEnvData.getPoints()[0].value;
    pitchEnvData.setDefaultValue(hz);
    subKnobs[0].setValue(hz, juce::dontSendNotification);
    envelopeCurveEditor.setDisplayCycles(
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  }

  // DIST
  const bool distCtrl = distEnvData.isEnvelopeControlled();
  subKnobs[3].setEnabled(!distCtrl);
  subKnobs[3].setTooltip(distCtrl ? "Value is controlled by envelope" : "");
  if (!distCtrl && distEnvData.hasPoints()) {
    const float v = distEnvData.getPoints()[0].value;
    distEnvData.setDefaultValue(v);
    subKnobs[3].setValue(v * 100.0, juce::dontSendNotification);
  }

  // BLEND
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

  envelopeCurveEditor.setOnEditTargetChanged(
      [this](EnvelopeCurveEditor::EditTarget target) {
        using enum EnvelopeCurveEditor::EditTarget;
        const auto kLabel = UIConstants::Colours::labelText;
        subKnobLabels[0].setColour(juce::Label::textColourId,
                                   target == pitch ? juce::Colours::cyan
                                                   : kLabel);
        subKnobLabels[1].setColour(juce::Label::textColourId,
                                   target == amp ? UIConstants::Colours::subArc
                                                 : kLabel);
        subKnobLabels[3].setColour(juce::Label::textColourId,
                                   target == dist ? juce::Colour(0xFFFF9500)
                                                  : kLabel);
        subKnobLabels[2].setColour(juce::Label::textColourId,
                                   target == blend ? juce::Colour(0xFF4CAF50)
                                                   : kLabel);
      });
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
  addChildComponent(expandableArea);
  addChildComponent(keyboard);
  addChildComponent(envelopeCurveEditor);

  // 展開ボタン → 親に通知
  subPanel.setOnExpandRequested([this] { requestExpand(ExpandChannel::sub); });
  clickPanel.setOnExpandRequested(
      [this] { requestExpand(ExpandChannel::click); });
  directPanel.setOnExpandRequested(
      [this] { requestExpand(ExpandChannel::direct); });

  pitchEnvData.setDefaultValue(200.0f);

  setupPanelRouting(p);
  setupEnvelopeCurveEditor();
  setupSubKnobsRow();
  setupWaveShapeCombo();
  setupLengthBox();
  setupPitchKnob();
  setupAmpKnob();
  setupBlendKnob();
  setupDistKnob();
  setupHarmonicKnobs();

  setSize(UIConstants::windowWidth, UIConstants::windowHeight);
}

BabySquatchAudioProcessorEditor::~BabySquatchAudioProcessorEditor() {
  waveShapeCombo.setLookAndFeel(nullptr);
}

void BabySquatchAudioProcessorEditor::paint(juce::Graphics &g) {
  using enum ExpandChannel;
  g.fillAll(UIConstants::Colours::background);

  // 展開エリアの背景
  if (activeChannel != none) {
    g.setColour(UIConstants::Colours::panelBg);
    g.fillRoundedRectangle(expandableArea.getBounds().toFloat(), 6.0f);
  }
}

void BabySquatchAudioProcessorEditor::resized() {
  using enum ExpandChannel;
  auto area = getLocalBounds().reduced(UIConstants::panelPadding);

  // 展開エリアを下から確保
  if (activeChannel != none) {
    auto expandArea = area.removeFromBottom(UIConstants::expandedAreaHeight);

    // 1. 鍵盤を展開エリア下部に配置
    keyboard.setBounds(
        expandArea.removeFromBottom(UIConstants::keyboardHeight));

    // 2. エンベロープカーブエディタ → 残り全域
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
    // 底行: [Length ボックス | waveShapeCombo]
    auto bottomRow = contentArea.removeFromBottom(22);
    contentArea.removeFromBottom(4);                   // ギャップ
    constexpr int lengthTotalW = 44 + 34 + 18 + 2 * 2; // 100px
    layoutLengthBox(bottomRow.removeFromLeft(lengthTotalW));
    constexpr int waveLabelW = 38;
    waveLabel.setBounds(bottomRow.removeFromLeft(waveLabelW));
    waveShapeCombo.setBounds(bottomRow);
    layoutSubKnobsRow(contentArea);
  }
}

void BabySquatchAudioProcessorEditor::requestExpand(ExpandChannel ch) {
  using enum ExpandChannel;
  // 同じチャンネルをもう一度押したら閉じる
  if (activeChannel == ch) {
    activeChannel = none;
  } else {
    activeChannel = ch;
  }

  const bool isOpen = (activeChannel != none);
  expandableArea.setVisible(isOpen);
  keyboard.setVisible(isOpen);
  updateEnvelopeEditorVisibility();

  if (isOpen)
    keyboard.grabFocus();

  const int extra =
      isOpen ? UIConstants::expandedAreaHeight + UIConstants::panelGap : 0;
  setSize(UIConstants::windowWidth, UIConstants::windowHeight + extra);

  // setSize でサイズが変わらない場合（チャンネル切替）は resized() が
  // 自動呼出しされないため、明示的にレイアウトを更新
  resized();

  updateExpandIndicators();
}

void BabySquatchAudioProcessorEditor::updateExpandIndicators() {
  using enum ExpandChannel;
  subPanel.setExpandIndicator(activeChannel == sub);
  clickPanel.setExpandIndicator(activeChannel == click);
  directPanel.setExpandIndicator(activeChannel == direct);
}

void BabySquatchAudioProcessorEditor::updateEnvelopeEditorVisibility() {
  using enum ExpandChannel;
  const bool isOpen = (activeChannel != none);
  envelopeCurveEditor.setVisible(isOpen);
}
