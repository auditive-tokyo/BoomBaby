#pragma once

#include "DSP/EnvelopeData.h"
#include "GUI/ClickShapeEditor.h"
#include "GUI/CustomSliderLAF.h"
#include "GUI/EnvelopeCurveEditor.h"
#include "GUI/KeyboardComponent.h"
#include "GUI/PanelComponent.h"
#include "PluginProcessor.h"

#include <array>

class BabySquatchAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit BabySquatchAudioProcessorEditor(BabySquatchAudioProcessor &);
  ~BabySquatchAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  // 展開チャンネル（None = 閉じた状態）
  enum class ExpandChannel { none, sub, click, direct };

  void requestExpand(ExpandChannel ch);
  void updateExpandIndicators();
  void updateEnvelopeEditorVisibility();
  void onEnvelopeChanged();

  // ── コンストラクター分割ヘルパー ──
  void bakeAmpLut();
  void bakePitchLut();
  void bakeDistLut();
  void bakeBlendLut();
  void setupPanelRouting(BabySquatchAudioProcessor &p);
  void setupEnvelopeCurveEditor();
  void switchEditTarget(EnvelopeCurveEditor::EditTarget t);
  void setupClickParams();
  void layoutClickParams(juce::Rectangle<int> area);
  void layoutLengthBox(juce::Rectangle<int> btnRow);
  void setupLengthBox();
  void setupWaveShapeCombo();
  void setupPitchKnob();
  void setupAmpKnob();
  void setupBlendKnob();
  void setupDistKnob();
  void setupHarmonicKnobs();
  void layoutSubKnobsRow(juce::Rectangle<int> knobRow);
  void setupSubKnobsRow();

  PanelComponent subPanel{"SUB", UIConstants::Colours::subArc};
  PanelComponent clickPanel{"CLICK", UIConstants::Colours::clickArc};
  PanelComponent directPanel{"DIRECT", UIConstants::Colours::directArc};

  // ── 共有展開エリア（3パネル横断） ──
  juce::Component expandableArea;
  ExpandChannel activeChannel = ExpandChannel::none;

  // ── MIDI 鍵盤（展開パネル下部・全チャンネル共通） ──
  BabySquatchAudioProcessor &processorRef;
  KeyboardComponent keyboard;
  EnvelopeData ampEnvData;
  EnvelopeData pitchEnvData;
  EnvelopeData distEnvData;
  EnvelopeData blendEnvData;
  EnvelopeCurveEditor envelopeCurveEditor{ampEnvData, pitchEnvData, distEnvData,
                                          blendEnvData};

  // ── SUB展開パネル: LAF（subKnobs より先に宣言し、後に破棄されるようにする）
  // ──
  ColouredSliderLAF subKnobLAF{UIConstants::Colours::subArc,
                               UIConstants::Colours::subThumb};
  // ── SUB展開パネル: Oscノブ行（8本） ──
  std::array<juce::Slider, 8> subKnobs;
  std::array<juce::Label, 8> subKnobLabels;
  // ── SUB展開パネル: 波形選択（プルダウン）用 LAF ──
  struct DarkComboLAF : public juce::LookAndFeel_V4 {
    DarkComboLAF() {
      setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF333333));
      setColour(juce::ComboBox::textColourId, juce::Colour(0xFFDDDDDD));
      setColour(juce::ComboBox::outlineColourId,
                juce::Colours::white.withAlpha(0.20f));
      setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFBBBBBB));
      setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF2A2A2A));
      setColour(juce::PopupMenu::textColourId, juce::Colour(0xFFDDDDDD));
      setColour(juce::PopupMenu::highlightedBackgroundColourId,
                juce::Colour(0xFF00AAFF).withAlpha(0.6f));
      setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }
    juce::Font getComboBoxFont(juce::ComboBox &) override {
      return juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));
    }
    juce::Font getPopupMenuFont() override {
      return juce::Font(juce::FontOptions(UIConstants::fontSizeMedium));
    }
  };
  DarkComboLAF darkComboLAF;
  // ── SUB展開パネル: 波形選択（プルダウン） ──
  struct SubWaveUI {
    juce::Label label;
    juce::ComboBox combo;
  };
  SubWaveUI subWave;
  // ── SUB展開パネル: Length ボックス ──
  struct LengthBox {
    juce::Label prefix;
    juce::TextEditor editor;
    juce::Label suffix;
  };
  LengthBox lengthBox;

  // ── CLICK展開パネル ──
  struct ClickUI {
    juce::GroupComponent shapeGroup;
    ClickShapeEditor shapeEditor;
    juce::GroupComponent typeLengthGroup;
    juce::ComboBox typeCombo;
    juce::Label lengthPrefix;
    juce::TextEditor lengthEditor;
    juce::Label lengthSuffix;
  };
  ClickUI clickUI;

  // ── ツールチップ（Gainノブの無効時などに使用） ──
  juce::TooltipWindow tooltipWindow{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessorEditor)
};
