#pragma once

#include "UIConstants.h"
#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────
// 色付きロータリースライダー用 LookAndFeel
// arcColour / thumbColour を差し替えるだけで 3 種類に対応
// ────────────────────────────────────────────────────
class ColouredSliderLAF : public juce::LookAndFeel_V4 {
public:
  ColouredSliderLAF(juce::Colour arc, juce::Colour thumb)
      : arcColour(arc), thumbColour(thumb) {}

  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override {
    const float radius = (float)juce::jmin(width, height) * 0.40f;
    const float centreX = (float)x + (float)width * 0.5f;
    const float centreY = (float)y + (float)height * 0.5f;
    const float stroke = UIConstants::knobStrokeWidth;

    // ── 背景円 ──
    g.setColour(UIConstants::Colours::knobBg);
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f,
                  radius * 2.0f);

    // ── 背景アーク（暗いトラック） ──
    {
      juce::Path bgArc;
      bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                          rotaryStartAngle, rotaryEndAngle, true);
      g.setColour(juce::Colours::black.withAlpha(0.4f));
      g.strokePath(bgArc,
                   juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
    }

    // ── 値アーク（グラデーショングロー） ──
    if (const float angle =
            std::lerp(rotaryStartAngle, rotaryEndAngle, sliderPosProportional);
        angle > rotaryStartAngle + 0.01f) {
      const float totalArcLen = angle - rotaryStartAngle;
      // グラデーションを段階的に描画（暗→明）
      constexpr int numSegments = 64;

      for (int i = 0; i < numSegments; ++i) {
        const float t0 = (float)i / (float)numSegments;
        const float t1 = (float)(i + 1) / (float)numSegments;

        const float segStart = rotaryStartAngle + totalArcLen * t0;
        const float segEnd = rotaryStartAngle + totalArcLen * t1;

        // t が 1.0 に近づくほど急激に明るくなる指数カーブ
        const float brightness =
            t0 * t0 * t0 * t0; // 四次曲線でよりexponentialに
        const float alpha = 0.08f + 0.92f * brightness; // 始点はほぼ透明
        const auto segColour =
            arcColour.interpolatedWith(thumbColour, brightness)
                .withAlpha(alpha);

        juce::Path seg;
        seg.addCentredArc(centreX, centreY, radius, radius, 0.0f, segStart,
                          segEnd, true);
        g.setColour(segColour);
        g.strokePath(seg,
                     juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
      }

      // 先端に薄いソフトグロー（にじみ）
      const float glowSpan =
          juce::jmin(totalArcLen, juce::MathConstants<float>::pi * 0.12f);
      juce::Path softGlow;
      softGlow.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                             angle - glowSpan, angle, true);
      g.setColour(thumbColour.withAlpha(0.25f));
      g.strokePath(softGlow, juce::PathStrokeType(
                                 stroke + 3.0f, juce::PathStrokeType::curved,
                                 juce::PathStrokeType::rounded));
    }

    // ── ノブ中央に値を描画（テキストエディタが開いていないときだけ） ──
    {
      bool editorOpen = false;
      for (auto *child : slider.getChildren())
        if (const auto *lbl = dynamic_cast<juce::Label *>(child))
          if (lbl->getCurrentTextEditor() != nullptr) {
            editorOpen = true;
            break;
          }

      if (!editorOpen) {
        const auto valueText = slider.textFromValueFunction
                                   ? slider.getTextFromValue(slider.getValue())
                                   : juce::String(slider.getValue(), 1) +
                                         slider.getTextValueSuffix();
        const float fontSize = radius * 0.38f;
        g.setFont(fontSize);
        g.setColour(UIConstants::Colours::text);

        const auto textBounds = juce::Rectangle<float>(
            centreX - radius * 0.7f, centreY - fontSize * 0.5f, radius * 1.4f,
            fontSize);
        g.drawText(valueText, textBounds, juce::Justification::centred, false);
      }
    }
  }

  // ── ラベル描画を抑制（値はdrawRotarySlider内で描画済み） ──
  void drawLabel(juce::Graphics &, juce::Label &) override {
    // Intentionally empty: the value text is rendered directly inside
    // drawRotarySlider(), so the default Label paint must be suppressed
    // to avoid double-drawing over the knob face.
  }

  // ── テキストボックスをノブ中央に配置 ──
  juce::Slider::SliderLayout getSliderLayout(juce::Slider &slider) override {
    auto bounds = slider.getLocalBounds();
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = bounds;

    const int dim = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float rad = static_cast<float>(dim) * 0.40f;
    const auto textW = static_cast<int>(rad * 1.5f);
    const int textH = juce::jmax(static_cast<int>(rad * 0.50f), 14);
    layout.textBoxBounds = bounds.withSizeKeepingCentre(textW, textH);
    return layout;
  }

  // ── テキストボックスの Label / TextEditor スタイル ──
  juce::Label *createSliderTextBox(juce::Slider &slider) override {
    auto *label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setJustificationType(juce::Justification::centred);
    // ラベル自体は透明（値はdrawRotarySliderで描画）
    label->setColour(juce::Label::textColourId,
                     juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundColourId,
                     juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId,
                     juce::Colours::transparentBlack);
    label->setInterceptsMouseClicks(true, true);

    // エディタが開いたときのスタイル
    label->onEditorShow = [label] {
      if (auto *ed = label->getCurrentTextEditor()) {
        ed->setJustification(juce::Justification::centred);
        ed->setColour(juce::TextEditor::backgroundColourId,
                      UIConstants::Colours::knobBg);
        ed->setColour(juce::TextEditor::textColourId,
                      UIConstants::Colours::text);
        ed->setColour(juce::TextEditor::highlightColourId,
                      juce::Colour(0xFF4488AA));
        ed->setColour(juce::TextEditor::outlineColourId,
                      juce::Colours::transparentBlack);
        ed->setColour(juce::TextEditor::focusedOutlineColourId,
                      juce::Colours::transparentBlack);
        ed->selectAll();
      }
    };
    // エディタが閉じたあとノブを再描画
    label->onEditorHide = [label] {
      if (auto *parent = label->getParentComponent())
        parent->repaint();
    };
    return label;
  }

private:
  juce::Colour arcColour;
  juce::Colour thumbColour;
};
