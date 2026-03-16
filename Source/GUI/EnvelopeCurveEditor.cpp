#include "EnvelopeCurveEditor.h"
#include "../DSP/EnvelopeData.h"
#include "CustomSliderLAF.h"
#include "UIConstants.h"

#include <array>
#include <cmath>
#include <utility>

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

// ── PointValueEditor
// 定義（unique_ptrの完全型要件を満たすためコンストラクタ前に配置）──

struct EnvelopeCurveEditor::PointValueEditor : public juce::Component {
  enum class Mode { Value, Time };

  juce::TextEditor editor;
  EnvelopeCurveEditor *owner;
  int pointIndex;
  Mode mode;

  PointValueEditor(EnvelopeCurveEditor *e, int idx, juce::Rectangle<int> bounds,
                   const juce::String &currentText, Mode m)
      : owner(e), pointIndex(idx), mode(m) {
    editor.setFont(juce::Font{juce::FontOptions{11.0f}});
    editor.setJustification(juce::Justification::centred);
    editor.setColour(juce::TextEditor::backgroundColourId,
                     juce::Colour(0xFF1A1A1A));
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::outlineColourId,
                     juce::Colours::white.withAlpha(0.4f));
    editor.setColour(juce::TextEditor::focusedOutlineColourId,
                     juce::Colours::white);
    editor.setText(currentText, false);
    editor.selectAll();
    editor.onReturnKey = [this] { commit(); };
    editor.onEscapeKey = [this] { dismiss(); };
    addAndMakeVisible(editor);
    setBounds(bounds);
    e->addAndMakeVisible(this);
    editor.grabKeyboardFocus();
  }

  void resized() override { editor.setBounds(getLocalBounds()); }

  void focusOfChildComponentChanged(FocusChangeType) override {
    if (!hasKeyboardFocus(true))
      commit();
  }

  void commit() {
    if (const auto &pts = owner->editEnvData->getPoints();
        pointIndex < static_cast<int>(pts.size())) {
      if (mode == Mode::Time) {
        // 時間編集: ms 直値、[0, displayDurationMs] にクランプ
        const float newMs =
            juce::jlimit(0.0f, owner->displayDurationMs,
                         editor.getText().trim().getFloatValue());
        const float curVal = pts[static_cast<size_t>(pointIndex)].value;
        owner->editEnvData->movePoint(pointIndex, newMs, curVal);
      } else {
        // 値編集
        if (const auto val = owner->parseDisplayStringToValue(editor.getText());
            val.has_value()) {
          owner->editEnvData->setPointValue(pointIndex, *val);
        } else {
          dismiss();
          return;
        }
      }
      if (owner->onChange)
        owner->onChange();
      owner->repaint();
    }
    dismiss();
  }

  void dismiss() {
    juce::Component::SafePointer<PointValueEditor> self(this);
    juce::MessageManager::callAsync([self] {
      if (self)
        self->owner->pointValueEditor_.reset();
    });
  }
};

// デストラクタ: PointValueEditor 完全型の後に定義
EnvelopeCurveEditor::~EnvelopeCurveEditor() = default;

EnvelopeCurveEditor::EnvelopeCurveEditor(EnvelopeData &ampData,
                                         EnvelopeData &freqData,
                                         EnvelopeData &distData,
                                         EnvelopeData &mixData,
                                         EnvelopeData &clickAmpData,
                                         EnvelopeData &directAmpData)
    : envDatas_{&ampData, &freqData,     &distData,
                &mixData, &clickAmpData, &directAmpData},
      editEnvData(&ampData) {
  setOpaque(true);
}

void EnvelopeCurveEditor::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const float totalH = bounds.getHeight();
  const auto c = makeCoords();
  const float centreY = c.plotH * 0.5f;

  g.fillAll(UIConstants::Colours::waveformBg);

  if (c.w < 1.0f || c.plotH < 1.0f)
    return;

  const bool anySolo = channelVis_.subSoloed || channelVis_.clickSoloed ||
                       channelVis_.directSoloed;
  if (!channelVis_.subMuted && (!anySolo || channelVis_.subSoloed)) {
    g.beginTransparencyLayer(UIConstants::subWaveOpacity);
    PaintHelper::waveform(*this, g, c, centreY);
    g.endTransparencyLayer();
  }
  if (!channelVis_.clickMuted && (!anySolo || channelVis_.clickSoloed)) {
    g.beginTransparencyLayer(UIConstants::clickWaveOpacity);
    if (clickPreview_.noiseEnvFn)
      PaintHelper::clickNoiseBand(*this, g, c, centreY);
    else
      PaintHelper::clickSampleWave(*this, g, c, centreY);
    g.endTransparencyLayer();
  }
  if (!channelVis_.directMuted && (!anySolo || channelVis_.directSoloed)) {
    g.beginTransparencyLayer(UIConstants::directWaveOpacity);
    PaintHelper::directWaveform(*this, g, c, centreY);
    g.endTransparencyLayer();
  }
  PaintHelper::envelopeOverlay(*this, g, c);
  PaintHelper::pointTooltip(*this, g, c);
  PaintHelper::timeline(g, c, totalH);
}

