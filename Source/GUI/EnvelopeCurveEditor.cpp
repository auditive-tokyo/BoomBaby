#include "EnvelopeCurveEditor.h"
#include "../DSP/EnvelopeData.h"
#include "UIConstants.h"

#include <array>
#include <cmath>

namespace {

// 位相から波形サンプル値を返す（プレビュー用算術式）
float shapeOscValue(WaveShape shape, float phase) {
  const float twoPi = juce::MathConstants<float>::twoPi;
  float p = std::fmod(phase, twoPi);
  if (p < 0.0f)
    p += twoPi;

  switch (shape) {
    using enum WaveShape;
  case Tri:
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

// 現在の編集対象に応じた値の {min, max}
std::pair<float, float> editValueRange(EnvelopeCurveEditor::EditTarget target) {
  using enum EnvelopeCurveEditor::EditTarget;
  if (target == freq)
    return {20.0f, 20000.0f};
  if (target == saturate)
    return {0.0f, 1.0f};
  if (target == mix)
    return {-1.0f, 1.0f};
  return {0.0f, 2.0f}; // amp / clickAmp
}

} // namespace

EnvelopeCurveEditor::EnvelopeCurveEditor(EnvelopeData &ampData,
                                         EnvelopeData &freqData,
                                         EnvelopeData &distData,
                                         EnvelopeData &mixData,
                                         EnvelopeData &clickAmpData)
    : envDatas_{&ampData, &freqData, &distData, &mixData, &clickAmpData},
      editEnvData(&ampData) {
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

  g.beginTransparencyLayer(UIConstants::subWaveOpacity);
  paintWaveform(g, w, h, centreY);
  g.endTransparencyLayer();
  g.beginTransparencyLayer(UIConstants::clickWaveOpacity);
  if (clickNoiseEnvFn_)
    paintClickNoiseBand(g, w, h, centreY);
  else
    paintClickSampleWave(g, w, h, centreY);
  g.endTransparencyLayer();
  g.beginTransparencyLayer(UIConstants::directWaveOpacity);
  paintDirectWaveform(g, w, h, centreY);
  g.endTransparencyLayer();
  paintEnvelopeOverlay(g, w);
  paintTimeline(g, w, h, bounds.getHeight());
}

// ── paint() 分割ヘルパー ──

void EnvelopeCurveEditor::paintWaveform(juce::Graphics &g, float w, float h,
                                        float centreY) const {
  const auto numPixels = static_cast<int>(w);
  // 2点以上のみエンベロープ制御とみなす。1点はノブ制御（フラット）。
  const auto &ampEnv = *envDatas_[0];
  const auto &freqEnv = *envDatas_[1];
  const auto &distEnv = *envDatas_[2];
  const auto &mixEnv = *envDatas_[3];
  const bool hasAmpPoints = ampEnv.isEnvelopeControlled();
  const bool hasFreqPoints = freqEnv.isEnvelopeControlled();
  const bool hasMixPoints = mixEnv.isEnvelopeControlled();
  const bool hasDistPoints = distEnv.isEnvelopeControlled();

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
    const float hz =
        hasFreqPoints ? freqEnv.evaluate(timeMs) : freqEnv.getValue();
    if (i > 0)
      phase += hz * (dtMs / 1000.0f) * juce::MathConstants<float>::twoPi;

    // Mix: エンベロープポイントがあれば時間軸で評価、なければノブ値
    const float mix = hasMixPoints ? mixEnv.evaluate(timeMs) : previewMix;

    // 波形モーフィング
    const float sinVal = std::sin(phase);
    float waveVal = computePreviewWaveValue(sinVal, mix, phase);

    // Saturate: tanh ソフトクリップ（drive01=0〜1 → driveAmount=1〜10）
    // make-up gain で drive に依らずピーク振幅を一定に保つ
    if (const float drive01 =
            hasDistPoints ? distEnv.evaluate(timeMs) : distEnv.getValue();
        drive01 > 0.001f) {
      const float driveAmount = 1.0f + drive01 * 9.0f;
      waveVal = std::tanh(waveVal * driveAmount) / std::tanh(driveAmount);
    }

    // Gain: 振幅
    const float amplitude =
        hasAmpPoints ? ampEnv.evaluate(timeMs) : ampEnv.getValue();

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

  // グラジエント塗りつぶし: 波形外縁→中心ラインへ subArc カラー→透明
  const juce::Colour baseBlue = UIConstants::Colours::subArc;
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

void EnvelopeCurveEditor::setDirectProvider(
    std::function<std::pair<float, float>(float)> fn) {
  directPreviewFn_ = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::setClickPreviewProvider(
    std::function<std::pair<float, float>(float)> fn) {
  clickNoiseEnvFn_ = nullptr;
  clickPreviewFn_ = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::setClickDecayMs(float ms) {
  clickDecayMs_ = ms;
  repaint();
}

void EnvelopeCurveEditor::setClickNoiseEnvProvider(
    std::function<float(float)> fn) {
  clickPreviewFn_ = nullptr;
  clickNoiseEnvFn_ = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::paintDirectWaveform(juce::Graphics &g, float w,
                                              float h, float centreY) const {
  if (!directPreviewFn_)
    return;

  const juce::Colour baseColour = UIConstants::Colours::directArc;
  const auto numPixels = static_cast<int>(w);
  const float dtSec = (displayDurationMs / 1000.0f) / w;

  // 波形帯（ピクセルiの最小値→最大値を封た形）
  juce::Path fillPath;
  juce::Path topLine;
  juce::Path botLine;

  // 上側エッジ (左→右, maxVal)
  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const auto [mn, mx] = directPreviewFn_(x * dtSec);
    const float yTop = juce::jlimit(0.0f, h, centreY - mx * centreY);
    if (i == 0) {
      fillPath.startNewSubPath(x, yTop);
      topLine.startNewSubPath(x, yTop);
    } else {
      fillPath.lineTo(x, yTop);
      topLine.lineTo(x, yTop);
    }
  }
  // 下側エッジ (右→左, minVal)で封じる
  for (int i = numPixels; i >= 0; --i) {
    const auto x = static_cast<float>(i);
    const auto [mn, mx] = directPreviewFn_(x * dtSec);
    const float yBot = juce::jlimit(0.0f, h, centreY - mn * centreY);
    fillPath.lineTo(x, yBot);
    if (i == numPixels)
      botLine.startNewSubPath(x, yBot);
    else
      botLine.lineTo(x, yBot);
  }
  fillPath.closeSubPath();

  // グラデーション塗りつぶし（上から下へ）
  juce::ColourGradient fillGrad(baseColour.withAlpha(0.25f), 0.0f, 0.0f,
                                baseColour.withAlpha(0.03f), 0.0f, h, false);
  g.setGradientFill(fillGrad);
  g.fillPath(fillPath);

  for (const auto *line : {&topLine, &botLine}) {
    g.setColour(baseColour.withAlpha(0.07f));
    g.strokePath(*line, juce::PathStrokeType(6.0f));
    g.setColour(baseColour.withAlpha(0.30f));
    g.strokePath(*line, juce::PathStrokeType(3.5f));
    g.setColour(baseColour.withAlpha(0.90f));
    g.strokePath(*line, juce::PathStrokeType(1.2f));
  }
}

void EnvelopeCurveEditor::paintClickNoiseBand(juce::Graphics &g, float w,
                                              float h, float centreY) const {
  const juce::Colour baseYellow = UIConstants::Colours::clickArc;
  const auto numPixels = static_cast<int>(w);
  const float dtSec = (displayDurationMs / 1000.0f) / w;

  juce::Path bandPath;
  juce::Path upperLine;
  juce::Path lowerLine;

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float env = juce::jlimit(0.0f, 1.0f, clickNoiseEnvFn_(x * dtSec));
    const float y = juce::jlimit(0.0f, h, centreY - env * centreY);
    if (i == 0) {
      bandPath.startNewSubPath(x, y);
      upperLine.startNewSubPath(x, y);
    } else {
      bandPath.lineTo(x, y);
      upperLine.lineTo(x, y);
    }
  }
  for (int i = numPixels; i >= 0; --i) {
    const auto x = static_cast<float>(i);
    const float env = juce::jlimit(0.0f, 1.0f, clickNoiseEnvFn_(x * dtSec));
    const float y = juce::jlimit(0.0f, h, centreY + env * centreY);
    bandPath.lineTo(x, y);
    if (i == numPixels)
      lowerLine.startNewSubPath(x, y);
    else
      lowerLine.lineTo(x, y);
  }
  bandPath.closeSubPath();

  juce::ColourGradient fillGrad(baseYellow.withAlpha(0.18f), 0.0f, 0.0f,
                                baseYellow.withAlpha(0.02f), 0.0f, centreY,
                                false);
  g.setGradientFill(fillGrad);
  g.fillPath(bandPath);

  for (const auto *line : {&upperLine, &lowerLine}) {
    g.setColour(baseYellow.withAlpha(0.07f));
    g.strokePath(*line, juce::PathStrokeType(6.0f));
    g.setColour(baseYellow.withAlpha(0.90f));
    g.strokePath(*line, juce::PathStrokeType(1.2f));
  }
}

void EnvelopeCurveEditor::paintClickSampleWave(juce::Graphics &g, float w,
                                               float h, float centreY) const {
  if (!clickPreviewFn_)
    return;

  const juce::Colour baseYellow = UIConstants::Colours::clickArc;
  const auto numPixels = static_cast<int>(w);
  const float dtSec = (displayDurationMs / 1000.0f) / w;
  const auto &clickAmpEnv = *envDatas_[4];
  const bool hasClickAmpEnv = clickAmpEnv.isEnvelopeControlled();

  constexpr float clickFadeOutMs = 5.0f;
  const float clickFadeStartMs = std::max(0.0f, clickDecayMs_ - clickFadeOutMs);

  auto getAmpMul = [&](float timeMs) {
    float ampMul = hasClickAmpEnv ? clickAmpEnv.evaluate(timeMs)
                                  : clickAmpEnv.getDefaultValue();
    if (timeMs >= clickDecayMs_) {
      ampMul = 0.0f;
    } else if (timeMs > clickFadeStartMs && clickFadeOutMs > 0.0f) {
      const float t = (timeMs - clickFadeStartMs) / clickFadeOutMs;
      ampMul *= 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
    }
    return ampMul;
  };

  // Direct と同じバンド描画: 上側エッジ (左→右, max)
  juce::Path fillPath;
  juce::Path topLine;
  juce::Path botLine;

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / w) * displayDurationMs;
    const float ampMul = getAmpMul(timeMs);
    const auto [mn, mx] = clickPreviewFn_(x * dtSec);
    const float yTop = juce::jlimit(0.0f, h, centreY - mx * ampMul * centreY);
    if (i == 0) {
      fillPath.startNewSubPath(x, yTop);
      topLine.startNewSubPath(x, yTop);
    } else {
      fillPath.lineTo(x, yTop);
      topLine.lineTo(x, yTop);
    }
  }
  // 下側エッジ (右→左, min) で封じる
  for (int i = numPixels; i >= 0; --i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / w) * displayDurationMs;
    const float ampMul = getAmpMul(timeMs);
    const auto [mn, mx] = clickPreviewFn_(x * dtSec);
    const float yBot = juce::jlimit(0.0f, h, centreY - mn * ampMul * centreY);
    fillPath.lineTo(x, yBot);
    if (i == numPixels)
      botLine.startNewSubPath(x, yBot);
    else
      botLine.lineTo(x, yBot);
  }
  fillPath.closeSubPath();

  juce::ColourGradient fillGrad(baseYellow.withAlpha(0.28f), 0.0f, 0.0f,
                                baseYellow.withAlpha(0.02f), 0.0f, centreY,
                                false);
  g.setGradientFill(fillGrad);
  g.fillPath(fillPath);

  for (const auto *line : {&topLine, &botLine}) {
    g.setColour(baseYellow.withAlpha(0.07f));
    g.strokePath(*line, juce::PathStrokeType(6.0f));
    g.setColour(baseYellow.withAlpha(0.28f));
    g.strokePath(*line, juce::PathStrokeType(3.5f));
    g.setColour(baseYellow.withAlpha(0.90f));
    g.strokePath(*line, juce::PathStrokeType(1.2f));
  }
}

void EnvelopeCurveEditor::setWaveShape(WaveShape shape) {
  if (previewShape != shape) {
    previewShape = shape;
    repaint();
  }
}

void EnvelopeCurveEditor::setPreviewMix(float mix) {
  const float clamped = juce::jlimit(-1.0f, 1.0f, mix);
  if (std::abs(previewMix - clamped) > 1e-6f) {
    previewMix = clamped;
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

float EnvelopeCurveEditor::computePreviewWaveValue(float sinVal, float mix,
                                                   float phase) const {
  if (mix <= 0.0f) {
    // -100 側: Sine → WaveShape
    if (previewShape == WaveShape::Sine)
      return sinVal;
    return std::lerp(sinVal, shapeOscValue(previewShape, phase), -mix);
  }
  // +100 側: Sine → Additive（Tone1〜Tone4）
  float addVal = 0.0f;
  for (size_t n = 0; n < 4; ++n) {
    if (previewHarmonicGains[n] > 0.0f)
      addVal +=
          previewHarmonicGains[n] * std::sin(phase * static_cast<float>(n + 1));
  }
  return std::lerp(sinVal, addVal, mix);
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
  case freq:
    envColour = juce::Colours::cyan;
    break;
  case saturate:
    envColour = juce::Colour(0xFFFF9500);
    break; // オレンジ
  case mix:
    envColour = juce::Colour(0xFF4CAF50);
    break; // グリーン
  case clickAmp:
    envColour = UIConstants::Colours::clickArc;
    break;
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

    if (i == drag_.pointIndex) {
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
    const bool active = (i == drag_.curveSegment);
    g.setColour(active ? juce::Colours::white : envColour.withAlpha(0.6f));
    g.fillPath(diamond);
  }
}

void EnvelopeCurveEditor::paintTimeline(juce::Graphics &g, float w, float h,
                                        float totalH) const {
  g.setColour(juce::Colours::white.withAlpha(0.15f));
  g.drawHorizontalLine(static_cast<int>(h), 0.0f, w);

  // ── セクション背景色（Attack / Body / Decay / Tail） ──
  {
    const float dur = displayDurationMs;
    const float labelRowY = h + 16.0f; // ms行(~14px) の下
    const float labelRowH = totalH - labelRowY;

    struct Section {
      float startMs;
      float endMs;
      const char *name;
      juce::Colour colour;
    };
    const std::array<Section, 4> sections = {{
        {0.0f, std::min(10.0f, dur), "ATK", juce::Colour(0xFFDD4444)},
        {10.0f, std::min(40.0f, dur), "BODY", juce::Colour(0xFF44BB66)},
        {40.0f, std::min(140.0f, dur), "DECAY", juce::Colour(0xFF4488DD)},
        {140.0f, dur, "TAIL", juce::Colour(0xFFAA55DD)},
    }};

    g.setFont(juce::Font(juce::FontOptions(8.0f)));

    for (const auto &[startMs, endMs, name, colour] : sections) {
      if (startMs >= dur)
        break;

      const float x0 = timeMsToX(startMs);
      const float x1 = timeMsToX(endMs);
      const float sectionW = x1 - x0;

      // 波形エリア全体を薄い色で塗りつぶす
      g.setColour(colour.withAlpha(0.08f));
      g.fillRect(juce::Rectangle<float>(x0, 0.0f, sectionW, totalH));

      // セクション間の境界線（細く明確に）
      if (startMs > 0.0f && startMs < dur) {
        g.setColour(colour.withAlpha(0.35f));
        g.drawVerticalLine(static_cast<int>(x0), 0.0f, totalH);
      }

      // セクションラベル
      if (sectionW > 20.0f) {
        g.setColour(colour.brighter(0.3f));
        g.drawText(name,
                   juce::Rectangle<float>(x0, labelRowY, sectionW, labelRowH),
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

    g.setColour(juce::Colours::white);
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
  // Amp: value 0.0 → 下端、value 2.0 → 上端
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
    const auto [lo, hi] = editValueRange(editTarget);
    const float value = std::clamp(yToValue(py), lo, hi);
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
    drag_.curveSegment = findSegmentAtPixel(px, py);
    if (drag_.curveSegment >= 0) {
      drag_.startY = py;
      const auto &pts = editEnvData->getPoints();
      drag_.startVal = pts[static_cast<size_t>(drag_.curveSegment)].curve;
    }
    drag_.pointIndex = -1;
  } else {
    drag_.curveSegment = -1;
    drag_.pointIndex = findPointAtPixel(px, py);
  }
}

void EnvelopeCurveEditor::mouseDrag(const juce::MouseEvent &e) {
  if (drag_.curveSegment >= 0) {
    // Shift+ドラッグ: 上方向でカーブ増、下方向でカーブ減
    const float dy = static_cast<float>(e.y) - drag_.startY;
    constexpr float sensitivity = 200.0f; // px あたりの curve 変化量
    const float newCurve =
        std::clamp(drag_.startVal + dy / sensitivity, -1.0f, 1.0f);
    editEnvData->setSegmentCurve(drag_.curveSegment, newCurve);

    if (onChange)
      onChange();
    repaint();
    return;
  }

  if (drag_.pointIndex < 0)
    return;

  const float timeMs =
      std::clamp(xToTimeMs(static_cast<float>(e.x)), 0.0f, displayDurationMs);
  const auto [lo, hi] = editValueRange(editTarget);
  const float value = std::clamp(yToValue(static_cast<float>(e.y)), lo, hi);
  drag_.pointIndex = editEnvData->movePoint(drag_.pointIndex, timeMs, value);

  if (onChange)
    onChange();
  repaint();
}

void EnvelopeCurveEditor::mouseUp(const juce::MouseEvent & /*e*/) {
  drag_.pointIndex = -1;
  drag_.curveSegment = -1;
}

void EnvelopeCurveEditor::setEditTarget(EditTarget target) {
  editTarget = target;
  editEnvData = envDatas_[static_cast<size_t>(target)];
  drag_.pointIndex = -1;
  repaint();
}

void EnvelopeCurveEditor::setOnEditTargetChanged(
    std::function<void(EditTarget)> cb) {
  onEditTargetChanged = std::move(cb);
}
