#pragma once

#include <algorithm>
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace UIConstants {
// ── ウィンドウ ──
constexpr int windowWidth = 1100;
constexpr int windowHeight =
    294; // 260 + waveShapeButtonRowHeight(28) + panelGap(6)

// ── パネル ──
constexpr int panelPadding = 8;
constexpr int panelGap = 6;

// ── ノブ ──
constexpr int knobSize = 100;
constexpr float knobStrokeWidth = 4.0f;

// ── ラベル ──
constexpr int labelHeight = 22;
constexpr int valueHeight = 20;

// ── 展開パネル ──
constexpr int expandButtonHeight = 20;
constexpr int subKnobRowHeight =
    88; // パネル内配置に変更済み（expand area では未使用）
constexpr int waveShapeButtonRowHeight = 28;
constexpr int expandedAreaHeight =
    360; // 290(curve) + 70(keyboard)（length/wave行はパネル内に移動）
constexpr int waveformDisplayHeight = 80;

// ── チャンネルフェーダー ──
constexpr int meterWidth = 28;
constexpr float meterFontSize = 8.0f;       // dB スケール目盛りラベル
constexpr float faderValueFontSize = 12.0f; // フェーダー現在値ラベル

// ── マスターフェーダー ──
constexpr float masterGainFontSize = 14.0f; // マスターゲイン設定値表示
constexpr int masterGainLabelWidth = 52;    // 右端に確保する幅 (px)

// ── 鍵盤 ──
constexpr int keyboardHeight = 70;

// ── プリセットバー ──
constexpr int presetBarHeight = 28;

// ── フォント ──
constexpr float fontSizeSmall = 10.0f; // ノブラベル、エンベロープ等
constexpr float fontSizeMedium = 12.0f; // ComboBox、Length ボックス等

// ── 波形表示 opacity ──
constexpr float subWaveOpacity = 0.8f;    // Sub 波形
constexpr float clickWaveOpacity = 0.8f;  // Click 波形
constexpr float directWaveOpacity = 0.8f; // Direct 波形

// ── カラーパレット ──
namespace Colours {
// 背景
inline const juce::Colour background{0xFF1A1A1A};
inline const juce::Colour panelBg{0xFF2A2A2A};

// Sub（青系）
inline const juce::Colour subArc{0xFF00AAFF};
inline const juce::Colour subThumb{0xFF00CCFF};

// Click（黄系）
inline const juce::Colour clickArc{0xFFEEFF00};
inline const juce::Colour clickThumb{0xFFFFFF33};

// Direct（ピンク/マゼンタ系）
inline const juce::Colour directArc{0xFFCC44CC};
inline const juce::Colour directThumb{0xFFFF66FF};

// 共通
inline const juce::Colour knobBg{0xFF333333};
inline const juce::Colour text{0xFFDDDDDD};
inline const juce::Colour labelText{0xFFBBBBBB};

// InfoBox
inline const juce::Colour infoBoxText{0xFFFFFFFF};

// 波形表示エリア
inline const juce::Colour waveformBg{0xFF1E1E1E};

// Mute/Solo ボタン
inline const juce::Colour muteOff{0xFF444444};
inline const juce::Colour muteOn{0xFF8833CC};
inline const juce::Colour soloOff{0xFF444444};
inline const juce::Colour soloOn{0xFF22AA44};
} // namespace Colours

/// ON 時に上→下グラデーションを描画するボタン用 LookAndFeel。
/// offColour = 非アクティブ色、onColour = アクティブ時のベース色。
class GradientButtonLAF : public juce::LookAndFeel_V4 {
public:
  GradientButtonLAF(juce::Colour off, juce::Colour on)
      : offColour_(off), onColour_(on) {}

  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour & /*bgColour*/,
                            bool isHighlighted, bool isDown) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    const juce::Colour base = on ? onColour_ : offColour_;

    if (on) {
      // 上端: 明るい青白 → 下端: 深い青  で立体感を出す
      const juce::Colour top =
          base.withBrightness(juce::jmin(1.0f, base.getBrightness() * 1.55f))
              .withSaturation(0.5f);
      const juce::Colour bottom =
          base.withBrightness(base.getBrightness() * 0.55f);
      const juce::ColourGradient grad(top, 0.0f, bounds.getY(), bottom, 0.0f,
                                      bounds.getBottom(), false);
      g.setGradientFill(grad);
    } else {
      const juce::Colour fill = isHighlighted ? base.brighter(0.15f) : base;
      g.setColour(isDown ? fill.darker(0.2f) : fill);
    }
    g.fillRoundedRectangle(bounds, 3.0f);

    // ボーダー
    g.setColour(on ? onColour_.brighter(0.4f).withAlpha(0.6f)
                   : juce::Colours::black.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 3.0f, 0.8f);
  }

