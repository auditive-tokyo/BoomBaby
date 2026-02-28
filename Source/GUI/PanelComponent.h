#pragma once

#include "LevelMeter.h"
#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────
// 1 つのパネル = タイトル + フェーダー兼レベルメーター + M/S ボタン
// LevelMeter を背景に、LinearVertical フェーダーを重ねた
// Ableton スタイルのチャンネルストリップ
// ────────────────────────────────────────────────────
class PanelComponent : public juce::Component {
public:
  PanelComponent(const juce::String &name, juce::Colour accentColour);
  ~PanelComponent() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // フェーダースライダーへの参照（PluginEditor から配線）
  juce::Slider &getFader();

  // Mute / Solo コールバック設定
  void setOnMuteChanged(std::function<void(bool)> cb);
  void setOnSoloChanged(std::function<void(bool)> cb);

  // 外部からボタン状態を更新（Solo ハイライト用）
  void setMuteState(bool muted);
  void setSoloState(bool soloed);

  // レベルメーター: dB 値を返すコールバックを設定
  void setLevelProvider(std::function<float()> provider);

private:
  // ── フェーダーサム LookAndFeel ──
  // トラックは透明（LevelMeter が背景）、◁ 付き水平バーでサムを描画
  class FaderLAF : public juce::LookAndFeel_V4 {
  public:
    explicit FaderLAF(juce::Colour accent) : accent_(accent) {}
    void drawLinearSlider(juce::Graphics &, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle,
                          juce::Slider &) override;
    int getSliderThumbRadius(juce::Slider &) override { return 0; }

  private:
    juce::Colour accent_;
  };

  FaderLAF faderLAF;
  juce::Slider fader;
  juce::Label titleLabel;
  LevelMeter levelMeter;
  juce::TextButton muteButton{"M"};
  juce::TextButton soloButton{"S"};
  std::function<void(bool)> onMuteChanged;
  std::function<void(bool)> onSoloChanged;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelComponent)
};
