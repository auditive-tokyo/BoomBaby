#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace UIConstants {
// ── ウィンドウ ──
constexpr int windowWidth = 800;
constexpr int windowHeight = 260;

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
constexpr int subKnobRowHeight = 88;
constexpr int waveShapeButtonRowHeight = 28;
constexpr int expandedAreaHeight = 476;  // 360 + subKnobRowHeight(88) + waveShapeButtonRowHeight(28)
constexpr int waveformDisplayHeight = 80;

// ── レベルメーター ──
constexpr int meterWidth = 28;

// ── 鍵盤 ──
constexpr int keyboardHeight = 70;

// ── カラーパレット ──
namespace Colours {
// 背景
inline const juce::Colour background{0xFF1A1A1A};
inline const juce::Colour panelBg{0xFF2A2A2A};

// Sub（青系）
inline const juce::Colour subArc{0xFF00AAFF};
inline const juce::Colour subThumb{0xFF00CCFF};

// Click（黄系）
inline const juce::Colour clickArc{0xFFCCAA00};
inline const juce::Colour clickThumb{0xFFFFDD00};

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
} // namespace UIConstants
