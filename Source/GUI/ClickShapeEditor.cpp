#include "ClickShapeEditor.h"
#include "UIConstants.h"

ClickShapeEditor::ClickShapeEditor() { setOpaque(false); }

// ── 座標変換 ──────────────────────────────────────────

juce::Point<float> ClickShapeEditor::toPixel(float nx, float ny) const {
  const auto b = getLocalBounds().toFloat().reduced(1.0f);
  return {b.getX() + nx * b.getWidth(), b.getBottom() - ny * b.getHeight()};
}

juce::Point<float> ClickShapeEditor::fromPixel(float px, float py) const {
  const auto b = getLocalBounds().toFloat().reduced(1.0f);
  const float nx =
      (b.getWidth() > 0.0f) ? (px - b.getX()) / b.getWidth() : 0.0f;
  const float ny =
      (b.getHeight() > 0.0f) ? (b.getBottom() - py) / b.getHeight() : 0.0f;
  return {juce::jlimit(0.0f, 1.0f, nx), juce::jlimit(0.0f, 1.0f, ny)};
}

juce::Point<float> ClickShapeEditor::peakPixel() const {
  return toPixel(peakX, peakY);
}

void ClickShapeEditor::setPeak(float x, float y) {
  peakX = juce::jlimit(0.01f, 0.99f, x);
  peakY = juce::jlimit(0.01f, 1.0f, y);
  if (onPeakChanged)
    onPeakChanged(peakX, peakY);
  repaint();
}

// ── 角マーク ──────────────────────────────────────────

static void paintCornerMark(juce::Graphics &g, juce::Point<float> corner, // NOSONAR(cpp:S995)
                            float dx, float dy, float size) {
  // L字型: corner から水平・垂直方向にそれぞれ size px
  g.drawLine(corner.x, corner.y, corner.x + dx * size, corner.y, 1.5f);
  g.drawLine(corner.x, corner.y, corner.x, corner.y + dy * size, 1.5f);
}

// ── paint ────────────────────────────────────────────

void ClickShapeEditor::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const auto inner = bounds.reduced(1.0f);
  const juce::Colour accent = UIConstants::Colours::clickArc;

  // -- 背景 --
  g.setColour(UIConstants::Colours::waveformBg);
  g.fillRoundedRectangle(bounds, 3.0f);

  // -- 枠線（薄め） --
  g.setColour(accent.withAlpha(0.25f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

  // -- 角マーク（L字型） --
  g.setColour(accent.withAlpha(0.80f));
  const float cs = kCornerSize;
  paintCornerMark(g, inner.getTopLeft(), +1, +1, cs);     // ┌
  paintCornerMark(g, inner.getTopRight(), -1, +1, cs);    // ┐
  paintCornerMark(g, inner.getBottomLeft(), +1, -1, cs);  // └
  paintCornerMark(g, inner.getBottomRight(), -1, -1, cs); // ┘

  // -- グリッドライン（中央の十字、非常に薄い） --
  g.setColour(juce::Colours::white.withAlpha(0.05f));
  g.drawHorizontalLine(static_cast<int>(inner.getCentreY()), inner.getX(),
                       inner.getRight());
  g.drawVerticalLine(static_cast<int>(inner.getCentreX()), inner.getY(),
                     inner.getBottom());

  // -- トランジェント形状 --
  // 3点: 左端底 → ピーク → 右端底
  const auto startPt = toPixel(0.0f, 0.0f);
  const auto peak = peakPixel();
  const auto endPt = toPixel(1.0f, 0.0f);

  juce::Path shape;
  shape.startNewSubPath(startPt.x, startPt.y);
  shape.lineTo(peak.x, peak.y);
  shape.lineTo(endPt.x, endPt.y);
  shape.closeSubPath();

  // 塗りつぶし（グラジエント）
  juce::ColourGradient fillGrad(accent.withAlpha(0.25f), peak.x, peak.y,
                                accent.withAlpha(0.04f), peak.x, startPt.y,
                                false);
  g.setGradientFill(fillGrad);
  g.fillPath(shape);

  // ストローク（グロー 2 層）
  juce::Path line;
  line.startNewSubPath(startPt.x, startPt.y);
  line.lineTo(peak.x, peak.y);
  line.lineTo(endPt.x, endPt.y);
  g.setColour(accent.withAlpha(0.15f));
  g.strokePath(line, juce::PathStrokeType(5.0f));
  g.setColour(accent.withAlpha(0.90f));
  g.strokePath(line, juce::PathStrokeType(1.5f));

  // -- ピーク制御点 --
  constexpr float r = kPointRadius;
  g.setColour(juce::Colours::white.withAlpha(0.85f));
  g.fillEllipse(peak.x - r, peak.y - r, r * 2.0f, r * 2.0f);
  g.setColour(accent);
  g.drawEllipse(peak.x - r, peak.y - r, r * 2.0f, r * 2.0f, 1.5f);
}

// ── マウス操作 ────────────────────────────────────────

void ClickShapeEditor::mouseDown(const juce::MouseEvent &e) {
  const auto pp = peakPixel();
  const float dx = e.position.x - pp.x;
  const float dy = e.position.y - pp.y;
  dragging = (dx * dx + dy * dy <= kHitRadius * kHitRadius);
}

void ClickShapeEditor::mouseDrag(const juce::MouseEvent &e) {
  if (!dragging)
    return;
  const auto n = fromPixel(e.position.x, e.position.y);
  setPeak(n.x, n.y);
}

void ClickShapeEditor::mouseUp(const juce::MouseEvent &) { dragging = false; }

void ClickShapeEditor::mouseDoubleClick(const juce::MouseEvent &) {
  // ダブルクリックでデフォルトリセット
  setPeak(0.2f, 1.0f);
}
