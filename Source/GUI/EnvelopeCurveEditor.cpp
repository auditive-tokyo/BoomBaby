#include "EnvelopeCurveEditor.h"
#include "../DSP/EnvelopeData.h"
#include "UIConstants.h"

#include <array>
#include <cmath>

EnvelopeCurveEditor::EnvelopeCurveEditor(EnvelopeData &ampData,
                                         EnvelopeData &pitchData,
                                         EnvelopeData &distData,
                                         EnvelopeData &blendData)
    : ampEnvData(ampData), pitchEnvData(pitchData), distEnvData(distData),
      blendEnvData(blendData), editEnvData(&ampData) {
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
  const bool hasAmpPoints   = ampEnvData.hasPoints();
  const bool hasPitchPoints = pitchEnvData.hasPoints();
  const bool hasBlendPoints = blendEnvData.hasPoints();
  const bool hasDistPoints  = distEnvData.hasPoints();

  juce::Path fillPath;
  juce::Path waveLine;
  fillPath.startNewSubPath(0.0f, centreY);

  // 位相累積方式: 各ピクセルで瞬時周波数→位相増分を積算
  float phase = 0.0f;
  const float dtMs = displayDurationMs / w; // 1ピクセルあたりの時間(ms)

  // 末尾 fadeout: DSP と同じ 5ms half-cosine
  constexpr float fadeOutMs = 5.0f;
  const float fadeStartMs = std::max(0.0f, displayDurationMs - fadeOutMs);

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = x * dtMs;

    // Pitch
    const float hz = hasPitchPoints ? pitchEnvData.evaluate(timeMs)
                                    : pitchEnvData.getValue();
    if (i > 0)
      phase += hz * (dtMs / 1000.0f) * juce::MathConstants<float>::twoPi;

    // BLEND: エンベロープポイントがあれば時間軸で評価、なければノブ値
    const float blend = hasBlendPoints ? blendEnvData.evaluate(timeMs)
                                       : previewBlend;

    // 波形モーフィング
    const float sinVal  = std::sin(phase);
    float waveVal       = computePreviewWaveValue(sinVal, blend, phase);

    // DIST: tanh ソフトクリップ（drive01=0〜1 → driveAmount=1〜10）
    if (const float drive01 = hasDistPoints ? distEnvData.evaluate(timeMs)
                                            : distEnvData.getValue();
        drive01 > 0.001f)
      waveVal = std::tanh(waveVal * (1.0f + drive01 * 9.0f));

    // AMP: 振幅
    const float amplitude =
        hasAmpPoints ? ampEnvData.evaluate(timeMs) : ampEnvData.getValue();

    // 末尾 fadeout ゲイン
    float fadeGain = 1.0f;
    if (timeMs > fadeStartMs && fadeOutMs > 0.0f) {
      const float t = (timeMs - fadeStartMs) / fadeOutMs;
      fadeGain = 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
    }

    const float scaledAmp = std::min(amplitude, 2.0f) * centreY * fadeGain;
    const float y = juce::jlimit(0.0f, h, centreY - waveVal * scaledAmp);

    fillPath.lineTo(x, y);

    if (i == 0)
      waveLine.startNewSubPath(x, y);
    else
      waveLine.lineTo(x, y);
  }

  fillPath.lineTo(w, centreY);
  fillPath.closeSubPath();

  // グラジエント塗りつぶし: 波形外縁→中心ラインへ青→透明
  const juce::Colour baseBlue{0xFF2080FF}; // 純粋なブルー（シアン寄り排除）
  juce::ColourGradient fillGrad(baseBlue.withAlpha(0.40f), 0.0f, 0.0f,
                                baseBlue.withAlpha(0.02f), 0.0f, centreY,
                                false);
  g.setGradientFill(fillGrad);
  g.fillPath(fillPath);

  // グロー ストローク 3層（外→コア）
  g.setColour(baseBlue.withAlpha(0.08f));
  g.strokePath(waveLine, juce::PathStrokeType(9.0f));

  g.setColour(baseBlue.withAlpha(0.30f));
  g.strokePath(waveLine, juce::PathStrokeType(3.5f));

  g.setColour(baseBlue.withAlpha(0.95f));
  g.strokePath(waveLine, juce::PathStrokeType(1.2f));

  g.setColour(juce::Colours::white.withAlpha(0.06f));
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
      addVal +=
          previewHarmonicGains[n] * std::sin(phase * static_cast<float>(n + 1));
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

  // タブカラーに合わせたエンベロープ描画
  using enum EditTarget;
  juce::Colour envColour;
  switch (editTarget) {
  case amp:
    envColour = UIConstants::Colours::subArc.brighter(0.4f);
    break;
  case pitch:
    envColour = juce::Colours::cyan;
    break;
  case dist:
    envColour = juce::Colour(0xFFFF9500);
    break; // オレンジ
  case blend:
    envColour = juce::Colour(0xFF4CAF50);
    break; // グリーン
  }
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

  // ── セクション境界（Attack / Body / Decay / Tail） ──
  {
    // 境界 ms 値（displayDurationMs にクランプ）
    const float bAttack = std::min(10.0f, displayDurationMs);
    const float bBody = std::min(40.0f, displayDurationMs);
    const float bDecay = std::min(140.0f, displayDurationMs);

    // 境界線の配列（表示範囲内のもののみ描画）
    const std::array<float, 3> boundaries = {bAttack, bBody, bDecay};
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    for (const float ms : boundaries) {
      if (ms < displayDurationMs) {
        const float bx = timeMsToX(ms);
        const auto ix = static_cast<int>(bx);
        g.drawVerticalLine(ix, 0.0f, h);
      }
    }

    // セクションラベル（各区間の中央に描画）
    const float dur = displayDurationMs;
    struct Section {
      float startMs;
      float endMs;
      const char *name;
    };
    const std::array<Section, 4> sections = {{
        {0.0f, std::min(10.0f, dur), "ATK"},
        {10.0f, std::min(40.0f, dur), "BODY"},
        {40.0f, std::min(140.0f, dur), "DECAY"},
        {140.0f, dur, "TAIL"},
    }};

    g.setFont(juce::Font(juce::FontOptions(8.0f)));
    g.setColour(juce::Colours::white.withAlpha(0.25f));

    for (const auto &[startMs, endMs, name] : sections) {
      if (startMs >= dur)
        break; // この区間以降は表示範囲外
      const float x0 = timeMsToX(startMs);
      const float x1 = timeMsToX(endMs);
      if (x1 - x0 > 20.0f) { // ラベルが収まる最小幅
        g.drawText(name, juce::Rectangle<float>(x0, h - 12.0f, x1 - x0, 12.0f),
                   juce::Justification::centred, false);
      }
    }
  }

  // ── ms 目盛り ──
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
  if (editTarget == pitch)
    return 20.0f;
  if (editTarget == blend)
    return -1.0f;
  return 0.0f; // amp: 0〜2, dist: 0〜1
}

