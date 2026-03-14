#include "PluginEditor.h"
#include "DSP/Saturator.h"
#include "GUI/LutBaker.h"
#include "ParamIDs.h"
#include "PluginProcessor.h"
#include <span>

// ────────────────────────────────────────────────────
// パネルルーティング（Mute/Solo/レベルメーター）
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupPanelRouting(
    BoomBabyAudioProcessor &p) {
  using enum ChannelState::Channel;
  subPanel.setOnMuteChanged([&p, this](bool m) {
    p.channelState().setMute(sub, m);
    syncParam(ParamIDs::subMute, m ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelMuted(EnvelopeCurveEditor::Channel::sub, m);
  });
  subPanel.setOnSoloChanged([&p, this](bool s) {
    p.channelState().setSolo(sub, s);
    syncParam(ParamIDs::subSolo, s ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelSoloed(EnvelopeCurveEditor::Channel::sub, s);
  });
  clickPanel.setOnMuteChanged([&p, this](bool m) {
    p.channelState().setMute(click, m);
    syncParam(ParamIDs::clickMute, m ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelMuted(EnvelopeCurveEditor::Channel::click, m);
  });
  clickPanel.setOnSoloChanged([&p, this](bool s) {
    p.channelState().setSolo(click, s);
    syncParam(ParamIDs::clickSolo, s ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelSoloed(EnvelopeCurveEditor::Channel::click,
                                         s);
  });
  directPanel.setOnMuteChanged([&p, this](bool m) {
    p.channelState().setMute(direct, m);
    syncParam(ParamIDs::directMute, m ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelMuted(EnvelopeCurveEditor::Channel::direct,
                                        m);
  });
  directPanel.setOnSoloChanged([&p, this](bool s) {
    p.channelState().setSolo(direct, s);
    syncParam(ParamIDs::directSolo, s ? 1.0f : 0.0f);
    envelopeCurveEditor.setChannelSoloed(EnvelopeCurveEditor::Channel::direct,
                                         s);
  });

  subPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(sub); });
  clickPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(click); });
  directPanel.setLevelProvider(
      [&p]() { return p.channelState().getChannelLevelDb(direct); });

  // Sub フェーダー → ゲインコントロール
  subPanel.getFader().onValueChange = [this] {
    processorRef.subEngine().setGainDb(
        static_cast<float>(subPanel.getFader().getValue()));
    syncParam(ParamIDs::subGain,
              static_cast<float>(subPanel.getFader().getValue()));
  };
  // Click フェーダー → ゲインコントロール
  clickPanel.getFader().onValueChange = [this] {
    processorRef.clickEngine().setGainDb(
        static_cast<float>(clickPanel.getFader().getValue()));
    syncParam(ParamIDs::clickGain,
              static_cast<float>(clickPanel.getFader().getValue()));
  };
  // Direct フェーダー → ゲインコントロール
  directPanel.getFader().onValueChange = [this] {
    processorRef.directEngine().setGainDb(
        static_cast<float>(directPanel.getFader().getValue()));
    syncParam(ParamIDs::directGain,
              static_cast<float>(directPanel.getFader().getValue()));
  };
}

// ────────────────────────────────────────────────────
// エンベロープ変更時のノブ同期
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::onEnvelopeChanged() {
  // スライダー値から直接読むことで、editor 再開時に displayDurationMs が
  // デフォルト (300ms) のまま焼かれるバグを防止。
  const auto subLenMs = static_cast<float>(subUI.length.slider.getValue());
  envelopeCurveEditor.setDisplayDurationMs(subLenMs);
  // Sub LUT: エンベロープ実効区間に 512 点を集中させる
  bakeLut(envDatas.amp, processorRef.subEngine().envLut(),
          effectiveLutDuration(envDatas.amp, subLenMs));
  bakeLut(envDatas.freq, processorRef.subEngine().freqLut(),
          effectiveLutDuration(envDatas.freq, subLenMs));
  bakeLut(envDatas.dist, processorRef.subEngine().distLut(),
          effectiveLutDuration(envDatas.dist, subLenMs));
  bakeLut(envDatas.mix, processorRef.subEngine().mixLut(),
          effectiveLutDuration(envDatas.mix, subLenMs));
  const auto clickDecayMs =
      static_cast<float>(clickUI.sample.decay.slider.getValue());
  bakeLut(envDatas.clickAmp, processorRef.clickEngine().clickAmpLut(),
          clickDecayMs);
  envelopeCurveEditor.setClickDecayMs(clickDecayMs);
  const auto directDecayMs =
      static_cast<float>(directUI.decay.slider.getValue());
  bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
          directDecayMs);
  // 1点=ノブ制御（有効化＋ポイント値をノブに反映）、2点以上=エンベロープ制御（無効化）

  // Amp
  const bool ampCtrl = envDatas.amp.isEnvelopeControlled();
  subUI.knobs[0].setEnabled(!ampCtrl);
  subUI.knobs[0].setTooltip(ampCtrl ? "Click on Amp label to edit envelope"
                                    : "");
  if (!ampCtrl && envDatas.amp.hasPoints()) {
    const float v = envDatas.amp.getPoints()[0].value;
    envDatas.amp.setDefaultValue(v);
    subUI.knobs[0].setValue(v * 100.0, juce::dontSendNotification);
  }

  // Freq
  const bool freqCtrl = envDatas.freq.isEnvelopeControlled();
  subUI.knobs[1].setEnabled(!freqCtrl);
  subUI.knobs[1].setTooltip(freqCtrl ? "Click on Freq label to edit envelope"
                                     : "");
  if (!freqCtrl && envDatas.freq.hasPoints()) {
    const float hz = envDatas.freq.getPoints()[0].value;
    envDatas.freq.setDefaultValue(hz);
    subUI.knobs[1].setValue(hz, juce::dontSendNotification);
    envelopeCurveEditor.setDisplayCycles(
        hz * envelopeCurveEditor.getDisplayDurationMs() / 1000.0f);
  }

  // Saturate
  const bool distCtrl = envDatas.dist.isEnvelopeControlled();
  subUI.knobs[3].setEnabled(!distCtrl);
  subUI.knobs[3].setTooltip(
      distCtrl ? "Click on Saturate label to edit envelope" : "");
  if (!distCtrl && envDatas.dist.hasPoints()) {
    const float v = envDatas.dist.getPoints()[0].value;
    envDatas.dist.setDefaultValue(v);
    subUI.knobs[3].setValue(v * 24.0, juce::dontSendNotification);
  }

  // Mix
  const bool mixCtrl = envDatas.mix.isEnvelopeControlled();
  subUI.knobs[2].setEnabled(!mixCtrl);
  subUI.knobs[2].setTooltip(mixCtrl ? "Click on Mix label to edit envelope"
                                    : "");
  if (!mixCtrl && envDatas.mix.hasPoints()) {
    const float v = envDatas.mix.getPoints()[0].value;
    envDatas.mix.setDefaultValue(v);
    subUI.knobs[2].setValue(v * 100.0, juce::dontSendNotification);
    envelopeCurveEditor.setPreviewMix(v);
  }

  // Click Amp
  const bool clickAmpCtrl = envDatas.clickAmp.isEnvelopeControlled();
  clickUI.sample.amp.slider.setEnabled(!clickAmpCtrl);
  clickUI.sample.amp.slider.setTooltip(
      clickAmpCtrl ? "Click on Amp label to edit envelope" : "");
  if (!clickAmpCtrl && envDatas.clickAmp.hasPoints()) {
    const float v = envDatas.clickAmp.getPoints()[0].value;
    envDatas.clickAmp.setDefaultValue(v);
    clickUI.sample.amp.slider.setValue(v * 100.0, juce::dontSendNotification);
  }

  // Direct Amp
  const bool directAmpCtrl = envDatas.directAmp.isEnvelopeControlled();
  directUI.amp.slider.setEnabled(!directAmpCtrl);
  directUI.amp.slider.setTooltip(
      directAmpCtrl ? "Click on Amp label to edit envelope" : "");
  if (!directAmpCtrl && envDatas.directAmp.hasPoints()) {
    const float v = envDatas.directAmp.getPoints()[0].value;
    envDatas.directAmp.setDefaultValue(v);
    directUI.amp.slider.setValue(v * 100.0, juce::dontSendNotification);
  }

  // エンベロープデータを APVTS に保存（状態永続化）
  saveEnvelopesToState();
}