// ── PaintHelper メソッド ──

void EnvelopeCurveEditor::PaintHelper::waveform(const EnvelopeCurveEditor &e,
                                                juce::Graphics &g,
                                                const CoordMapper &c,
                                                float centreY) {
  const auto numPixels = static_cast<int>(c.w);
  const auto &ampEnv = *e.envDatas_[0];
  const auto &freqEnv = *e.envDatas_[1];
  const auto &distEnv = *e.envDatas_[2];
  const auto &mixEnv = *e.envDatas_[3];
  const bool hasAmpPoints = ampEnv.isEnvelopeControlled();
  const bool hasFreqPoints = freqEnv.isEnvelopeControlled();
  const bool hasMixPoints = mixEnv.isEnvelopeControlled();
  const bool hasDistPoints = distEnv.isEnvelopeControlled();

  juce::Path fillPath;
  juce::Path waveLine;
  fillPath.startNewSubPath(0.0f, centreY);

  float phase = 0.0f;
  const float dtMs = c.durationMs / c.w;

  constexpr float fadeOutMs = 5.0f;
  const float fadeStartMs = std::max(0.0f, c.durationMs - fadeOutMs);

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = x * dtMs;

    const float hz =
        hasFreqPoints ? freqEnv.evaluate(timeMs) : freqEnv.getValue();
    if (i > 0)
      phase += hz * (dtMs / 1000.0f) * juce::MathConstants<float>::twoPi;

    const float mix =
        hasMixPoints ? mixEnv.evaluate(timeMs) : e.wavePreview_.mix;

    const float sinVal = std::sin(phase);
    float waveVal = PaintHelper::previewWaveValue(e, sinVal, mix, phase);

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
    const float y = juce::jlimit(0.0f, c.plotH, centreY - waveVal * scaledAmp);

    fillPath.lineTo(x, y);

    if (i == 0)
      waveLine.startNewSubPath(x, y);
    else
      waveLine.lineTo(x, y);
  }

  fillPath.lineTo(c.w, centreY);
  fillPath.closeSubPath();

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
  g.drawHorizontalLine(static_cast<int>(centreY), 0.0f, c.w);
}

void EnvelopeCurveEditor::setDirectProvider(
    std::function<std::pair<float, float>(float)> fn) {
  directPreview_.fn = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::setRealtimePixels(
    std::vector<std::pair<float, float>> pixels) {
  directPreview_.realtimePixels = std::move(pixels);
  // repaint は呼び出し元 (Timer) が責任を持つ
}

void EnvelopeCurveEditor::setUseRealtimeInput(bool use) noexcept {
  directPreview_.useRealtime = use;
}

void EnvelopeCurveEditor::setChannelMuted(Channel ch, bool muted) {
  using enum Channel;
  switch (ch) {
  case sub:
    channelVis_.subMuted = muted;
    break;
  case click:
    channelVis_.clickMuted = muted;
    break;
  case direct:
    channelVis_.directMuted = muted;
    break;
  }
  repaint();
}

void EnvelopeCurveEditor::setChannelSoloed(Channel ch, bool soloed) {
  using enum Channel;
  switch (ch) {
  case sub:
    channelVis_.subSoloed = soloed;
    break;
  case click:
    channelVis_.clickSoloed = soloed;
    break;
  case direct:
    channelVis_.directSoloed = soloed;
    break;
  }
  repaint();
}

void EnvelopeCurveEditor::setClickPreviewProvider(
    std::function<std::pair<float, float>(float)> fn) {
  clickPreview_.noiseEnvFn = nullptr;
  clickPreview_.sampleFn = std::move(fn);
  repaint();
}

void EnvelopeCurveEditor::setClickDecayMs(float ms) {
  clickPreview_.decayMs = ms;
  repaint();
}

void EnvelopeCurveEditor::setClickNoiseEnvProvider(
    std::function<float(float)> fn) {
  clickPreview_.sampleFn = nullptr;
  clickPreview_.noiseEnvFn = std::move(fn);
  repaint();
}

template <typename GetSample, typename GetAmpMul>
void EnvelopeCurveEditor::PaintHelper::buildStereoWavePaths(
    juce::Path &fillPath, juce::Path &topLine, juce::Path &botLine,
    const CoordMapper &c, float centreY, GetSample getSample,
    GetAmpMul getAmpMul) {
  const auto numPixels = static_cast<int>(c.w);
  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / c.w) * c.durationMs;
    const float ampMul = getAmpMul(timeMs);
    const auto [mn, mx] = getSample(i);
    const float yTop =
        juce::jlimit(0.0f, c.plotH, centreY - mx * ampMul * centreY);
    if (i == 0) {
      fillPath.startNewSubPath(x, yTop);
      topLine.startNewSubPath(x, yTop);
    } else {
      fillPath.lineTo(x, yTop);
      topLine.lineTo(x, yTop);
    }
  }
  for (int i = numPixels; i >= 0; --i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / c.w) * c.durationMs;
    const float ampMul = getAmpMul(timeMs);
    const auto [mn, mx] = getSample(i);
    const float yBot =
        juce::jlimit(0.0f, c.plotH, centreY - mn * ampMul * centreY);
    fillPath.lineTo(x, yBot);
    if (i == numPixels)
      botLine.startNewSubPath(x, yBot);
    else
      botLine.lineTo(x, yBot);
  }
}

