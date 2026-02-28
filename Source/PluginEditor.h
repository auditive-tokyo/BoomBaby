#pragma once

#include "DSP/EnvelopeData.h"
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

  // ── コンストラクター分割ヘルパー ──
  void bakeAmpLut();
  void bakePitchLut();
  void bakeDistLut();
  void bakeBlendLut();
  void setupPanelRouting(BabySquatchAudioProcessor &p);
  void setupEnvelopeCurveEditor();
  void setupSubKnobsRow();
  void setupWaveShapeButtons();
  void setupPitchKnob();
  void setupAmpKnob();
  void setupBlendKnob();
  void setupDistKnob();
  void setupHarmonicKnobs();
  void deselectOtherWaveShapeButtons(size_t selectedIdx);
  void layoutSubKnobsRow(juce::Rectangle<int> knobRow);
  void layoutWaveShapeButtonRow(juce::Rectangle<int> btnRow);
  void setupLengthBox();

  PanelComponent subPanel{"SUB",    UIConstants::Colours::subArc};
  PanelComponent clickPanel{"CLICK",  UIConstants::Colours::clickArc};
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
  EnvelopeCurveEditor envelopeCurveEditor{ampEnvData, pitchEnvData, distEnvData, blendEnvData};

  // ── SUB展開パネル: LAF（subKnobs より先に宣言し、後に破棄されるようにする） ──
  ColouredSliderLAF subKnobLAF{UIConstants::Colours::subArc,
                                  UIConstants::Colours::subThumb};
  // ── SUB展開パネル: Oscノブ行（8本） ──
  std::array<juce::Slider, 8> subKnobs;
  std::array<juce::Label, 8> subKnobLabels;
  // ── SUB展開パネル: 波形選択ボタン行（Tri / SQR / SAW）+ Length ボックス ──
  std::array<juce::TextButton, 3> waveShapeButtons;
  juce::Label lengthPrefixLabel;
  juce::TextEditor lengthEditor;
  juce::Label lengthSuffixLabel;
  // ── ツールチップ（AMPノブの無効時などに使用） ──
  juce::TooltipWindow tooltipWindow{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessorEditor)
};
