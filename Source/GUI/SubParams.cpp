// SubParamKnobs.cpp
// BabySquatchAudioProcessorEditor の SUB
// チャンネル専用ノブ・ボタン設定メソッド群。 Click / Direct の実装時は同様に
// ClickParamKnobs.cpp / DirectParamKnobs.cpp を作成する。

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "LutBaker.h"

// ────────────────────────────────────────────────────
// Length ボックス
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupLengthBox() {
  const auto tinyFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));

  subUI.length.label.setText("length:", juce::dontSendNotification);
  subUI.length.label.setFont(tinyFont);
  subUI.length.label.setColour(juce::Label::textColourId,
                            UIConstants::Colours::labelText);
  subUI.length.label.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(subUI.length.label);

  subUI.length.slider.setSliderStyle(juce::Slider::LinearHorizontal);
  subUI.length.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 16);
  subUI.length.slider.setColour(juce::Slider::backgroundColourId,
                             UIConstants::Colours::waveformBg);
  subUI.length.slider.setColour(juce::Slider::trackColourId,
                             UIConstants::Colours::subArc.withAlpha(0.45f));
  subUI.length.slider.setColour(juce::Slider::thumbColourId,
                             UIConstants::Colours::subArc);
  subUI.length.slider.setColour(juce::Slider::textBoxTextColourId,
                             juce::Colours::white.withAlpha(0.90f));
  subUI.length.slider.setColour(juce::Slider::textBoxBackgroundColourId,
                             UIConstants::Colours::waveformBg);
  subUI.length.slider.setColour(juce::Slider::textBoxOutlineColourId,
                             juce::Colours::transparentBlack);
  subUI.length.slider.setRange(10.0, 2000.0, 1.0);
  subUI.length.slider.setTextValueSuffix(" ms");
  subUI.length.slider.setValue(300.0, juce::dontSendNotification);
  subUI.length.slider.onValueChange = [this] {
    const auto v = static_cast<float>(subUI.length.slider.getValue());
    envelopeCurveEditor.setDisplayDurationMs(v);
    processorRef.subEngine().setLengthMs(v);
    bakeLut(ampEnvData,   processorRef.subEngine().envLut(),   v);
    bakeLut(pitchEnvData, processorRef.subEngine().pitchLut(), v);
    bakeLut(distEnvData,  processorRef.subEngine().distLut(),  v);
    bakeLut(blendEnvData, processorRef.subEngine().blendLut(), v);
  };
  addAndMakeVisible(subUI.length.slider);
}

// ────────────────────────────────────────────────────
// SUB Osc ノブ行（8本）初期化
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupSubKnobsRow() {
  static constexpr std::array<const char *, 8> kLabels = {
      "Gain", "Freq", "Mix", "Saturate", "Tone1", "Tone2", "Tone3", "Tone4"};
  for (size_t i = 0; i < 8; ++i) {
    auto &knob = subUI.knobs[i];
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 1, 1);
    knob.setRange(0.0, 1.0);
    knob.setLookAndFeel(&subKnobLAF);
    addAndMakeVisible(knob);

    auto &label = subUI.knobLabels[i];
    label.setText(kLabels[i], juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(UIConstants::fontSizeSmall)));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
    addAndMakeVisible(label);
    // エンベロープ制御中でもラベルクリックで editTarget
    // を切り替えられるよう登録
    if (i < 4)
      label.addMouseListener(this, false);
  }
}

// ────────────────────────────────────────────────────
// 波形選択コンボボックス（Tri / SQR / SAW）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupWaveShapeCombo() {
  const auto smallFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));
  subUI.wave.label.setText("wave:", juce::dontSendNotification);
  subUI.wave.label.setFont(smallFont);
  subUI.wave.label.setColour(juce::Label::textColourId,
                          UIConstants::Colours::labelText);
  subUI.wave.label.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(subUI.wave.label);

  subUI.wave.combo.addItem("Tri", 1);
  subUI.wave.combo.addItem("SQR", 2);
  subUI.wave.combo.addItem("SAW", 3);
  subUI.wave.combo.setSelectedId(1, juce::dontSendNotification);
  subUI.wave.combo.setLookAndFeel(&darkComboLAF);
  subUI.wave.combo.onChange = [this] {
    using enum WaveShape;
    WaveShape shape;
    switch (subUI.wave.combo.getSelectedId()) {
    case 2:
      shape = Square;
      break;
    case 3:
      shape = Saw;
      break;
    default:
      shape = Tri;
      break;
    }
    processorRef.subEngine().oscillator().setWaveShape(shape);
    envelopeCurveEditor.setWaveShape(shape);
  };
  addAndMakeVisible(subUI.wave.combo);
}

