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
  directUI.sample.loadButton.setColour(
      juce::TextButton::buttonColourId,
      UIConstants::Colours::panelBg.brighter(0.15f));
  directUI.sample.loadButton.setColour(juce::TextButton::textColourOffId,
                                       UIConstants::Colours::labelText);
  directUI.sample.loadButton.setVisible(false);
  directUI.sample.loadButton.onClick = [this] { onSampleLoadClicked(); };
  directUI.sample.loadButton.setOnFileDropped(
      [this](const juce::File &file) { onSampleFileChosen(file); });
  addAndMakeVisible(directUI.sample.loadButton);

  // ── Mode コンボ変更時: ボタン表示切り替え ──
  directUI.modeCombo.onChange = [this] {
    const bool isSample = directUI.modeCombo.getSelectedId() ==
                          static_cast<int>(DirectUI::Mode::Sample);
    directUI.sample.loadButton.setVisible(isSample);
    if (!isSample)
      directUI.sample.loadButton.setButtonText("Drop or Click to Load");
    processorRef.setDirectSampleMode(isSample);
    refreshDirectPassthroughUI();
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
    const float v = static_cast<float>(directUI.amp.slider.getValue()) / 100.0f;
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
    const auto durMs = static_cast<float>(directUI.decay.slider.getValue());
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
  directUI.hpf.slope.setOnChange(
      [this](int dboct) { processorRef.directEngine().setHpfSlope(dboct); });
  addAndMakeVisible(directUI.hpf.slope);

  styleDirectKnob(directUI.hpf.slider, directKnobLAF);
  directUI.hpf.slider.setRange(20.0, 20000.0, 1.0);
  directUI.hpf.slider.setSkewFactorFromMidPoint(1000.0);
  directUI.hpf.slider.setTextValueSuffix(" Hz");
  directUI.hpf.slider.setValue(20.0, juce::dontSendNotification);
  directUI.hpf.slider.setDoubleClickReturnValue(true, 20.0);
  directUI.hpf.slider.onValueChange = [this] {
    processorRef.directEngine().setHpfFreq(
        static_cast<float>(directUI.hpf.slider.getValue()));
  };
  addAndMakeVisible(directUI.hpf.slider);

  styleDirectKnob(directUI.hpf.qSlider, directKnobLAF);
  directUI.hpf.qSlider.setRange(0.1, 18.0, 0.01);
  directUI.hpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.hpf.qSlider.setValue(0.707, juce::dontSendNotification);
  directUI.hpf.qSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.hpf.qSlider.onValueChange = [this] {
    processorRef.directEngine().setHpfQ(
        static_cast<float>(directUI.hpf.qSlider.getValue()));
  };
  addAndMakeVisible(directUI.hpf.qSlider);
  styleKnobLabelDirect(directUI.hpf.qLabel, "Q", knobFont);
  addAndMakeVisible(directUI.hpf.qLabel);

  // LPF: SlopeSelector (label) + freq knob + Q knob
  directUI.lpf.slope.setOnChange(
      [this](int dboct) { processorRef.directEngine().setLpfSlope(dboct); });
  addAndMakeVisible(directUI.lpf.slope);

  styleDirectKnob(directUI.lpf.slider, directKnobLAF);
  directUI.lpf.slider.setRange(20.0, 20000.0, 1.0);
  directUI.lpf.slider.setSkewFactorFromMidPoint(1000.0);
  directUI.lpf.slider.setTextValueSuffix(" Hz");
  directUI.lpf.slider.setValue(20000.0, juce::dontSendNotification);
  directUI.lpf.slider.setDoubleClickReturnValue(true, 20000.0);
  directUI.lpf.slider.onValueChange = [this] {
    processorRef.directEngine().setLpfFreq(
        static_cast<float>(directUI.lpf.slider.getValue()));
  };
  addAndMakeVisible(directUI.lpf.slider);

  styleDirectKnob(directUI.lpf.qSlider, directKnobLAF);
  directUI.lpf.qSlider.setRange(0.1, 18.0, 0.01);
  directUI.lpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.lpf.qSlider.setValue(0.707, juce::dontSendNotification);
  directUI.lpf.qSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.lpf.qSlider.onValueChange = [this] {
    processorRef.directEngine().setLpfQ(
        static_cast<float>(directUI.lpf.qSlider.getValue()));
  };
  addAndMakeVisible(directUI.lpf.qSlider);
  styleKnobLabelDirect(directUI.lpf.qLabel, "Q", knobFont);
  addAndMakeVisible(directUI.lpf.qLabel);

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

  // ── Threshold ノブ（パススルーモード時に Pitch 位置へ表示） ──
  styleDirectKnob(directUI.threshold.slider, directKnobLAF);
  directUI.threshold.slider.setRange(-60.0, 0.0, 0.1);
  directUI.threshold.slider.setDoubleClickReturnValue(true, -24.0);
  directUI.threshold.slider.setValue(-24.0, juce::dontSendNotification);
  directUI.threshold.slider.textFromValueFunction = [](double v) {
    return juce::String(v, 1) + " dB";
  };
  directUI.threshold.slider.onValueChange = [this] {
    processorRef.directMode().detector().setThresholdDb(
        static_cast<float>(directUI.threshold.slider.getValue()));
  };
  addAndMakeVisible(directUI.threshold.slider);
  styleKnobLabelDirect(directUI.threshold.label, "Thresh", knobFont);
  addAndMakeVisible(directUI.threshold.label);

  // ── Hold スライダー（mode ドロップダウン右） ──
  directUI.hold.slider.setSliderStyle(juce::Slider::LinearHorizontal);
  directUI.hold.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48,
                                       18);
  directUI.hold.slider.setRange(20.0, 500.0, 1.0);
  directUI.hold.slider.setDoubleClickReturnValue(true, 50.0);
  directUI.hold.slider.setValue(50.0, juce::dontSendNotification);
  directUI.hold.slider.textFromValueFunction = [](double v) {
    return juce::String(juce::roundToInt(v)) + " ms";
  };
  directUI.hold.slider.onValueChange = [this] {
    processorRef.directMode().detector().setHoldMs(
        static_cast<float>(directUI.hold.slider.getValue()));
  };
  directUI.hold.slider.setColour(juce::Slider::backgroundColourId,
                                 juce::Colour(0xFF333333));
  directUI.hold.slider.setColour(juce::Slider::trackColourId,
                                 UIConstants::Colours::directArc);
  directUI.hold.slider.setColour(juce::Slider::thumbColourId,
                                 UIConstants::Colours::directThumb);
  directUI.hold.slider.setColour(juce::Slider::textBoxTextColourId,
                                 UIConstants::Colours::text);
  directUI.hold.slider.setColour(juce::Slider::textBoxBackgroundColourId,
                                 juce::Colour(0xFF333333));
  directUI.hold.slider.setColour(juce::Slider::textBoxOutlineColourId,
                                 juce::Colours::transparentBlack);
  addAndMakeVisible(directUI.hold.slider);
  directUI.hold.label.setText("Hold", juce::dontSendNotification);
  directUI.hold.label.setFont(smallFont);
  directUI.hold.label.setColour(juce::Label::textColourId,
                                UIConstants::Colours::labelText);
  directUI.hold.label.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(directUI.hold.label);

  // 起動時のパススルー UI 状態を設定
  refreshDirectPassthroughUI();
}