// ────────────────────────────────────────────────────
// エンベロープカーブエディタ内部配線
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupEnvelopeCurveEditor() {
  envelopeCurveEditor.setOnChange([this] { onEnvelopeChanged(); });
  // 初期状態: Amp が選択済み（ラベル色を反映）
  switchEditTarget(EnvelopeCurveEditor::EditTarget::amp);
}

void BoomBabyAudioProcessorEditor::mouseDown(const juce::MouseEvent &e) {
  using enum EnvelopeCurveEditor::EditTarget;
  if (e.eventComponent == &subUI.knobLabels[0])
    switchEditTarget(amp);
  else if (e.eventComponent == &subUI.knobLabels[1])
    switchEditTarget(freq);
  else if (e.eventComponent == &subUI.knobLabels[2])
    switchEditTarget(mix);
  else if (e.eventComponent == &subUI.knobLabels[3])
    switchEditTarget(saturate);
  else if (e.eventComponent == &clickUI.sample.amp.label)
    switchEditTarget(clickAmp);
  else if (e.eventComponent == &directUI.amp.label)
    switchEditTarget(directAmp);
}

void BoomBabyAudioProcessorEditor::switchEditTarget(
    EnvelopeCurveEditor::EditTarget t) {
  envelopeCurveEditor.setEditTarget(t);
  using enum EnvelopeCurveEditor::EditTarget;
  const auto kLabel = UIConstants::Colours::labelText;
  subUI.knobLabels[0].setColour(juce::Label::textColourId,
                                t == amp ? UIConstants::Colours::subArc
                                         : kLabel);
  subUI.knobLabels[1].setColour(juce::Label::textColourId,
                                t == freq ? juce::Colours::cyan : kLabel);
  subUI.knobLabels[3].setColour(juce::Label::textColourId,
                                t == saturate ? juce::Colour(0xFFFF9500)
                                              : kLabel);
  subUI.knobLabels[2].setColour(juce::Label::textColourId,
                                t == mix ? juce::Colour(0xFF4CAF50) : kLabel);
  // Click Sample Amp label
  clickUI.sample.amp.label.setColour(
      juce::Label::textColourId,
      t == clickAmp ? UIConstants::Colours::clickArc : kLabel);
  // Direct Amp label
  directUI.amp.label.setColour(juce::Label::textColourId,
                               t == directAmp ? UIConstants::Colours::directArc
                                              : kLabel);
}

