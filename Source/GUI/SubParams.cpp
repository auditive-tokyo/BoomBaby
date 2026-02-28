// SubParamKnobs.cpp
// BabySquatchAudioProcessorEditor の SUB
// チャンネル専用ノブ・ボタン設定メソッド群。 Click / Direct の実装時は同様に
// ClickParamKnobs.cpp / DirectParamKnobs.cpp を作成する。

#include "PluginEditor.h"
#include "PluginProcessor.h"

// ────────────────────────────────────────────────────
// Length ボックス
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupLengthBox() {
  const auto smallFont = juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));

  lengthBox.prefix.setText("length:", juce::dontSendNotification);
  lengthBox.prefix.setFont(smallFont);
  lengthBox.prefix.setColour(juce::Label::textColourId,
                             UIConstants::Colours::labelText);
  lengthBox.prefix.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(lengthBox.prefix);

  lengthBox.editor.setFont(smallFont);
  lengthBox.editor.setText("300", false);
  lengthBox.editor.setInputRestrictions(4, "0123456789");
  lengthBox.editor.setJustification(juce::Justification::centred);
  lengthBox.editor.setColour(juce::TextEditor::backgroundColourId,
                             UIConstants::Colours::knobBg);
  lengthBox.editor.setColour(juce::TextEditor::textColourId,
                             UIConstants::Colours::text);
  lengthBox.editor.setColour(juce::TextEditor::outlineColourId,
                             juce::Colours::white.withAlpha(0.20f));
  lengthBox.editor.setColour(juce::TextEditor::focusedOutlineColourId,
                             juce::Colours::white.withAlpha(0.5f));
  auto applyLength = [this]() {
    const int v = juce::jlimit(10, 2000, lengthBox.editor.getText().getIntValue());
    lengthBox.editor.setText(juce::String(v), false);
    envelopeCurveEditor.setDisplayDurationMs(static_cast<float>(v));
    processorRef.setSubLengthMs(static_cast<float>(v));
    bakeAmpLut();
    bakePitchLut();
    bakeDistLut();
    bakeBlendLut();
  };
  lengthBox.editor.onReturnKey = applyLength;
  lengthBox.editor.onFocusLost = applyLength;
  addAndMakeVisible(lengthBox.editor);

  lengthBox.suffix.setText("ms", juce::dontSendNotification);
  lengthBox.suffix.setFont(smallFont);
  lengthBox.suffix.setColour(juce::Label::textColourId,
                             UIConstants::Colours::labelText);
  lengthBox.suffix.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(lengthBox.suffix);
}

// ────────────────────────────────────────────────────
// SUB Osc ノブ行（8本）初期化
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupSubKnobsRow() {
  static constexpr std::array<const char *, 8> kLabels = {
      "PITCH", "AMP", "BLEND", "DIST", "H1", "H2", "H3", "H4"};
  for (size_t i = 0; i < 8; ++i) {
    auto &knob = subKnobs[i];
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0);
    knob.setLookAndFeel(&subKnobLAF);
    addAndMakeVisible(knob);

    auto &label = subKnobLabels[i];
    label.setText(kLabels[i], juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(UIConstants::fontSizeSmall)));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
    addAndMakeVisible(label);
  }
}

// ────────────────────────────────────────────────────
// 波形選択コンボボックス（Sine / Tri / SQR / SAW）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupWaveShapeCombo() {
  waveShapeCombo.addItem("Sine", 1);
  waveShapeCombo.addItem("Tri", 2);
  waveShapeCombo.addItem("SQR", 3);
  waveShapeCombo.addItem("SAW", 4);
  waveShapeCombo.setSelectedId(1, juce::dontSendNotification);
  waveShapeCombo.setLookAndFeel(&darkComboLAF);
  waveShapeCombo.onChange = [this] {
    using enum WaveShape;
    WaveShape shape;
    switch (waveShapeCombo.getSelectedId()) {
    case 2: shape = Tri;    break;
    case 3: shape = Square; break;
    case 4: shape = Saw;    break;
    default: shape = Sine;  break;
    }
    processorRef.subOscillator().setWaveShape(shape);
    envelopeCurveEditor.setWaveShape(shape);
  };
  addAndMakeVisible(waveShapeCombo);
}

// ────────────────────────────────────────────────────
// PITCH ノブ（subKnobs[0]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupPitchKnob() {
  subKnobs[0].setRange(20.0, 20000.0);
  subKnobs[0].setSkewFactorFromMidPoint(200.0);
  subKnobs[0].setValue(200.0, juce::dontSendNotification);
  subKnobs[0].setDoubleClickReturnValue(true, 200.0);
  subKnobs[0].onValueChange = [this] {
    const auto hz = static_cast<float>(subKnobs[0].getValue());
    pitchEnvData.setDefaultValue(hz);
    if (!pitchEnvData.isEnvelopeControlled())
      pitchEnvData.setPointValue(0, hz);
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
  // 初期デフォルトポイント（1点：ノブ制御状態）
  pitchEnvData.addPoint(0.0f, initHz);
}

