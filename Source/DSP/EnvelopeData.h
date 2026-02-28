#pragma once

#include <algorithm>
#include <limits>
#include <vector>

/// エンベロープ制御点（Phase 2）
struct EnvelopePoint {
    float timeMs{0.0f};  ///< 0〜displayDurationMs
    float value{1.0f};   ///< 線形ゲイン 0.0〜2.0
};

/// Amplitude Envelope データモデル
///
/// - defaultValue: 線形ゲイン 0.0〜2.0（ポイントなし時に使用）
/// - points が空 → defaultValue（フラット）
/// - points が 1 → 定数
/// - points が 2 → 線形補間
/// - points が 3+ → Catmull-Rom スプライン補間
class EnvelopeData {
public:
    // ── デフォルト値（ポイントなし時のフラット値） ──

    float getValue() const { return defaultValue; }
    void setDefaultValue(float v) { defaultValue = v; }
    float getDefaultValue() const { return defaultValue; }

    // ── ポイント操作 ──

    bool hasPoints() const { return !points.empty(); }
    /// ポイントが 2 個以上 → エンベロープで制御
    /// ポイントが 1 個    → ノブで制御（点の value == knob value）
    bool isEnvelopeControlled() const { return points.size() > 1; }
    const std::vector<EnvelopePoint>& getPoints() const { return points; }

    /// 指定インデックスのポイント値のみを変更（時刻は保持）
    void setPointValue(int index, float value) {
        if (index >= 0 && index < static_cast<int>(points.size()))
            points[static_cast<size_t>(index)].value = value;
    }

    /// timeMs 昇順に挿入
    void addPoint(float timeMs, float value) {
        EnvelopePoint p{timeMs, value};
        auto it = std::ranges::lower_bound(
            points, p,
            [](const EnvelopePoint& a, const EnvelopePoint& b) {
                return a.timeMs < b.timeMs;
            });
        points.insert(it, p);
    }

    void removePoint(int index) {
        if (index >= 0 && index < static_cast<int>(points.size()))
            points.erase(points.begin() + index);
    }

    /// ポイントを移動。newTimeMs は隣接ポイント間にクランプされる。
    /// 戻り値: 移動後のインデックス（隣接クランプ済みのためソート不要、通常は入力と同じ）
    int movePoint(int index, float newTimeMs, float newValue) {
        if (index < 0 || index >= static_cast<int>(points.size()))
            return index;

        // 隣接ポイント間にクランプ（ソート不要を保証）
        const float minT = (index > 0)
            ? points[static_cast<size_t>(index - 1)].timeMs
            : 0.0f;
        const float maxT = (index < static_cast<int>(points.size()) - 1)
            ? points[static_cast<size_t>(index + 1)].timeMs
            : std::numeric_limits<float>::max();

        points[static_cast<size_t>(index)].timeMs = std::clamp(newTimeMs, minT, maxT);
        points[static_cast<size_t>(index)].value = newValue;  // clamp は呼び出し側で実施
        return index;
    }

    // ── 補間評価 ──

    /// timeMs における envelope 値を返す
    float evaluate(float timeMs) const {
        const auto n = static_cast<int>(points.size());
        if (n == 0)
            return defaultValue;
        if (n == 1)
            return points[0].value;

        // 範囲外 → 最端値でサスティン
        if (timeMs <= points[0].timeMs)
            return points[0].value;
        if (timeMs >= points[static_cast<size_t>(n - 1)].timeMs)
            return points[static_cast<size_t>(n - 1)].value;

        // timeMs が属するセグメント [i, i+1] を探す
        int seg = 0;
        for (int i = 0; i < n - 1; ++i) {
            if (timeMs < points[static_cast<size_t>(i + 1)].timeMs) {
                seg = i;
                break;
            }
        }

        const float t0 = points[static_cast<size_t>(seg)].timeMs;
        const float t1 = points[static_cast<size_t>(seg + 1)].timeMs;
        const float t = (t1 > t0) ? (timeMs - t0) / (t1 - t0) : 0.0f;

        if (n == 2) {
            // 線形補間
            return points[0].value + t * (points[1].value - points[0].value);
        }

        // Catmull-Rom スプライン補間（端点はファントムポイント複製）
        const float p0 = points[static_cast<size_t>(std::max(seg - 1, 0))].value;
        const float p1 = points[static_cast<size_t>(seg)].value;
        const float p2 = points[static_cast<size_t>(seg + 1)].value;
        const float p3 = points[static_cast<size_t>(std::min(seg + 2, n - 1))].value;

        return catmullRom(p0, p1, p2, p3, t);
    }

private:
    float defaultValue{1.0f};
    std::vector<EnvelopePoint> points;

    /// Catmull-Rom 補間（t ∈ [0,1]）
    static float catmullRom(float p0, float p1, float p2, float p3, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1)
            + (-p0 + p2) * t
            + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
            + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }
};