// ────────────────────────────────────────────────────
// paint
// ────────────────────────────────────────────────────
BoomBabyAudioProcessorEditor::BoomBabyAudioProcessorEditor(
    BoomBabyAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState()) {
  addAndMakeVisible(subPanel);
  addAndMakeVisible(clickPanel);
  addAndMakeVisible(directPanel);
  addAndMakeVisible(keyboard);
  addAndMakeVisible(envelopeCurveEditor);
  addAndMakeVisible(masterSection);
  addAndMakeVisible(infoBox);

  // infoBox 初期設定
  infoBox.setText("", juce::dontSendNotification);
  infoBox.setFont(juce::Font(juce::FontOptions(UIConstants::fontSizeSmall)));
  infoBox.setColour(juce::Label::textColourId, UIConstants::Colours::labelText);
  infoBox.setColour(juce::Label::backgroundColourId,
                    UIConstants::Colours::panelBg.withAlpha(0.0f));
  infoBox.setJustificationType(juce::Justification::centredLeft);

  envDatas.freq.setDefaultValue(200.0f);

  setupPanelRouting(p);
  setupEnvelopeCurveEditor();
  setupClickParams();
  setupDirectParams();
  setupSubKnobsRow();
  setupWaveShapeCombo();
  setupLengthBox();

  // マスターフェーダー配線
  masterSection.setOnValueChange([this](float db) {
    processorRef.master().setGain(db);
    syncParam(ParamIDs::masterGain, db);
  });
  masterSection.setLevelProvider(0, [this]() {
    return processorRef.master().getLevelDb(0); // L
  });
  masterSection.setLevelProvider(1, [this]() {
    return processorRef.master().getLevelDb(1); // R
  });

  setSize(UIConstants::windowWidth, UIConstants::windowHeight +
                                        UIConstants::expandedAreaHeight +
                                        UIConstants::panelGap);

  // 入力波形 Timer 開始（30fps）
  waveDisplayBuf_.assign(static_cast<std::size_t>(kWaveDisplayCapacity), 0.0f);

  // APVTS 状態から UI ウィジェットを復元（DAW が保存した値を反映）
  syncUIFromState();

  startTimerHz(30);
}

BoomBabyAudioProcessorEditor::~BoomBabyAudioProcessorEditor() {
  stopTimer();
  subUI.wave.combo.setLookAndFeel(nullptr);
  clickUI.modeCombo.setLookAndFeel(nullptr);
  directUI.modeCombo.setLookAndFeel(nullptr);
  directUI.hold.slider.setLookAndFeel(nullptr);
}

// ────────────────────────────────────────────────────
// APVTS ↔ UI 同期ヘルパー
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::syncParam(const char *id, float value) {
  if (auto *p = processorRef.getAPVTS().getParameter(id))
    p->setValueNotifyingHost(p->convertTo0to1(value));
}