// ────────────────────────────────────────────────────
// Freq ノブ（subUI.knobs[1]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupPitchKnob() {
  subUI.knobs[1].setRange(20.0, 20000.0);
  subUI.knobs[1].setSkewFactorFromMidPoint(200.0);
  subUI.knobs[1].setValue(200.0, juce::dontSendNotification);
  subUI.knobs[1].setDoubleClickReturnValue(true, 200.0);
  subUI.knobs[1].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::freq);
  };
  subUI.knobs[1].onValueChange = [this] {
    const auto hz = static_cast<float>(subUI.knobs[1].getValue());
    pitchEnvData.setDefaultValue(hz);
    if (!pitchEnvData.isEnvelopeControlled())
      pitchEnvData.setPointValue(0, hz);
    bakeLut(pitchEnvData, processorRef.subEngine().pitchLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    const float cycles =
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f;
    envelopeCurveEditor.setDisplayCycles(cycles);
    envelopeCurveEditor.repaint();
  };
  // 初期サイクル数 + Pitch LUT 初期ベイク
  constexpr float initHz = 200.0f;
  envelopeCurveEditor.setDisplayCycles(
      initHz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  bakeLut(pitchEnvData, processorRef.subEngine().pitchLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  // 初期デフォルトポイント（1点：ノブ制御状態）
  pitchEnvData.addPoint(0.0f, initHz);
}

// ────────────────────────────────────────────────────
// Gain ノブ（subUI.knobs[0]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupAmpKnob() {
  subUI.knobs[0].setRange(0.0, 200.0);
  subUI.knobs[0].setValue(ampEnvData.getDefaultValue() * 100.0,
                       juce::dontSendNotification);
  subUI.knobs[0].setDoubleClickReturnValue(true, 100.0);
  subUI.knobs[0].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::gain);
  };
  subUI.knobs[0].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[0].getValue()) / 100.0f;
    ampEnvData.setDefaultValue(v);
    if (!ampEnvData.isEnvelopeControlled())
      ampEnvData.setPointValue(0, v);
    bakeLut(ampEnvData, processorRef.subEngine().envLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.repaint();
  };
  // 初期デフォルトポイント（1点：ノブ制御状態）
  ampEnvData.addPoint(0.0f, ampEnvData.getDefaultValue());
  const bool controlled = ampEnvData.isEnvelopeControlled();
  subUI.knobs[0].setEnabled(!controlled);
  subUI.knobs[0].setTooltip(controlled ? "Value is controlled by envelope" : "");
}

// ────────────────────────────────────────────────────
// Mix ノブ（subUI.knobs[2]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupBlendKnob() {
  subUI.knobs[2].setRange(-100.0, 100.0, 1.0);
  subUI.knobs[2].setValue(0.0, juce::dontSendNotification);
  subUI.knobs[2].setDoubleClickReturnValue(true, 0.0);
  subUI.knobs[2].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::mix);
  };
  subUI.knobs[2].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[2].getValue()) / 100.0f;
    blendEnvData.setDefaultValue(v);
    if (!blendEnvData.isEnvelopeControlled())
      blendEnvData.setPointValue(0, v);
    bakeLut(blendEnvData, processorRef.subEngine().blendLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.setPreviewBlend(v);
  };
  blendEnvData.setDefaultValue(0.0f);
  bakeLut(blendEnvData, processorRef.subEngine().blendLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  // 初期デフォルトポイント（1点：ノブ制御状態）
  blendEnvData.addPoint(0.0f, 0.0f);
}

// ────────────────────────────────────────────────────
// Saturate ノブ（subUI.knobs[3]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupDistKnob() {
  subUI.knobs[3].setRange(0.0, 100.0, 1.0);
  subUI.knobs[3].setValue(0.0, juce::dontSendNotification);
  subUI.knobs[3].setDoubleClickReturnValue(true, 0.0);
  subUI.knobs[3].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::saturate);
  };
  subUI.knobs[3].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[3].getValue()) / 100.0f;
    distEnvData.setDefaultValue(v);
    if (!distEnvData.isEnvelopeControlled())
      distEnvData.setPointValue(0, v);
    bakeLut(distEnvData, processorRef.subEngine().distLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.repaint();
  };
  distEnvData.setDefaultValue(0.0f);
  bakeLut(distEnvData, processorRef.subEngine().distLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  // 初期デフォルトポイント（1点：ノブ制御状態）
  distEnvData.addPoint(0.0f, 0.0f);
}

// ────────────────────────────────────────────────────
// Tone1〜Tone4 ノブ（subUI.knobs[4〜7]）
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupHarmonicKnobs() {
  for (int i = 0; i < 4; ++i) {
    const auto idx = static_cast<size_t>(i + 4);
    subUI.knobs[idx].setRange(0.0, 100.0, 0.1);
    subUI.knobs[idx].setValue(25.0, juce::dontSendNotification);
    subUI.knobs[idx].setDoubleClickReturnValue(true, 25.0);
    const int harmonicNum = i + 1;
    subUI.knobs[idx].onValueChange = [this, idx, harmonicNum] {
      const auto gain = static_cast<float>(subUI.knobs[idx].getValue()) / 100.0f;
      processorRef.subEngine().oscillator().setHarmonicGain(harmonicNum, gain);
      envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, gain);
    };
    // 起動時デフォルト値を DSP へ反映
    processorRef.subEngine().oscillator().setHarmonicGain(harmonicNum, 0.25f);
    envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, 0.25f);
  }
}

// ────────────────────────────────────────────────────
// レイアウトヘルパー
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::layoutSubKnobsRow(
    juce::Rectangle<int> area) {
  // 上段ノブ: Gain, Freq, Mix, Saturate
  // 下段ノブ: Tone1, Tone2, Tone3, Tone4
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(row * 4 + col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY() + row * rowH,
                           slotW, rowH);
      subUI.knobLabels[idx].setBounds(slot.removeFromBottom(16));
      subUI.knobs[idx].setBounds(slot);
    }
  }
}

void BabySquatchAudioProcessorEditor::layoutLengthBox(
    juce::Rectangle<int> area) {
  constexpr int labelW = 36;
  subUI.length.label.setBounds(area.removeFromLeft(labelW));
  subUI.length.slider.setBounds(area);
}
