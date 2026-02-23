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

// ────────────────────────────────────────────────────
// パネルルーティング（Mute/Solo/レベルメーター）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupPanelRouting(
    BabySquatchAudioProcessor &p) {
  using enum ChannelState::Channel;
  oomphPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(oomph, m); });
  oomphPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(oomph, s); });
  clickPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(click, m); });
  clickPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(click, s); });
  dryPanel.setOnMuteChanged([&p](bool m) { p.channelState().setMute(dry, m); });
  dryPanel.setOnSoloChanged([&p](bool s) { p.channelState().setSolo(dry, s); });

  oomphPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(oomph); });
  clickPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(click); });
  dryPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(dry); });

  oomphPanel.getKnob().onValueChange = [this] {
    processorRef.setOomphGainDb(
        static_cast<float>(oomphPanel.getKnob().getValue()));
  };
}

// ────────────────────────────────────────────────────
// エンベロープカーブエディタ内部配線
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupEnvelopeCurveEditor() {
  envelopeCurveEditor.setOnChange([this] {
    bakeAmpLut();
    bakePitchLut();
    oomphKnobs[1].setEnabled(!ampEnvData.hasPoints());
    oomphKnobs[1].setTooltip(
        ampEnvData.hasPoints() ? "Value is controlled by envelope" : "");
    oomphKnobs[0].setEnabled(!pitchEnvData.hasPoints());
    oomphKnobs[0].setTooltip(
        pitchEnvData.hasPoints() ? "Value is controlled by envelope" : "");
  });

  envelopeCurveEditor.setOnEditTargetChanged(
      [this](EnvelopeCurveEditor::EditTarget target) {
        using enum EnvelopeCurveEditor::EditTarget;
        oomphKnobLabels[0].setColour(juce::Label::textColourId,
                                     target == pitch
                                         ? juce::Colours::cyan
                                         : UIConstants::Colours::labelText);
        oomphKnobLabels[1].setColour(juce::Label::textColourId,
                                     target == amp
                                         ? juce::Colours::white
                                         : UIConstants::Colours::labelText);
      });
}

// ────────────────────────────────────────────────────
// OOMPH Osc ノブ行（8本）初期化
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupOomphKnobsRow() {
  static constexpr std::array<const char *, 8> kLabels = {
      "PITCH", "AMP", "BLEND", "DIST", "H1", "H2", "H3", "H4"};
  for (size_t i = 0; i < 8; ++i) {
    auto &knob = oomphKnobs[i];
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0);
    addChildComponent(knob);

    auto &label = oomphKnobLabels[i];
    label.setText(kLabels[i], juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f)));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
    addChildComponent(label);
  }
}

// ────────────────────────────────────────────────────
// 波形選択ボタン（Tri / SQR / SAW）初期化
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::deselectOtherWaveShapeButtons(
    size_t selectedIdx) {
  for (size_t j = 0; j < 3; ++j) {
    if (j == selectedIdx)
      continue;
    waveShapeButtons[j].setToggleState(false, juce::dontSendNotification);
  }
}

void BabySquatchAudioProcessorEditor::setupWaveShapeButtons() {
  static constexpr std::array<const char *, 3> kWaveLabels = {"Tri", "SQR",
                                                              "SAW"};
  static constexpr std::array<WaveShape, 3> kShapes = {
      WaveShape::Tri, WaveShape::Square, WaveShape::Saw};
  for (size_t i = 0; i < 3; ++i) {
    auto &btn = waveShapeButtons[i];
    btn.setButtonText(kWaveLabels[i]);
    btn.setClickingTogglesState(true);
    btn.setColour(juce::TextButton::buttonColourId,
                  UIConstants::Colours::knobBg);
    btn.setColour(juce::TextButton::buttonOnColourId,
                  UIConstants::Colours::oomphArc);
    btn.setColour(juce::TextButton::textColourOffId,
                  UIConstants::Colours::text);
    btn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    const WaveShape shape = kShapes[i];
    btn.onClick = [this, i, shape] {
      if (waveShapeButtons[i].getToggleState()) {
        deselectOtherWaveShapeButtons(i);
        processorRef.oomphOscillator().setWaveShape(shape);
        envelopeCurveEditor.setWaveShape(shape);
      } else {
        processorRef.oomphOscillator().setWaveShape(WaveShape::Sine);
        envelopeCurveEditor.setWaveShape(WaveShape::Sine);
      }
    };
    addChildComponent(btn);
  }
}