void BoomBabyAudioProcessorEditor::syncUIFromState() {
  const auto &apvts = processorRef.getAPVTS();
  auto load = [&](const char *id) {
    return apvts.getRawParameterValue(id)->load();
  };

  // dontSendNotification で onValueChange を発火させない。
  // prepareToPlay() で DSP は初期化済みなので、ここでは表示のみ更新。
  constexpr auto notify = juce::dontSendNotification;

  // ── Sub ──
  // Length を先に復元（bakeLut が表示期間に依存するため）
  subUI.length.slider.setValue(load(ParamIDs::subLength), notify);
  subUI.wave.combo.setSelectedId(
      static_cast<int>(load(ParamIDs::subWaveShape)) + 1, notify);
  subUI.knobs[0].setValue(load(ParamIDs::subAmp), notify);
  subUI.knobs[1].setValue(load(ParamIDs::subFreq), notify);
  subUI.knobs[2].setValue(load(ParamIDs::subMix), notify);
  subUI.knobs[3].setValue(load(ParamIDs::subSatDrive), notify);
  subUI.saturateClipType.setSelected(
      static_cast<int>(load(ParamIDs::subSatClipType)), false);
  subUI.knobs[4].setValue(load(ParamIDs::subTone1), notify);
  subUI.knobs[5].setValue(load(ParamIDs::subTone2), notify);
  subUI.knobs[6].setValue(load(ParamIDs::subTone3), notify);
  subUI.knobs[7].setValue(load(ParamIDs::subTone4), notify);
  subPanel.getFader().setValue(load(ParamIDs::subGain), notify);
  subPanel.setMuteState(load(ParamIDs::subMute) >= 0.5f);
  subPanel.setSoloState(load(ParamIDs::subSolo) >= 0.5f);

  // ── Click ──
  const auto clickModeIdx = static_cast<int>(load(ParamIDs::clickMode));
  clickUI.modeCombo.setSelectedId(clickModeIdx + 1, notify);
  clickUI.noise.decaySlider.setValue(load(ParamIDs::clickNoiseDecay), notify);
  clickUI.noise.bpf1.freqSlider.setValue(load(ParamIDs::clickBpf1Freq), notify);
  clickUI.noise.bpf1.qSlider.setValue(load(ParamIDs::clickBpf1Q), notify);
  {
    constexpr std::array kSlopes = {12, 24, 48};
    clickUI.noise.bpf1.slopeSelector.setSlope(kSlopes[static_cast<std::size_t>(
        static_cast<int>(load(ParamIDs::clickBpf1Slope)))]);
    clickUI.hpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
        static_cast<int>(load(ParamIDs::clickHpfSlope)))]);
    clickUI.lpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
        static_cast<int>(load(ParamIDs::clickLpfSlope)))]);
  }
  clickUI.sample.pitch.slider.setValue(load(ParamIDs::clickSamplePitch),
                                       notify);
  clickUI.sample.amp.slider.setValue(load(ParamIDs::clickSampleAmp), notify);
  clickUI.sample.decay.slider.setValue(load(ParamIDs::clickSampleDecay),
                                       notify);
  clickUI.noise.saturator.driveSlider.setValue(load(ParamIDs::clickDrive),
                                               notify);
  clickUI.noise.saturator.clipType.setSelected(
      static_cast<int>(load(ParamIDs::clickClipType)), false);
  clickUI.hpf.slider.setValue(load(ParamIDs::clickHpfFreq), notify);
  clickUI.hpf.qSlider.setValue(load(ParamIDs::clickHpfQ), notify);
  clickUI.lpf.slider.setValue(load(ParamIDs::clickLpfFreq), notify);
  clickUI.lpf.qSlider.setValue(load(ParamIDs::clickLpfQ), notify);
  clickPanel.getFader().setValue(load(ParamIDs::clickGain), notify);
  clickPanel.setMuteState(load(ParamIDs::clickMute) >= 0.5f);
  clickPanel.setSoloState(load(ParamIDs::clickSolo) >= 0.5f);

  // Mode → 表示切替のみ（DSP は prepareToPlay で初期化済み）
  setClickModeVisible(clickModeIdx == 1);

  // ── Direct ──
  const auto directModeIdx = static_cast<int>(load(ParamIDs::directMode));
  directUI.modeCombo.setSelectedId(directModeIdx + 1, notify);
  directUI.pitch.slider.setValue(load(ParamIDs::directPitch), notify);
  directUI.amp.slider.setValue(load(ParamIDs::directAmp), notify);
  directUI.saturator.driveSlider.setValue(load(ParamIDs::directDrive), notify);
  directUI.saturator.clipType.setSelected(
      static_cast<int>(load(ParamIDs::directClipType)), false);
  directUI.decay.slider.setValue(load(ParamIDs::directDecay), notify);
  directUI.hpf.slider.setValue(load(ParamIDs::directHpfFreq), notify);
  directUI.hpf.qSlider.setValue(load(ParamIDs::directHpfQ), notify);
  {
    constexpr std::array kSlopes = {12, 24, 48};
    directUI.hpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
        static_cast<int>(load(ParamIDs::directHpfSlope)))]);
    directUI.lpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
        static_cast<int>(load(ParamIDs::directLpfSlope)))]);
  }
  directUI.lpf.slider.setValue(load(ParamIDs::directLpfFreq), notify);
  directUI.lpf.qSlider.setValue(load(ParamIDs::directLpfQ), notify);
  directUI.threshold.slider.setValue(load(ParamIDs::directThreshold), notify);
  directUI.hold.slider.setValue(load(ParamIDs::directHold), notify);
  directPanel.getFader().setValue(load(ParamIDs::directGain), notify);
  directPanel.setMuteState(load(ParamIDs::directMute) >= 0.5f);
  directPanel.setSoloState(load(ParamIDs::directSolo) >= 0.5f);

  // Mode → 表示切替のみ（DSP は prepareToPlay で初期化済み）
  const bool isSample = directModeIdx == 1;
  directUI.sample.loadButton.setVisible(isSample);
  refreshDirectPassthroughUI();

  // ── Master ──
  masterSection.setValueDb(load(ParamIDs::masterGain));

  // ── Mute/Solo: envelopeCurveEditor 同期 ──
  using EC = EnvelopeCurveEditor::Channel;
  envelopeCurveEditor.setChannelMuted(EC::sub, load(ParamIDs::subMute) >= 0.5f);
  envelopeCurveEditor.setChannelSoloed(EC::sub,
                                       load(ParamIDs::subSolo) >= 0.5f);
  envelopeCurveEditor.setChannelMuted(EC::click,
                                      load(ParamIDs::clickMute) >= 0.5f);
  envelopeCurveEditor.setChannelSoloed(EC::click,
                                       load(ParamIDs::clickSolo) >= 0.5f);
  envelopeCurveEditor.setChannelMuted(EC::direct,
                                      load(ParamIDs::directMute) >= 0.5f);
  envelopeCurveEditor.setChannelSoloed(EC::direct,
                                       load(ParamIDs::directSolo) >= 0.5f);

  // ── エンベロープデータ復元＋LUT 再ベイク ──
  loadEnvelopesFromState();

  // モード変更後のレイアウト更新（Sample ノブの bounds を確定させる）
  resized();
}