void EnvelopeCurveEditor::PaintHelper::directWaveform(
    const EnvelopeCurveEditor &e, juce::Graphics &g, const CoordMapper &c,
    float centreY) {
  const bool hasRealtime =
      e.directPreview_.useRealtime && !e.directPreview_.realtimePixels.empty();
  if (!hasRealtime && !e.directPreview_.fn)
    return;

  const juce::Colour baseColour = UIConstants::Colours::directArc;
  const float dtSec = (c.durationMs / 1000.0f) / c.w;

  const auto &directAmpEnv = *e.envDatas_[5];
  const bool hasDirectAmpEnv = directAmpEnv.isEnvelopeControlled();

  auto getSample = [hasRealtime, &e, dtSec](int i) -> std::pair<float, float> {
    if (hasRealtime) {
      if (i < static_cast<int>(e.directPreview_.realtimePixels.size()))
        return e.directPreview_.realtimePixels[static_cast<std::size_t>(i)];
      return {0.0f, 0.0f};
    }
    return e.directPreview_.fn(static_cast<float>(i) * dtSec);
  };

  auto getAmpMul = [hasDirectAmpEnv, &directAmpEnv](float timeMs) {
    return hasDirectAmpEnv ? directAmpEnv.evaluate(timeMs)
                           : directAmpEnv.getDefaultValue();
  };

  juce::Path fillPath;
  juce::Path topLine;
  juce::Path botLine;

  buildStereoWavePaths(fillPath, topLine, botLine, c, centreY, getSample,
                       getAmpMul);
  fillPath.closeSubPath();

  juce::ColourGradient fillGrad(baseColour.withAlpha(0.25f), 0.0f, 0.0f,
                                baseColour.withAlpha(0.03f), 0.0f, c.plotH,
                                false);
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

void EnvelopeCurveEditor::PaintHelper::clickNoiseBand(
    const EnvelopeCurveEditor &e, juce::Graphics &g, const CoordMapper &c,
    float centreY) {
  const juce::Colour baseYellow = UIConstants::Colours::clickArc;
  const auto numPixels = static_cast<int>(c.w);
  const float dtSec = (c.durationMs / 1000.0f) / c.w;

  juce::Path bandPath;
  juce::Path upperLine;
  juce::Path lowerLine;

  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float env =
        juce::jlimit(0.0f, 1.0f, e.clickPreview_.noiseEnvFn(x * dtSec));
    const float y = juce::jlimit(0.0f, c.plotH, centreY - env * centreY);
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
    const float env =
        juce::jlimit(0.0f, 1.0f, e.clickPreview_.noiseEnvFn(x * dtSec));
    const float y = juce::jlimit(0.0f, c.plotH, centreY + env * centreY);
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

void EnvelopeCurveEditor::PaintHelper::clickSampleWave(
    const EnvelopeCurveEditor &e, juce::Graphics &g, const CoordMapper &c,
    float centreY) {
  if (!e.clickPreview_.sampleFn)
    return;

  const juce::Colour baseYellow = UIConstants::Colours::clickArc;
  const float dtSec = (c.durationMs / 1000.0f) / c.w;
  const auto &clickAmpEnv = *e.envDatas_[4];
  const bool hasClickAmpEnv = clickAmpEnv.isEnvelopeControlled();

  constexpr float clickFadeOutMs = 5.0f;
  const float clickFadeStartMs =
      std::max(0.0f, e.clickPreview_.decayMs - clickFadeOutMs);

  auto getAmpMul = [hasClickAmpEnv, &clickAmpEnv, &e,
                    clickFadeStartMs](float timeMs) {
    float ampMul = hasClickAmpEnv ? clickAmpEnv.evaluate(timeMs)
                                  : clickAmpEnv.getDefaultValue();
    if (timeMs >= e.clickPreview_.decayMs) {
      ampMul = 0.0f;
    } else if (timeMs > clickFadeStartMs && clickFadeOutMs > 0.0f) {
      const float t = (timeMs - clickFadeStartMs) / clickFadeOutMs;
      ampMul *= 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
    }
    return ampMul;
  };

  juce::Path fillPath;
  juce::Path topLine;
  juce::Path botLine;

  auto getSample = [&e, dtSec](int i) {
    return e.clickPreview_.sampleFn(static_cast<float>(i) * dtSec);
  };
  buildStereoWavePaths(fillPath, topLine, botLine, c, centreY, getSample,
                       getAmpMul);
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
  if (wavePreview_.shape != shape) {
    wavePreview_.shape = shape;
    repaint();
  }
}

void EnvelopeCurveEditor::setPreviewMix(float mix) {
  const float clamped = juce::jlimit(-1.0f, 1.0f, mix);
  if (std::abs(wavePreview_.mix - clamped) > 1e-6f) {
    wavePreview_.mix = clamped;
    repaint();
  }
}

void EnvelopeCurveEditor::setPreviewHarmonicGain(int harmonicNum, float gain) {
  if (harmonicNum >= 1 && harmonicNum <= 4) {
    const auto idx = static_cast<size_t>(harmonicNum - 1);
    if (std::abs(wavePreview_.harmonicGains[idx] - gain) > 1e-6f) {
      wavePreview_.harmonicGains[idx] = gain;
      repaint();
    }
  }
}

float EnvelopeCurveEditor::PaintHelper::previewWaveValue(
    const EnvelopeCurveEditor &e, float sinVal, float mix, float phase) {
  if (mix <= 0.0f) {
    if (e.wavePreview_.shape == WaveShape::Sine)
      return sinVal;
    return std::lerp(sinVal, shapeOscValue(e.wavePreview_.shape, phase), -mix);
  }
  float addVal = 0.0f;
  for (size_t n = 0; n < 4; ++n) {
    if (e.wavePreview_.harmonicGains[n] > 0.0f)
      addVal += e.wavePreview_.harmonicGains[n] *
                std::sin(phase * static_cast<float>(n + 1));
  }
  return std::lerp(sinVal, addVal, mix);
}

void EnvelopeCurveEditor::PaintHelper::envelopeOverlay(
    const EnvelopeCurveEditor &e, juce::Graphics &g, const CoordMapper &c) {
  if (!e.editEnvData->hasPoints() || e.editTarget == EditTarget::none)
    return;

  const auto numPixels = static_cast<int>(c.w);

  juce::Path envLine;
  for (int i = 0; i <= numPixels; ++i) {
    const auto x = static_cast<float>(i);
    const float timeMs = (x / c.w) * c.durationMs;
    const float ey = c.valueToY(e.editEnvData->evaluate(timeMs));

    if (i == 0)
      envLine.startNewSubPath(x, ey);
    else
      envLine.lineTo(x, ey);
  }

  using enum EditTarget;
  juce::Colour envColour;
  switch (e.editTarget) {
  case amp:
    envColour = UIConstants::Colours::subArc.brighter(0.4f);
    break;
  case freq:
    envColour = juce::Colours::cyan;
    break;
  case saturate:
    envColour = juce::Colour(0xFFFF9500);
    break;
  case mix:
    envColour = juce::Colour(0xFF4CAF50);
    break;
  case clickAmp:
    envColour = UIConstants::Colours::clickArc;
    break;
  case directAmp:
    envColour = UIConstants::Colours::directArc;
    break;
  case none:
    return;
  }
  g.setColour(envColour);
  g.strokePath(envLine, juce::PathStrokeType(1.5f));

  const auto &pts = e.editEnvData->getPoints();
  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    const auto idx = static_cast<size_t>(i);
    const float px = c.timeMsToX(pts[idx].timeMs);
    const float py = c.valueToY(pts[idx].value);
    constexpr float r = pointHitRadius * 0.6f;

    if (i == e.drag_.pointIndex) {
      g.setColour(juce::Colours::white);
      g.fillEllipse(px - r, py - r, r * 2.0f, r * 2.0f);
    } else {
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.fillEllipse(px - r, py - r, r * 2.0f, r * 2.0f);
      g.setColour(juce::Colours::white);
      g.drawEllipse(px - r, py - r, r * 2.0f, r * 2.0f, 1.0f);
    }
  }

  for (int i = 0; i < static_cast<int>(pts.size()) - 1; ++i) {
    const auto si = static_cast<size_t>(i);
    if (std::abs(pts[si].curve) < 1e-4f)
      continue;
    const float midMs = (pts[si].timeMs + pts[si + 1].timeMs) * 0.5f;
    const float midX = c.timeMsToX(midMs);
    const float midY = c.valueToY(e.editEnvData->evaluate(midMs));
    constexpr float d = 4.0f;
    juce::Path diamond;
    diamond.startNewSubPath(midX, midY - d);
    diamond.lineTo(midX + d, midY);
    diamond.lineTo(midX, midY + d);
    diamond.lineTo(midX - d, midY);
    diamond.closeSubPath();
    const bool active = (i == e.drag_.curveSegment);
    g.setColour(active ? juce::Colours::white : envColour.withAlpha(0.6f));
    g.fillPath(diamond);
  }
}

void EnvelopeCurveEditor::PaintHelper::timeline(juce::Graphics &g,
                                                const CoordMapper &c,
                                                float totalH) {
  g.setColour(juce::Colours::white.withAlpha(0.15f));
  g.drawHorizontalLine(static_cast<int>(c.plotH), 0.0f, c.w);

  {
    const float dur = c.durationMs;
    const float labelRowY = c.plotH + 16.0f;
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

      const float x0 = c.timeMsToX(startMs);
      const float x1 = c.timeMsToX(endMs);
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
    if (c.durationMs / iv <= 10.0f) {
      interval = iv;
      break;
    }
  }

  const float fontSize = 9.0f;
  g.setFont(juce::Font(juce::FontOptions(fontSize)));

  const auto numMarks = static_cast<int>(c.durationMs / interval) + 1;
  for (int i = 0; i < numMarks; ++i) {
    const float ms = static_cast<float>(i) * interval;
    const float x = c.timeMsToX(ms);

    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.drawVerticalLine(static_cast<int>(x), c.plotH, c.plotH + 4.0f);

    g.setColour(juce::Colours::white);
    const juce::String label = (ms >= 1000.0f)
                                   ? juce::String(ms * 0.001f, 1) + "s"
                                   : juce::String(static_cast<int>(ms));

    constexpr float labelW = 36.0f;
    g.drawText(label,
               juce::Rectangle<float>(x - labelW * 0.5f, c.plotH + 2.0f, labelW,
                                      totalH - c.plotH - 2.0f),
               juce::Justification::centredTop, false);
  }
}

void EnvelopeCurveEditor::setDisplayCycles(float cycles) {
  wavePreview_.displayCycles = cycles;
  repaint();
}

void EnvelopeCurveEditor::setDisplayDurationMs(float ms) {
  displayDurationMs = ms;
  repaint();
}

void EnvelopeCurveEditor::setOnChange(std::function<void()> cb) {
  onChange = std::move(cb);
}

// ── CoordMapper メソッド + makeCoords() ──

EnvelopeCurveEditor::CoordMapper
EnvelopeCurveEditor::makeCoords() const noexcept {
  const float ph =
      std::max(1.0f, static_cast<float>(getHeight()) - timelineHeight);
  return {displayDurationMs, static_cast<float>(getWidth()), ph, editTarget};
}

float EnvelopeCurveEditor::CoordMapper::timeMsToX(float timeMs) const noexcept {
  return (durationMs > 0.0f) ? (timeMs / durationMs) * w : 0.0f;
}

float EnvelopeCurveEditor::CoordMapper::xToTimeMs(float x) const noexcept {
  return (w > 0.0f) ? (x / w) * durationMs : 0.0f;
}

float EnvelopeCurveEditor::CoordMapper::valueToY(float value) const noexcept {
  using enum EditTarget;
  if (editTarget == freq) {
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float logVal = std::log(std::max(value, 20.0f));
    const float norm = (logVal - lo) / (hi - lo);
    return plotH - norm * plotH;
  }
  if (editTarget == saturate)
    return plotH - value * plotH;
  if (editTarget == mix)
    return plotH - ((value + 1.0f) * 0.5f) * plotH;
  return plotH - (value / 2.0f) * plotH;
}

float EnvelopeCurveEditor::CoordMapper::yToValue(float y) const noexcept {
  using enum EditTarget;
  if (editTarget == freq) {
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    const float norm = (plotH > 0.0f) ? (1.0f - y / plotH) : 0.5f;
    return std::exp(std::lerp(lo, hi, norm));
  }
  if (editTarget == saturate)
    return (plotH > 0.0f) ? (1.0f - y / plotH) : 0.0f;
  if (editTarget == mix)
    return (plotH > 0.0f) ? (1.0f - y / plotH) * 2.0f - 1.0f : 0.0f;
  return (plotH > 0.0f) ? (1.0f - y / plotH) * 2.0f : 1.0f;
}

// ── HitTester メソッド ──

int EnvelopeCurveEditor::HitTester::findPoint(const EnvelopeCurveEditor &e,
                                              const CoordMapper &c, float px,
                                              float py) {
  const auto &pts = e.editEnvData->getPoints();
  const float r2 = pointHitRadius * pointHitRadius;

  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    const float dx = px - c.timeMsToX(pts[static_cast<size_t>(i)].timeMs);
    const float dy = py - c.valueToY(pts[static_cast<size_t>(i)].value);
    if (dx * dx + dy * dy <= r2)
      return i;
  }
  return -1;
}

