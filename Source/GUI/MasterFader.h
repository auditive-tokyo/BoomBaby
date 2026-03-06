#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

// ────────────────────────────────────────────────────────────────
// MasterFader
//   出力マスターゲイン用横向きフェーダーコンポーネント。
//   "MASTER" ラベル + 横向きレベルメーターバー + フェーダーサムを内包。
//
//   レイアウト (resized 内部で完結):
//     [ MASTER ラベル                ]
//     [ ═════════ メーターバー + サム ═══ ]
// ────────────────────────────────────────────────────────────────
class MasterFader : public juce::Component, private juce::Timer {
public:
  MasterFader();
  ~MasterFader() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &e) override;

  /// dB 値変化時のコールバックを登録する
  void setOnValueChange(std::function<void(float /*dB*/)> cb);

  /// L/Rレベルメーター用プロバイダーを登録する (0=L, 1=R)
  void setLevelProvider(int ch, std::function<float()> provider);

  /// 現在値を dB で取得
  float getValueDb() const { return static_cast<float>(fader.getValue()); }

  /// 値を dB でセット（通知なし）
  void setValueDb(float db) { fader.setValue(db, juce::dontSendNotification); }

  // ── dB スケール定数 ──
  static constexpr float minDb = -60.0f;
  static constexpr float maxDb = 12.0f;
  static constexpr int labelHeight = 16;

private:
  void timerCallback() override;

  std::function<void(float)> onValueChange;
  std::array<std::function<float()>, 2> levelProvider;

  juce::Label label;
  juce::Slider fader;

  // ── L/R メーター状態 ──
  struct MeterState {
    float displayDb{-100.0f};
    float peakDb{minDb};
    int peakHoldFrames{0};
    float peakFallVelocity{0.0f};
  };
  std::array<MeterState, 2> meter;

  static constexpr int timerHz = 30;
  static constexpr int peakHoldFrames_ = timerHz * 1;
  static constexpr float peakFallPerFrame = (maxDb - minDb) / (timerHz * 0.5f);
};