// ────────────────────────────────────────────────────
// APVTS ポーリング（timerCallback 用：サイレント更新）
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::pollUIFromAPVTS() {
  const auto &apvts = processorRef.getAPVTS();
  auto load = [&](const char *id) {
    return apvts.getRawParameterValue(id)->load();
  };
  constexpr auto silent = juce::dontSendNotification;
  constexpr std::array kSlopes = {12, 24, 48};

  // ── Sub ──
  subUI.length.slider.setValue(load(ParamIDs::subLength), silent);
  {
    const int waveId = static_cast<int>(load(ParamIDs::subWaveShape)) + 1;
    if (subUI.wave.combo.getSelectedId() != waveId)
      subUI.wave.combo.setSelectedId(waveId, silent);
  }
  subUI.knobs[0].setValue(load(ParamIDs::subAmp), silent);
  subUI.knobs[1].setValue(load(ParamIDs::subFreq), silent);
  subUI.knobs[2].setValue(load(ParamIDs::subMix), silent);
  subUI.knobs[3].setValue(load(ParamIDs::subSatDrive), silent);
  subUI.saturateClipType.setSelected(
      static_cast<int>(load(ParamIDs::subSatClipType)), false);
  subUI.knobs[4].setValue(load(ParamIDs::subTone1), silent);
  subUI.knobs[5].setValue(load(ParamIDs::subTone2), silent);
  subUI.knobs[6].setValue(load(ParamIDs::subTone3), silent);
  subUI.knobs[7].setValue(load(ParamIDs::subTone4), silent);
  subPanel.getFader().setValue(load(ParamIDs::subGain), silent);
  subPanel.setMuteState(load(ParamIDs::subMute) >= 0.5f);
  subPanel.setSoloState(load(ParamIDs::subSolo) >= 0.5f);

  // ── Click ──
  {
    const int modeId = static_cast<int>(load(ParamIDs::clickMode)) + 1;
    if (clickUI.modeCombo.getSelectedId() != modeId) {
      clickUI.modeCombo.setSelectedId(modeId, silent);
      setClickModeVisible(modeId == std::to_underlying(ClickUI::Mode::Sample));
    }
  }
  clickUI.noise.decaySlider.setValue(load(ParamIDs::clickNoiseDecay), silent);
  clickUI.noise.bpf1.freqSlider.setValue(load(ParamIDs::clickBpf1Freq), silent);
  clickUI.noise.bpf1.qSlider.setValue(load(ParamIDs::clickBpf1Q), silent);
  clickUI.noise.bpf1.slopeSelector.setSlope(kSlopes[static_cast<std::size_t>(
      static_cast<int>(load(ParamIDs::clickBpf1Slope)))]);
  clickUI.sample.pitch.slider.setValue(load(ParamIDs::clickSamplePitch),
                                       silent);
  clickUI.sample.amp.slider.setValue(load(ParamIDs::clickSampleAmp), silent);
  clickUI.sample.decay.slider.setValue(load(ParamIDs::clickSampleDecay),
                                       silent);
  clickUI.noise.saturator.driveSlider.setValue(load(ParamIDs::clickDrive),
                                               silent);
  clickUI.noise.saturator.clipType.setSelected(
      static_cast<int>(load(ParamIDs::clickClipType)), false);
  clickUI.hpf.slider.setValue(load(ParamIDs::clickHpfFreq), silent);
  clickUI.hpf.qSlider.setValue(load(ParamIDs::clickHpfQ), silent);
  clickUI.hpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
      static_cast<int>(load(ParamIDs::clickHpfSlope)))]);
  clickUI.lpf.slider.setValue(load(ParamIDs::clickLpfFreq), silent);
  clickUI.lpf.qSlider.setValue(load(ParamIDs::clickLpfQ), silent);
  clickUI.lpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
      static_cast<int>(load(ParamIDs::clickLpfSlope)))]);
  clickPanel.getFader().setValue(load(ParamIDs::clickGain), silent);
  clickPanel.setMuteState(load(ParamIDs::clickMute) >= 0.5f);
  clickPanel.setSoloState(load(ParamIDs::clickSolo) >= 0.5f);

  // ── Direct ──
  {
    const int modeId = static_cast<int>(load(ParamIDs::directMode)) + 1;
    if (directUI.modeCombo.getSelectedId() != modeId) {
      directUI.modeCombo.setSelectedId(modeId, silent);
      const bool isSample =
          (modeId == std::to_underlying(DirectUI::Mode::Sample));
      directUI.sample.loadButton.setVisible(isSample);
      refreshDirectPassthroughUI();
    }
  }
  directUI.pitch.slider.setValue(load(ParamIDs::directPitch), silent);
  directUI.amp.slider.setValue(load(ParamIDs::directAmp), silent);
  directUI.saturator.driveSlider.setValue(load(ParamIDs::directDrive), silent);
  directUI.saturator.clipType.setSelected(
      static_cast<int>(load(ParamIDs::directClipType)), false);
  directUI.decay.slider.setValue(load(ParamIDs::directDecay), silent);
  directUI.hpf.slider.setValue(load(ParamIDs::directHpfFreq), silent);
  directUI.hpf.qSlider.setValue(load(ParamIDs::directHpfQ), silent);
  directUI.hpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
      static_cast<int>(load(ParamIDs::directHpfSlope)))]);
  directUI.lpf.slider.setValue(load(ParamIDs::directLpfFreq), silent);
  directUI.lpf.qSlider.setValue(load(ParamIDs::directLpfQ), silent);
  directUI.lpf.slope.setSlope(kSlopes[static_cast<std::size_t>(
      static_cast<int>(load(ParamIDs::directLpfSlope)))]);
  directUI.threshold.slider.setValue(load(ParamIDs::directThreshold), silent);
  directUI.hold.slider.setValue(load(ParamIDs::directHold), silent);
  directPanel.getFader().setValue(load(ParamIDs::directGain), silent);
  directPanel.setMuteState(load(ParamIDs::directMute) >= 0.5f);
  directPanel.setSoloState(load(ParamIDs::directSolo) >= 0.5f);

  // ── Master ──
  masterSection.setValueDb(load(ParamIDs::masterGain));
}

