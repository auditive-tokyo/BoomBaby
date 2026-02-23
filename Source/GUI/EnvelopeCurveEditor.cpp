#include "EnvelopeCurveEditor.h"
#include "../DSP/EnvelopeData.h"
#include "UIConstants.h"

#include <array>
#include <cmath>

EnvelopeCurveEditor::EnvelopeCurveEditor(EnvelopeData &ampData,
                                         EnvelopeData &pitchData)
    : ampEnvData(ampData), pitchEnvData(pitchData), editEnvData(&ampData) {
  setOpaque(true);
}

void EnvelopeCurveEditor::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const float w = bounds.getWidth();
  const float h = plotHeight();
  const float centreY = h * 0.5f;

  g.fillAll(UIConstants::Colours::waveformBg);

  if (w < 1.0f || h < 1.0f)
    return;

  paintWaveform(g, w, h, centreY);
  paintEnvelopeOverlay(g, w);
  paintTimeline(g, w, h, bounds.getHeight());
  paintTabs(g);
}

// ── paint() 分割ヘルパー ──

void EnvelopeCurveEditor::paintWaveform(juce::Graphics &g, float w, float h,
                                        float centreY) const {
  const auto numPixels = static_cast<int>(w);
  const bool hasAmpPoints = ampEnvData.hasPoints();
  const bool hasPitchPoints = pitchEnvData.hasPoints();

  juce::Path fillPath;
  juce::Path waveLine;
  fillPath.startNewSubPath(0.0f, centreY);

  // 位相累積方式: 各ピクセルで瞬時周波数→位相増分を積算
  float phase = 0.0f;
  const float dtMs = displayDurationMs / w; // 1ピクセルあたりの時間(ms)

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = x * dtMs;

    // Pitch: Hz
    // を取得（エンベロープポイントがあればevaluate、なければdefaultValue）
    const float hz = hasPitchPoints ? pitchEnvData.evaluate(timeMs)
                                    : pitchEnvData.getValue();
    // 位相増分 = Hz × (dtMs / 1000) × 2π
    if (i > 0)
      phase += hz * (dtMs / 1000.0f) * juce::MathConstants<float>::twoPi;

    // 波形モーフィング: BLEND クロスフェード
    const float sinVal = std::sin(phase);
    const float waveVal = computePreviewWaveValue(sinVal, previewBlend, phase);

    // AMP: 振幅
    const float amplitude =
        hasAmpPoints ? ampEnvData.evaluate(timeMs) : ampEnvData.getValue();
    const float scaledAmp = std::min(amplitude, 2.0f) * centreY;
    const float y = juce::jlimit(0.0f, h, centreY - waveVal * scaledAmp);

    fillPath.lineTo(x, y);

    if (i == 0)
      waveLine.startNewSubPath(x, y);
    else
      waveLine.lineTo(x, y);
  }

  fillPath.lineTo(w, centreY);
  fillPath.closeSubPath();

  g.setColour(UIConstants::Colours::oomphArc.withAlpha(0.3f));
  g.fillPath(fillPath);

  g.setColour(UIConstants::Colours::oomphArc);
  g.strokePath(waveLine, juce::PathStrokeType(1.5f));

  g.setColour(juce::Colours::white.withAlpha(0.08f));
  g.drawHorizontalLine(static_cast<int>(centreY), 0.0f, w);
}

void EnvelopeCurveEditor::setWaveShape(WaveShape shape) {
  if (previewShape != shape) {
    previewShape = shape;
    repaint();
  }
}

void EnvelopeCurveEditor::setPreviewBlend(float blend) {
  const float clamped = juce::jlimit(-1.0f, 1.0f, blend);
  if (std::abs(previewBlend - clamped) > 1e-6f) {
    previewBlend = clamped;
    repaint();
  }
}

void EnvelopeCurveEditor::setPreviewHarmonicGain(int harmonicNum, float gain) {
  if (harmonicNum >= 1 && harmonicNum <= 4) {
    const auto idx = static_cast<size_t>(harmonicNum - 1);
    if (std::abs(previewHarmonicGains[idx] - gain) > 1e-6f) {
      previewHarmonicGains[idx] = gain;
      repaint();
    }
  }
}

float EnvelopeCurveEditor::computePreviewWaveValue(float sinVal, float blend,
                                                   float phase) const {
  if (blend <= 0.0f) {
    // -100 側: Sine → WaveShape
    if (previewShape == WaveShape::Sine)
      return sinVal;
    return std::lerp(sinVal, shapeOscValue(previewShape, phase), -blend);
  }
  // +100 側: Sine → Additive（H1〜H4）
  float addVal = 0.0f;
  for (size_t n = 0; n < 4; ++n) {
    if (previewHarmonicGains[n] > 0.0f)
      addVal += previewHarmonicGains[n] *
                std::sin(phase * static_cast<float>(n + 1));
  }
  return std::lerp(sinVal, addVal, blend);
}

