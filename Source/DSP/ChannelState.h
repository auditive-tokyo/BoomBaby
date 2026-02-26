#pragma once

#include "LevelDetector.h"
#include <array>
#include <atomic>

/// チャンネル単位の Mute / Solo / レベル計測 を一元管理するヘルパー。
/// BabySquatchAudioProcessor から分離し、メソッド数を削減する。
class ChannelState {
public:
    enum class Channel { sub = 0, click = 1, direct = 2 };

    /// Mute / Solo 状態に基づく各チャンネルの通過判定。
    struct Passes {
        bool sub;
        bool direct;
    };

    // ── UI スレッドから呼び出す setter ──

    void setMute(Channel ch, bool muted) {
        mute_[static_cast<size_t>(ch)].store(muted);
    }

    void setSolo(Channel ch, bool soloed) {
        solo_[static_cast<size_t>(ch)].store(soloed);
    }

    // ── UI スレッドから呼び出す getter ──

    [[nodiscard]] float getChannelLevelDb(Channel ch) const noexcept {
        return detectors_[static_cast<size_t>(ch)].getPeakDb();
    }

    // ── オーディオスレッドから呼び出す ──

    /// 現在の mute/solo 状態から各チャンネルの通過判定を算出。
    [[nodiscard]] Passes computePasses() const {
        using enum Channel;
        const bool anySolo = solo_[static_cast<size_t>(sub)].load()
                          || solo_[static_cast<size_t>(click)].load()
                          || solo_[static_cast<size_t>(direct)].load();
        const auto isMuted  = [&](Channel ch) { return mute_[static_cast<size_t>(ch)].load(); };
        const auto isSoloed = [&](Channel ch) { return solo_[static_cast<size_t>(ch)].load(); };
        return {
            !isMuted(sub) && (!anySolo || isSoloed(sub)),
            !isMuted(direct)   && (!anySolo || isSoloed(direct))
        };
    }

    /// LevelDetector への直接アクセス（オーディオスレッドから process() 呼び出し用）。
    LevelDetector& detector(Channel ch) noexcept {
        return detectors_[static_cast<size_t>(ch)];
    }

    /// prepareToPlay() で呼び出す。
    void resetDetectors() noexcept {
        for (auto &d : detectors_)
            d.reset();
    }

private:
    std::array<std::atomic<bool>, 3> mute_{};
    std::array<std::atomic<bool>, 3> solo_{};
    std::array<LevelDetector, 3> detectors_;
};
