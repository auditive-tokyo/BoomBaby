#pragma once

#include "UIConstants.h"
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

// ── Hz → 音名変換ヘルパー（例: 440 → "A4", 88.45 → "F2 +22"）──
inline juce::String hzToNoteName(double hz) {
  if (hz <= 0.0)
    return {};
  const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
  const auto midiRound = static_cast<int>(std::round(midi));
  const auto cents = static_cast<int>(std::round((midi - midiRound) * 100.0));
  const int octave = midiRound / 12 - 1;
  const int idx = ((midiRound % 12) + 12) % 12;
  static constexpr std::array<const char *, 12> names = {
      "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  juce::String s =
      juce::String(names[static_cast<size_t>(idx)]) + juce::String(octave);
  if (cents > 0)
    s += " +" + juce::String(cents);
  else if (cents < 0)
    s += " " + juce::String(cents);
  return s;
}

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

    // ── disabled 時はグレースケールで描画 ──
    const bool enabled = slider.isEnabled();
    const juce::Colour effectiveArc =
        enabled ? arcColour : juce::Colour(0xFF555555);
    const juce::Colour effectiveThumb =
        enabled ? thumbColour : juce::Colour(0xFF777777);

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
            effectiveArc.interpolatedWith(effectiveThumb, brightness)
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
      g.setColour(effectiveThumb.withAlpha(0.25f));
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
        const juce::Colour textCol =
            enabled ? UIConstants::Colours::text
                    : UIConstants::Colours::text.withAlpha(0.4f);

        // subFreq ノブは 2 段描画（周波数 + 音名）
        const bool showNote = slider.getComponentID() == "subFreq";
        if (showNote) {
          const juce::String noteName = hzToNoteName(slider.getValue());
          const float lineH = fontSize * 1.05f;
          // 上段: 周波数値
          g.setFont(fontSize);
          g.setColour(textCol);
          g.drawText(valueText,
                     juce::Rectangle<float>(centreX - radius * 0.8f,
                                            centreY - lineH, radius * 1.6f,
                                            fontSize),
                     juce::Justification::centred, false);
          // 下段: 音名（同サイズ・subArc カラー）
          g.setFont(fontSize);
          g.setColour(enabled ? UIConstants::Colours::subArc
                              : UIConstants::Colours::subArc.withAlpha(0.4f));
          g.drawText(noteName,
                     juce::Rectangle<float>(centreX - radius * 0.8f,
                                            centreY + fontSize * 0.12f,
                                            radius * 1.6f, fontSize),
                     juce::Justification::centred, false);
        } else {
          g.setFont(fontSize);
          g.setColour(textCol);
          const auto textBounds = juce::Rectangle<float>(
              centreX - radius * 0.7f, centreY - fontSize * 0.5f, radius * 1.4f,
              fontSize);
          g.drawText(valueText, textBounds, juce::Justification::centred,
                     false);
        }
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

// ────────────────────────────────────────────────────
// macOS Natural Scrolling 対応 Slider
// JUCE デフォルトは isReversed を反転してしまうため、
// フラグをそのまま渡して OS の慣性スクロール方向に従わせる
// ────────────────────────────────────────────────────
class CustomSlider : public juce::Slider {
public:
  using juce::Slider::Slider;

  // ── 音名文字列 → Hz 変換（例: "F1", "G#3", "Bb1"）──
  static std::optional<double> parseNoteNameToHz(const juce::String &input) {
    const auto s = input.trim();
    if (s.isEmpty())
      return {};
    static constexpr std::array<int, 7> noteTable = {9, 11, 0, 2,
                                                     4, 5,  7}; // A B C D E F G
    const auto first = static_cast<char>(std::toupper(s[0]));
    if (first < 'A' || first > 'G')
      return {};
    int noteIdx = noteTable[static_cast<size_t>(first - 'A')];
    int pos = 1;
    int accidental = 0;
    while (pos < s.length()) {
      if (s[pos] == '#') {
        ++accidental;
        ++pos;
      } else if (s[pos] == 'b' || s[pos] == 'B') {
        --accidental;
        ++pos;
      } else
        break;
    }
    if (pos >= s.length())
      return {};
    // りんのオクターブ（負値も許容: C-1 等）
    const int octave = s.substring(pos).getIntValue();
    const int midi = (octave + 1) * 12 + noteIdx + accidental;
    if (midi < 0 || midi > 127)
      return {};
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
  }

  // ── 音名インラインエディタ（ノブ上に直接オーバーレイ）──
  struct NoteEditorOverlay : public juce::Component {
    juce::TextEditor editor;
    CustomSlider *owner;

    NoteEditorOverlay(CustomSlider *s, juce::Rectangle<int> bounds) : owner(s) {
      const auto side =
          static_cast<float>(juce::jmin(s->getWidth(), s->getHeight()));
      const float fontSize = side * 0.40f * 0.38f;

      editor.setFont(
          juce::Font(juce::FontOptions(juce::jmax(fontSize, 11.0f))));
      editor.setJustification(juce::Justification::centred);
      editor.setColour(juce::TextEditor::backgroundColourId,
                       UIConstants::Colours::knobBg);
      editor.setColour(juce::TextEditor::textColourId,
                       UIConstants::Colours::subArc);
      editor.setColour(juce::TextEditor::outlineColourId,
                       UIConstants::Colours::subArc.withAlpha(0.6f));
      editor.setColour(juce::TextEditor::focusedOutlineColourId,
                       UIConstants::Colours::subArc);
      editor.setColour(juce::TextEditor::highlightColourId,
                       UIConstants::Colours::subArc.withAlpha(0.35f));
      editor.setText(hzToNoteName(s->getValue()), false);
      editor.selectAll();
      addAndMakeVisible(editor);

      setBounds(bounds);
      s->addAndMakeVisible(this);
      editor.grabKeyboardFocus();

      editor.onReturnKey = [this] { commit(); };
      editor.onEscapeKey = [this] { dismiss(); };
    }

    void resized() override { editor.setBounds(getLocalBounds()); }

    // フォーカスが外れたらコミットして閉じる
    void focusOfChildComponentChanged(FocusChangeType) override {
      if (!hasKeyboardFocus(true))
        commit();
    }

    void commit() {
      if (const auto hz = parseNoteNameToHz(editor.getText()); hz.has_value()) {
        const double clamped =
            juce::jlimit(owner->getMinimum(), owner->getMaximum(), hz.value());
        owner->setValue(clamped, juce::sendNotificationSync);
      }
      dismiss();
    }

    void dismiss() {
      // owner の unique_ptr を reset することで安全に自身を破棄
      // callAsync で現在のコールスタックを抜けてから実行
      juce::Component::SafePointer<NoteEditorOverlay> self(this);
      juce::MessageManager::callAsync([self] {
        if (auto *p = self.getComponent())
          p->owner->noteOverlay_.reset(); // unique_ptr が delete + 親から除去
      });
    }
  };

  std::unique_ptr<NoteEditorOverlay> noteOverlay_;

  void mouseDown(const juce::MouseEvent &e) override {
    if (getComponentID() == "subFreq") {
      // 音名テキストエリア（ノブ下半）をクリックしたときにインラインエディタ起動
      const float cy = static_cast<float>(getHeight()) * 0.5f;
      if (static_cast<float>(e.y) > cy && !e.mods.isAnyModifierKeyDown()) {
        // 既存エディタがあれば何もしない
        if (noteOverlay_ != nullptr)
          return;

        // ノブ描画と同じ座標計算でオーバーレイ位置を決める
        const auto side =
            static_cast<float>(juce::jmin(getWidth(), getHeight()));
        const float rad = side * 0.40f;
        const float cx = static_cast<float>(getWidth()) * 0.5f;
        const float fontSize = rad * 0.38f;
        const auto edW = static_cast<int>(rad * 1.6f);
        const int edH = juce::jmax(static_cast<int>(fontSize * 1.3f), 18);
        const auto edX = static_cast<int>(cx - static_cast<float>(edW) * 0.5f);
        const auto edY = static_cast<int>(cy + fontSize * 0.08f);

        noteOverlay_ = std::make_unique<NoteEditorOverlay>(
            this, juce::Rectangle<int>{edX, edY, edW, edH});
        return;
      }
    }
    juce::Slider::mouseDown(e);
  }

  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override {
    auto w = wheel;
    w.isReversed = !wheel.isReversed;
    juce::Slider::mouseWheelMove(e, w);
  }
};