// ────────────────────────────────────────────────────
// エンベロープデータ ↔ APVTS ValueTree シリアライズ
// ────────────────────────────────────────────────────
namespace {
const juce::Identifier kEnvelopeTag{"ENVELOPE"};
const juce::Identifier kPointTag{"POINT"};
const juce::Identifier kPropName{"name"};
const juce::Identifier kPropDefault{"defaultValue"};
const juce::Identifier kPropTimeMs{"timeMs"};
const juce::Identifier kPropValue{"value"};
const juce::Identifier kPropCurve{"curve"};

juce::ValueTree envelopeToTree(const char *name, const EnvelopeData &env) {
  juce::ValueTree tree{kEnvelopeTag};
  tree.setProperty(kPropName, juce::String(name), nullptr);
  tree.setProperty(kPropDefault, env.getDefaultValue(), nullptr);
  for (auto &pt : env.getPoints()) {
    juce::ValueTree ptTree{kPointTag};
    ptTree.setProperty(kPropTimeMs, pt.timeMs, nullptr);
    ptTree.setProperty(kPropValue, pt.value, nullptr);
    ptTree.setProperty(kPropCurve, pt.curve, nullptr);
    tree.addChild(ptTree, -1, nullptr);
  }
  return tree;
}

void treeToEnvelope(const juce::ValueTree &tree, EnvelopeData &env) {
  env.clearPoints();
  env.setDefaultValue(tree.getProperty(kPropDefault, 1.0f));
  for (int i = 0; i < tree.getNumChildren(); ++i) {
    auto pt = tree.getChild(i);
    if (!pt.hasType(kPointTag))
      continue;
    const float t = pt.getProperty(kPropTimeMs, 0.0f);
    const float v = pt.getProperty(kPropValue, 1.0f);
    const float c = pt.getProperty(kPropCurve, 0.0f);
    env.addPoint(t, v);
    // addPoint doesn't set curve — set it on the last-added point
    const int idx = static_cast<int>(env.getPoints().size()) - 1;
    env.setSegmentCurve(idx, c);
  }
}

const juce::Identifier kClickModeStateTag{"CLICK_MODE_STATE"};

} // namespace

juce::ValueTree BoomBabyAudioProcessorEditor::modeStateToTree(
    const char *name, const ClickUI::ModeState &ms) const {
  juce::ValueTree t{kClickModeStateTag};
  t.setProperty("name", juce::String(name), nullptr);
  t.setProperty("hpfFreq", ms.hpfFreq, nullptr);
  t.setProperty("hpfQ", ms.hpfQ, nullptr);
  t.setProperty("hpfSlope", ms.hpfSlope, nullptr);
  t.setProperty("lpfFreq", ms.lpfFreq, nullptr);
  t.setProperty("lpfQ", ms.lpfQ, nullptr);
  t.setProperty("lpfSlope", ms.lpfSlope, nullptr);
  t.setProperty("drive", ms.drive, nullptr);
  t.setProperty("clipType", ms.clipType, nullptr);
  return t;
}

void BoomBabyAudioProcessorEditor::treeToModeState(
    const juce::ValueTree &t, ClickUI::ModeState &ms) const {
  ms.hpfFreq = static_cast<double>(t.getProperty("hpfFreq", 20.0));
  ms.hpfQ = static_cast<double>(t.getProperty("hpfQ", 0.71));
  ms.hpfSlope = static_cast<int>(t.getProperty("hpfSlope", 12));
  ms.lpfFreq = static_cast<double>(t.getProperty("lpfFreq", 20000.0));
  ms.lpfQ = static_cast<double>(t.getProperty("lpfQ", 0.71));
  ms.lpfSlope = static_cast<int>(t.getProperty("lpfSlope", 12));
  ms.drive = static_cast<double>(t.getProperty("drive", 0.0));
  ms.clipType = static_cast<int>(t.getProperty("clipType", 0));
}