// ────────────────────────────────────────────────────
// PITCHノブ（oomphKnobs[0]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupPitchKnob() {
  oomphKnobs[0].setRange(20.0, 20000.0);
  oomphKnobs[0].setSkewFactorFromMidPoint(200.0);
  oomphKnobs[0].setValue(200.0, juce::dontSendNotification);
  oomphKnobs[0].setDoubleClickReturnValue(true, 200.0);
  oomphKnobs[0].onValueChange = [this] {
    const auto hz = static_cast<float>(oomphKnobs[0].getValue());
    pitchEnvData.setDefaultValue(hz);
    bakePitchLut();
    const float cycles =
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f;
    envelopeCurveEditor.setDisplayCycles(cycles);
    envelopeCurveEditor.repaint();
  };
  // 初期サイクル数 + Pitch LUT 初期ベイク
  constexpr float initHz = 200.0f;
  envelopeCurveEditor.setDisplayCycles(
      initHz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  bakePitchLut();
}

// ────────────────────────────────────────────────────
// AMPノブ（oomphKnobs[1]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupAmpKnob() {
  oomphKnobs[1].setRange(0.0, 200.0);
  oomphKnobs[1].setValue(ampEnvData.getDefaultValue() * 100.0,
                         juce::dontSendNotification);
  oomphKnobs[1].setDoubleClickReturnValue(true, 100.0);
  oomphKnobs[1].onValueChange = [this] {
    ampEnvData.setDefaultValue(static_cast<float>(oomphKnobs[1].getValue()) /
                               100.0f);
    bakeAmpLut();
    envelopeCurveEditor.repaint();
  };
  const bool controlled = ampEnvData.hasPoints();
  oomphKnobs[1].setEnabled(!controlled);
  oomphKnobs[1].setTooltip(controlled ? "Value is controlled by envelope" : "");
}

// ────────────────────────────────────────────────────
// BLENDノブ（oomphKnobs[2]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupBlendKnob() {
  oomphKnobs[2].setRange(-100.0, 100.0, 1.0);
  oomphKnobs[2].setValue(0.0, juce::dontSendNotification);
  oomphKnobs[2].setDoubleClickReturnValue(true, 0.0);
  oomphKnobs[2].onValueChange = [this] {
    const float blendNorm =
        static_cast<float>(oomphKnobs[2].getValue()) / 100.0f;
    processorRef.oomphOscillator().setBlend(blendNorm);
    envelopeCurveEditor.setPreviewBlend(blendNorm);
  };
}

// ────────────────────────────────────────────────────
// H1〜H4ノブ（oomphKnobs[4〜7]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupHarmonicKnobs() {
  for (int i = 0; i < 4; ++i) {
    const auto idx = static_cast<size_t>(i + 4);
    oomphKnobs[idx].setRange(0.0, 1.0, 0.01);
    oomphKnobs[idx].setValue(0.0, juce::dontSendNotification);
    oomphKnobs[idx].setDoubleClickReturnValue(true, 0.0);
    const int harmonicNum = i + 1;
    oomphKnobs[idx].onValueChange = [this, idx, harmonicNum] {
      const auto gain = static_cast<float>(oomphKnobs[idx].getValue());
      processorRef.oomphOscillator().setHarmonicGain(harmonicNum, gain);
      envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, gain);
    };
  }
}

// ────────────────────────────────────────────────────
// resized() レイアウトヘルパー
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::layoutOomphKnobsRow(
    juce::Rectangle<int> knobRow) {
  const int slotW = knobRow.getWidth() / 8;
  for (int i = 0; i < 8; ++i) {
    const auto idx = static_cast<size_t>(i);
    auto slot = knobRow.removeFromLeft(slotW);
    oomphKnobLabels[idx].setBounds(slot.removeFromBottom(18));
    oomphKnobs[idx].setBounds(slot);
  }
}

