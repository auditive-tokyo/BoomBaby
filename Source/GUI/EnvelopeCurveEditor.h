#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../DSP/SubOscillator.h" // WaveShape enum

class EnvelopeData;

/// Gain / Freq Envelope カーブエディタ
///
/// 波形プレビュー: sin(位相累積(freqHz)) × gainEnv(t) を描画
/// オーバーレイ: 編集中のエンベロープ（Gain / Freq / Saturate /
/// Mix）のカーブ＋制御点
class EnvelopeCurveEditor : public juce::Component {
public:
  EnvelopeCurveEditor(EnvelopeData &ampData, EnvelopeData &freqData,
                      EnvelopeData &distData, EnvelopeData &mixData,
                      EnvelopeData &clickAmpData, EnvelopeData &directAmpData);

  void paint(juce::Graphics &g) override;

  /// 編集対象のエンベロープを切り替え（Amp / Freq / Saturate / Mix / Click
  /// Amp）
  enum class EditTarget { amp, freq, saturate, mix, clickAmp, directAmp };
  void setEditTarget(EditTarget target);

  /// 波形プレビュー用: 選択波形を設定（Sine/Tri/Square/Saw）
  void setWaveShape(WaveShape shape);

  /// 波形プレビュー用: Mix 値を設定（-1.0〜+1.0）
  void setPreviewMix(float mix);

  /// Direct チャンネル波形オーバーレイ用プロバイダーを設定。
  /// fn(timeSec) → {min, max}（-1、1）の波形値を返すラムダ。
  /// nullptr を渡すとオーバーレイを無効化。
  void setDirectProvider(std::function<std::pair<float, float>(float)> fn);

  /// リアルタイム入力波形データを設定（Direct パススルーモード用、UI
  /// スレッドから呼ぶ） pixels: コンポーネント幅分の {min, max} ペア配列。
  void setRealtimePixels(std::vector<std::pair<float, float>> pixels);
  /// リアルタイム入力波形表示モード切り替え（true: 入力波形 / false:
  /// サンプルプレビュー）
  void setUseRealtimeInput(bool use) noexcept;

  /// Click 波形オーバーレイ用プロバイダーを設定。
  /// fn(timeSec) → {min, max}（-1〜1）の波形値を返すラムダ。
  /// nullptr を渡すとオーバーレイを無効化。
  void
  setClickPreviewProvider(std::function<std::pair<float, float>(float)> fn);

  /// Click Sample Decay 期間を設定（5msフェードアウトの基準）
  void setClickDecayMs(float ms);

  /// Noise モード用: fn(timeSec) → 振幅 0〜1 を返す。±env の帯として描画。
  void setClickNoiseEnvProvider(std::function<float(float)> fn);

  /// 波形プレビュー用: Tone1〜Tone4 倍音ゲインを設定（harmonicNum: 1〜4）
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

  // ── マウス操作 ──
  void mouseDoubleClick(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  static constexpr float pointHitRadius = 8.0f;
  static constexpr float timelineHeight = 30.0f;

private:
  // ── 座標変換ヘルパー群（nested struct: outer class の private に直接アクセス可能） ──
  struct CoordMapper {
    float durationMs;
    float w;
    float plotH;        ///< getHeight() - timelineHeight
    EditTarget editTarget;
    float timeMsToX(float timeMs) const noexcept;
    float valueToY(float value) const noexcept;
    float xToTimeMs(float x) const noexcept;
    float yToValue(float y) const noexcept;
  };
  CoordMapper makeCoords() const noexcept;

  // ── ヒットテスト群 ──
  struct HitTester {
    /// ピクセル空間でのポイントヒット判定（-1: なし）
    static int findPoint(const EnvelopeCurveEditor &e,
                         const CoordMapper &c, float px, float py);
    /// Shift+クリック用: 最近傍セグメント（-1: なし）
    static int findSegment(const EnvelopeCurveEditor &e,
                           const CoordMapper &c, float px, float py);
  };

  // ── paint() 分割ヘルパー群 ──
  struct PaintHelper {
    static void waveform(const EnvelopeCurveEditor &e, juce::Graphics &g,
                         const CoordMapper &c, float centreY);
    static void clickNoiseBand(const EnvelopeCurveEditor &e, juce::Graphics &g,
                               const CoordMapper &c, float centreY);
    static void clickSampleWave(const EnvelopeCurveEditor &e, juce::Graphics &g,
                                const CoordMapper &c, float centreY);
    static void directWaveform(const EnvelopeCurveEditor &e, juce::Graphics &g,
                               const CoordMapper &c, float centreY);
    static void envelopeOverlay(const EnvelopeCurveEditor &e, juce::Graphics &g,
                                const CoordMapper &c);
    static void timeline(juce::Graphics &g,
                         const CoordMapper &c, float totalH);
    /// Mix + 波形選択に応じた1サンプルを返す
    static float previewWaveValue(const EnvelopeCurveEditor &e,
                                  float sinVal, float mix, float phase);
  };

  // [0]=amp, [1]=freq, [2]=dist, [3]=mix, [4]=clickAmp
  std::array<EnvelopeData *, 6> envDatas_;
  EnvelopeData *editEnvData; // 編集中のエンベロープ

  EditTarget editTarget = EditTarget::amp;
  float displayDurationMs = 300.0f;
  float displayCycles = 4.0f;
  WaveShape previewShape = WaveShape::Sine;
  float previewMix = 0.0f;
  std::array<float, 4> previewHarmonicGains = {0.0f, 0.0f, 0.0f, 0.0f};

  struct DragState {
    int pointIndex{-1};
    int curveSegment{-1};
    float startY{0.0f};
    float startVal{0.0f};
  };
  DragState drag_;

  std::function<void()> onChange;
  std::function<void(EditTarget)> onEditTargetChanged;
  std::function<std::pair<float, float>(float)> clickPreviewFn_;
  float clickDecayMs_ = 300.0f;
  std::function<float(float)> clickNoiseEnvFn_;
  std::function<std::pair<float, float>(float)> directPreviewFn_;
  std::vector<std::pair<float, float>>
      realtimePixels_; ///< リアルタイム per-pixel {min,max}
  bool useRealtimeInput_ = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeCurveEditor)
};
