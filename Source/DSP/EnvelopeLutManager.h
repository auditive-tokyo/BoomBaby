#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>

/// エンベロープ LUT のダブルバッファ管理。
/// UI スレッドが bake() で書き込み → アトミックフリップ、
/// オーディオスレッドが getActiveLut() で読み出す。
class EnvelopeLutManager {
public:
    static constexpr int lutSize = 512;

    /// UIスレッドから呼び出し: 非アクティブ側バッファにコピー → アトミックフリップ。
    void bake(const float *data, int size) {
        const int activeIdx = activeIndex_.load(std::memory_order_acquire);
        auto &dst = (activeIdx == 0) ? lut1_ : lut0_;

        // ソースを LUT サイズにリサンプル（線形補間）
        for (int i = 0; i < lutSize; ++i) {
            const float srcPos = static_cast<float>(i)
                               / static_cast<float>(lutSize - 1)
                               * static_cast<float>(size - 1);
            const auto idx0  = static_cast<int>(srcPos);
            const auto idx1  = std::min(idx0 + 1, size - 1);
            const float frac = srcPos - static_cast<float>(idx0);
            dst[static_cast<size_t>(i)] =
                data[idx0] * (1.0f - frac)       // NOLINT: pointer arithmetic
              + data[idx1] * frac;
        }

        // フリップ
        activeIndex_.store(1 - activeIdx, std::memory_order_release);
    }

    void setDurationMs(float ms) { durationMs_.store(ms); }

    [[nodiscard]] float getDurationMs() const { return durationMs_.load(); }

    /// オーディオスレッドから呼び出し: 現在アクティブな LUT を取得。
    [[nodiscard]] const std::array<float, lutSize>& getActiveLut() const noexcept {
        const int idx = activeIndex_.load(std::memory_order_acquire);
        return (idx == 0) ? lut0_ : lut1_;
    }

    /// prepareToPlay() で呼び出す。
    void reset() noexcept {
        lut0_.fill(1.0f);
        lut1_.fill(1.0f);
        activeIndex_.store(0);
    }

    /// LUT エンベロープ振幅を計算（末尾 5ms half-cosine フェード付き）。
    /// ClickEngine / DirectEngine の computeSampleAmp() から共用。
    [[nodiscard]] static float computeAmp(
        const std::array<float, lutSize> &ampLut,
        float ampDurMs,
        float noteTimeMs) noexcept {
        const float lutPos =
            (ampDurMs > 0.0f)
                ? (noteTimeMs / ampDurMs) * static_cast<float>(lutSize - 1)
                : 0.0f;
        const auto lutIdx = std::min(static_cast<int>(lutPos), lutSize - 1);
        float amp = ampLut[static_cast<std::size_t>(lutIdx)];

        constexpr float fadeOutMs = 5.0f;
        if (const float fadeStartMs = std::max(0.0f, ampDurMs - fadeOutMs);
            noteTimeMs > fadeStartMs) {
            const float t = (noteTimeMs - fadeStartMs) / fadeOutMs;
            amp *= 0.5f * (1.0f + std::cos(t * std::numbers::pi_v<float>));
        }
        return amp;
    }

private:
    std::array<float, lutSize> lut0_{};
    std::array<float, lutSize> lut1_{};
    std::atomic<int> activeIndex_{0};
    std::atomic<float> durationMs_{300.0f};
};
