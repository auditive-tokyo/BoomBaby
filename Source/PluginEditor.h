#pragma once

#include "DSP/EnvelopeData.h"
#include "GUI/CustomSliderLAF.h"
#include "GUI/EnvelopeCurveEditor.h"
#include "GUI/KeyboardComponent.h"
#include "GUI/MasterFader.h"
#include "GUI/PanelComponent.h"
#include "PluginProcessor.h"

#include <array>

class BabySquatchAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit BabySquatchAudioProcessorEditor(BabySquatchAudioProcessor &);
  ~BabySquatchAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &e) override;

private:
  void onEnvelopeChanged();

  // ── コンストラクター分割ヘルパー ──
  void setupPanelRouting(BabySquatchAudioProcessor &p);
  void setupEnvelopeCurveEditor();
  void switchEditTarget(EnvelopeCurveEditor::EditTarget t);
  void setupClickParams();
  void layoutClickParams(juce::Rectangle<int> area);
  void setupDirectParams();
  void layoutDirectParams(juce::Rectangle<int> area);
  void onSampleLoadClicked();
  void onSampleFileChosen(const juce::File &file);
  void refreshDirectProvider();
  void onClickSampleLoadClicked();
  void onClickSampleFileChosen(const juce::File &file);
  void refreshClickSampleProvider();
  void applyClickSampleMode();
  void applyClickToneNoiseMode(int m);
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

  // ── 共有展開エリア（常時表示） ──
  BabySquatchAudioProcessor &processorRef;
  KeyboardComponent keyboard;
  EnvelopeData ampEnvData;
  EnvelopeData pitchEnvData;
  EnvelopeData distEnvData;
  EnvelopeData blendEnvData;
  EnvelopeCurveEditor envelopeCurveEditor{ampEnvData, pitchEnvData, distEnvData,
                                          blendEnvData};

  // ── マスターセクション（鍵盤右余白エリア） ──
  MasterFader masterSection;
  juce::Label  infoBox;

  // ── SUB展開パネル: LAF（subUI より先に宣言し、後に破棄されるようにする） ──
  ColouredSliderLAF subKnobLAF{UIConstants::Colours::subArc,
                               UIConstants::Colours::subThumb};
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
  // ── SUB展開パネル: Oscノブ行 / 波形選択 / Length（まとめて管理） ──
  struct SubUI {
    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8>  knobLabels;
    struct { juce::Label label; juce::ComboBox combo;  } wave;
    struct { juce::Label label; juce::Slider   slider; } length;
  };
  SubUI subUI;

  // ── CLICK展開パネル: LAF（clickUI のスライダーより先に宣言） ──
  ColouredSliderLAF clickKnobLAF{UIConstants::Colours::clickArc,
                                UIConstants::Colours::clickThumb};
  // ── DIRECT展開パネル: LAF（directUI のスライダーより先に宣言） ──
  ColouredSliderLAF directKnobLAF{UIConstants::Colours::directArc,
                                 UIConstants::Colours::directThumb};
  // ── CLICK展開パネル ──
  struct ClickUI {
    enum class Mode { Tone = 1, Noise, Sample };
    struct KnobUI  { juce::Label label; juce::Slider slider; };
    struct BpfBand { juce::Label freqLabel; juce::Slider freqSlider;
                     juce::Label qLabel;   juce::Slider qSlider; };
    struct FilterBand {
      UIConstants::SlopeSelector slope;
      juce::Slider slider;
      juce::Label  qLabel;
      juce::Slider qSlider;
      explicit FilterBand(const char *tag) : slope{tag} {}
    };
    struct ToneNoiseUI {
      juce::Label  decayLabel;
      juce::Slider decaySlider;
      BpfBand bpf1; ///< Freq / Focus
      BpfBand bpf2; ///< Air  / Focus
    };
    struct SampleUI {
      UIConstants::SampleDropButton loadButton{"Drop or Click to Load"};
      juce::String loadedFilePath;
      std::unique_ptr<juce::FileChooser> fileChooser;
      std::vector<float> thumbMin;
      std::vector<float> thumbMax;
      double thumbDurSec = 0.0;
      KnobUI pitch;
      KnobUI attack;
      KnobUI hold;
      KnobUI release;
    };

    juce::Label    modeLabel;
    juce::ComboBox modeCombo;
    ToneNoiseUI    toneNoise;
    FilterBand     hpf{"HP"};
    FilterBand     lpf{"LP"};
    SampleUI       sample;
    std::function<float(float)> toneProvider;
    std::function<float(float)> noiseProvider;
  };
  ClickUI clickUI;

  // ── DIRECTパネル ──
  struct DirectUI {
    enum class Mode { Direct = 1, Sample };
    juce::Label      modeLabel;
    juce::ComboBox   modeCombo;
    UIConstants::SampleDropButton sampleLoadButton{"Drop or Click to Load"};
    juce::String     loadedFilePath;
    std::unique_ptr<juce::FileChooser> fileChooser;
    // サムネイルデータ（Pitch プレビュー更新用）
    std::vector<float> thumbMin;
    std::vector<float> thumbMax;
    double thumbDurSec = 0.0;
    // ── Pitch / Envelope ノブ（上段） ──
    struct KnobUI { juce::Label label; juce::Slider slider; };
    KnobUI pitch;
    KnobUI attack;
    KnobUI decay;
    KnobUI release;
    // ── フィルター ノブ（下段） ──
    UIConstants::SlopeSelector hpfSlope{"HP", UIConstants::Colours::directArc};
    juce::Slider hpfSlider;
    juce::Label  hpfQLabel;
    juce::Slider hpfQSlider;
    UIConstants::SlopeSelector lpfSlope{"LP", UIConstants::Colours::directArc};
    juce::Slider lpfSlider;
    juce::Label  lpfQLabel;
    juce::Slider lpfQSlider;
  };
  DirectUI directUI;

  // ── ツールチップ（Gainノブの無効時などに使用） ──
  juce::TooltipWindow tooltipWindow{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessorEditor)
};
