#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../DSP/OomphOscillator.h" // WaveShape enum

class EnvelopeData;

/// AMP / Pitch Envelope カーブエディタ
///
/// 波形プレビュー: sin(位相累積(pitchHz)) × ampEnv(t) を描画
/// オーバーレイ: 編集中のエンベロープ（AMP or Pitch）のカーブ＋制御点
class EnvelopeCurveEditor : public juce::Component {
public:
  EnvelopeCurveEditor(EnvelopeData &ampData, EnvelopeData &pitchData,
                      EnvelopeData &distData, EnvelopeData &blendData);

  void paint(juce::Graphics &g) override;

  /// 編集対象のエンベロープを切り替え（AMP / PITCH / DIST / BLEND）
  enum class EditTarget { amp, pitch, dist, blend };
  void setEditTarget(EditTarget target);
  EditTarget getEditTarget() const { return editTarget; }

  /// 波形プレビュー用: 選択波形を設定（Sine/Tri/Square/Saw）
  void setWaveShape(WaveShape shape);

  /// 波形プレビュー用: BLEND 値を設定（-1.0〜+1.0）
  void setPreviewBlend(float blend);

  /// 波形プレビュー用: H1〜H4 倍音ゲインを設定（harmonicNum: 1〜4）
  void setPreviewHarmonicGain(int harmonicNum, float gain);

  /// 表示するサイン波のサイクル数を設定
  void setDisplayCycles(float cycles);

  /// エンベロープの表示期間（ms）を設定
  void setDisplayDurationMs(float ms);
  float getDisplayDurationMs() const { return displayDurationMs; }

  /// ポイント変更時コールバック（LUT ベイク等に使用）
  void setOnChange(std::function<void()> cb);

  /// 編集対象が切り替わった時のコールバック（外部UI同期用）
  void setOnEditTargetChanged(std::function<void(EditTarget)> cb);

  /// Length ボックス変更時コールバック（全 LUT 再ベイク用）

  // ── マウス操作（Phase 2） ──
  void mouseDoubleClick(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

private:
  // ── 座標変換ヘルパー ──
  float timeMsToX(float timeMs) const;
  float valueToY(float value) const;
  float xToTimeMs(float x) const;
  float yToValue(float y) const;

  /// 現在の編集対象に応じた値の下限・上限
  float editMinValue() const;
  float editMaxValue() const;

  /// ピクセル空間でのヒット判定（-1: なし）
  int findPointAtPixel(float px, float py) const;

  // ── paint() 分割ヘルパー ──
  void paintWaveform(juce::Graphics &g, float w, float h, float centreY) const;
  void paintEnvelopeOverlay(juce::Graphics &g, float w) const;
  void paintTimeline(juce::Graphics &g, float w, float h, float totalH) const;
  void paintTabs(juce::Graphics &g) const;

  /// タブ矩形（右上の AMP / PITCH / DIST / BLEND ボタン）
  juce::Rectangle<float> tabRect(EditTarget target) const;
  static constexpr float tabW = 38.0f;
  static constexpr float tabH = 18.0f;
  static constexpr float tabPad = 4.0f;

  static constexpr float pointHitRadius = 8.0f;
  static constexpr float timelineHeight = 18.0f;

  /// プロット領域の高さ（タイムライン分を差し引いた値）
  float plotHeight() const;

  /// 位相から波形サンプル値を返す（プレビュー用算術式）
  static float shapeOscValue(WaveShape shape, float phase);

  /// BLEND + 波形選択に応じた1サンプルを返す（paintWaveform から委譲）
  float computePreviewWaveValue(float sinVal, float blend, float phase) const;

  EnvelopeData &ampEnvData;
  EnvelopeData &pitchEnvData;
  EnvelopeData &distEnvData;
  EnvelopeData &blendEnvData;
  EnvelopeData *editEnvData; // 編集中のエンベロープ（amp / pitch / dist / blend）
  EditTarget editTarget = EditTarget::amp;
  float displayDurationMs = 300.0f;
  float displayCycles = 4.0f;
  WaveShape previewShape = WaveShape::Sine;
  float previewBlend = 0.0f; // -1.0〜+1.0
  std::array<float, 4> previewHarmonicGains = {0.0f, 0.0f, 0.0f, 0.0f};
  int dragPointIndex{-1};
  std::function<void()> onChange;
  std::function<void(EditTarget)> onEditTargetChanged;


  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeCurveEditor)
};
