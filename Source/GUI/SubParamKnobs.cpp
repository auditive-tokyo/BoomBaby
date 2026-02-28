// SubParamKnobs.cpp
// BabySquatchAudioProcessorEditor の SUB チャンネル専用ノブ・ボタン設定メソッド群。
// Click / Direct の実装時は同様に ClickParamKnobs.cpp / DirectParamKnobs.cpp を作成する。

#include "PluginEditor.h"
#include "PluginProcessor.h"

// ────────────────────────────────────────────────────
// Length ボックス
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
    processorRef.setSubLengthMs(static_cast<float>(v));
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
    addChildComponent(knob);

    auto &label = subKnobLabels[i];
    label.setText(kLabels[i], juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f)));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
    addChildComponent(label);
  }
}

// ────────────────────────────────────────────────────
// 波形選択ボタン（Tri / SQR / SAW）
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
                  UIConstants::Colours::subArc);
    btn.setColour(juce::TextButton::textColourOffId,
                  UIConstants::Colours::text);
    btn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    const WaveShape shape = kShapes[i];
    btn.onClick = [this, i, shape] {
      if (waveShapeButtons[i].getToggleState()) {
        deselectOtherWaveShapeButtons(i);
        processorRef.subOscillator().setWaveShape(shape);
        envelopeCurveEditor.setWaveShape(shape);
      } else {
        processorRef.subOscillator().setWaveShape(WaveShape::Sine);
        envelopeCurveEditor.setWaveShape(WaveShape::Sine);
      }
    };
    addChildComponent(btn);
  }
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
// AMP ノブ（subKnobs[1]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupAmpKnob() {
  subKnobs[1].setRange(0.0, 200.0);
  subKnobs[1].setValue(ampEnvData.getDefaultValue() * 100.0,
                       juce::dontSendNotification);
  subKnobs[1].setDoubleClickReturnValue(true, 100.0);
  subKnobs[1].onValueChange = [this] {
    ampEnvData.setDefaultValue(static_cast<float>(subKnobs[1].getValue()) /
                               100.0f);
    bakeAmpLut();
    envelopeCurveEditor.repaint();
  };
  const bool controlled = ampEnvData.hasPoints();
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
    const float blendNorm =
        static_cast<float>(subKnobs[2].getValue()) / 100.0f;
    blendEnvData.setDefaultValue(blendNorm);
    bakeBlendLut();
    envelopeCurveEditor.setPreviewBlend(blendNorm);
  };
  blendEnvData.setDefaultValue(0.0f);
  bakeBlendLut();
}

// ────────────────────────────────────────────────────
// DIST ノブ（subKnobs[3]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupDistKnob() {
  subKnobs[3].setRange(0.0, 100.0, 1.0);
  subKnobs[3].setValue(0.0, juce::dontSendNotification);
  subKnobs[3].setDoubleClickReturnValue(true, 0.0);
  subKnobs[3].onValueChange = [this] {
    const float drive01 = static_cast<float>(subKnobs[3].getValue()) / 100.0f;
    distEnvData.setDefaultValue(drive01);
    bakeDistLut();
    envelopeCurveEditor.repaint();
  };
  distEnvData.setDefaultValue(0.0f);
  bakeDistLut();
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
    juce::Rectangle<int> knobRow) {
  const int slotW = knobRow.getWidth() / 8;
  for (int i = 0; i < 8; ++i) {
    const auto idx = static_cast<size_t>(i);
    auto slot = knobRow.removeFromLeft(slotW);
    subKnobLabels[idx].setBounds(slot.removeFromBottom(18));
    subKnobs[idx].setBounds(slot);
  }
}

void BabySquatchAudioProcessorEditor::layoutWaveShapeButtonRow(
    juce::Rectangle<int> btnRow) {
  constexpr int btnW     = 64;
  constexpr int btnGap   = 6;
  constexpr int rowH     = 22;
  constexpr int prefixW  = 44;
  constexpr int editorW  = 34;
  constexpr int suffixW  = 18;
  constexpr int innerGap = 2;
  constexpr int lengthTotalW = prefixW + editorW + suffixW + innerGap * 2;

  // Length ボックスを左端に配置
  auto lengthArea =
      btnRow.removeFromLeft(lengthTotalW).withSizeKeepingCentre(lengthTotalW, rowH);
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