float EnvelopeCurveEditor::shapeOscValue(WaveShape shape, float phase) {
  // phase を 0〜2π に正規化
  const float twoPi = juce::MathConstants<float>::twoPi;
  float p = std::fmod(phase, twoPi);
  if (p < 0.0f)
    p += twoPi;

  switch (shape) {
    using enum WaveShape;
  case Tri:
    // Triangle: asin(sin(phase)) * 2/π
    return std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
  case Square:
    return p < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
  case Saw:
    return (p / juce::MathConstants<float>::pi) - 1.0f;
  case Sine:
  default:
    return std::sin(phase);
  }
}

void EnvelopeCurveEditor::paintEnvelopeOverlay(juce::Graphics &g,
                                               float w) const {
  if (!editEnvData->hasPoints())
    return;

  const auto numPixels = static_cast<int>(w);

  // エンベロープカーブライン
  juce::Path envLine;
  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / w) * displayDurationMs;
    const float ey = valueToY(editEnvData->evaluate(timeMs));

    if (i == 0)
      envLine.startNewSubPath(x, ey);
    else
      envLine.lineTo(x, ey);
  }

  // Pitch=シアン、AMP=oomphArc で描画
  using enum EditTarget;
  const auto envColour = (editTarget == pitch)
                             ? juce::Colours::cyan
                             : UIConstants::Colours::oomphArc.brighter(0.4f);
  g.setColour(envColour);
  g.strokePath(envLine, juce::PathStrokeType(1.5f));

  // コントロールポイント
  const auto &pts = editEnvData->getPoints();
  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    const auto idx = static_cast<size_t>(i);
    const float px = timeMsToX(pts[idx].timeMs);
    const float py = valueToY(pts[idx].value);
    constexpr float r = pointHitRadius * 0.6f;

    if (i == dragPointIndex) {
      g.setColour(juce::Colours::white);
      g.fillEllipse(px - r, py - r, r * 2.0f, r * 2.0f);
    } else {
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.fillEllipse(px - r, py - r, r * 2.0f, r * 2.0f);
      g.setColour(juce::Colours::white);
      g.drawEllipse(px - r, py - r, r * 2.0f, r * 2.0f, 1.0f);
    }
  }
}

void EnvelopeCurveEditor::paintTimeline(juce::Graphics &g, float w, float h,
                                        float totalH) const {
  g.setColour(juce::Colours::white.withAlpha(0.15f));
  g.drawHorizontalLine(static_cast<int>(h), 0.0f, w);

  // ms 間隔を displayDurationMs に応じて自動選択
  constexpr std::array<float, 9> intervals = {10,  20,   50,   100, 200,
                                              500, 1000, 2000, 5000};
  float interval = intervals[0];
  for (const float iv : intervals) {
    if (displayDurationMs / iv <= 10.0f) {
      interval = iv;
      break;
    }
  }

  const float fontSize = 9.0f;
  g.setFont(juce::Font(juce::FontOptions(fontSize)));

  const auto numMarks = static_cast<int>(displayDurationMs / interval) + 1;
  for (int i = 0; i < numMarks; ++i) {
    const float ms = static_cast<float>(i) * interval;
    const float x = timeMsToX(ms);

    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.drawVerticalLine(static_cast<int>(x), h, h + 4.0f);

    g.setColour(juce::Colour(0xFF999999));
    const juce::String label = (ms >= 1000.0f)
                                   ? juce::String(ms * 0.001f, 1) + "s"
                                   : juce::String(static_cast<int>(ms));

    constexpr float labelW = 36.0f;
    g.drawText(label,
               juce::Rectangle<float>(x - labelW * 0.5f, h + 2.0f, labelW,
                                      totalH - h - 2.0f),
               juce::Justification::centredTop, false);
  }
}

void EnvelopeCurveEditor::setDisplayCycles(float cycles) {
  displayCycles = cycles;
  repaint();
}

void EnvelopeCurveEditor::setDisplayDurationMs(float ms) {
  displayDurationMs = ms;
  repaint();
}

void EnvelopeCurveEditor::setOnChange(std::function<void()> cb) {
  onChange = std::move(cb);
}

// ── 座標変換ヘルパー ──

float EnvelopeCurveEditor::timeMsToX(float timeMs) const {
  const auto w = static_cast<float>(getWidth());
  return (displayDurationMs > 0.0f) ? (timeMs / displayDurationMs) * w : 0.0f;
}

float EnvelopeCurveEditor::plotHeight() const {
  return std::max(1.0f, static_cast<float>(getHeight()) - timelineHeight);
}

float EnvelopeCurveEditor::editMinValue() const {
  using enum EditTarget;
  return (editTarget == pitch) ? 20.0f : 0.0f;
}

float EnvelopeCurveEditor::editMaxValue() const {
  using enum EditTarget;
  return (editTarget == pitch) ? 20000.0f : 2.0f;
}

float EnvelopeCurveEditor::valueToY(float value) const {
  using enum EditTarget;
  const float h = plotHeight();
  if (editTarget == pitch) {
    // 対数スケール: 20 Hz → 下端、20000 Hz → 上端
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float logVal = std::log(std::max(value, 20.0f));
    const float norm = (logVal - lo) / (hi - lo); // 0..1
    return h - norm * h;
  }
  // AMP: value 0.0 → 下端、value 2.0 → 上端
  return h - (value / 2.0f) * h;
}