private:
  juce::Colour offColour_;
  juce::Colour onColour_;
};

/// HPF/LPF スロープ（12/24/48 dB/oct）をクリックで切り替えるミニセレクター。
/// ラベル行（14px 程度）にそのまま配置できるサイズで設計。
class SlopeSelector : public juce::Component {
public:
  explicit SlopeSelector(juce::String prefix = {},
                         juce::Colour accentColour = Colours::clickArc)
      : prefix_(std::move(prefix)), accent_(accentColour) {}

  /// 選択変更時に slope 値（12/24/48）を通知するコールバックを登録する
  void setOnChange(std::function<void(int)> cb) { onChange_ = std::move(cb); }

  int getSlope() const noexcept {
    return kSlopes[static_cast<std::size_t>(selected_)];
  }

  void setSlope(int slope, bool notify = false) {
    for (int i = 0; i < 3; ++i) {
      if (kSlopes[static_cast<std::size_t>(i)] == slope) {
        selected_ = i;
        repaint();
        if (notify && onChange_)
          onChange_(slope);
        return;
      }
    }
  }

  void paint(juce::Graphics &g) override {
    const auto font = juce::Font(juce::FontOptions(fontSizeSmall));
    g.setFont(font);
    const int h = getHeight();
    const int prefixW = prefix_.isEmpty() ? 0 : 18;
    if (!prefix_.isEmpty()) {
      g.setColour(juce::Colour(0xFFBBBBBB));
      g.drawText(prefix_, 0, 0, prefixW, h, juce::Justification::centredLeft,
                 false);
    }
    const int slotsW = getWidth() - prefixW;
    const int slotW = slotsW / 3;
    for (int i = 0; i < 3; ++i) {
      g.setColour(i == selected_ ? accent_ : juce::Colour(0x88BBBBBB));
      g.drawText(juce::String(kSlopes[static_cast<std::size_t>(i)]),
                 prefixW + i * slotW, 0, slotW, h, juce::Justification::centred,
                 false);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const int prefixW = prefix_.isEmpty() ? 0 : 18;
    const int xInSlots = e.x - prefixW;
    if (xInSlots < 0)
      return;
    const int slotsW = getWidth() - prefixW;
    const int slotW = slotsW / 3;
    if (const int idx = juce::jlimit(0, 2, xInSlots / slotW);
        idx != selected_) {
      selected_ = idx;
      repaint();
      if (onChange_)
        onChange_(kSlopes[static_cast<std::size_t>(selected_)]);
    }
  }

private:
  std::function<void(int)> onChange_;
  juce::String prefix_;
  juce::Colour accent_;
  int selected_ = 0;
  static constexpr std::array<int, 3> kSlopes = {12, 24, 48};
};

/// 3択テキストセレクター（ClipType 等に使用）。
/// SlopeSelector の文字列版。コールバックは選択インデックス（0/1/2）を返す。
class LabelSelector : public juce::Component {
public:
  using Labels = std::array<const char *, 3>;
  explicit LabelSelector(Labels labels = {"Soft", "Hard", "Tube"},
                         juce::Colour accentColour = Colours::clickArc)
      : labels_(labels), accent_(accentColour) {}

  void setOnChange(std::function<void(int)> cb) { onChange_ = std::move(cb); }
  void setOnClicked(std::function<void()> cb) { onClicked_ = std::move(cb); }
  int getSelected() const noexcept { return selected_; }

  void setSelected(int idx, bool notify = false) {
    idx = juce::jlimit(0, 2, idx);
    if (idx != selected_) {
      selected_ = idx;
      repaint();
      if (notify && onChange_)
        onChange_(selected_);
    }
  }

