// SubParamKnobs.cpp
// BoomBabyAudioProcessorEditor の SUB
// チャンネル専用ノブ・ボタン設定メソッド群。 Click / Direct の実装時は同様に
// ClickParamKnobs.cpp / DirectParamKnobs.cpp を作成する。

#include "InfoBoxText.h"
#include "LutBaker.h"
#include "ParamIDs.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

// ────────────────────────────────────────────────────
// Length ボックス
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupLengthBox() {
  const auto tinyFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));

  subUI.length.label.setText("length:", juce::dontSendNotification);
  subUI.length.label.setFont(tinyFont);
  subUI.length.label.setColour(juce::Label::textColourId,
                               UIConstants::Colours::labelText);
  subUI.length.label.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(subUI.length.label);

  subUI.length.slider.setSliderStyle(juce::Slider::LinearHorizontal);
  subUI.length.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54,
                                      16);
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
  subUI.length.slider.textFromValueFunction = [](double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " ms"
                      : juce::String(v / 1000.0, 2) + " s";
  };
  subUI.length.slider.setDoubleClickReturnValue(true, 300.0);
  subUI.length.slider.setValue(300.0, juce::dontSendNotification);
  subUI.length.slider.onValueChange = [this] {
    const auto v = static_cast<float>(subUI.length.slider.getValue());
    syncParam(ParamIDs::subLength, v);
    updateDisplayDuration();
    processorRef.subEngine().setLengthMs(v);
    // Sub LUT: エンベロープ実効区間に 512 点を集中させる
    bakeLut(envDatas.amp, processorRef.subEngine().envLut(),
            effectiveLutDuration(envDatas.amp, v));
    bakeLut(envDatas.freq, processorRef.subEngine().freqLut(),
            effectiveLutDuration(envDatas.freq, v));
    bakeLut(envDatas.dist, processorRef.subEngine().distLut(),
            effectiveLutDuration(envDatas.dist, v));
    bakeLut(envDatas.mix, processorRef.subEngine().mixLut(),
            effectiveLutDuration(envDatas.mix, v));
  };
  // Click/Direct Decay の初期 LUT ベイク
  const auto initLen =
      static_cast<float>(subUI.length.slider.getValue()); // 300 ms
  clickUI.sample.decay.slider.setValue(static_cast<double>(initLen),
                                       juce::dontSendNotification);
  bakeLut(envDatas.clickAmp, processorRef.clickEngine().clickAmpLut(),
          effectiveLutDuration(envDatas.clickAmp, initLen));
  envelopeCurveEditor.setClickDecayMs(initLen);
  directUI.decay.slider.setValue(static_cast<double>(initLen),
                                 juce::dontSendNotification);
  bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
          effectiveLutDuration(envDatas.directAmp, initLen));
  addAndMakeVisible(subUI.length.slider);

  InfoBox::setInfo(subUI.length.slider, InfoText::subLength);
}

