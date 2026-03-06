#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────────────────
// MasterFader
//   出力マスターゲイン用横向きフェーダーコンポーネント。
//   "MASTER" ラベル + LinearHorizontal スライダー を内包する。
//
//   レイアウト (resized 内部で完結):
//     [ MASTER ラベル ]
//     [   ─── スライダー ───  ]
//
//   onValueChange に dB 値が渡される。
// ────────────────────────────────────────────────────────────────
class MasterFader : public juce::Component {
public:
  MasterFader();

  void resized() override;

  /// dB 値変化時のコールバックを登録する
  void setOnValueChange(std::function<void(float /*dB*/)> cb);

  /// 現在値を dB で取得
  float getValueDb() const { return static_cast<float>(fader.getValue()); }

  /// 値を dB でセット（通知なし）
  void setValueDb(float db) { fader.setValue(db, juce::dontSendNotification); }

  // ── dB スケール定数 ──
  static constexpr float minDb = -60.0f;
  static constexpr float maxDb = 12.0f;

  static constexpr int labelHeight = 16;

private:
  std::function<void(float)> onValueChange;
  juce::Label label;
  juce::Slider fader;
};