float EnvelopeCurveEditor::editMaxValue() const {
  using enum EditTarget;
  if (editTarget == pitch)
    return 20000.0f;
  if (editTarget == dist)
    return 1.0f;
  if (editTarget == blend)
    return 1.0f;
  return 2.0f; // amp
}

float EnvelopeCurveEditor::valueToY(float value) const {
  using enum EditTarget;
  const float h = plotHeight();
  if (editTarget == pitch) {
    // 対数スケール: 20 Hz → 下端、20000 Hz → 上端
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float logVal = std::log(std::max(value, 20.0f));
    const float norm = (logVal - lo) / (hi - lo);
    return h - norm * h;
  }
  if (editTarget == dist) {
    // DIST: 0.0 → 下端、1.0 → 上端
    return h - value * h;
  }
  if (editTarget == blend) {
    // BLEND: -1.0 → 下端、0.0 → 中央、+1.0 → 上端
    return h - ((value + 1.0f) * 0.5f) * h;
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
  if (editTarget == dist) {
    return (h > 0.0f) ? (1.0f - y / h) : 0.0f;
  }
  if (editTarget == blend) {
    // norm 0..1 → value -1..+1
    return (h > 0.0f) ? (1.0f - y / h) * 2.0f - 1.0f : 0.0f;
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
  for (const auto t : {amp, pitch, dist, blend}) {
    if (tabRect(t).contains(pt)) {
      setEditTarget(t);
      if (onEditTargetChanged)
        onEditTargetChanged(t);
      return;
    }
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
  switch (target) {
  case amp:
    editEnvData = &ampEnvData;
    break;
  case pitch:
    editEnvData = &pitchEnvData;
    break;
  case dist:
    editEnvData = &distEnvData;
    break;
  case blend:
    editEnvData = &blendEnvData;
    break;
  }
  dragPointIndex = -1;
  repaint();
}

void EnvelopeCurveEditor::setOnEditTargetChanged(
    std::function<void(EditTarget)> cb) {
  onEditTargetChanged = std::move(cb);
}

// ── タブ描画・ヒット領域 ──

// 右から左に: BLEND=0, DIST=1, PITCH=2, AMP=3
juce::Rectangle<float> EnvelopeCurveEditor::tabRect(EditTarget target) const {
  using enum EditTarget;
  int slot = 0;
  switch (target) {
  case blend:
    slot = 0;
    break;
  case dist:
    slot = 1;
    break;
  case pitch:
    slot = 2;
    break;
  case amp:
    slot = 3;
    break;
  }
  const float x = static_cast<float>(getWidth()) - tabPad -
                  tabW * static_cast<float>(slot + 1) -
                  static_cast<float>(slot) * 2.0f;
  return {x, tabPad, tabW, tabH};
}

void EnvelopeCurveEditor::paintTabs(juce::Graphics &g) const {
  using enum EditTarget;
  const auto ampR = tabRect(amp);
  const auto pitchR = tabRect(pitch);
  const auto distR = tabRect(dist);
  const auto blendR = tabRect(blend);

  using enum EditTarget;
  g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));

  // AMP タブ
  {
    const bool active = (editTarget == amp);
    g.setColour(active ? UIConstants::Colours::subArc.withAlpha(0.8f)
                       : juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(ampR, 3.0f);
    g.setColour(active ? juce::Colours::white
                       : juce::Colours::white.withAlpha(0.5f));
    g.drawText("AMP", ampR, juce::Justification::centred, false);
  }
  // PITCH タブ
  {
    const bool active = (editTarget == pitch);
    g.setColour(active ? juce::Colours::cyan.withAlpha(0.8f)
                       : juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(pitchR, 3.0f);
    g.setColour(active ? juce::Colours::white
                       : juce::Colours::white.withAlpha(0.5f));
    g.drawText("PITCH", pitchR, juce::Justification::centred, false);
  }
  // DIST タブ
  {
    const bool active = (editTarget == dist);
    g.setColour(active ? juce::Colour(0xFFFF9500).withAlpha(0.8f)
                       : juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(distR, 3.0f);
    g.setColour(active ? juce::Colours::white
                       : juce::Colours::white.withAlpha(0.5f));
    g.drawText("DIST", distR, juce::Justification::centred, false);
  }
  // BLEND タブ
  {
    const bool active = (editTarget == blend);
    g.setColour(active ? juce::Colour(0xFF4CAF50).withAlpha(0.8f)
                       : juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(blendR, 3.0f);
    g.setColour(active ? juce::Colours::white
                       : juce::Colours::white.withAlpha(0.5f));
    g.drawText("BLEND", blendR, juce::Justification::centred, false);
  }
}
