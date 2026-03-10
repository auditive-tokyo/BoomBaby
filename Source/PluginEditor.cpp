#include "PluginEditor.h"
#include "GUI/LutBaker.h"
#include "PluginProcessor.h"
#include <span>

// ────────────────────────────────────────────────────
// パネルルーティング（Mute/Solo/レベルメーター）
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::setupPanelRouting(
    BoomBabyAudioProcessor &p) {
  using enum ChannelState::Channel;
  subPanel.setOnMuteChanged([&p](bool m) { p.channelState().setMute(sub, m); });
  subPanel.setOnSoloChanged([&p](bool s) { p.channelState().setSolo(sub, s); });
  clickPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(click, m); });
  clickPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(click, s); });
  directPanel.setOnMuteChanged(
      [&p](bool m) { p.channelState().setMute(direct, m); });
  directPanel.setOnSoloChanged(
      [&p](bool s) { p.channelState().setSolo(direct, s); });

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
  };
  // Click フェーダー → ゲインコントロール
  clickPanel.getFader().onValueChange = [this] {
    processorRef.clickEngine().setGainDb(
        static_cast<float>(clickPanel.getFader().getValue()));
  };
  // Direct フェーダー → ゲインコントロール
  directPanel.getFader().onValueChange = [this] {
    processorRef.directEngine().setGainDb(
        static_cast<float>(directPanel.getFader().getValue()));
  };
}

// ────────────────────────────────────────────────────
// エンベロープ変更時のノブ同期
// ────────────────────────────────────────────────────
void BoomBabyAudioProcessorEditor::onEnvelopeChanged() {
  const float durationMs = envelopeCurveEditor.getDisplayDurationMs();
  bakeLut(envDatas.amp, processorRef.subEngine().envLut(), durationMs);
  bakeLut(envDatas.freq, processorRef.subEngine().freqLut(), durationMs);
  bakeLut(envDatas.dist, processorRef.subEngine().distLut(), durationMs);
  bakeLut(envDatas.mix, processorRef.subEngine().mixLut(), durationMs);
  bakeLut(envDatas.clickAmp, processorRef.clickEngine().clickAmpLut(),
          durationMs);
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
    bakeLut(envDatas.directAmp, processorRef.directEngine().directAmpLut(),
            processorRef.directEngine().directAmpLut().getDurationMs());
  }
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
  masterSection.setOnValueChange(
      [this](float db) { processorRef.master().setGain(db); });
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
  startTimerHz(30);
}

BoomBabyAudioProcessorEditor::~BoomBabyAudioProcessorEditor() {
  stopTimer();
  subUI.wave.combo.setLookAndFeel(nullptr);
  clickUI.modeCombo.setLookAndFeel(nullptr);
  directUI.modeCombo.setLookAndFeel(nullptr);
  directUI.hold.slider.setLookAndFeel(nullptr);
}

void BoomBabyAudioProcessorEditor::timerCallback() {
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
    writeToRing(std::span<const float>{src.data() + s1, static_cast<std::size_t>(sz1)});
    if (sz2 > 0)
      writeToRing(std::span<const float>{src.data() + s2, static_cast<std::size_t>(sz2)});
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

  envelopeCurveEditor.setRealtimePixels(std::move(pixels));
  envelopeCurveEditor.setUseRealtimeInput(true);
  envelopeCurveEditor.repaint();
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