int EnvelopeCurveEditor::HitTester::findSegment(const EnvelopeCurveEditor &e,
                                                const CoordMapper &c, float px,
                                                float py) {
  const auto &pts = e.editEnvData->getPoints();
  const auto n = static_cast<int>(pts.size());
  if (n < 2)
    return -1;

  constexpr float maxDist = 12.0f;
  int bestSeg = -1;
  float bestDist = maxDist;

  for (int i = 0; i < n - 1; ++i) {
    const auto si = static_cast<size_t>(i);
    const float x0 = c.timeMsToX(pts[si].timeMs);
    if (const float x1 = c.timeMsToX(pts[si + 1].timeMs); px < x0 || px > x1)
      continue;
    const float timeMs = c.xToTimeMs(px);
    const float ey = c.valueToY(e.editEnvData->evaluate(timeMs));
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
  if (editTarget == EditTarget::none)
    return;
  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);
  const auto c = makeCoords();

  if (e.mods.isShiftDown()) {
    if (const int seg = HitTester::findSegment(*this, c, px, py); seg >= 0) {
      editEnvData->setSegmentCurve(seg, 0.0f);
      if (onChange)
        onChange();
      repaint();
    }
    return;
  }

  if (const int hit = HitTester::findPoint(*this, c, px, py); hit >= 0) {
    if (editEnvData->getPoints().size() > 1)
      editEnvData->removePoint(hit);
  } else {
    const float timeMs = std::clamp(c.xToTimeMs(px), 0.0f, displayDurationMs);
    const auto [lo, hi] = editValueRange(editTarget);
    const float value = std::clamp(c.yToValue(py), lo, hi);
    editEnvData->addPoint(timeMs, value);
  }

  if (onChange)
    onChange();
  repaint();
}