void BoomBabyAudioProcessorEditor::saveEnvelopesToState() {
  auto &state = processorRef.getAPVTS().state;

  // 既存の ENVELOPE 子ノードを全削除
  for (int i = state.getNumChildren(); --i >= 0;)
    if (state.getChild(i).hasType(kEnvelopeTag))
      state.removeChild(i, nullptr);

  // 6 つのエンベロープを書き込み
  state.addChild(envelopeToTree("amp", envDatas.amp), -1, nullptr);
  state.addChild(envelopeToTree("freq", envDatas.freq), -1, nullptr);
  state.addChild(envelopeToTree("dist", envDatas.dist), -1, nullptr);
  state.addChild(envelopeToTree("mix", envDatas.mix), -1, nullptr);
  state.addChild(envelopeToTree("clickAmp", envDatas.clickAmp), -1, nullptr);
  state.addChild(envelopeToTree("directAmp", envDatas.directAmp), -1, nullptr);

  // サンプルファイルパスを保存
  state.setProperty("directSamplePath", directUI.sample.loadedFilePath,
                    nullptr);
  state.setProperty("clickSamplePath", clickUI.sample.loadedFilePath, nullptr);

  // Click ModeState を保存（アクティブモードはウィジェットから取得）
  const bool clickIsSample = clickUI.modeCombo.getSelectedId() ==
                             std::to_underlying(ClickUI::Mode::Sample);
  auto noiseSt = clickUI.noiseState;
  auto sampleSt = clickUI.sampleState;
  if (clickIsSample)
    clickUI.saveModeState(sampleSt);
  else
    clickUI.saveModeState(noiseSt);

  for (int i = state.getNumChildren(); --i >= 0;)
    if (state.getChild(i).hasType(kClickModeStateTag))
      state.removeChild(i, nullptr);
  state.addChild(modeStateToTree("clickNoise", noiseSt), -1, nullptr);
  state.addChild(modeStateToTree("clickSample", sampleSt), -1, nullptr);
}

void BoomBabyAudioProcessorEditor::loadEnvelopesFromState() {
  const auto &state = processorRef.getAPVTS().state;

  auto findEnv = [&](const char *name) -> juce::ValueTree {
    for (int i = 0; i < state.getNumChildren(); ++i) {
      auto child = state.getChild(i);
      if (child.hasType(kEnvelopeTag) &&
          child.getProperty(kPropName).toString() == name)
        return child;
    }
    return {};
  };

  struct EnvEntry {
    const char *name;
    EnvelopeData &data;
  };
  std::array<EnvEntry, 6> entries = {{
      {"amp", envDatas.amp},
      {"freq", envDatas.freq},
      {"dist", envDatas.dist},
      {"mix", envDatas.mix},
      {"clickAmp", envDatas.clickAmp},
      {"directAmp", envDatas.directAmp},
  }};

  for (auto &[name, data] : entries) {
    auto tree = findEnv(name);
    if (tree.isValid())
      treeToEnvelope(tree, data);
  }

  // サンプルファイルパスを先に復元（onEnvelopeChanged → saveEnvelopesToState
  // で上書きされる前に loadedFilePath をセットしておく）
  if (const auto directPath = state.getProperty("directSamplePath").toString();
      directPath.isNotEmpty()) {
    const juce::File f{directPath};
    if (f.existsAsFile())
      onSampleFileChosen(f);
  }

  if (const auto clickPath = state.getProperty("clickSamplePath").toString();
      clickPath.isNotEmpty()) {
    const juce::File f{clickPath};
    if (f.existsAsFile())
      onClickSampleFileChosen(f);
  }

  // Click ModeState を復元
  for (int i = 0; i < state.getNumChildren(); ++i) {
    auto child = state.getChild(i);
    if (!child.hasType(kClickModeStateTag))
      continue;
    const auto name = child.getProperty(kPropName).toString();
    if (name == "clickNoise")
      treeToModeState(child, clickUI.noiseState);
    else if (name == "clickSample")
      treeToModeState(child, clickUI.sampleState);
  }

  // アクティブモードのウィジェットに反映
  const bool clickIsSample = clickUI.modeCombo.getSelectedId() ==
                             std::to_underlying(ClickUI::Mode::Sample);
  clickUI.restoreModeState(clickIsSample ? clickUI.sampleState
                                         : clickUI.noiseState,
                           processorRef.clickEngine());

  // LUT 再ベイク＋ノブ有効/無効同期
  onEnvelopeChanged();

  // onEnvelopeChanged() で正しい decay duration がベイクされた後に
  // Direct プレビューを再構築（lambda が ampDurMs をキャプチャするため）
  refreshDirectProvider();
}

