// DirectParams.cpp
// Direct panel UI setup / layout
#include "../PluginEditor.h"
#include "LutBaker.h"
#include "WaveformUtils.h"

namespace {
void styleDirectKnob(juce::Slider &s, ColouredSliderLAF &laf) {
  s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 1, 1);
  s.setLookAndFeel(&laf);
}
void styleKnobLabelDirect(juce::Label &label, const juce::String &text,
                          const juce::Font &font) {
  label.setText(text, juce::dontSendNotification);
  label.setFont(font);
  label.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  label.setJustificationType(juce::Justification::centred);
}
} // namespace

void BabySquatchAudioProcessorEditor::setupDirectParams() {
  const auto smallFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));

  // ── Mode label ──
  directUI.modeLabel.setText("mode:", juce::dontSendNotification);
  directUI.modeLabel.setFont(smallFont);
  directUI.modeLabel.setColour(juce::Label::textColourId,
                               UIConstants::Colours::labelText);
  directUI.modeLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(directUI.modeLabel);

  // ── Mode combo ──
  directUI.modeCombo.addItem("Direct",
                             static_cast<int>(DirectUI::Mode::Direct));
  directUI.modeCombo.addItem("Sample",
                             static_cast<int>(DirectUI::Mode::Sample));
  directUI.modeCombo.setSelectedId(static_cast<int>(DirectUI::Mode::Direct),
                                   juce::dontSendNotification);
  directUI.modeCombo.setLookAndFeel(&darkComboLAF);
  addAndMakeVisible(directUI.modeCombo);

  // ── Sample load button ──
  directUI.sampleLoadButton.setColour(
      juce::TextButton::buttonColourId,
      UIConstants::Colours::panelBg.brighter(0.15f));
  directUI.sampleLoadButton.setColour(juce::TextButton::textColourOffId,
                                      UIConstants::Colours::labelText);
  directUI.sampleLoadButton.setVisible(false);
  directUI.sampleLoadButton.onClick = [this] { onSampleLoadClicked(); };
  directUI.sampleLoadButton.setOnFileDropped(
      [this](const juce::File &file) { onSampleFileChosen(file); });
  addAndMakeVisible(directUI.sampleLoadButton);

  // ── Mode コンボ変更時: ボタン表示切り替え ──
  directUI.modeCombo.onChange = [this] {
    const bool isSample = directUI.modeCombo.getSelectedId() ==
                          static_cast<int>(DirectUI::Mode::Sample);
    directUI.sampleLoadButton.setVisible(isSample);
    if (!isSample)
      directUI.sampleLoadButton.setButtonText("Drop or Click to Load");
    processorRef.setDirectSampleMode(isSample);
    resized();
  };
  // ── 8 ノブ セットアップ ──
  const auto knobFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));

  // Pitch: -24 〜 +24 半音
  styleDirectKnob(directUI.pitch.slider, directKnobLAF);
  directUI.pitch.slider.setRange(-24.0, 24.0, 1.0);
  directUI.pitch.slider.setDoubleClickReturnValue(true, 0.0);
  directUI.pitch.slider.setValue(0.0, juce::dontSendNotification);
  directUI.pitch.slider.onValueChange = [this] {
    processorRef.directEngine().setPitchSemitones(
        static_cast<float>(directUI.pitch.slider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.pitch.slider);
  styleKnobLabelDirect(directUI.pitch.label, "Pitch", knobFont);
  addAndMakeVisible(directUI.pitch.label);

  // Amp: 0 〜 200%（LUT 経由で DSP へ反映 — Click の Amp と同一方式）
  styleDirectKnob(directUI.amp.slider, directKnobLAF);
  directUI.amp.slider.setRange(0.0, 200.0, 0.1);
  directUI.amp.slider.setDoubleClickReturnValue(true, 100.0);
  directUI.amp.slider.setValue(100.0, juce::dontSendNotification);
  directUI.amp.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::directAmp);
  };
  directUI.amp.slider.onValueChange = [this] {
    const float v =
        static_cast<float>(directUI.amp.slider.getValue()) / 100.0f;
    envDatas.directAmp.setDefaultValue(v);
    if (!envDatas.directAmp.isEnvelopeControlled())
      envDatas.directAmp.setPointValue(0, v);
    bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
            processorRef.directEngine().directAmpLut().getDurationMs());
    refreshDirectProvider();
  };
  // 初期デフォルトポイント（1点：ノブ制御状態）
  envDatas.directAmp.addPoint(0.0f, envDatas.directAmp.getDefaultValue());
  {
    const bool controlled = envDatas.directAmp.isEnvelopeControlled();
    directUI.amp.slider.setEnabled(!controlled);
    directUI.amp.slider.setTooltip(
        controlled ? "Click on Amp label to edit envelope" : "");
  }
  styleKnobLabelDirect(directUI.amp.label, "Amp", knobFont);
  directUI.amp.label.addMouseListener(this, false);
  addAndMakeVisible(directUI.amp.slider);
  addAndMakeVisible(directUI.amp.label);

  // Drive: 0 〜 24 dB
  styleDirectKnob(directUI.saturator.driveSlider, directKnobLAF);
  directUI.saturator.driveSlider.setRange(0.0, 24.0, 0.1);
  directUI.saturator.driveSlider.setDoubleClickReturnValue(true, 0.0);
  directUI.saturator.driveSlider.setValue(0.0, juce::dontSendNotification);
  directUI.saturator.driveSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 1) + " dB";
  };
  directUI.saturator.driveSlider.onValueChange = [this] {
    processorRef.directEngine().setDriveDb(
        static_cast<float>(directUI.saturator.driveSlider.getValue()));
  };
  addAndMakeVisible(directUI.saturator.driveSlider);

  // ClipType セレクター（Soft / Hard / Tube）— Drive ノブ上部ラベルを兼ねる
  directUI.saturator.clipType.setOnChange(
      [this](int t) { processorRef.directEngine().setClipType(t); });
  addAndMakeVisible(directUI.saturator.clipType);

  // Decay: 10 〜 2000 ms（LUT の再生期間を制御 — Click の Sample Decay と同一）
  styleDirectKnob(directUI.decay.slider, directKnobLAF);
  directUI.decay.slider.setRange(10.0, 2000.0, 1.0);
  directUI.decay.slider.setSkewFactorFromMidPoint(300.0);
  directUI.decay.slider.setDoubleClickReturnValue(true, 300.0);
  directUI.decay.slider.setValue(300.0, juce::dontSendNotification);
  directUI.decay.slider.textFromValueFunction = [](double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " ms"
                      : juce::String(v / 1000.0, 2) + " s";
  };
  directUI.decay.slider.onValueChange = [this] {
    const auto durMs =
        static_cast<float>(directUI.decay.slider.getValue());
    bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
            durMs);
    refreshDirectProvider();
  };
  // 起動時に LUT 期間を初期値へ反映
  bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
          300.0f);
  addAndMakeVisible(directUI.decay.slider);
  styleKnobLabelDirect(directUI.decay.label, "Decay", knobFont);
  addAndMakeVisible(directUI.decay.label);

  // HPF: SlopeSelector (label) + freq knob + Q knob
  directUI.hpfSlope.setOnChange(
      [this](int dboct) { processorRef.directEngine().setHpfSlope(dboct); });
  addAndMakeVisible(directUI.hpfSlope);

  styleDirectKnob(directUI.hpfSlider, directKnobLAF);
  directUI.hpfSlider.setRange(20.0, 20000.0, 1.0);
  directUI.hpfSlider.setSkewFactorFromMidPoint(1000.0);
  directUI.hpfSlider.setTextValueSuffix(" Hz");
  directUI.hpfSlider.setValue(20.0, juce::dontSendNotification);
  directUI.hpfSlider.setDoubleClickReturnValue(true, 20.0);
  directUI.hpfSlider.onValueChange = [this] {
    processorRef.directEngine().setHpfFreq(
        static_cast<float>(directUI.hpfSlider.getValue()));
  };
  addAndMakeVisible(directUI.hpfSlider);

  styleDirectKnob(directUI.hpfQSlider, directKnobLAF);
  directUI.hpfQSlider.setRange(0.1, 6.0, 0.01);
  directUI.hpfQSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.hpfQSlider.setValue(0.707, juce::dontSendNotification);
  directUI.hpfQSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.hpfQSlider.onValueChange = [this] {
    processorRef.directEngine().setHpfQ(
        static_cast<float>(directUI.hpfQSlider.getValue()));
  };
  addAndMakeVisible(directUI.hpfQSlider);
  styleKnobLabelDirect(directUI.hpfQLabel, "Q", knobFont);
  addAndMakeVisible(directUI.hpfQLabel);

  // LPF: SlopeSelector (label) + freq knob + Q knob
  directUI.lpfSlope.setOnChange(
      [this](int dboct) { processorRef.directEngine().setLpfSlope(dboct); });
  addAndMakeVisible(directUI.lpfSlope);

  styleDirectKnob(directUI.lpfSlider, directKnobLAF);
  directUI.lpfSlider.setRange(20.0, 20000.0, 1.0);
  directUI.lpfSlider.setSkewFactorFromMidPoint(1000.0);
  directUI.lpfSlider.setTextValueSuffix(" Hz");
  directUI.lpfSlider.setValue(20000.0, juce::dontSendNotification);
  directUI.lpfSlider.setDoubleClickReturnValue(true, 20000.0);
  directUI.lpfSlider.onValueChange = [this] {
    processorRef.directEngine().setLpfFreq(
        static_cast<float>(directUI.lpfSlider.getValue()));
  };
  addAndMakeVisible(directUI.lpfSlider);

  styleDirectKnob(directUI.lpfQSlider, directKnobLAF);
  directUI.lpfQSlider.setRange(0.1, 6.0, 0.01);
  directUI.lpfQSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.lpfQSlider.setValue(0.707, juce::dontSendNotification);
  directUI.lpfQSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.lpfQSlider.onValueChange = [this] {
    processorRef.directEngine().setLpfQ(
        static_cast<float>(directUI.lpfQSlider.getValue()));
  };
  addAndMakeVisible(directUI.lpfQSlider);
  styleKnobLabelDirect(directUI.lpfQLabel, "Q", knobFont);
  addAndMakeVisible(directUI.lpfQLabel);

  // ── 起動時デフォルト値を DSP へ反映 ──
  processorRef.directEngine().setPitchSemitones(0.0f);
  processorRef.directEngine().setDriveDb(0.0f);
  processorRef.directEngine().setClipType(0);
  processorRef.directEngine().setHpfFreq(20.0f); // 20Hz = バイパス
  processorRef.directEngine().setHpfQ(0.707f);
  processorRef.directEngine().setHpfSlope(12);
  processorRef.directEngine().setLpfFreq(20000.0f); // 20kHz = バイパス
  processorRef.directEngine().setLpfQ(0.707f);
  processorRef.directEngine().setLpfSlope(12);
}