void EnvelopeCurveEditor::mouseDown(const juce::MouseEvent &e) {
  if (editTarget == EditTarget::none)
    return;
  // 右クリック: ポイント上ならコンテキストメニュー
  if (e.mods.isRightButtonDown()) {
    const auto c = makeCoords();
    if (const int hit = HitTester::findPoint(*this, c, static_cast<float>(e.x),
                                             static_cast<float>(e.y));
        hit >= 0)
      showPointContextMenu(hit, e.getScreenPosition());
    return;
  }

  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);
  const auto c = makeCoords();

  if (e.mods.isShiftDown()) {
    drag_.curveSegment = HitTester::findSegment(*this, c, px, py);
    if (drag_.curveSegment >= 0) {
      drag_.startY = py;
      const auto &pts = editEnvData->getPoints();
      drag_.startVal = pts[static_cast<size_t>(drag_.curveSegment)].curve;
    }
    drag_.pointIndex = -1;
  } else {
    drag_.curveSegment = -1;
    drag_.pointIndex = HitTester::findPoint(*this, c, px, py);
  }
}

void EnvelopeCurveEditor::mouseDrag(const juce::MouseEvent &e) {
  if (drag_.curveSegment >= 0) {
    const float dy = static_cast<float>(e.y) - drag_.startY;
    constexpr float sensitivity = 200.0f;
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

  const auto c = makeCoords();
  const float timeMs =
      std::clamp(c.xToTimeMs(static_cast<float>(e.x)), 0.0f, displayDurationMs);
  const auto [lo, hi] = editValueRange(editTarget);
  const float value = std::clamp(c.yToValue(static_cast<float>(e.y)), lo, hi);
  drag_.pointIndex = editEnvData->movePoint(drag_.pointIndex, timeMs, value);

  if (onChange)
    onChange();
  repaint();
}

void EnvelopeCurveEditor::mouseUp(const juce::MouseEvent & /*e*/) {
  drag_.pointIndex = -1;
  drag_.curveSegment = -1;
}

void EnvelopeCurveEditor::mouseMove(const juce::MouseEvent &e) {
  if (editTarget == EditTarget::none) {
    if (hoverPointIndex_ != -1) {
      hoverPointIndex_ = -1;
      repaint();
    }
    return;
  }
  const auto px = static_cast<float>(e.x);
  const auto py = static_cast<float>(e.y);
  const auto c = makeCoords();
  if (const int hit = HitTester::findPoint(*this, c, px, py);
      hit != hoverPointIndex_) {
    hoverPointIndex_ = hit;
    repaint();
  }
}

void EnvelopeCurveEditor::mouseExit(const juce::MouseEvent & /*e*/) {
  if (hoverPointIndex_ != -1) {
    hoverPointIndex_ = -1;
    repaint();
  }
}

void EnvelopeCurveEditor::setEditTarget(EditTarget target) {
  editTarget = target;
  if (target != EditTarget::none)
    editEnvData =
        envDatas_[static_cast<std::size_t>(std::to_underlying(target))];
  drag_.pointIndex = -1;
  repaint();
}

void EnvelopeCurveEditor::setOnEditTargetChanged(
    std::function<void(EditTarget)> cb) {
  onEditTargetChanged = std::move(cb);
}

// ── ポイント右クリックメニュー ──

void EnvelopeCurveEditor::showPointContextMenu(int pointIndex,
                                               juce::Point<int> screenPos) {
  juce::PopupMenu menu;
  menu.addItem(1, "Edit Value");
  menu.addItem(2, "Set Position");
  menu.addSeparator();
  menu.addItem(3, "Reset");

  const auto opts = juce::PopupMenu::Options().withTargetScreenArea(
      juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1));

  menu.showMenuAsync(opts, [this, pointIndex](int result) {
    if (result == 1) {
      startPointValueEdit(pointIndex);
    } else if (result == 2) {
      startPointTimeEdit(pointIndex);
    } else if (result == 3) {
      // index 1 以降を逆順で全削除（最初のポイントの値はそのまま保持）
      for (int i = static_cast<int>(editEnvData->getPoints().size()) - 1;
           i >= 1; --i)
        editEnvData->removePoint(i);
      if (onChange)
        onChange();
      repaint();
    }
  });
}