void BabySquatchAudioProcessorEditor::layoutDirectParams(
    juce::Rectangle<int> area) {
  const bool isPassthrough = processorRef.directMode().isPassthrough();

  // 上段: mode ラベル + コンボ [+ Hold / サンプルロードボタン]
  auto topRow = area.removeFromTop(22);
  area.removeFromTop(4);
  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 68;
  directUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  topRow.removeFromLeft(2);
  directUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));

  if (isPassthrough) {
    // Hold ラベル + スライダーを mode 右に配置
    topRow.removeFromLeft(4);
    constexpr int holdLabelW = 30;
    directUI.hold.label.setBounds(topRow.removeFromLeft(holdLabelW));
    topRow.removeFromLeft(2);
    directUI.hold.slider.setBounds(topRow);
  } else {
    topRow.removeFromLeft(4);
    directUI.sample.loadButton.setBounds(topRow);
  }

  // 残りエリア: 2 行 × 4 列グリッド
  const int slotW = area.getWidth() / 4;
  const int rowH = area.getHeight() / 2;

  // 上段行: [Pitch or Threshold] / Amp / Drive(+ClipType) / Decay
  {
    // パススルー時: Threshold を Pitch 位置に表示
    juce::Slider *col0Knob =
        isPassthrough ? &directUI.threshold.slider : &directUI.pitch.slider;
    juce::Component *col0Label =
        isPassthrough
            ? static_cast<juce::Component *>(&directUI.threshold.label)
            : static_cast<juce::Component *>(&directUI.pitch.label);

    const std::array<juce::Slider *, 4> rowKnobs = {{
        col0Knob,
        &directUI.amp.slider,
        &directUI.saturator.driveSlider,
        &directUI.decay.slider,
    }};
    const std::array<juce::Component *, 4> rowLabels = {{
        col0Label,
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
  {
    const std::array<juce::Slider *, 4> rowKnobs = {{
        &directUI.hpf.slider,
        &directUI.hpf.qSlider,
        &directUI.lpf.slider,
        &directUI.lpf.qSlider,
    }};
    const std::array<juce::Component *, 4> rowLabels = {{
        &directUI.hpf.slope,
        &directUI.hpf.qLabel,
        &directUI.lpf.slope,
        &directUI.lpf.qLabel,
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

void BabySquatchAudioProcessorEditor::refreshDirectPassthroughUI() {
  const bool isPassthrough = processorRef.directMode().isPassthrough();

  // Pitch ⇔ Threshold 表示切り替え
  directUI.pitch.slider.setVisible(!isPassthrough);
  directUI.pitch.label.setVisible(!isPassthrough);
  directUI.threshold.slider.setVisible(isPassthrough);
  directUI.threshold.label.setVisible(isPassthrough);

  // Hold スライダー: パススルーモード時のみ表示
  directUI.hold.label.setVisible(isPassthrough);
  directUI.hold.slider.setVisible(isPassthrough);

  // Auto Trigger: パススルーモード時は自動有効、サンプルモード時は無効
  processorRef.directMode().detector().setEnabled(isPassthrough);
}

void BabySquatchAudioProcessorEditor::onSampleLoadClicked() {
  directUI.sample.fileChooser = std::make_unique<juce::FileChooser>(
      "Load Sample",
      juce::File::getSpecialLocation(juce::File::userMusicDirectory),
      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  directUI.sample.fileChooser->launchAsync(
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
  directUI.sample.loadedFilePath = file.getFullPathName();
  directUI.sample.loadButton.setButtonText(file.getFileNameWithoutExtension());
  directUI.sample.loadButton.setTooltip(directUI.sample.loadedFilePath);
  processorRef.directEngine().sampler().loadSample(file);

  // サムネイルデータをメンバーに保存してプロバイダーを登録
  if (!processorRef.directEngine().sampler().copyThumbnail(
          directUI.sample.thumbMin, directUI.sample.thumbMax))
    return;
  directUI.sample.thumbDurSec =
      processorRef.directEngine().sampler().durationSec();
  refreshDirectProvider();
}

void BabySquatchAudioProcessorEditor::refreshDirectProvider() {
  if (directUI.sample.thumbMin.empty() || directUI.sample.thumbDurSec <= 0.0)
    return;

  // Pitch (semitones) → 再生速度倍率
  const auto semitones = static_cast<float>(directUI.pitch.slider.getValue());
  const float speedRatio = std::pow(2.0f, semitones / 12.0f);
  const double durSec =
      directUI.sample.thumbDurSec / static_cast<double>(speedRatio);

  // LUT 期間 (ms) + Amp (0、2.0)
  const float ampDurMs =
      processorRef.directEngine().directAmpLut().getDurationMs();
  const float ampScale =
      static_cast<float>(directUI.amp.slider.getValue()) / 100.0f;

  auto minPtr = std::make_shared<std::vector<float>>(directUI.sample.thumbMin);
  auto maxPtr = std::make_shared<std::vector<float>>(directUI.sample.thumbMax);

  envelopeCurveEditor.setDirectProvider(
      [minPtr, maxPtr, durSec, ampDurMs, ampScale](float timeSec) {
        return WaveformUtils::computeLutPreview(*minPtr, *maxPtr, durSec,
                                                ampDurMs, ampScale, timeSec);
      });
}