  void paint(juce::Graphics &g) override {
    const auto font = juce::Font(juce::FontOptions(fontSizeSmall));
    g.setFont(font);
    const int h = getHeight();
    const int slotW = getWidth() / 3;
    for (int i = 0; i < 3; ++i) {
      g.setColour(i == selected_ ? accent_ : juce::Colour(0x88BBBBBB));
      g.drawText(labels_[static_cast<std::size_t>(i)], i * slotW, 0, slotW, h,
                 juce::Justification::centred, false);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const int slotW = getWidth() / 3;
    if (const int idx = juce::jlimit(0, 2, e.x / slotW); idx != selected_) {
      selected_ = idx;
      repaint();
      if (onChange_)
        onChange_(selected_);
    }
    if (onClicked_)
      onClicked_();
  }

private:
  std::function<void(int)> onChange_;
  std::function<void()> onClicked_;
  Labels labels_;
  juce::Colour accent_;
  int selected_ = 0;
};

/// ファイルをドラッグ＆ドロップまたはクリックで読み込むボタン。
/// ドラッグ中はボーダーをハイライト表示し、対応拡張子のみ受け付ける。
class SampleDropButton : public juce::TextButton,
                         public juce::FileDragAndDropTarget {
public:
  using juce::TextButton::TextButton;

  /// ファイルがドロップまたはクリック選択されたときのコールバックを登録する
  void setOnFileDropped(std::function<void(const juce::File &)> cb) {
    onFileDropped_ = std::move(cb);
  }

  /// サンプルクリア（X ボタン）コールバックを登録する
  void setOnClear(std::function<void()> cb) { onClear_ = std::move(cb); }

  /// ファイルがロード済みかどうか（X ボタン表示制御用）
  void setHasFile(bool loaded) {
    hasFile_ = loaded;
    repaint();
  }

  bool isInterestedInFileDrag(const juce::StringArray &files) override {
    return std::ranges::any_of(files, [](const juce::String &f) {
      const auto ext = juce::File(f).getFileExtension().toLowerCase();
      return ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
             ext == ".flac" || ext == ".ogg";
    });
  }

  void fileDragEnter(const juce::StringArray &, int, int) override {
    dragHovered_ = true;
    repaint();
  }

  void fileDragExit(const juce::StringArray &) override {
    dragHovered_ = false;
    repaint();
  }

  void filesDropped(const juce::StringArray &files, int, int) override {
    dragHovered_ = false;
    repaint();
    for (const auto &path : files) {
      const juce::File file(path);
      if (file.existsAsFile()) {
        if (onFileDropped_)
          onFileDropped_(file);
        return; // 最初の1ファイルのみ処理
      }
    }
  }

  void paintButton(juce::Graphics &g, bool highlighted, bool down) override {
    juce::TextButton::paintButton(g, highlighted || dragHovered_, down);
    if (dragHovered_) {
      g.setColour(juce::Colours::white.withAlpha(0.15f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 3.0f,
                             1.5f);
    }
    // ファイルロード済みなら右端に半透明 X を描画
    if (hasFile_) {
      const int sz = getHeight() - 6;
      const auto xArea =
          juce::Rectangle<int>(getWidth() - sz - 4, 3, sz, sz).toFloat();
      const float alpha = clearHovered_ ? 0.8f : 0.4f;
      g.setColour(juce::Colours::white.withAlpha(alpha));
      const float m = 3.0f; // X 線のマージン
      g.drawLine(xArea.getX() + m, xArea.getY() + m, xArea.getRight() - m,
                 xArea.getBottom() - m, 1.5f);
      g.drawLine(xArea.getRight() - m, xArea.getY() + m, xArea.getX() + m,
                 xArea.getBottom() - m, 1.5f);
    }
  }

  void mouseMove(const juce::MouseEvent &e) override {
    const bool wasHovered = clearHovered_;
    clearHovered_ = hasFile_ && hitClearArea(e.x);
    if (clearHovered_ != wasHovered)
      repaint();
    juce::TextButton::mouseMove(e);
  }

  void mouseExit(const juce::MouseEvent &e) override {
    if (clearHovered_) {
      clearHovered_ = false;
      repaint();
    }
    juce::TextButton::mouseExit(e);
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (hasFile_ && hitClearArea(e.x) && onClear_) {
      onClear_();
      return; // ボタン onClick を発火させない
    }
    juce::TextButton::mouseUp(e);
  }

private:
  bool hitClearArea(int mx) const {
    const int sz = getHeight() - 6;
    return mx >= getWidth() - sz - 4;
  }

  std::function<void(const juce::File &)> onFileDropped_;
  std::function<void()> onClear_;
  bool dragHovered_ = false;
  bool hasFile_ = false;
  bool clearHovered_ = false;
};

} // namespace UIConstants
