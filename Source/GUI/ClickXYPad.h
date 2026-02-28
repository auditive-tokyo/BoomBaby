#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────
// ClickXYPad
// Click サウンドの 2 パラメータを XY ドラッグで制御する
//   X 軸: Decay  （左=LONG  ←→  右=SHORT）
//   Y 軸: BLEND  （下=SQR   ↑   上=SAW）
// ────────────────────────────────────────────────────
class ClickXYPad : public juce::Component {
public:
  ClickXYPad();

  void paint(juce::Graphics &g) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;

  // ── 値アクセス（0.0 〜 1.0） ──
  float getBlend() const { return blend; } // 0=SQR, 1=SAW  (Y軸)
  float getDecay() const { return decay; } // 0=LONG, 1=SHORT (X軸)
  void setValues(float b, float d);

  // ── コールバック ──
  void setOnChanged(std::function<void(float blend, float decay)> cb) {
    onChanged = std::move(cb);
  }

private:
  static constexpr float kCornerSize  = 8.0f;
  static constexpr float kPointRadius = 5.0f;
  static constexpr float kHitRadius   = 14.0f;

  float blend    = 0.5f; // デフォルト: 中間（SQR/SAW 等量）
  float decay    = 0.35f; // デフォルト: やや短め
  bool  dragging = false;

  std::function<void(float, float)> onChanged;

  juce::Point<float> toPixel(float bx, float dy) const;
  juce::Point<float> fromPixel(float px, float py) const;
  juce::Point<float> controlPixel() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickXYPad)
};