// ────────────────────────────────────────────────────
// AMP ノブ（subKnobs[1]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupAmpKnob() {
  subKnobs[1].setRange(0.0, 200.0);
  subKnobs[1].setValue(ampEnvData.getDefaultValue() * 100.0,
                       juce::dontSendNotification);
  subKnobs[1].setDoubleClickReturnValue(true, 100.0);
  subKnobs[1].onValueChange = [this] {
    const float v = static_cast<float>(subKnobs[1].getValue()) / 100.0f;
    ampEnvData.setDefaultValue(v);
    if (!ampEnvData.isEnvelopeControlled())
      ampEnvData.setPointValue(0, v);
    bakeAmpLut();
    envelopeCurveEditor.repaint();
  };
  // 初期デフォルトポイント（1点：ノブ制御状態）
  ampEnvData.addPoint(0.0f, ampEnvData.getDefaultValue());
  const bool controlled = ampEnvData.isEnvelopeControlled();
  subKnobs[1].setEnabled(!controlled);
  subKnobs[1].setTooltip(controlled ? "Value is controlled by envelope" : "");
}

// ────────────────────────────────────────────────────
// BLEND ノブ（subKnobs[2]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupBlendKnob() {
  subKnobs[2].setRange(-100.0, 100.0, 1.0);
  subKnobs[2].setValue(0.0, juce::dontSendNotification);
  subKnobs[2].setDoubleClickReturnValue(true, 0.0);
  subKnobs[2].onValueChange = [this] {
    const float v = static_cast<float>(subKnobs[2].getValue()) / 100.0f;
    blendEnvData.setDefaultValue(v);
    if (!blendEnvData.isEnvelopeControlled())
      blendEnvData.setPointValue(0, v);
    bakeBlendLut();
    envelopeCurveEditor.setPreviewBlend(v);
  };
  blendEnvData.setDefaultValue(0.0f);
  bakeBlendLut();
  // 初期デフォルトポイント（1点：ノブ制御状態）
  blendEnvData.addPoint(0.0f, 0.0f);
}

// ────────────────────────────────────────────────────
// DIST ノブ（subKnobs[3]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupDistKnob() {
  subKnobs[3].setRange(0.0, 100.0, 1.0);
  subKnobs[3].setValue(0.0, juce::dontSendNotification);
  subKnobs[3].setDoubleClickReturnValue(true, 0.0);
  subKnobs[3].onValueChange = [this] {
    const float v = static_cast<float>(subKnobs[3].getValue()) / 100.0f;
    distEnvData.setDefaultValue(v);
    if (!distEnvData.isEnvelopeControlled())
      distEnvData.setPointValue(0, v);
    bakeDistLut();
    envelopeCurveEditor.repaint();
  };
  distEnvData.setDefaultValue(0.0f);
  bakeDistLut();
  // 初期デフォルトポイント（1点：ノブ制御状態）
  distEnvData.addPoint(0.0f, 0.0f);
}

// ────────────────────────────────────────────────────
// H1〜H4 ノブ（subKnobs[4〜7]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupHarmonicKnobs() {
  for (int i = 0; i < 4; ++i) {
    const auto idx = static_cast<size_t>(i + 4);
    subKnobs[idx].setRange(0.0, 1.0, 0.01);
    subKnobs[idx].setValue(0.0, juce::dontSendNotification);
    subKnobs[idx].setDoubleClickReturnValue(true, 0.0);
    const int harmonicNum = i + 1;
    subKnobs[idx].onValueChange = [this, idx, harmonicNum] {
      const auto gain = static_cast<float>(subKnobs[idx].getValue());
      processorRef.subOscillator().setHarmonicGain(harmonicNum, gain);
      envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, gain);
    };
  }
}

// ────────────────────────────────────────────────────
// レイアウトヘルパー
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::layoutSubKnobsRow(
    juce::Rectangle<int> area) {
  // 上段ノブ: PITCH, AMP, BLEND, DIST
  // 下段ノブ: H1,   H2,  H3,   H4
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(row * 4 + col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY() + row * rowH,
                           slotW, rowH);
      subKnobLabels[idx].setBounds(slot.removeFromBottom(16));
      subKnobs[idx].setBounds(slot);
    }
  }
}

void BabySquatchAudioProcessorEditor::layoutLengthBox(
    juce::Rectangle<int> btnRow) {
  constexpr int rowH = 22;
  constexpr int prefixW = 44;
  constexpr int editorW = 34;
  constexpr int suffixW = 18;
  constexpr int innerGap = 2;
  constexpr int totalW = prefixW + editorW + suffixW + innerGap * 2;

  auto la = btnRow.removeFromLeft(totalW).withSizeKeepingCentre(totalW, rowH);
  int lx = la.getX();
  const int ly = la.getY();
  lengthBox.prefix.setBounds(lx, ly, prefixW, rowH);
  lx += prefixW + innerGap;
  lengthBox.editor.setBounds(lx, ly, editorW, rowH);
  lx += editorW + innerGap;
  lengthBox.suffix.setBounds(lx, ly, suffixW, rowH);
}