std::optional<juce::Rectangle<int>>
EnvelopeCurveEditor::makeEditorBounds(int pointIndex) {
  const auto &pts = editEnvData->getPoints();
  if (pointIndex >= static_cast<int>(pts.size()))
    return std::nullopt;

  const auto &pt = pts[static_cast<size_t>(pointIndex)];
  const auto c = makeCoords();
  const float px = c.timeMsToX(pt.timeMs);
  const float py = c.valueToY(pt.value);

  constexpr int editorW = 90;
  constexpr int editorH = 22;
  int bx = static_cast<int>(px) - editorW / 2;
  int by = static_cast<int>(py) - editorH - 10;
  if (by < 0)
    by = static_cast<int>(py) + 10;
  bx = juce::jlimit(0, getWidth() - editorW, bx);
  by = juce::jlimit(0, static_cast<int>(c.plotH) - editorH, by);

  return juce::Rectangle<int>(bx, by, editorW, editorH);
}

void EnvelopeCurveEditor::startPointValueEdit(int pointIndex) {
  const auto bounds = makeEditorBounds(pointIndex);
  if (!bounds)
    return;

  const auto &pt = editEnvData->getPoints()[static_cast<size_t>(pointIndex)];
  pointValueEditor_ = std::make_unique<PointValueEditor>(
      this, pointIndex, *bounds, pointValueToDisplayString(pt.value),
      PointValueEditor::Mode::Value);
}