void BoomBabyAudioProcessorEditor::timerCallback() {
  // DAW Undo/Redo / オートメーション: APVTS 値とウィジェットを同期
  pollUIFromAPVTS();

  // Direct がパススルーモードの場合のみリアルタイム入力波形を表示
  if (!processorRef.directMode().isPassthrough()) {
    envelopeCurveEditor.setUseRealtimeInput(false);
    return;
  }

  // FIFO から利用可能なサンプルをローリング表示バッファに追記
  auto &fifo = processorRef.inputMonitor().fifo();
  const auto &src = processorRef.inputMonitor().data();
  if (const int avail = fifo.getNumReady(); avail > 0) {
    int s1;
    int sz1;
    int s2;
    int sz2;
    fifo.prepareToRead(avail, s1, sz1, s2, sz2);
    const int cap = kWaveDisplayCapacity;
    auto writeToRing = [&](std::span<const float> data) {
      for (const float s : data) {
        waveDisplayBuf_[static_cast<std::size_t>(waveDisplayPos_)] = s;
        waveDisplayPos_ = (waveDisplayPos_ + 1) % cap;
        waveDisplayFilled_ = std::min(waveDisplayFilled_ + 1, cap);
      }
    };
    writeToRing(
        std::span<const float>{src.data() + s1, static_cast<std::size_t>(sz1)});
    if (sz2 > 0)
      writeToRing(std::span<const float>{src.data() + s2,
                                         static_cast<std::size_t>(sz2)});
    fifo.finishedRead(sz1 + sz2);
  }

  const int w = envelopeCurveEditor.getWidth();
  if (w <= 0 || waveDisplayFilled_ == 0)
    return;

  // 最新 500ms 分（最大 waveDisplayFilled_ サンプル）を幅ピクセルに射影
  const int cap = kWaveDisplayCapacity;
  const auto smp500ms =
      static_cast<int>(processorRef.getSampleRate() * 0.5); // SR に依存
  const int dispSmp = std::min(waveDisplayFilled_, std::min(smp500ms, cap));
  const int startPos = (waveDisplayPos_ - dispSmp + cap) % cap;
  const float spf = static_cast<float>(dispSmp) / static_cast<float>(w);

  std::vector<std::pair<float, float>> pixels(static_cast<std::size_t>(w));
  for (int px = 0; px < w; ++px) {
    const auto iStart = static_cast<int>(static_cast<float>(px) * spf);
    const int iEnd =
        std::min(static_cast<int>(static_cast<float>(px + 1) * spf), dispSmp);
    float mn = 0.0f;
    float mx = 0.0f;
    for (int si = iStart; si < iEnd; ++si) {
      const float s =
          waveDisplayBuf_[static_cast<std::size_t>((startPos + si) % cap)];
      mn = std::min(mn, s);
      mx = std::max(mx, s);
    }
    pixels[static_cast<std::size_t>(px)] = {mn, mx};
  }

  // Drive + ClipType をピクセル min/max に適用（Saturator は単調関数）
  const auto driveDb =
      static_cast<float>(directUI.saturator.driveSlider.getValue());
  const int clipType = directUI.saturator.clipType.getSelected();
  const auto nPx = static_cast<std::size_t>(w);
  std::vector<float> pxMin(nPx);
  std::vector<float> pxMax(nPx);
  for (std::size_t i = 0; i < nPx; ++i) {
    pxMin[i] = Saturator::process(pixels[i].first, driveDb, clipType);
    pxMax[i] = Saturator::process(pixels[i].second, driveDb, clipType);
  }

  // HPF / LPF をピクセルデータに適用
  applyDirectFilters(pxMin, pxMax);

  // 処理済みデータをピクセルに戻す
  for (std::size_t i = 0; i < nPx; ++i)
    pixels[i] = {pxMin[i], pxMax[i]};

  envelopeCurveEditor.setRealtimePixels(std::move(pixels));
  envelopeCurveEditor.setUseRealtimeInput(true);
  envelopeCurveEditor.repaint();
}

void BoomBabyAudioProcessorEditor::visibilityChanged() {
  if (isVisible())
    // peer生成完了後に実行（visibilityChanged時点ではpeerが未完成の場合がある）
    juce::Timer::callAfterDelay(
        100, [safe = juce::Component::SafePointer<juce::Component>(this)] {
          if (safe != nullptr)
            safe->grabKeyboardFocus();
        });
}

void BoomBabyAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(UIConstants::Colours::background);

  // 展開エリアの背景
  g.setColour(UIConstants::Colours::panelBg);
  g.fillRoundedRectangle(envelopeCurveEditor.getBounds().toFloat(), 6.0f);
}

void BoomBabyAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(UIConstants::panelPadding);

  // 常時表示の展開エリア（下部）
  {
    auto expandArea = area.removeFromBottom(UIConstants::expandedAreaHeight);

    // 鍵盤行: 鍵盤自然幅で左寄せ、余白にマスターセクション
    auto keyboardRow = expandArea.removeFromBottom(UIConstants::keyboardHeight);
    const int kbWidth =
        juce::jmin(keyboardRow.getWidth(), keyboard.getPreferredWidth());
    keyboard.setBounds(keyboardRow.removeFromLeft(kbWidth));

    // マスターセクション（鍵盤の右余白）
    if (keyboardRow.getWidth() > 0) {
      constexpr int masterFaderW = 180;
      auto masterArea = keyboardRow.removeFromLeft(
          juce::jmin(keyboardRow.getWidth(), masterFaderW));
      masterSection.setBounds(masterArea);

      infoBox.setBounds(keyboardRow.reduced(6, 4));
    }

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
    // 底行: [Length ボックス | subUI.wave.combo]
    auto bottomRow = contentArea.removeFromBottom(22);
    contentArea.removeFromBottom(4); // ギャップ
    const int lengthPartW = bottomRow.getWidth() / 2;
    {
      auto lengthArea = bottomRow.removeFromLeft(lengthPartW);
      constexpr int labelW = 36;
      subUI.length.label.setBounds(lengthArea.removeFromLeft(labelW));
      subUI.length.slider.setBounds(lengthArea);
    }
    constexpr int waveLabelW = 36;
    subUI.wave.label.setBounds(bottomRow.removeFromLeft(waveLabelW));
    subUI.wave.combo.setBounds(bottomRow);
    layoutSubKnobsRow(contentArea);
  }

  // CLICK コンテンツをパネル内に常時配置
  {
    const auto b = clickPanel.getBounds();
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
    layoutClickParams(contentArea);
  }

  // DIRECT コンテンツをパネル内に常時配置
  {
    const auto b = directPanel.getBounds();
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
    layoutDirectParams(contentArea);
  }
}