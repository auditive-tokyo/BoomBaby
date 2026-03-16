#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/Saturator.h"

using namespace Catch::Matchers;

// ─── driveGainFromDb ────────────────────────────────────────

// driveGainFromDb(0dB) が線形ゲイン 1.0（ユニティゲイン）を返すことを確認する
TEST_CASE("Saturator: driveGainFromDb(0) returns unity gain", "[saturator]")
{
    REQUIRE_THAT(Saturator::driveGainFromDb(0.0f), WithinAbs(1.0f, 1e-6f));
}

// driveGainFromDb(20dB) が線形ゲイン 10.0（20dB = 10倍）を返すことを確認する
TEST_CASE("Saturator: driveGainFromDb(20) returns 10x gain", "[saturator]")
{
    REQUIRE_THAT(Saturator::driveGainFromDb(20.0f), WithinAbs(10.0f, 1e-4f));
}

// ─── ゼロ入力 ───────────────────────────────────────────────

// Soft モードで入力ゼロのとき出力もゼロになることを確認する
TEST_CASE("Saturator: Soft mode zero input produces zero output", "[saturator]")
{
    REQUIRE_THAT(Saturator::process(0.0f, 0.0f, static_cast<int>(Saturator::ClipType::Soft)),
                 WithinAbs(0.0f, 1e-6f));
}

// Hard モードで入力ゼロのとき出力もゼロになることを確認する
TEST_CASE("Saturator: Hard mode zero input produces zero output", "[saturator]")
{
    REQUIRE_THAT(Saturator::process(0.0f, 0.0f, static_cast<int>(Saturator::ClipType::Hard)),
                 WithinAbs(0.0f, 1e-6f));
}

// Tube モードは設計上 DC バイアス (≈0.0909) を持つことを確認する
// ※ DirectEngine 側の早期リターンでリークを防止している（回帰テスト）
TEST_CASE("Saturator: Tube mode zero input produces expected bias (DC offset by design)", "[saturator]")
{
    // Tube: (0 + 0.1) / (1 + |0 + 0.1|) = 0.1 / 1.1 ≈ 0.09090...
    constexpr float bias     = 0.1f;
    constexpr float expected = bias / (1.0f + bias); // ≈ 0.0909f
    REQUIRE_THAT(Saturator::process(0.0f, 0.0f, static_cast<int>(Saturator::ClipType::Tube)),
                 WithinAbs(expected, 1e-6f));
}

// ─── Hard クリップ境界 ──────────────────────────────────────

// 大振幅入力（±100）に対して Hard クリップの出力が [-1, 1] に収まることを確認する
TEST_CASE("Saturator: Hard clip is bounded to [-1, 1] for large inputs", "[saturator]")
{
    REQUIRE(Saturator::process( 100.0f, 24.0f, static_cast<int>(Saturator::ClipType::Hard)) <= 1.0f);
    REQUIRE(Saturator::process(-100.0f, 24.0f, static_cast<int>(Saturator::ClipType::Hard)) >= -1.0f);
}

// ─── 単調性 ─────────────────────────────────────────────────

// 全 ClipType で入力が増加するにつれ出力も単調非減少になることを確認する（逆位相・符号反転バグの検出）
TEST_CASE("Saturator: each ClipType is monotonically non-decreasing", "[saturator]")
{
    for (int type = 0; type <= 2; ++type)
    {
        const float y1 = Saturator::process(-0.5f, 0.0f, type);
        const float y2 = Saturator::process( 0.0f, 0.0f, type);
        const float y3 = Saturator::process( 0.5f, 0.0f, type);
        REQUIRE(y1 <= y2);
        REQUIRE(y2 <= y3);
    }
}