// ────────────────────────────────────────────────────
// SUB Osc ノブ行（8本）初期化
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupSubKnobsRow() {
  static constexpr std::array<const char *, 8> kLabels = {
      "Amp", "Freq", "Mix", "Saturate", "Tone1", "Tone2", "Tone3", "Tone4"};
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

  // ── Freq ノブ（subUI.knobs[1]） ──
  subUI.knobs[1].setComponentID("subFreq");
  subUI.knobs[1].setRange(20.0, 20000.0);
  subUI.knobs[1].setSkewFactorFromMidPoint(200.0);
  subUI.knobs[1].setValue(200.0, juce::dontSendNotification);
  subUI.knobs[1].setDoubleClickReturnValue(true, 200.0);
  subUI.knobs[1].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::freq);
  };
  subUI.knobs[1].onValueChange = [this] {
    const auto hz = static_cast<float>(subUI.knobs[1].getValue());
    syncParam(ParamIDs::subFreq, hz);
    envDatas.freq.setDefaultValue(hz);
    if (!envDatas.freq.isEnvelopeControlled())
      envDatas.freq.setPointValue(0, hz);
    bakeLut(envDatas.freq, processorRef.subEngine().freqLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    const float cycles =
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f;
    envelopeCurveEditor.setDisplayCycles(cycles);
    envelopeCurveEditor.repaint();
  };
  constexpr float initHz = 200.0f;
  envelopeCurveEditor.setDisplayCycles(
      initHz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  bakeLut(envDatas.freq, processorRef.subEngine().freqLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  envDatas.freq.addPoint(0.0f, initHz);

  // ── Amp ノブ（subUI.knobs[0]） ──
  subUI.knobs[0].setRange(0.0, 200.0);
  subUI.knobs[0].setValue(envDatas.amp.getDefaultValue() * 100.0,
                          juce::dontSendNotification);
  subUI.knobs[0].setDoubleClickReturnValue(true, 100.0);
  subUI.knobs[0].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::amp);
  };
  subUI.knobs[0].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[0].getValue()) / 100.0f;
    syncParam(ParamIDs::subAmp, static_cast<float>(subUI.knobs[0].getValue()));
    envDatas.amp.setDefaultValue(v);
    if (!envDatas.amp.isEnvelopeControlled())
      envDatas.amp.setPointValue(0, v);
    bakeLut(envDatas.amp, processorRef.subEngine().envLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.repaint();
  };
  envDatas.amp.addPoint(0.0f, envDatas.amp.getDefaultValue());
  {
    const bool controlled = envDatas.amp.isEnvelopeControlled();
    subUI.knobs[0].setEnabled(!controlled);
    subUI.knobs[0].setTooltip(controlled ? "Click on Amp label to edit envelope"
                                         : "");
  }

  // ── Mix ノブ（subUI.knobs[2]） ──
  subUI.knobs[2].setRange(-100.0, 100.0, 1.0);
  subUI.knobs[2].setValue(0.0, juce::dontSendNotification);
  subUI.knobs[2].setDoubleClickReturnValue(true, 0.0);
  subUI.knobs[2].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::mix);
  };
  subUI.knobs[2].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[2].getValue()) / 100.0f;
    syncParam(ParamIDs::subMix, static_cast<float>(subUI.knobs[2].getValue()));
    envDatas.mix.setDefaultValue(v);
    if (!envDatas.mix.isEnvelopeControlled())
      envDatas.mix.setPointValue(0, v);
    bakeLut(envDatas.mix, processorRef.subEngine().mixLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.setPreviewMix(v);
  };
  envDatas.mix.setDefaultValue(0.0f);
  bakeLut(envDatas.mix, processorRef.subEngine().mixLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  envDatas.mix.addPoint(0.0f, 0.0f);

  // ── Saturate ノブ（subUI.knobs[3]） ──
  subUI.knobs[3].setRange(0.0, 24.0, 0.1);
  subUI.knobs[3].setValue(0.0, juce::dontSendNotification);
  subUI.knobs[3].setDoubleClickReturnValue(true, 0.0);
  subUI.knobs[3].onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::saturate);
  };
  subUI.knobs[3].onValueChange = [this] {
    const float v = static_cast<float>(subUI.knobs[3].getValue()) / 24.0f;
    syncParam(ParamIDs::subSatDrive,
              static_cast<float>(subUI.knobs[3].getValue()));
    envDatas.dist.setDefaultValue(v);
    if (!envDatas.dist.isEnvelopeControlled())
      envDatas.dist.setPointValue(0, v);
    bakeLut(envDatas.dist, processorRef.subEngine().distLut(),
            envelopeCurveEditor.getDisplayDurationMs());
    envelopeCurveEditor.repaint();
  };
  envDatas.dist.setDefaultValue(0.0f);
  bakeLut(envDatas.dist, processorRef.subEngine().distLut(),
          envelopeCurveEditor.getDisplayDurationMs());
  envDatas.dist.addPoint(0.0f, 0.0f);

  // ClipType セレクター（Soft / Hard / Tube）— Saturate ノブ上部ラベルを兼ねる
  subUI.saturateClipType.setOnChange([this](int t) {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::saturate);
    processorRef.subEngine().oscillator().setClipType(t);
    syncParam(ParamIDs::subSatClipType, static_cast<float>(t));
  });
  addAndMakeVisible(subUI.saturateClipType);

  // ── Tone1〜Tone4 ノブ（subUI.knobs[4〜7]） ──
  for (int i = 0; i < 4; ++i) {
    const auto idx = static_cast<size_t>(i + 4);
    subUI.knobs[idx].setRange(0.0, 100.0, 0.1);
    subUI.knobs[idx].setValue(25.0, juce::dontSendNotification);
    subUI.knobs[idx].setDoubleClickReturnValue(true, 25.0);
    const int harmonicNum = i + 1;
    subUI.knobs[idx].onDragStart = [this] {
      switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
    };
    subUI.knobs[idx].onValueChange = [this, idx, harmonicNum] {
      const auto gain =
          static_cast<float>(subUI.knobs[idx].getValue()) / 100.0f;
      processorRef.subEngine().oscillator().setHarmonicGain(harmonicNum, gain);
      envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, gain);
      static constexpr std::array<const char *, 4> kToneIDs = {
          ParamIDs::subTone1, ParamIDs::subTone2, ParamIDs::subTone3,
          ParamIDs::subTone4};
      syncParam(kToneIDs[static_cast<std::size_t>(harmonicNum - 1)],
                static_cast<float>(subUI.knobs[idx].getValue()));
    };
    processorRef.subEngine().oscillator().setHarmonicGain(harmonicNum, 0.25f);
    envelopeCurveEditor.setPreviewHarmonicGain(harmonicNum, 0.25f);
  }

  // InfoBox descriptions
  InfoBox::setInfo(subUI.knobs[0], InfoText::subAmp);
  InfoBox::setInfo(subUI.knobs[1], InfoText::subFreq);
  InfoBox::setInfo(subUI.knobs[2], InfoText::subMix);
  InfoBox::setInfo(subUI.knobs[3], InfoText::subSaturate);
  InfoBox::setInfo(subUI.saturateClipType, InfoText::subClipType);
  InfoBox::setInfo(subUI.knobs[4], InfoText::subTone2);
  InfoBox::setInfo(subUI.knobs[5], InfoText::subTone3);
  InfoBox::setInfo(subUI.knobs[6], InfoText::subTone4);
  InfoBox::setInfo(subUI.knobs[7], InfoText::subTone5);
}

// ────────────────────────────────────────────────────
// 波形選択コンボボックス（Tri / SQR / SAW）
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupWaveShapeCombo() {
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
    syncParam(ParamIDs::subWaveShape,
              static_cast<float>(subUI.wave.combo.getSelectedId() - 1));
  };
  addAndMakeVisible(subUI.wave.combo);

  InfoBox::setInfo(subUI.wave.combo, InfoText::subWave);
}

void BoomBabyAudioProcessorEditor::layoutSubKnobsRow(
    juce::Rectangle<int> area) {
  // 上段ノブ: Amp, Freq, Mix, Saturate
  // 下段ノブ: Tone1, Tone2, Tone3, Tone4
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      const auto idx = static_cast<size_t>(row * 4 + col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY() + row * rowH,
                           slotW, rowH);
      // Saturate (idx==3): LabelSelector をラベル代わりに使う
      if (idx == 3) {
        subUI.saturateClipType.setBounds(slot.removeFromBottom(16));
        subUI.knobLabels[idx].setVisible(false);
      } else {
        subUI.knobLabels[idx].setBounds(slot.removeFromBottom(16));
      }
      subUI.knobs[idx].setBounds(slot);
    }
  }
}
