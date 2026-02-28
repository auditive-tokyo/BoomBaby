#include "ClickXYPad.h"
#include "UIConstants.h"

ClickXYPad::ClickXYPad() { setOpaque(false); }

// ── 座標変換 ──────────────────────────────────────────
//   X: 0=SQR(左端) → 1=SAW(右端)
//   Y: 0=SHORT(下端) → 1=LONG(上端)  ← 画面 Y は反転

juce::Point<float> ClickXYPad::toPixel(float bx, float dy) const {
  const auto b = getLocalBounds().toFloat().reduced(1.0f);
  return {b.getX() + bx * b.getWidth(),
          b.getBottom() - dy * b.getHeight()};
}

juce::Point<float> ClickXYPad::fromPixel(float px, float py) const {
  const auto b = getLocalBounds().toFloat().reduced(1.0f);
  const float bx =
      (b.getWidth()  > 0.0f) ? (px - b.getX())   / b.getWidth()  : 0.0f;
  const float dy =
      (b.getHeight() > 0.0f) ? (b.getBottom() - py) / b.getHeight() : 0.0f;
  return {juce::jlimit(0.0f, 1.0f, bx),
          juce::jlimit(0.0f, 1.0f, dy)};
}

juce::Point<float> ClickXYPad::controlPixel() const {
  // X=decay, Y=blend
  return toPixel(decay, blend);
}

// ── 値セット ──────────────────────────────────────────
void ClickXYPad::setValues(float b, float d) {
  blend = juce::jlimit(0.0f, 1.0f, b);
  decay = juce::jlimit(0.0f, 1.0f, d);
  if (onChanged)
    onChanged(blend, decay);
  repaint();
}

// ── 角マーク（ローカル free 関数） ────────────────────
static void paintCorner(juce::Graphics &g, // NOSONAR(cpp:S995)
                        juce::Point<float> pt, float dx, float dy, float size) {
  g.drawLine(pt.x, pt.y, pt.x + dx * size, pt.y,           1.5f);
  g.drawLine(pt.x, pt.y, pt.x,             pt.y + dy * size, 1.5f);
}

// ── paint ────────────────────────────────────────────
void ClickXYPad::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const auto inner  = bounds.reduced(1.0f);
  const juce::Colour accent = UIConstants::Colours::clickArc;

  // -- 背景 --
  g.setColour(UIConstants::Colours::waveformBg);
  g.fillRoundedRectangle(bounds, 3.0f);

  // -- 枠線 --
  g.setColour(accent.withAlpha(0.25f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

  // -- 角マーク --
  g.setColour(accent.withAlpha(0.80f));
  const float cs = kCornerSize;
  paintCorner(g, inner.getTopLeft(),     +1, +1, cs);
  paintCorner(g, inner.getTopRight(),    -1, +1, cs);
  paintCorner(g, inner.getBottomLeft(),  +1, -1, cs);
  paintCorner(g, inner.getBottomRight(), -1, -1, cs);

  // -- グリッド（3 本ずつ） --
  g.setColour(juce::Colours::white.withAlpha(0.06f));
  for (int i = 1; i <= 2; ++i) {
    const float xf = inner.getX() + inner.getWidth()  * (static_cast<float>(i) / 3.0f);
    const float yf = inner.getY() + inner.getHeight() * (static_cast<float>(i) / 3.0f);
    g.drawVerticalLine(static_cast<int>(xf), inner.getY(), inner.getBottom());
    g.drawHorizontalLine(static_cast<int>(yf), inner.getX(), inner.getRight());
  }

  // -- 軸ラベル --
  const auto labelFont = juce::Font(juce::FontOptions(UIConstants::fontSizeSmall));
  g.setFont(labelFont);
  constexpr float labelAlpha = 0.45f;
  constexpr int   labelH     = 11;
  constexpr int   margin     = 4;

  // X 軸ラベル（左辺・右辺の中央）
  g.setColour(accent.withAlpha(labelAlpha));
  const int midY = static_cast<int>(inner.getCentreY()) - labelH / 2;
  g.drawText("LONG",  static_cast<int>(inner.getX()),        midY,
             36, labelH, juce::Justification::centredLeft,  false);
  g.drawText("SHORT", static_cast<int>(inner.getRight()) - 36, midY,
             36, labelH, juce::Justification::centredRight, false);

  // Y 軸ラベル（上辺・下辺の中央）
  const int midX = static_cast<int>(inner.getCentreX()) - 18;
  g.drawText("SAW", midX, static_cast<int>(inner.getY()) + margin,
             36, labelH, juce::Justification::centred, false);
  g.drawText("SQR", midX, static_cast<int>(inner.getBottom()) - labelH - margin,
             36, labelH, juce::Justification::centred, false);

  // -- クロスヘア（制御点から軸への破線） --
  const auto cp = controlPixel();
  g.setColour(accent.withAlpha(0.25f));
  // 縦線
  g.drawLine(cp.x, inner.getY(), cp.x, inner.getBottom(), 1.0f);
  // 横線
  g.drawLine(inner.getX(), cp.y, inner.getRight(), cp.y, 1.0f);

  // -- 制御点 --
  constexpr float r = kPointRadius;
  g.setColour(juce::Colours::white.withAlpha(0.90f));
  g.fillEllipse(cp.x - r, cp.y - r, r * 2.0f, r * 2.0f);
  g.setColour(accent);
  g.drawEllipse(cp.x - r, cp.y - r, r * 2.0f, r * 2.0f, 1.5f);

  // 制御点の内側に accent グロー
  g.setColour(accent.withAlpha(0.50f));
  constexpr float ri = kPointRadius - 2.0f;
  g.fillEllipse(cp.x - ri, cp.y - ri, ri * 2.0f, ri * 2.0f);
}

// ── マウス操作 ────────────────────────────────────────
void ClickXYPad::mouseDown(const juce::MouseEvent &e) {
  const auto cp = controlPixel();
  const float dx = e.position.x - cp.x;
  const float dy = e.position.y - cp.y;
  dragging = (dx * dx + dy * dy <= kHitRadius * kHitRadius);
  if (!dragging) {
    dragging = true;
    const auto n = fromPixel(e.position.x, e.position.y);
    setValues(n.y, n.x); // blend=Y, decay=X
  }
}

void ClickXYPad::mouseDrag(const juce::MouseEvent &e) {
  if (!dragging)
    return;
  const auto n = fromPixel(e.position.x, e.position.y);
  setValues(n.y, n.x); // blend=Y, decay=X
}

void ClickXYPad::mouseUp(const juce::MouseEvent &) { dragging = false; }

void ClickXYPad::mouseDoubleClick(const juce::MouseEvent &) {
  // ダブルクリックでデフォルトリセット
  setValues(0.5f, 0.35f);
}