void EnvelopeCurveEditor::startPointTimeEdit(int pointIndex) {
  const auto bounds = makeEditorBounds(pointIndex);
  if (!bounds)
    return;

  const auto &pt = editEnvData->getPoints()[static_cast<size_t>(pointIndex)];
  // 現在の時間を表示文字列に変換
  const float ms = pt.timeMs;
  juce::String currentText;
  if (ms < 10.0f)
    currentText = juce::String(ms, 2);
  else if (ms < 100.0f)
    currentText = juce::String(ms, 1);
  else
    currentText = juce::String(static_cast<int>(std::round(ms)));

  pointValueEditor_ = std::make_unique<PointValueEditor>(
      this, pointIndex, *bounds, currentText, PointValueEditor::Mode::Time);
}

juce::String EnvelopeCurveEditor::pointValueToDisplayString(float value) const {
  using enum EditTarget;
  switch (editTarget) {
  case freq:
    return juce::String(static_cast<int>(std::round(value)));
  case saturate:
    return juce::String(value * 24.0f, 1);
  case mix:
    return juce::String(static_cast<int>(std::round(value * 100.0f)));
  case amp:
  case clickAmp:
  case directAmp:
    return juce::String(static_cast<int>(std::round(value * 100.0f)));
  case none:
    break;
  }
  return juce::String(value, 2);
}

