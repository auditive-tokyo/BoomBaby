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
  enum class ExpandChannel { none, oomph, click, dry };

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
  void setupOomphKnobsRow();
  void setupWaveShapeButtons();
  void setupPitchKnob();
  void setupAmpKnob();
  void setupBlendKnob();
  void setupDistKnob();
  void setupHarmonicKnobs();
  void deselectOtherWaveShapeButtons(size_t selectedIdx);
  void layoutOomphKnobsRow(juce::Rectangle<int> knobRow);
  void layoutWaveShapeButtonRow(juce::Rectangle<int> btnRow);

  PanelComponent oomphPanel{"OOMPH", UIConstants::Colours::oomphArc,
                            UIConstants::Colours::oomphThumb};

  PanelComponent clickPanel{"CLICK", UIConstants::Colours::clickArc,
                            UIConstants::Colours::clickThumb};

  PanelComponent dryPanel{"DRY", UIConstants::Colours::dryArc,
                          UIConstants::Colours::dryThumb};

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

  // ── OOMPH展開パネル: LAF（oomphKnobs より先に宣言し、後に破棄されるようにする） ──
  ColouredSliderLAF oomphKnobLAF{UIConstants::Colours::oomphArc,
                                  UIConstants::Colours::oomphThumb};
  // ── OOMPH展開パネル: Oscノブ行（8本） ──
  std::array<juce::Slider, 8> oomphKnobs;
  std::array<juce::Label, 8> oomphKnobLabels;
  // ── OOMPH展開パネル: 波形選択ボタン行（Tri / SQR / SAW） ──
  std::array<juce::TextButton, 3> waveShapeButtons;
  // ── ツールチップ（AMPノブの無効時などに使用） ──
  juce::TooltipWindow tooltipWindow{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessorEditor)
};
