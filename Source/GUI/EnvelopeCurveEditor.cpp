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
  paintClickWaveform(g, w, h, centreY);
  paintEnvelopeOverlay(g, w);
  paintTimeline(g, w, h, bounds.getHeight());
}

// ── paint() 分割ヘルパー ──

void EnvelopeCurveEditor::paintWaveform(juce::Graphics &g, float w, float h,
                                        float centreY) const {
  const auto numPixels = static_cast<int>(w);
  // 2点以上のみエンベロープ制御とみなす。1点はノブ制御（フラット）。
  const bool hasAmpPoints = ampEnvData.isEnvelopeControlled();
  const bool hasPitchPoints = pitchEnvData.isEnvelopeControlled();
  const bool hasBlendPoints = blendEnvData.isEnvelopeControlled();
  const bool hasDistPoints = distEnvData.isEnvelopeControlled();

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

    // Freq
    const float hz = hasPitchPoints ? pitchEnvData.evaluate(timeMs)
                                    : pitchEnvData.getValue();
    if (i > 0)
      phase += hz * (dtMs / 1000.0f) * juce::MathConstants<float>::twoPi;

    // Mix: エンベロープポイントがあれば時間軸で評価、なければノブ値
    const float blend =
        hasBlendPoints ? blendEnvData.evaluate(timeMs) : previewBlend;

    // 波形モーフィング
    const float sinVal = std::sin(phase);
    float waveVal = computePreviewWaveValue(sinVal, blend, phase);

    // Saturate: tanh ソフトクリップ（drive01=0〜1 → driveAmount=1〜10）
    // make-up gain で drive に依らずピーク振幅を一定に保つ
    if (const float drive01 = hasDistPoints ? distEnvData.evaluate(timeMs)
                                            : distEnvData.getValue();
        drive01 > 0.001f) {
      const float driveAmount = 1.0f + drive01 * 9.0f;
      waveVal = std::tanh(waveVal * driveAmount) / std::tanh(driveAmount);
    }

    // Gain: 振幅
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

void EnvelopeCurveEditor::setClickPreviewProvider(
    std::function<float(float)> fn) {
  clickPreviewFn_ = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::paintClickWaveform(juce::Graphics &g, float w,
                                              float h, float centreY) const {
  if (!clickPreviewFn_)
    return;

  const auto numPixels = static_cast<int>(w);
  const float dtSec = (displayDurationMs / 1000.0f) / w;

  juce::Path fillPath;
  juce::Path waveLine;
  fillPath.startNewSubPath(0.0f, centreY);

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeSec = x * dtSec;
    const float sample = juce::jlimit(-1.0f, 1.0f, clickPreviewFn_(timeSec));
    const float y = juce::jlimit(0.0f, h, centreY - sample * centreY);

    fillPath.lineTo(x, y);
    if (i == 0)
      waveLine.startNewSubPath(x, y);
    else
      waveLine.lineTo(x, y);
  }

  fillPath.lineTo(w, centreY);
  fillPath.closeSubPath();

  // 黄色グラジエント塗りつぶし
  const juce::Colour baseYellow{0xFFFFCC00};
  juce::ColourGradient fillGrad(baseYellow.withAlpha(0.28f), 0.0f, 0.0f,
                                baseYellow.withAlpha(0.02f), 0.0f, centreY,
                                false);
  g.setGradientFill(fillGrad);
  g.fillPath(fillPath);

  // グロー ストローク 3層（外→コア）
  g.setColour(baseYellow.withAlpha(0.07f));
  g.strokePath(waveLine, juce::PathStrokeType(9.0f));

  g.setColour(baseYellow.withAlpha(0.28f));
  g.strokePath(waveLine, juce::PathStrokeType(3.5f));

  g.setColour(baseYellow.withAlpha(0.90f));
  g.strokePath(waveLine, juce::PathStrokeType(1.2f));
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
  // +100 側: Sine → Additive（Tone1〜Tone4）
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
  case gain:
    envColour = UIConstants::Colours::subArc.brighter(0.4f);
    break;
  case freq:
    envColour = juce::Colours::cyan;
    break;
  case saturate:
    envColour = juce::Colour(0xFFFF9500);
    break; // オレンジ
  case mix:
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

  // カーブハンドル（セグメント中点に菱形◇を描画）
  for (int i = 0; i < static_cast<int>(pts.size()) - 1; ++i) {
    const auto si = static_cast<size_t>(i);
    if (std::abs(pts[si].curve) < 1e-4f)
      continue; // curve=0 のセグメントは表示しない
    // セグメント中点の時刻
    const float midMs = (pts[si].timeMs + pts[si + 1].timeMs) * 0.5f;
    const float midX = timeMsToX(midMs);
    const float midY = valueToY(editEnvData->evaluate(midMs));
    constexpr float d = 4.0f; // 菱形の半径
    juce::Path diamond;
    diamond.startNewSubPath(midX, midY - d);
    diamond.lineTo(midX + d, midY);
    diamond.lineTo(midX, midY + d);
    diamond.lineTo(midX - d, midY);
    diamond.closeSubPath();
    const bool active = (i == dragCurveSegment);
    g.setColour(active ? juce::Colours::white
                       : envColour.withAlpha(0.6f));
    g.fillPath(diamond);
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
  if (editTarget == freq)
    return 20.0f;
  if (editTarget == mix)
    return -1.0f;
  return 0.0f; // gain: 0〜2, saturate: 0〜1
}

float EnvelopeCurveEditor::editMaxValue() const {
  using enum EditTarget;
  if (editTarget == freq)
    return 20000.0f;
  if (editTarget == saturate)
    return 1.0f;
  if (editTarget == mix)
    return 1.0f;
  return 2.0f; // gain
}

float EnvelopeCurveEditor::valueToY(float value) const {
  using enum EditTarget;
  const float h = plotHeight();
  if (editTarget == freq) {
    // 対数スケール: 20 Hz → 下端、20000 Hz → 上端
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float logVal = std::log(std::max(value, 20.0f));
    const float norm = (logVal - lo) / (hi - lo);
    return h - norm * h;
  }
  if (editTarget == saturate) {
    // Saturate: 0.0 → 下端、1.0 → 上端
    return h - value * h;
  }
  if (editTarget == mix) {
    // Mix: -1.0 → 下端、0.0 → 中央、+1.0 → 上端
    return h - ((value + 1.0f) * 0.5f) * h;
  }
  // Gain: value 0.0 → 下端、value 2.0 → 上端
  return h - (value / 2.0f) * h;
}

float EnvelopeCurveEditor::xToTimeMs(float x) const {
  const auto w = static_cast<float>(getWidth());
  return (w > 0.0f) ? (x / w) * displayDurationMs : 0.0f;
}

float EnvelopeCurveEditor::yToValue(float y) const {
  using enum EditTarget;
  const float h = plotHeight();
  if (editTarget == freq) {
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float norm = (h > 0.0f) ? (1.0f - y / h) : 0.5f;
    return std::exp(std::lerp(lo, hi, norm));
  }
  if (editTarget == saturate) {
    return (h > 0.0f) ? (1.0f - y / h) : 0.0f;
  }
  if (editTarget == mix) {
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

int EnvelopeCurveEditor::findSegmentAtPixel(float px, float py) const {
  const auto &pts = editEnvData->getPoints();
  const auto n = static_cast<int>(pts.size());
  if (n < 2)
    return -1;

  constexpr float maxDist = 12.0f; // ヒット判定の最大距離(px)
  int bestSeg = -1;
  float bestDist = maxDist;

  for (int i = 0; i < n - 1; ++i) {
    const auto si = static_cast<size_t>(i);
    const float x0 = timeMsToX(pts[si].timeMs);
    if (const float x1 = timeMsToX(pts[si + 1].timeMs); px < x0 || px > x1)
      continue;
    // セグメント上の Y を evaluate で求める
    const float timeMs = xToTimeMs(px);
    const float ey = valueToY(editEnvData->evaluate(timeMs));
    const float dist = std::abs(py - ey);
    if (dist < bestDist) {
      bestDist = dist;
      bestSeg = i;
    }
  }
  return bestSeg;
}

// ── マウス操作 ──

void EnvelopeCurveEditor::mouseDoubleClick(const juce::MouseEvent &e) {
  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);

  // Shift+ダブルクリック: セグメントのカーブをリセット
  if (e.mods.isShiftDown()) {
    if (const int seg = findSegmentAtPixel(px, py); seg >= 0) {
      editEnvData->setSegmentCurve(seg, 0.0f);
      if (onChange)
        onChange();
      repaint();
    }
    return;
  }

  if (const int hit = findPointAtPixel(px, py); hit >= 0) {
    // 最低 1 点は常に維持する
    if (editEnvData->getPoints().size() > 1)
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
  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);

  if (e.mods.isShiftDown()) {
    // Shift+クリック: セグメントのカーブ編集
    dragCurveSegment = findSegmentAtPixel(px, py);
    if (dragCurveSegment >= 0) {
      dragCurveStartY = py;
      const auto &pts = editEnvData->getPoints();
      dragCurveStartVal = pts[static_cast<size_t>(dragCurveSegment)].curve;
    }
    dragPointIndex = -1;
  } else {
    dragCurveSegment = -1;
    dragPointIndex = findPointAtPixel(px, py);
  }
}

void EnvelopeCurveEditor::mouseDrag(const juce::MouseEvent &e) {
  if (dragCurveSegment >= 0) {
    // Shift+ドラッグ: 上方向でカーブ増、下方向でカーブ減
    const float dy = static_cast<float>(e.y) - dragCurveStartY;
    constexpr float sensitivity = 200.0f; // px あたりの curve 変化量
    const float newCurve = std::clamp(
        dragCurveStartVal + dy / sensitivity, -1.0f, 1.0f);
    editEnvData->setSegmentCurve(dragCurveSegment, newCurve);

    if (onChange)
      onChange();
    repaint();
    return;
  }

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
  dragCurveSegment = -1;
}

void EnvelopeCurveEditor::setEditTarget(EditTarget target) {
  using enum EditTarget;
  editTarget = target;
  switch (target) {
  case gain:
    editEnvData = &ampEnvData;
    break;
  case freq:
    editEnvData = &pitchEnvData;
    break;
  case saturate:
    editEnvData = &distEnvData;
    break;
  case mix:
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