void BabySquatchAudioProcessorEditor::layoutDirectParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ [+ サンプルロードボタン]
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);
  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 68;
  directUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  topRow.removeFromLeft(2);
  directUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));

  if (const bool isSample = directUI.modeCombo.getSelectedId() ==
                            static_cast<int>(DirectUI::Mode::Sample);
      isSample) {
    topRow.removeFromLeft(4);
    directUI.sampleLoadButton.setBounds(topRow);
  }

  // 残りエリア: 2 行 × 4 列グリッド
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;

  // 上段行: Pitch / Amp / Drive(+ClipType) / Decay
  {
    const std::array<juce::Slider *, 4> rowKnobs = {{
        &directUI.pitch.slider,
        &directUI.amp.slider,
        &directUI.saturator.driveSlider,
        &directUI.decay.slider,
    }};
    const std::array<juce::Component *, 4> rowLabels = {{
        &directUI.pitch.label,
        &directUI.amp.label,
        &directUI.saturator.clipType,
        &directUI.decay.label,
    }};
    for (int col = 0; col < 4; ++col) {
      const auto col_u = static_cast<std::size_t>(col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY(), slotW, rowH);
      rowLabels[col_u]->setBounds(slot.removeFromBottom(14));
      rowKnobs[col_u]->setBounds(slot);
    }
  }

  // 下段行: HP (slope label + freq knob) | Q | LP (slope label + freq knob)
  // | Q
  //   Click と同パターン: SlopeSelector はノブのラベル位置を占める
  {
    const std::array<juce::Slider *, 4> rowKnobs = {{
        &directUI.hpfSlider,
        &directUI.hpfQSlider,
        &directUI.lpfSlider,
        &directUI.lpfQSlider,
    }};
    const std::array<juce::Component *, 4> rowLabels = {{
        &directUI.hpfSlope,
        &directUI.hpfQLabel,
        &directUI.lpfSlope,
        &directUI.lpfQLabel,
    }};
    for (int col = 0; col < 4; ++col) {
      const auto col_u = static_cast<std::size_t>(col);
      juce::Rectangle slot(area.getX() + col * slotW, area.getY() + rowH, slotW,
                           rowH);
      rowLabels[col_u]->setBounds(slot.removeFromBottom(14));
      rowKnobs[col_u]->setBounds(slot);
    }
  }
}

