// ────────────────────────────────────────────────────
// ClickParams.cpp
// Click展開パネルの UI セットアップ / レイアウト
// ────────────────────────────────────────────────────
#include "../PluginEditor.h"

// ────────────────────────────────────────────────────
// セットアップ
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::setupClickParams() {
  const juce::Colour accent = UIConstants::Colours::clickArc;
  const auto smallFont =
      juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));

  // ── SHAPE グループ ──
  clickUI.shapeGroup.setText("SHAPE");
  clickUI.shapeGroup.setColour(juce::GroupComponent::outlineColourId,
                            accent.withAlpha(0.45f));
  clickUI.shapeGroup.setColour(juce::GroupComponent::textColourId, accent);
  addAndMakeVisible(clickUI.shapeGroup);

  // ── ClickShapeEditor ──
  clickUI.shapeEditor.setOnPeakChanged([](float /*x*/, float /*y*/) {
    // TODO: DSP パラメータへの書き込み
  });
  addAndMakeVisible(clickUI.shapeEditor);

  // ── TYPE/LENGTH グループ ──
  clickUI.typeLengthGroup.setText("TYPE/LENGTH");
  clickUI.typeLengthGroup.setColour(juce::GroupComponent::outlineColourId,
                                 accent.withAlpha(0.45f));
  clickUI.typeLengthGroup.setColour(juce::GroupComponent::textColourId, accent);
  addAndMakeVisible(clickUI.typeLengthGroup);

  // ── TYPE コンボボックス ──
  clickUI.typeCombo.addItem("Sine",  1);
  clickUI.typeCombo.addItem("Tri",   2);
  clickUI.typeCombo.addItem("Noise", 3);
  clickUI.typeCombo.setSelectedId(1, juce::dontSendNotification);
  clickUI.typeCombo.setLookAndFeel(&darkComboLAF);
  clickUI.typeCombo.onChange = [] {
    // TODO: DSP パラメータへの書き込み（type）
  };
  addAndMakeVisible(clickUI.typeCombo);

  // ── LENGTH ラベル + 入力 ──
  clickUI.lengthPrefix.setText("length:", juce::dontSendNotification);
  clickUI.lengthPrefix.setFont(smallFont);
  clickUI.lengthPrefix.setColour(juce::Label::textColourId,
                              UIConstants::Colours::labelText);
  clickUI.lengthPrefix.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(clickUI.lengthPrefix);

  clickUI.lengthEditor.setText("20");
  clickUI.lengthEditor.setFont(smallFont);
  clickUI.lengthEditor.setColour(juce::TextEditor::backgroundColourId,
                              juce::Colour(0xFF333333));
  clickUI.lengthEditor.setColour(juce::TextEditor::textColourId,
                              juce::Colour(0xFFDDDDDD));
  clickUI.lengthEditor.setColour(juce::TextEditor::outlineColourId,
                              juce::Colours::white.withAlpha(0.20f));
  clickUI.lengthEditor.setJustification(juce::Justification::centred);
  clickUI.lengthEditor.setInputRestrictions(0, "0123456789");
  clickUI.lengthEditor.onReturnKey = [this] {
    // TODO: DSP パラメータへの書き込み（length ms）
    clickUI.lengthEditor.giveAwayKeyboardFocus();
  };
  addAndMakeVisible(clickUI.lengthEditor);

  clickUI.lengthSuffix.setText("ms", juce::dontSendNotification);
  clickUI.lengthSuffix.setFont(smallFont);
  clickUI.lengthSuffix.setColour(juce::Label::textColourId,
                              UIConstants::Colours::labelText);
  clickUI.lengthSuffix.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(clickUI.lengthSuffix);
}

// ────────────────────────────────────────────────────
// レイアウト
// ────────────────────────────────────────────────────
void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  // 下部: TYPE/LENGTH グループ（高さ 48px）
  constexpr int typeLengthH = 48;
  auto typeLengthArea = area.removeFromBottom(typeLengthH);
  area.removeFromBottom(4); // ギャップ

  clickUI.typeLengthGroup.setBounds(typeLengthArea);

  // グループ内インセット
  const auto inner = typeLengthArea.reduced(UIConstants::panelPadding, 4)
                         .withTrimmedTop(8); // テキストラベル分

  // [length: ][editor][ms]  [type: combo]
  auto row = inner;
  constexpr int prefixW = 50;
  constexpr int editorW = 34;
  constexpr int suffixW = 22;
  constexpr int gapW    = 12;
  constexpr int typeW   = 70;

  clickUI.lengthPrefix.setBounds(row.removeFromLeft(prefixW));
  clickUI.lengthEditor.setBounds(row.removeFromLeft(editorW));
  clickUI.lengthSuffix.setBounds(row.removeFromLeft(suffixW));
  row.removeFromLeft(gapW);
  // "type:" ラベルは不要（コンボ自体に意味が明確なため省略）
  clickUI.typeCombo.setBounds(row.removeFromLeft(typeW));

  // 上部: SHAPE グループが残り全域
  clickUI.shapeGroup.setBounds(area);

  // SHAPE グループ内のエディタ
  const auto shapeInner =
      area.reduced(UIConstants::panelPadding).withTrimmedTop(8);
  clickUI.shapeEditor.setBounds(shapeInner);
}
