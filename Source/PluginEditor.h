#pragma once

#include "DSP/EnvelopeData.h"
#include "GUI/CustomSliderLAF.h"
#include "GUI/EnvelopeCurveEditor.h"
#include "GUI/KeyboardComponent.h"
#include "GUI/MasterFader.h"
#include "GUI/PanelComponent.h"
#include "PluginProcessor.h"

#include <array>

class BabySquatchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer {
public:
  explicit BabySquatchAudioProcessorEditor(BabySquatchAudioProcessor &);
  ~BabySquatchAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &e) override;

private:
  void onEnvelopeChanged();

  // ── 入力波形リアルタイム表示（30fps Timer）──
  void timerCallback() override;
  static constexpr int kWaveDisplayCapacity = 48000; // ~1sec @ 48kHz
  std::vector<float> waveDisplayBuf_;
  int waveDisplayPos_ = 0;    // 次の書き込み位置
  int waveDisplayFilled_ = 0; // 実際に充際されたサンプル数

  // ── コンストラクター分割ヘルパー ──
  void setupPanelRouting(BabySquatchAudioProcessor &p);
  void setupEnvelopeCurveEditor();
  void switchEditTarget(EnvelopeCurveEditor::EditTarget t);
  void setupClickParams();
  void layoutClickParams(juce::Rectangle<int> area);
  void setupDirectParams();
  void layoutDirectParams(juce::Rectangle<int> area);
  void refreshDirectPassthroughUI();
  void onSampleLoadClicked();
  void onSampleFileChosen(const juce::File &file);
  void refreshDirectProvider();
  void onClickSampleLoadClicked();
  void onClickSampleFileChosen(const juce::File &file);
  void refreshClickSampleProvider();
  void setClickModeVisible(bool isSample);
  void applyClickSampleMode();
  void applyClickNoiseMode(int m);
  void setupLengthBox();
  void setupWaveShapeCombo();
  void layoutSubKnobsRow(juce::Rectangle<int> knobRow);
  void setupSubKnobsRow();

  PanelComponent subPanel{"SUB", UIConstants::Colours::subArc};
  PanelComponent clickPanel{"CLICK", UIConstants::Colours::clickArc};
  PanelComponent directPanel{"DIRECT", UIConstants::Colours::directArc};

  // ── 共有展開エリア（常時表示） ──
  BabySquatchAudioProcessor &processorRef;
  KeyboardComponent keyboard;
  struct EnvelopeDatas {
    EnvelopeData amp;
    EnvelopeData freq;
    EnvelopeData dist;
    EnvelopeData mix;
    EnvelopeData clickAmp;
    EnvelopeData directAmp;
  };
  EnvelopeDatas envDatas;
  EnvelopeCurveEditor envelopeCurveEditor{
      envDatas.amp, envDatas.freq,     envDatas.dist,
      envDatas.mix, envDatas.clickAmp, envDatas.directAmp};

  // ── マスターセクション（鍵盤右余白エリア） ──
  MasterFader masterSection;
  juce::Label infoBox;

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
    std::array<CustomSlider, 8> knobs;
    std::array<juce::Label, 8> knobLabels;
    UIConstants::LabelSelector saturateClipType{{"Soft", "Hard", "Tube"},
                                                UIConstants::Colours::subArc};
    struct {
      juce::Label label;
      juce::ComboBox combo;
    } wave;
    struct {
      juce::Label label;
      CustomSlider slider;
    } length;
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
    enum class Mode { Noise = 1, Sample };
    struct KnobUI {
      juce::Label label;
      CustomSlider slider;
    };
    /// Drive + ClipType 共通モジュール（Sub / Direct でも再利用可）
    struct SaturatorUI {
      UIConstants::LabelSelector
          clipType; ///< Soft/Hard/Tube セレクター（ノブ上部ラベルを兼ねる）
      CustomSlider driveSlider;
    };
    struct Bpf1Band {
      UIConstants::SlopeSelector slopeSelector{"BP"};
      CustomSlider freqSlider;
      juce::Label qLabel;
      CustomSlider qSlider;
    };
    struct FilterBand {
      UIConstants::SlopeSelector slope;
      CustomSlider slider;
      juce::Label qLabel;
      CustomSlider qSlider;
      explicit FilterBand(const char *tag) : slope{tag} {}
    };
    struct NoiseUI {
      juce::Label decayLabel;
      CustomSlider decaySlider;
      Bpf1Band bpf1; ///< BP Slope / Q
      SaturatorUI saturator;
    };
    struct SampleUI {
      UIConstants::SampleDropButton loadButton{"Drop or Click to Load"};
      juce::String loadedFilePath;
      std::unique_ptr<juce::FileChooser> fileChooser;
      std::vector<float> thumbMin;
      std::vector<float> thumbMax;
      double thumbDurSec = 0.0;
      KnobUI pitch;
      KnobUI amp; ///< 0〜200% 振幅スケーラー（Sub の Amp と同等）
      KnobUI decay; ///< Noiseモードの Decay と完全別管理（初期値 = Sub length）
    };

    /// モード毎に独立保存されるパラメーター状態
    struct ModeState {
      double hpfFreq = 20.0; // Hz (20=bypass)
      double hpfQ = 0.71;
      int hpfSlope = 12;        // 12/24/48
      double lpfFreq = 20000.0; // Hz (20000=bypass)
      double lpfQ = 0.71;
      int lpfSlope = 12;
      double drive = 0.0; // dB
      int clipType = 0;   // 0=Soft,1=Hard,2=Tube
    };

    juce::Label modeLabel;
    juce::ComboBox modeCombo;
    NoiseUI noise;
    FilterBand hpf{"HP"};
    FilterBand lpf{"LP"};
    SampleUI sample;
    ModeState noiseState;  ///< Noise モード保存値
    ModeState sampleState; ///< Sample モード保存値
    std::function<float(float)> noiseProvider;

    /// 共有ウィジェットの状態を ModeState へ保存
    void saveModeState(ModeState &dst) const;
    /// ModeState から共有ウィジェット＋DSP へ復元
    void restoreModeState(const ModeState &src, ClickEngine &eng);
  };
  ClickUI clickUI;

  // ── DIRECTパネル ──
  struct DirectUI {
    enum class Mode { Direct = 1, Sample };
    juce::Label modeLabel;
    juce::ComboBox modeCombo;
    UIConstants::SampleDropButton sampleLoadButton{"Drop or Click to Load"};
    juce::String loadedFilePath;
    std::unique_ptr<juce::FileChooser> fileChooser;
    // サムネイルデータ（Pitch プレビュー更新用）
    std::vector<float> thumbMin;
    std::vector<float> thumbMax;
    double thumbDurSec = 0.0;
    // ── Pitch / Amp / Drive / Decay ノブ（上段） ──
    struct KnobUI {
      juce::Label label;
      CustomSlider slider;
    };
    /// Drive + ClipType モジュール（ClickUI::SaturatorUI と同形）
    struct SaturatorUI {
      UIConstants::LabelSelector clipType{{"Soft", "Hard", "Tube"},
                                          UIConstants::Colours::directArc};
      CustomSlider driveSlider;
    };
    KnobUI pitch;
    KnobUI amp; ///< 0〜200% 振幅スケーラー
    SaturatorUI saturator;
    KnobUI decay;
    // ── フィルター ノブ（下段） ──
    UIConstants::SlopeSelector hpfSlope{"HP", UIConstants::Colours::directArc};
    CustomSlider hpfSlider;
    juce::Label hpfQLabel;
    CustomSlider hpfQSlider;
    UIConstants::SlopeSelector lpfSlope{"LP", UIConstants::Colours::directArc};
    CustomSlider lpfSlider;
    juce::Label lpfQLabel;
    CustomSlider lpfQSlider;    // ── Threshold ノブ（パススルーモード時に Pitch 位置へ表示） ──
    KnobUI threshold;
    // ── Hold スライダー（mode ドロップダウン右） ──
    juce::Label holdLabel;
    CustomSlider holdSlider;  };
  DirectUI directUI;

  // ── ツールチップ（Gainノブの無効時などに使用） ──
  juce::TooltipWindow tooltipWindow{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BabySquatchAudioProcessorEditor)
};
