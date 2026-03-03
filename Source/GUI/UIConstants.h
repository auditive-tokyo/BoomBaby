#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace UIConstants {
// ── ウィンドウ ──
constexpr int windowWidth = 1100;
constexpr int windowHeight = 294; // 260 + waveShapeButtonRowHeight(28) + panelGap(6)

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
constexpr int subKnobRowHeight = 88; // パネル内配置に変更済み（expand area では未使用）
constexpr int waveShapeButtonRowHeight = 28;
constexpr int expandedAreaHeight =
    360; // 290(curve) + 70(keyboard)（length/wave行はパネル内に移動）
constexpr int waveformDisplayHeight = 80;

// ── レベルメーター ──
constexpr int meterWidth = 28;

// ── 鍵盤 ──
constexpr int keyboardHeight = 70;

// ── フォント ──
constexpr float fontSizeSmall  = 10.0f;  // ノブラベル、エンベロープ等
constexpr float fontSizeMedium = 12.0f;  // ComboBox、Length ボックス等

// ── 波形表示 opacity ──
constexpr float subWaveOpacity    = 0.8f;  // Sub 波形
constexpr float clickWaveOpacity  = 0.8f;  // Click 波形
constexpr float directWaveOpacity = 0.8f;  // Direct 波形

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

// 波形表示エリア
inline const juce::Colour waveformBg{0xFF1E1E1E};

// Mute/Solo ボタン
inline const juce::Colour muteOff{0xFF444444};
inline const juce::Colour muteOn{0xFFCC2222};
inline const juce::Colour soloOff{0xFF444444};
inline const juce::Colour soloOn{0xFFCCAA00};
} // namespace Colours

/// HPF/LPF スロープ（12/24/48 dB/oct）をクリックで切り替えるミニセレクター。
/// ラベル行（14px 程度）にそのまま配置できるサイズで設計。
class SlopeSelector : public juce::Component {
public:
  explicit SlopeSelector(juce::String prefix = {},
                         juce::Colour accentColour = Colours::clickArc)
      : prefix_(std::move(prefix)), accent_(accentColour) {}

  /// 選択変更時に slope 値（12/24/48）を通知するコールバックを登録する
  void setOnChange(std::function<void(int)> cb) { onChange_ = std::move(cb); }

  int getSlope() const noexcept { return kSlopes[static_cast<std::size_t>(selected_)]; }

  void setSlope(int slope, bool notify = false) {
    for (int i = 0; i < 3; ++i) {
      if (kSlopes[static_cast<std::size_t>(i)] == slope) {
        selected_ = i;
        repaint();
        if (notify && onChange_) onChange_(slope);
        return;
      }
    }
  }

  void paint(juce::Graphics &g) override {
    const auto font = juce::Font(juce::FontOptions(fontSizeSmall));
    g.setFont(font);
    const int h       = getHeight();
    const int prefixW = prefix_.isEmpty() ? 0 : 18;
    if (!prefix_.isEmpty()) {
      g.setColour(juce::Colour(0xFFBBBBBB));
      g.drawText(prefix_, 0, 0, prefixW, h, juce::Justification::centredLeft, false);
    }
    const int slotsW = getWidth() - prefixW;
    const int slotW  = slotsW / 3;
    for (int i = 0; i < 3; ++i) {
      g.setColour(i == selected_ ? accent_
                                 : juce::Colour(0x88BBBBBB));
      g.drawText(juce::String(kSlopes[static_cast<std::size_t>(i)]),
                 prefixW + i * slotW, 0, slotW, h,
                 juce::Justification::centred, false);
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    const int prefixW  = prefix_.isEmpty() ? 0 : 18;
    const int xInSlots = e.x - prefixW;
    if (xInSlots < 0) return;
    const int slotsW = getWidth() - prefixW;
    const int slotW  = slotsW / 3;
    const int idx    = juce::jlimit(0, 2, xInSlots / slotW);
    if (idx != selected_) {
      selected_ = idx;
      repaint();
      if (onChange_) onChange_(kSlopes[static_cast<std::size_t>(selected_)]);
    }
  }

private:
  std::function<void(int)> onChange_;
  juce::String prefix_;
  juce::Colour accent_;
  int          selected_ = 0;
  static constexpr std::array<int, 3> kSlopes = {12, 24, 48};
};

/// ファイルをドラッグ＆ドロップまたはクリックで読み込むボタン。
/// ドラッグ中はボーダーをハイライト表示し、対応拡張子のみ受け付ける。
class SampleDropButton : public juce::TextButton,
                         public juce::FileDragAndDropTarget {
public:
  using juce::TextButton::TextButton;

  /// ファイルがドロップまたはクリック選択されたときのコールバック
  std::function<void(const juce::File &)> onFileDropped;

  bool isInterestedInFileDrag(const juce::StringArray &files) override {
    for (const auto &f : files) {
      const auto ext = juce::File(f).getFileExtension().toLowerCase();
      if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
          ext == ".flac" || ext == ".ogg")
        return true;
    }
    return false;
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
        if (onFileDropped)
          onFileDropped(file);
        return; // 最初の1ファイルのみ処理
      }
    }
  }

  void paintButton(juce::Graphics &g, bool highlighted, bool down) override {
    juce::TextButton::paintButton(g, highlighted || dragHovered_, down);
    if (dragHovered_) {
      g.setColour(juce::Colours::white.withAlpha(0.15f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f),
                             3.0f, 1.5f);
    }
  }

private:
  bool dragHovered_ = false;
};

} // namespace UIConstants
