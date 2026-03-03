// DirectParams.cpp
// Direct panel UI setup / layout
#include "../PluginEditor.h"

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
  addAndMakeVisible(directUI.sampleLoadButton);

  // ── Mode コンボ変更時: ボタン表示切り替え ──
  directUI.modeCombo.onChange = [this] {
    const bool isSample = directUI.modeCombo.getSelectedId() ==
                          static_cast<int>(DirectUI::Mode::Sample);
    directUI.sampleLoadButton.setVisible(isSample);
    if (!isSample)
      directUI.sampleLoadButton.setButtonText("Load Sample");
    resized();
  };
}

void BabySquatchAudioProcessorEditor::layoutDirectParams(
    juce::Rectangle<int> area) {
  // 上段: mode ラベル + コンボ [+ サンプルロードボタン]
  auto topRow = area.removeFromTop(22);
  constexpr int modeLabelW = 38;
  constexpr int modeComboW = 68;
  directUI.modeLabel.setBounds(topRow.removeFromLeft(modeLabelW));
  topRow.removeFromLeft(2);
  directUI.modeCombo.setBounds(topRow.removeFromLeft(modeComboW));

  const bool isSample = directUI.modeCombo.getSelectedId() ==
                        static_cast<int>(DirectUI::Mode::Sample);
  if (isSample) {
    topRow.removeFromLeft(4);
    directUI.sampleLoadButton.setBounds(topRow);
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
  processorRef.directEngine().loadSample(file);

  // ロード完了後に波形サムネイルをプレビュープロバイダーとして設定
  std::vector<float> thumbMin;
  std::vector<float> thumbMax;
  if (!processorRef.directEngine().copyWaveformThumbnail(thumbMin, thumbMax))
    return;

  const double durSec = processorRef.directEngine().getSampleDurationSec();
  auto minPtr = std::make_shared<std::vector<float>>(std::move(thumbMin));
  auto maxPtr = std::make_shared<std::vector<float>>(std::move(thumbMax));
  envelopeCurveEditor.setDirectProvider(
      [minPtr, maxPtr, durSec](float timeSec) -> std::pair<float, float> {
        if (durSec <= 0.0 || timeSec < 0.0f ||
            timeSec >= static_cast<float>(durSec))
          return {0.0f, 0.0f};
        const float t   = timeSec / static_cast<float>(durSec);
        const auto  n   = static_cast<int>(minPtr->size());
        const int   idx = juce::jlimit(
            0, n - 1, static_cast<int>(t * static_cast<float>(n)));
        return {(*minPtr)[static_cast<std::size_t>(idx)],
                (*maxPtr)[static_cast<std::size_t>(idx)]};
      });
}