void BabySquatchAudioProcessorEditor::onSampleLoadClicked() {
  directUI.fileChooser = std::make_unique<juce::FileChooser>(
      "Load Sample",
      juce::File::getSpecialLocation(juce::File::userMusicDirectory),
      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  directUI.fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser &fc) {
        const auto result = fc.getResult();
        if (result.existsAsFile())
          onSampleFileChosen(result);
      });
}

void BabySquatchAudioProcessorEditor::onSampleFileChosen(
    const juce::File &file) {
  directUI.loadedFilePath = file.getFullPathName();
  directUI.sampleLoadButton.setButtonText(file.getFileNameWithoutExtension());
  directUI.sampleLoadButton.setTooltip(directUI.loadedFilePath);
  processorRef.directEngine().sampler().loadSample(file);

  // サムネイルデータをメンバーに保存してプロバイダーを登録
  if (!processorRef.directEngine().sampler().copyThumbnail(directUI.thumbMin,
                                                           directUI.thumbMax))
    return;
  directUI.thumbDurSec = processorRef.directEngine().sampler().durationSec();
  refreshDirectProvider();
}

void BabySquatchAudioProcessorEditor::refreshDirectProvider() {
  if (directUI.thumbMin.empty() || directUI.thumbDurSec <= 0.0)
    return;

  // Pitch (semitones) → 再生速度倍率
  const auto semitones = static_cast<float>(directUI.pitch.slider.getValue());
  const float speedRatio = std::pow(2.0f, semitones / 12.0f);
  const double durSec = directUI.thumbDurSec / static_cast<double>(speedRatio);

  // LUT 期間 (ms) + Amp (0〜2.0)
  const float ampDurMs =
      processorRef.directEngine().directAmpLut().getDurationMs();
  const float ampScale =
      static_cast<float>(directUI.amp.slider.getValue()) / 100.0f;

  auto minPtr = std::make_shared<std::vector<float>>(directUI.thumbMin);
  auto maxPtr = std::make_shared<std::vector<float>>(directUI.thumbMax);

  envelopeCurveEditor.setDirectProvider(
      [minPtr, maxPtr, durSec, ampDurMs, ampScale](float timeSec) {
        return WaveformUtils::computeLutPreview(*minPtr, *maxPtr, durSec,
                                                ampDurMs, ampScale, timeSec);
      });
}
