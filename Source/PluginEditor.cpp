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
    bakeDistLut();
    bakeBlendLut();
    // AMP / PITCH / DIST / BLEND ノブの有効無効 (envelope 有無に連動)
    oomphKnobs[1].setEnabled(!ampEnvData.hasPoints());
    oomphKnobs[1].setTooltip(
        ampEnvData.hasPoints() ? "Value is controlled by envelope" : "");
    oomphKnobs[0].setEnabled(!pitchEnvData.hasPoints());
    oomphKnobs[0].setTooltip(
        pitchEnvData.hasPoints() ? "Value is controlled by envelope" : "");
    oomphKnobs[3].setEnabled(!distEnvData.hasPoints());
    oomphKnobs[3].setTooltip(
        distEnvData.hasPoints() ? "Value is controlled by envelope" : "");
    oomphKnobs[2].setEnabled(!blendEnvData.hasPoints());
    oomphKnobs[2].setTooltip(
        blendEnvData.hasPoints() ? "Value is controlled by envelope" : "");
  });

  envelopeCurveEditor.setOnEditTargetChanged(
      [this](EnvelopeCurveEditor::EditTarget target) {
        using enum EnvelopeCurveEditor::EditTarget;
        const auto kLabel = UIConstants::Colours::labelText;
        oomphKnobLabels[0].setColour(juce::Label::textColourId,
                                     target == pitch ? juce::Colours::cyan
                                                     : kLabel);
        oomphKnobLabels[1].setColour(
            juce::Label::textColourId,
            target == amp ? UIConstants::Colours::oomphArc : kLabel);
        oomphKnobLabels[3].setColour(juce::Label::textColourId,
                                     target == dist ? juce::Colour(0xFFFF9500)
                                                    : kLabel);
        oomphKnobLabels[2].setColour(juce::Label::textColourId,
                                     target == blend ? juce::Colour(0xFF4CAF50)
                                                     : kLabel);
      });

}

// ────────────────────────────────────────────────────
// OOMPH Osc ノブ行（8本）初期化
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupLengthBox() {
  const auto smallFont = juce::Font(juce::FontOptions(10.0f));

  lengthPrefixLabel.setText("length:", juce::dontSendNotification);
  lengthPrefixLabel.setFont(smallFont);
  lengthPrefixLabel.setColour(juce::Label::textColourId,
                              juce::Colours::white.withAlpha(0.6f));
  lengthPrefixLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(lengthPrefixLabel);

  lengthEditor.setFont(smallFont);
  lengthEditor.setText("300", false);
  lengthEditor.setInputRestrictions(4, "0123456789");
  lengthEditor.setJustification(juce::Justification::centred);
  lengthEditor.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colours::black.withAlpha(0.45f));
  lengthEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
  lengthEditor.setColour(juce::TextEditor::outlineColourId,
                         juce::Colours::white.withAlpha(0.20f));
  lengthEditor.setColour(juce::TextEditor::focusedOutlineColourId,
                         juce::Colours::white.withAlpha(0.5f));
  auto applyLength = [this]() {
    const int v = juce::jlimit(10, 2000, lengthEditor.getText().getIntValue());
    lengthEditor.setText(juce::String(v), false);
    envelopeCurveEditor.setDisplayDurationMs(static_cast<float>(v));
    processorRef.setOomphLengthMs(static_cast<float>(v));
    bakeAmpLut();
    bakePitchLut();
    bakeDistLut();
    bakeBlendLut();
  };
  lengthEditor.onReturnKey = applyLength;
  lengthEditor.onFocusLost = applyLength;
  addAndMakeVisible(lengthEditor);

  lengthSuffixLabel.setText("ms", juce::dontSendNotification);
  lengthSuffixLabel.setFont(smallFont);
  lengthSuffixLabel.setColour(juce::Label::textColourId,
                              juce::Colours::white.withAlpha(0.6f));
  lengthSuffixLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(lengthSuffixLabel);
}

void BabySquatchAudioProcessorEditor::setupOomphKnobsRow() {
  static constexpr std::array<const char *, 8> kLabels = {
      "PITCH", "AMP", "BLEND", "DIST", "H1", "H2", "H3", "H4"};
  for (size_t i = 0; i < 8; ++i) {
    auto &knob = oomphKnobs[i];
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0);
    knob.setLookAndFeel(&oomphKnobLAF);
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
    blendEnvData.setDefaultValue(blendNorm);
    bakeBlendLut();
    envelopeCurveEditor.setPreviewBlend(blendNorm);
  };
  // 初期 LUT ベイク（デフォルト値 0.0）
  blendEnvData.setDefaultValue(0.0f);
  bakeBlendLut();
}

// ────────────────────────────────────────────────────
// H1〜H4ノブ（oomphKnobs[4〜7]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupDistKnob() {
  oomphKnobs[3].setRange(0.0, 100.0, 1.0);
  oomphKnobs[3].setValue(0.0, juce::dontSendNotification);
  oomphKnobs[3].setDoubleClickReturnValue(true, 0.0);
  oomphKnobs[3].onValueChange = [this] {
    const float drive01 = static_cast<float>(oomphKnobs[3].getValue()) / 100.0f;
    distEnvData.setDefaultValue(drive01);
    bakeDistLut();
  };
  // 初期 LUT ベイク（デフォルト値 0.0）
  distEnvData.setDefaultValue(0.0f);
  bakeDistLut();
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
  constexpr int btnW   = 64;
  constexpr int btnGap = 6;
  constexpr int rowH   = 22;

  // Length ボックスを左端に配置
  constexpr int prefixW = 44;
  constexpr int editorW = 34;
  constexpr int suffixW = 18;
  constexpr int innerGap = 2;
  constexpr int lengthTotalW = prefixW + editorW + suffixW + innerGap * 2;
  auto lengthArea = btnRow.removeFromLeft(lengthTotalW).withSizeKeepingCentre(lengthTotalW, rowH);
  int lx = lengthArea.getX();
  const int ly = lengthArea.getY();
  lengthPrefixLabel.setBounds(lx, ly, prefixW, rowH);
  lx += prefixW + innerGap;
  lengthEditor.setBounds(lx, ly, editorW, rowH);
  lx += editorW + innerGap;
  lengthSuffixLabel.setBounds(lx, ly, suffixW, rowH);

  // Tri/SQR/SAW ボタンを残り領域に中央揃えで配置
  constexpr int totalW = btnW * 3 + btnGap * 2;
  auto row = btnRow.withSizeKeepingCentre(totalW, rowH);
  for (size_t i = 0; i < 3; ++i) {
    waveShapeButtons[i].setBounds(row.removeFromLeft(btnW));
    if (i < 2)
      row.removeFromLeft(btnGap);
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

  pitchEnvData.setDefaultValue(200.0f);

  setupPanelRouting(p);
  setupEnvelopeCurveEditor();
  setupOomphKnobsRow();
  setupWaveShapeButtons();
  setupLengthBox();
  setupPitchKnob();
  setupAmpKnob();
  setupBlendKnob();
  setupDistKnob();
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

    // 1. 鍵盤を展開エリア下部に配置
    keyboard.setBounds(
        expandArea.removeFromBottom(UIConstants::keyboardHeight));

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