float EnvelopeCurveEditor::xToTimeMs(float x) const {
  const auto w = static_cast<float>(getWidth());
  return (w > 0.0f) ? (x / w) * displayDurationMs : 0.0f;
}

float EnvelopeCurveEditor::yToValue(float y) const {
  using enum EditTarget;
  const float h = plotHeight();
  if (editTarget == pitch) {
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float norm = (h > 0.0f) ? (1.0f - y / h) : 0.5f;
    return std::exp(std::lerp(lo, hi, norm));
  }
  return (h > 0.0f) ? (1.0f - y / h) * 2.0f : 1.0f;
}

int EnvelopeCurveEditor::findPointAtPixel(float px, float py) const {
  const auto &pts = editEnvData->getPoints();
  const float r2 = pointHitRadius * pointHitRadius;

  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    const float dx = px - timeMsToX(pts[static_cast<size_t>(i)].timeMs);
    const float dy = py - valueToY(pts[static_cast<size_t>(i)].value);
    if (dx * dx + dy * dy <= r2)
      return i;
  }
  return -1;
}

// ── マウス操作 ──

void EnvelopeCurveEditor::mouseDoubleClick(const juce::MouseEvent &e) {
  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);
  if (const int hit = findPointAtPixel(px, py); hit >= 0) {
    editEnvData->removePoint(hit);
  } else {
    const float timeMs = std::clamp(xToTimeMs(px), 0.0f, displayDurationMs);
    const float value =
        std::clamp(yToValue(py), editMinValue(), editMaxValue());
    editEnvData->addPoint(timeMs, value);
  }

  if (onChange)
    onChange();
  repaint();
}

void EnvelopeCurveEditor::mouseDown(const juce::MouseEvent &e) {
  using enum EditTarget;
  const auto pt = e.position;

  // タブクリック判定
  if (ampTabRect().contains(pt)) {
    setEditTarget(amp);
    if (onEditTargetChanged)
      onEditTargetChanged(amp);
    return;
  }
  if (pitchTabRect().contains(pt)) {
    setEditTarget(pitch);
    if (onEditTargetChanged)
      onEditTargetChanged(pitch);
    return;
  }

  dragPointIndex =
      findPointAtPixel(static_cast<float>(e.x), static_cast<float>(e.y));
}

void EnvelopeCurveEditor::mouseDrag(const juce::MouseEvent &e) {
  if (dragPointIndex < 0)
    return;

  const float timeMs =
      std::clamp(xToTimeMs(static_cast<float>(e.x)), 0.0f, displayDurationMs);
  const float value = std::clamp(yToValue(static_cast<float>(e.y)),
                                 editMinValue(), editMaxValue());
  dragPointIndex = editEnvData->movePoint(dragPointIndex, timeMs, value);

  if (onChange)
    onChange();
  repaint();
}

void EnvelopeCurveEditor::mouseUp(const juce::MouseEvent & /*e*/) {
  dragPointIndex = -1;
}

void EnvelopeCurveEditor::setEditTarget(EditTarget target) {
  using enum EditTarget;
  editTarget = target;
  editEnvData = (target == amp) ? &ampEnvData : &pitchEnvData;
  dragPointIndex = -1;
  repaint();
}

void EnvelopeCurveEditor::setOnEditTargetChanged(
    std::function<void(EditTarget)> cb) {
  onEditTargetChanged = std::move(cb);
}

// ── タブ描画・ヒット領域 ──

juce::Rectangle<float> EnvelopeCurveEditor::ampTabRect() const {
  const float x = static_cast<float>(getWidth()) - tabPad - tabW * 2.0f - 2.0f;
  return {x, tabPad, tabW, tabH};
}

juce::Rectangle<float> EnvelopeCurveEditor::pitchTabRect() const {
  const float x = static_cast<float>(getWidth()) - tabPad - tabW;
  return {x, tabPad, tabW, tabH};
}

void EnvelopeCurveEditor::paintTabs(juce::Graphics &g) const {
  const auto ampRect = ampTabRect();
  const auto pitchRect = pitchTabRect();

  using enum EditTarget;
  const bool isAmp = (editTarget == amp);

  // AMP タブ
  g.setColour(isAmp ? UIConstants::Colours::oomphArc.withAlpha(0.8f)
                    : juce::Colours::white.withAlpha(0.12f));
  g.fillRoundedRectangle(ampRect, 3.0f);
  g.setColour(isAmp ? juce::Colours::white
                    : juce::Colours::white.withAlpha(0.5f));
  g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
  g.drawText("AMP", ampRect, juce::Justification::centred, false);

  // PITCH タブ
  g.setColour(!isAmp ? juce::Colours::cyan.withAlpha(0.8f)
                     : juce::Colours::white.withAlpha(0.12f));
  g.fillRoundedRectangle(pitchRect, 3.0f);
  g.setColour(!isAmp ? juce::Colours::white
                     : juce::Colours::white.withAlpha(0.5f));
  g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
  g.drawText("PITCH", pitchRect, juce::Justification::centred, false);
}