void BabySquatchAudioProcessorEditor::layoutWaveShapeButtonRow(
    juce::Rectangle<int> btnRow) {
  constexpr int btnW = 64;
  constexpr int gap = 6;
  constexpr int totalW = btnW * 3 + gap * 2;
  auto row = btnRow.withSizeKeepingCentre(totalW, 22);
  for (size_t i = 0; i < 3; ++i) {
    waveShapeButtons[i].setBounds(row.removeFromLeft(btnW));
    if (i < 2)
      row.removeFromLeft(gap);
  }
}

// ────────────────────────────────────────────────────
// コンストラクター
// ────────────────────────────────────────────────────
BabySquatchAudioProcessorEditor::BabySquatchAudioProcessorEditor(
    BabySquatchAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState()) {
  addAndMakeVisible(oomphPanel);
  addAndMakeVisible(clickPanel);
  addAndMakeVisible(dryPanel);
  addChildComponent(expandableArea);
  addChildComponent(keyboard);
  addChildComponent(envelopeCurveEditor);

  // 展開ボタン → 親に通知
  oomphPanel.setOnExpandRequested(
      [this] { requestExpand(ExpandChannel::oomph); });
  clickPanel.setOnExpandRequested(
      [this] { requestExpand(ExpandChannel::click); });
  dryPanel.setOnExpandRequested([this] { requestExpand(ExpandChannel::dry); });

  // 鍵盤モード変更コールバック
  keyboard.setOnModeChanged([this, &p](KeyboardComponent::Mode mode) {
    p.setFixedModeActive(mode == KeyboardComponent::Mode::fixed);
    updateEnvelopeEditorVisibility();
  });

  pitchEnvData.setDefaultValue(200.0f);

  setupPanelRouting(p);
  setupEnvelopeCurveEditor();
  setupOomphKnobsRow();
  setupWaveShapeButtons();
  setupPitchKnob();
  setupAmpKnob();
  setupBlendKnob();
  setupHarmonicKnobs();

  setSize(UIConstants::windowWidth, UIConstants::windowHeight);
}

BabySquatchAudioProcessorEditor::~BabySquatchAudioProcessorEditor() = default;

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

    // 1. 鍵盤を展開エリア下部に配置（モードボタン + 鍵盤本体）
    keyboard.setBounds(expandArea.removeFromBottom(
        UIConstants::keyboardHeight + UIConstants::modeButtonHeight));

    // 2. 全チャンネル共通: パラメータノブ行のスペースを上部から確保
    //    （OOMPH以外ではノブは非表示だが、波形エリアの高さを揃えるため常に確保）
    {
      auto knobRow = expandArea.removeFromTop(UIConstants::oomphKnobRowHeight);
      if (activeChannel == oomph)
        layoutOomphKnobsRow(knobRow);
    }

    // 3. 波形選択ボタン行（OOMPH のみ表示）
    {
      auto btnRow =
          expandArea.removeFromTop(UIConstants::waveShapeButtonRowHeight);
      if (activeChannel == oomph)
        layoutWaveShapeButtonRow(btnRow);
    }

    // 4. エンベロープカーブエディタ → 残り全域
    envelopeCurveEditor.setBounds(expandArea);
    area.removeFromBottom(UIConstants::panelGap);
  }

  // 残りを3パネルで均等分割
  const int panelWidth = (area.getWidth() - UIConstants::panelGap * 2) / 3;

  oomphPanel.setBounds(area.removeFromLeft(panelWidth));
  area.removeFromLeft(UIConstants::panelGap);

  clickPanel.setBounds(area.removeFromLeft(panelWidth));
  area.removeFromLeft(UIConstants::panelGap);

  dryPanel.setBounds(area);
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
  oomphPanel.setExpandIndicator(activeChannel == oomph);
  clickPanel.setExpandIndicator(activeChannel == click);
  dryPanel.setExpandIndicator(activeChannel == dry);
}

void BabySquatchAudioProcessorEditor::updateEnvelopeEditorVisibility() {
  using enum ExpandChannel;
  const bool isOpen = (activeChannel != none);
  envelopeCurveEditor.setVisible(isOpen);

  const bool oomphOpen = (activeChannel == oomph);
  for (size_t i = 0; i < 8; ++i) {
    oomphKnobs[i].setVisible(oomphOpen);
    oomphKnobLabels[i].setVisible(oomphOpen);
  }
  for (size_t i = 0; i < 3; ++i)
    waveShapeButtons[i].setVisible(oomphOpen);
}
