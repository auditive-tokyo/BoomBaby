#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────
// ClickShapeEditor
// Clickトランジェントの形状（Attack/Decay比）をインタラクティブに編集する
// 三角形エンベロープ: 原点→ピーク→終端 の2区間ライン
// ────────────────────────────────────────────────────
class ClickShapeEditor : public juce::Component {
public:
  ClickShapeEditor();

  void paint(juce::Graphics &g) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;
  void mouseDoubleClick(const juce::MouseEvent &e) override;

  // ── ピーク位置（正規化: 0..1） ──
  // peakX: 左端からピークまでの時間比率（Attack/Length）
  // peakY: ピーク振幅（0=最小, 1=最大）
  float getPeakX() const { return peakX; }
  float getPeakY() const { return peakY; }
  void setPeak(float x, float y);

  // ── コールバック ──
  void setOnPeakChanged(std::function<void(float, float)> cb)
  {
    onPeakChanged = std::move(cb);
  }

private:
  std::function<void(float, float)> onPeakChanged;
  // ── 表示領域 ──
  // SHAPE エリア（角マーク付き枠＋波形）は全体 bounds を使用
  static constexpr float kCornerSize = 8.0f; // 角マーク長さ
  static constexpr float kPointRadius = 5.0f;
  static constexpr float kHitRadius = 12.0f;

  float peakX = 0.2f; // デフォルト: 短い Attack（左寄りピーク）
  float peakY = 1.0f; // デフォルト: 最大振幅
  bool dragging = false;

  // ピーク点（ピクセル座標）
  juce::Point<float> peakPixel() const;

  // 正規化座標 → ピクセル
  juce::Point<float> toPixel(float nx, float ny) const;
  // ピクセル → 正規化座標
  juce::Point<float> fromPixel(float px, float py) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickShapeEditor)
};
