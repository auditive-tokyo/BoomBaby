// DirectParams.cpp
// Direct panel UI setup / layout
#include "../DSP/Saturator.h"
#include "../ParamIDs.h"
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

void BoomBabyAudioProcessorEditor::setupDirectParams() {
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
                             std::to_underlying(DirectUI::Mode::Direct));
  directUI.modeCombo.addItem("Sample",
                             std::to_underlying(DirectUI::Mode::Sample));
  directUI.modeCombo.setSelectedId(std::to_underlying(DirectUI::Mode::Direct),
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
  directUI.sample.loadButton.setOnClear([this] {
    processorRef.directEngine().sampler().unloadSample();
    directUI.sample.loadedFilePath.clear();
    directUI.sample.thumbMin.clear();
    directUI.sample.thumbMax.clear();
    directUI.sample.thumbDurSec = 0.0;
    directUI.sample.loadButton.setButtonText("Drop or Click to Load");
    directUI.sample.loadButton.setTooltip({});
    directUI.sample.loadButton.setHasFile(false);
    processorRef.getAPVTS().state.setProperty("directSamplePath", "", nullptr);
    envelopeCurveEditor.setDirectProvider(nullptr);
  });
  addAndMakeVisible(directUI.sample.loadButton);

  // ── Mode コンボ変更時: ボタン表示切り替え ──
  directUI.modeCombo.onChange = [this] {
    const bool isSample = directUI.modeCombo.getSelectedId() ==
                          std::to_underlying(DirectUI::Mode::Sample);
    directUI.sample.loadButton.setVisible(isSample);
    if (isSample) {
      // サンプルが既にロード済みならファイル名を復元
      if (directUI.sample.loadedFilePath.isNotEmpty()) {
        directUI.sample.loadButton.setButtonText(
            juce::File(directUI.sample.loadedFilePath)
                .getFileNameWithoutExtension());
        directUI.sample.loadButton.setHasFile(true);
      } else {
        directUI.sample.loadButton.setButtonText("Drop or Click to Load");
        directUI.sample.loadButton.setHasFile(false);
      }
    }
    processorRef.setDirectSampleMode(isSample);
    syncParam(ParamIDs::directMode, isSample ? 1.0f : 0.0f);
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
  directUI.pitch.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.pitch.slider.onValueChange = [this] {
    processorRef.directEngine().setPitchSemitones(
        static_cast<float>(directUI.pitch.slider.getValue()));
    syncParam(ParamIDs::directPitch,
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
    syncParam(ParamIDs::directAmp,
              static_cast<float>(directUI.amp.slider.getValue()));
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
  directUI.saturator.driveSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.saturator.driveSlider.onValueChange = [this] {
    processorRef.directEngine().setDriveDb(
        static_cast<float>(directUI.saturator.driveSlider.getValue()));
    syncParam(ParamIDs::directDrive,
              static_cast<float>(directUI.saturator.driveSlider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.saturator.driveSlider);

  // ClipType セレクター（Soft / Hard / Tube）— Drive ノブ上部ラベルを兼ねる
  directUI.saturator.clipType.setOnChange([this](int t) {
    processorRef.directEngine().setClipType(t);
    syncParam(ParamIDs::directClipType, static_cast<float>(t));
    refreshDirectProvider();
  });
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
  directUI.decay.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.decay.slider.onValueChange = [this] {
    const auto durMs = static_cast<float>(directUI.decay.slider.getValue());
    syncParam(ParamIDs::directDecay, durMs);
    bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
            durMs);
    refreshDirectProvider();
  };
  // ※ LUT は syncUIFromState() → onEnvelopeChanged() で正しい値にベイクされる
  addAndMakeVisible(directUI.decay.slider);
  styleKnobLabelDirect(directUI.decay.label, "Decay", knobFont);
  addAndMakeVisible(directUI.decay.label);

  // HPF: SlopeSelector (label) + freq knob + Q knob
  directUI.hpf.slope.setOnChange([this](int dboct) {
    processorRef.directEngine().setHpfSlope(dboct);
    constexpr std::array kSlopes = {12, 24, 48};
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == dboct)
        syncParam(ParamIDs::directHpfSlope, static_cast<float>(i));
    refreshDirectProvider();
  });
  addAndMakeVisible(directUI.hpf.slope);

  styleDirectKnob(directUI.hpf.slider, directKnobLAF);
  directUI.hpf.slider.setRange(20.0, 20000.0, 1.0);
  directUI.hpf.slider.setSkewFactorFromMidPoint(1000.0);
  directUI.hpf.slider.setTextValueSuffix(" Hz");
  directUI.hpf.slider.setValue(20.0, juce::dontSendNotification);
  directUI.hpf.slider.setDoubleClickReturnValue(true, 20.0);
  directUI.hpf.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.hpf.slider.onValueChange = [this] {
    processorRef.directEngine().setHpfFreq(
        static_cast<float>(directUI.hpf.slider.getValue()));
    syncParam(ParamIDs::directHpfFreq,
              static_cast<float>(directUI.hpf.slider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.hpf.slider);

  styleDirectKnob(directUI.hpf.qSlider, directKnobLAF);
  directUI.hpf.qSlider.setRange(0.1, 18.0, 0.01);
  directUI.hpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.hpf.qSlider.setValue(0.707, juce::dontSendNotification);
  directUI.hpf.qSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.hpf.qSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.hpf.qSlider.onValueChange = [this] {
    processorRef.directEngine().setHpfQ(
        static_cast<float>(directUI.hpf.qSlider.getValue()));
    syncParam(ParamIDs::directHpfQ,
              static_cast<float>(directUI.hpf.qSlider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.hpf.qSlider);
  styleKnobLabelDirect(directUI.hpf.qLabel, "Q", knobFont);
  addAndMakeVisible(directUI.hpf.qLabel);

  // LPF: SlopeSelector (label) + freq knob + Q knob
  directUI.lpf.slope.setOnChange([this](int dboct) {
    processorRef.directEngine().setLpfSlope(dboct);
    constexpr std::array kSlopes = {12, 24, 48};
    for (int i = 0; i < 3; ++i)
      if (kSlopes[static_cast<std::size_t>(i)] == dboct)
        syncParam(ParamIDs::directLpfSlope, static_cast<float>(i));
    refreshDirectProvider();
  });
  addAndMakeVisible(directUI.lpf.slope);

  styleDirectKnob(directUI.lpf.slider, directKnobLAF);
  directUI.lpf.slider.setRange(20.0, 20000.0, 1.0);
  directUI.lpf.slider.setSkewFactorFromMidPoint(1000.0);
  directUI.lpf.slider.setTextValueSuffix(" Hz");
  directUI.lpf.slider.setValue(20000.0, juce::dontSendNotification);
  directUI.lpf.slider.setDoubleClickReturnValue(true, 20000.0);
  directUI.lpf.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.lpf.slider.onValueChange = [this] {
    processorRef.directEngine().setLpfFreq(
        static_cast<float>(directUI.lpf.slider.getValue()));
    syncParam(ParamIDs::directLpfFreq,
              static_cast<float>(directUI.lpf.slider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.lpf.slider);

  styleDirectKnob(directUI.lpf.qSlider, directKnobLAF);
  directUI.lpf.qSlider.setRange(0.1, 18.0, 0.01);
  directUI.lpf.qSlider.textFromValueFunction = [](double v) {
    return juce::String(v, 2);
  };
  directUI.lpf.qSlider.setValue(0.707, juce::dontSendNotification);
  directUI.lpf.qSlider.setDoubleClickReturnValue(true, 0.707);
  directUI.lpf.qSlider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.lpf.qSlider.onValueChange = [this] {
    processorRef.directEngine().setLpfQ(
        static_cast<float>(directUI.lpf.qSlider.getValue()));
    syncParam(ParamIDs::directLpfQ,
              static_cast<float>(directUI.lpf.qSlider.getValue()));
    refreshDirectProvider();
  };
  addAndMakeVisible(directUI.lpf.qSlider);
  styleKnobLabelDirect(directUI.lpf.qLabel, "Q", knobFont);
  addAndMakeVisible(directUI.lpf.qLabel);

  // ※ DSP デフォルト値の設定は不要 — プロセッサの setStateInformation で
  // 正しい値が既に適用されており、syncUIFromState() で復元される。

  // ── Threshold ノブ（パススルーモード時に Pitch 位置へ表示） ──
  styleDirectKnob(directUI.threshold.slider, directKnobLAF);
  directUI.threshold.slider.setRange(-60.0, 0.0, 0.1);
  directUI.threshold.slider.setDoubleClickReturnValue(true, -24.0);
  directUI.threshold.slider.setValue(-24.0, juce::dontSendNotification);
  directUI.threshold.slider.textFromValueFunction = [](double v) {
    return juce::String(v, 1) + " dB";
  };
  directUI.threshold.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.threshold.slider.onValueChange = [this] {
    processorRef.directMode().detector().setThresholdDb(
        static_cast<float>(directUI.threshold.slider.getValue()));
    syncParam(ParamIDs::directThreshold,
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
  directUI.hold.slider.onDragStart = [this] {
    switchEditTarget(EnvelopeCurveEditor::EditTarget::none);
  };
  directUI.hold.slider.onValueChange = [this] {
    processorRef.directMode().detector().setHoldMs(
        static_cast<float>(directUI.hold.slider.getValue()));
    syncParam(ParamIDs::directHold,
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

void BoomBabyAudioProcessorEditor::layoutDirectParams(
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
    // Hold ラベル + スライダーを mode 右に配置（残り幅いっぱい使用）
    topRow.removeFromLeft(4);
    constexpr int holdLabelW = 30;
    directUI.hold.label.setBounds(topRow.removeFromLeft(holdLabelW));
    topRow.removeFromLeft(2);
    directUI.hold.slider.setBounds(topRow); // 残り全幅
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

void BoomBabyAudioProcessorEditor::refreshDirectPassthroughUI() {
  const bool isPassthrough = processorRef.directMode().isPassthrough();

  // Pitch ⇔ Threshold 表示切り替え
  directUI.pitch.slider.setVisible(!isPassthrough);
  directUI.pitch.label.setVisible(!isPassthrough);
  directUI.threshold.slider.setVisible(isPassthrough);
  directUI.threshold.label.setVisible(isPassthrough);

  // Hold: パススルーモード時のみ表示
  directUI.hold.label.setVisible(isPassthrough);
  directUI.hold.slider.setVisible(isPassthrough);

  // Auto Trigger: パススルーモード時は自動有効、サンプルモード時は無効
  processorRef.directMode().detector().setEnabled(isPassthrough);
}

void BoomBabyAudioProcessorEditor::onSampleLoadClicked() {
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

void BoomBabyAudioProcessorEditor::onSampleFileChosen(const juce::File &file) {
  directUI.sample.loadedFilePath = file.getFullPathName();
  directUI.sample.loadButton.setButtonText(file.getFileNameWithoutExtension());
  directUI.sample.loadButton.setTooltip(directUI.sample.loadedFilePath);
  directUI.sample.loadButton.setHasFile(true);
  processorRef.getAPVTS().state.setProperty(
      "directSamplePath", directUI.sample.loadedFilePath, nullptr);
  processorRef.directEngine().sampler().loadSample(file);

  // サムネイルデータをメンバーに保存してプロバイダーを登録
  if (!processorRef.directEngine().sampler().copyThumbnail(
          directUI.sample.thumbMin, directUI.sample.thumbMax))
    return;
  directUI.sample.thumbDurSec =
      processorRef.directEngine().sampler().durationSec();
  refreshDirectProvider();
}

void BoomBabyAudioProcessorEditor::refreshDirectProvider() {
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

  // Drive + ClipType を thumb min/max に適用
  const auto driveDb =
      static_cast<float>(directUI.saturator.driveSlider.getValue());
  const int clipType = directUI.saturator.clipType.getSelected();
  const std::size_t n = directUI.sample.thumbMin.size();
  auto minPtr = std::make_shared<std::vector<float>>(n);
  auto maxPtr = std::make_shared<std::vector<float>>(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*minPtr)[i] =
        Saturator::process(directUI.sample.thumbMin[i], driveDb, clipType);
    (*maxPtr)[i] =
        Saturator::process(directUI.sample.thumbMax[i], driveDb, clipType);
  }

  // HPF / LPF を thumb データに適用（DSP: Saturator → HPF → LPF の順）
  applyDirectFilters(*minPtr, *maxPtr);

  envelopeCurveEditor.setDirectProvider(
      [minPtr, maxPtr, durSec, ampDurMs, ampScale](float timeSec) {
        return WaveformUtils::computeLutPreview(*minPtr, *maxPtr, durSec,
                                                ampDurMs, ampScale, timeSec);
      });
}

void BoomBabyAudioProcessorEditor::applyDirectFilters(
    std::vector<float> &vecMin, std::vector<float> &vecMax) const {
  const double rawSr = processorRef.getSampleRate();
  const float sr = rawSr > 0.0 ? static_cast<float>(rawSr) : 44100.0f;
  if (const auto hpfFreq = static_cast<float>(directUI.hpf.slider.getValue());
      hpfFreq > 20.5f) {
    const auto hpfQ = static_cast<float>(directUI.hpf.qSlider.getValue());
    const int hpfSlope = directUI.hpf.slope.getSlope();
    int hpfStages = 1;
    if (hpfSlope >= 48)
      hpfStages = 4;
    else if (hpfSlope >= 24)
      hpfStages = 2;
    SvfPassUtils::applySvfPass(vecMin, hpfFreq, hpfQ, hpfStages, 0, sr);
    SvfPassUtils::applySvfPass(vecMax, hpfFreq, hpfQ, hpfStages, 0, sr);
  }
  if (const auto lpfFreq = static_cast<float>(directUI.lpf.slider.getValue());
      lpfFreq < 19999.5f) {
    const auto lpfQ = static_cast<float>(directUI.lpf.qSlider.getValue());
    const int lpfSlope = directUI.lpf.slope.getSlope();
    int lpfStages = 1;
    if (lpfSlope >= 48)
      lpfStages = 4;
    else if (lpfSlope >= 24)
      lpfStages = 2;
    SvfPassUtils::applySvfPass(vecMin, lpfFreq, lpfQ, lpfStages, 1, sr);
    SvfPassUtils::applySvfPass(vecMax, lpfFreq, lpfQ, lpfStages, 1, sr);
  }
}