std::optional<float>
EnvelopeCurveEditor::parseDisplayStringToValue(const juce::String &text) const {
  const auto s = text.trim();
  if (s.isEmpty())
    return {};

  using enum EditTarget;
  switch (editTarget) {
  case freq: {
    // 音名（例: G3）→ Hz 変換を優先
    if (const auto hz = CustomSlider::parseNoteNameToHz(s); hz.has_value())
      return static_cast<float>(juce::jlimit(20.0, 20000.0, *hz));
    // 数値（Hz直値）
    if (const float hz = s.getFloatValue(); hz > 0.0f)
      return juce::jlimit(20.0f, 20000.0f, hz);
    return {};
  }
  case saturate:
    return juce::jlimit(0.0f, 1.0f, s.getFloatValue() / 24.0f);
  case mix:
    return juce::jlimit(-1.0f, 1.0f, s.getFloatValue() / 100.0f);
  case amp:
  case clickAmp:
  case directAmp:
    return juce::jlimit(0.0f, 2.0f, s.getFloatValue() / 100.0f);
  case none:
    break;
  }
  return {};
}

void EnvelopeCurveEditor::PaintHelper::pointTooltip(
    const EnvelopeCurveEditor &e, juce::Graphics &g, const CoordMapper &c) {
  if (e.editTarget == EditTarget::none)
    return;
  // ドラッグ中 > ホバー の優先順位で表示対象を決定
  const int idx =
      (e.drag_.pointIndex >= 0) ? e.drag_.pointIndex : e.hoverPointIndex_;
  if (idx < 0)
    return;

  const auto &pts = e.editEnvData->getPoints();
  if (idx >= static_cast<int>(pts.size()))
    return;

  const auto &pt = pts[static_cast<std::size_t>(idx)];
  const float px = c.timeMsToX(pt.timeMs);
  const float py = c.valueToY(pt.value);

  // 時間ラベル
  juce::String timeStr;
  if (pt.timeMs < 10.0f)
    timeStr = juce::String(pt.timeMs, 2) + " ms";
  else if (pt.timeMs < 100.0f)
    timeStr = juce::String(pt.timeMs, 1) + " ms";
  else
    timeStr = juce::String(static_cast<int>(std::round(pt.timeMs))) + " ms";

  // 値ラベル（EditTarget に応じてフォーマット）
  juce::String valueStr;
  using enum EditTarget;
  switch (e.editTarget) {
  case freq: {
    if (pt.value >= 1000.0f)
      valueStr = juce::String(pt.value / 1000.0f, 2) + " kHz";
    else
      valueStr = juce::String(static_cast<int>(std::round(pt.value))) + " Hz";
    if (const juce::String noteName =
            hzToNoteName(static_cast<double>(pt.value));
        noteName.isNotEmpty())
      valueStr += "  " + noteName;
    break;
  }
  case saturate: {
    // DSP 0.0〜1.0 → 表示 0〜24 dB
    const float dB = pt.value * 24.0f;
    valueStr = juce::String(dB, 1) + " dB";
    break;
  }
  case mix: {
    // DSP -1.0〜1.0 → 表示 -100〜100
    const auto pct = static_cast<int>(std::round(pt.value * 100.0f));
    valueStr = juce::String(pct);
    break;
  }
  case amp:
  case clickAmp:
  case directAmp: {
    // DSP 0.0〜2.0 → 表示 0〜200%
    const auto pct = static_cast<int>(std::round(pt.value * 100.0f));
    valueStr = juce::String(pct) + "%";
    break;
  }
  case none:
    break;
  }

  const juce::String label = timeStr + "  /  " + valueStr;

  constexpr float padX = 6.0f;
  constexpr float padY = 4.0f;
  constexpr float fontSize = 10.0f;

  const juce::Font font{juce::FontOptions{fontSize}};
  juce::GlyphArrangement ga;
  ga.addLineOfText(font, label, 0.0f, 0.0f);
  const float textW = ga.getBoundingBox(0, -1, true).getWidth();
  const float boxW = textW + padX * 2.0f;
  const float boxH = fontSize + padY * 2.0f;

  // ポイントの上に表示。境界を超えないようにクランプ
  float bx = px - boxW * 0.5f;
  float by = py - boxH - 8.0f;
  if (by < 0.0f)
    by = py + 10.0f; // 上が切れる場合は下に表示
  bx = juce::jlimit(0.0f, c.w - boxW, bx);
  by = juce::jlimit(0.0f, c.plotH - boxH, by);

  const auto box = juce::Rectangle<float>(bx, by, boxW, boxH);
  g.setColour(juce::Colours::black.withAlpha(0.80f));
  g.fillRoundedRectangle(box, 3.0f);
  g.setColour(juce::Colours::white.withAlpha(0.40f));
  g.drawRoundedRectangle(box, 3.0f, 0.5f);
  g.setFont(font);
  g.setColour(juce::Colours::white);
  g.drawText(label, box, juce::Justification::centred, false);
}
