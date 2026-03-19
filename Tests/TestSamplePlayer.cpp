#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DSP/SamplePlayer.h"
#include <cmath>
#include <memory>
#include <vector>

using Catch::Matchers::WithinAbs;

constexpr double kSampleRate = 44100.0;
constexpr int kTestLen = 256;

/// prepare 済みの SamplePlayer を返すヘルパー
static std::unique_ptr<SamplePlayer> makePrepared() {
  auto sp = std::make_unique<SamplePlayer>();
  sp->prepare();
  return sp;
}

/// テスト用 WAV ファイルを一時ディレクトリに書き出すヘルパー。
/// numSamples サンプル・1ch のサイン波（440Hz）を生成。
static juce::File writeTestWav(int numSamples, double sr = kSampleRate) {
  juce::AudioBuffer<float> buf(1, numSamples);
  for (int i = 0; i < numSamples; ++i)
    buf.setSample(0, i,
                  static_cast<float>(
                      std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 *
                               static_cast<double>(i) / sr)));

  auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
  auto file = dir.getChildFile("boombaby_test_sample.wav");

  // WAV 16bit で書き出し
  juce::WavAudioFormat wav;
  std::unique_ptr<juce::AudioFormatWriter> writer(
      wav.createWriterFor(new juce::FileOutputStream(file), sr, 1, 16, {}, 0)); // NOSONAR
  REQUIRE(writer != nullptr);
  writer->writeFromAudioSampleBuffer(buf, 0, numSamples);
  writer.reset(); // flush

  return file;
}

/// テスト用ステレオ WAV を書き出すヘルパー（モノ化テスト用）
static juce::File writeStereoTestWav(int numSamples, double sr = kSampleRate) {
  juce::AudioBuffer<float> buf(2, numSamples);
  for (int i = 0; i < numSamples; ++i) {
    const auto t = static_cast<float>(i) / static_cast<float>(numSamples);
    buf.setSample(0, i, t);  // L: ramp 0→1
    buf.setSample(1, i, -t); // R: ramp 0→-1
  }

  auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
  auto file = dir.getChildFile("boombaby_test_stereo.wav");

  juce::WavAudioFormat wav;
  std::unique_ptr<juce::AudioFormatWriter> writer(
      wav.createWriterFor(new juce::FileOutputStream(file), sr, 2, 16, {}, 0)); // NOSONAR
  REQUIRE(writer != nullptr);
  writer->writeFromAudioSampleBuffer(buf, 0, numSamples);
  writer.reset();

  return file;
}

// ─── 初期状態 ──────────────────────────────────────────────────

// prepare 後、サンプル未ロード状態では isLoaded() == false であることを確認する
TEST_CASE("SamplePlayer: initial state is not loaded", "[sample_player]") {
  auto sp = makePrepared();
  REQUIRE_FALSE(sp->isLoaded());
  REQUIRE(sp->durationSec() == 0.0);
}

// ─── loadSample / unloadSample ─────────────────────────────────

// 存在しないファイルを loadSample
// しても安全に失敗（クラッシュしない）することを確認する
TEST_CASE("SamplePlayer: loadSample with non-existent file does not crash",
          "[sample_player]") {
  auto sp = makePrepared();
  auto bogusFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                     .getChildFile("does_not_exist_12345.wav");
  sp->loadSample(bogusFile);
  REQUIRE_FALSE(sp->isLoaded());
}

// WAV ファイルをロード後、isLoaded()==true かつ durationSec
// が正値になることを確認する
TEST_CASE("SamplePlayer: loadSample loads valid WAV", "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  REQUIRE(sp->isLoaded());
  REQUIRE(sp->durationSec() > 0.0);
  REQUIRE_THAT(sp->sampleRate(), WithinAbs(kSampleRate, 1.0));

  file.deleteFile();
}

// unloadSample 後に isLoaded()==false に戻ることを確認する
TEST_CASE("SamplePlayer: unloadSample resets state", "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);
  REQUIRE(sp->isLoaded());

  sp->unloadSample();
  REQUIRE_FALSE(sp->isLoaded());
  REQUIRE(sp->durationSec() == 0.0);

  file.deleteFile();
}

// ─── ステレオ → モノ化 ──────────────────────────────────────────

// ステレオ WAV をロードした場合、内部バッファがモノ（L+R
// 平均）になることを確認する
TEST_CASE("SamplePlayer: stereo file is downmixed to mono", "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeStereoTestWav(kTestLen);
  sp->loadSample(file);
  REQUIRE(sp->isLoaded());

  auto view = sp->lock();
  REQUIRE(static_cast<bool>(view));
  // L=ramp, R=-ramp → 平均は 0 に近い（16bit 量子化誤差あり）
  float maxAbs = 0.0f;
  for (int i = 0; i < view.length; ++i)
    maxAbs = std::max(maxAbs, std::abs(view.data[i]));
  // 16bit 量子化誤差を考慮して0に近いことを確認
  REQUIRE(maxAbs < 0.01f);

  file.deleteFile();
}

// ─── copyThumbnail ──────────────────────────────────────────────

// 未ロード時 copyThumbnail が false を返すことを確認する
TEST_CASE("SamplePlayer: copyThumbnail returns false when not loaded",
          "[sample_player]") {
  auto sp = makePrepared();
  std::vector<float> mn;
  std::vector<float> mx;
  REQUIRE_FALSE(sp->copyThumbnail(mn, mx));
}

// ロード後 copyThumbnail が 512 ビンのサムネイルを返すことを確認する
TEST_CASE("SamplePlayer: copyThumbnail returns 512-bin data after load",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(4096);
  sp->loadSample(file);

  std::vector<float> mn;
  std::vector<float> mx;
  REQUIRE(sp->copyThumbnail(mn, mx));
  REQUIRE(mn.size() == 512);
  REQUIRE(mx.size() == 512);

  // min <= max が全ビンで成り立つ
  for (std::size_t i = 0; i < mn.size(); ++i)
    REQUIRE(mn[i] <= mx[i]);

  file.deleteFile();
}

// ─── readInterpolated / readNext ────────────────────────────────

// playRate=1.0 で先頭から読むと finished==false
// のサンプルが得られることを確認する
TEST_CASE("SamplePlayer: readNext returns samples at playRate 1.0",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  sp->resetPlayhead();
  bool finished = false;
  float first = sp->readNext(1.0, finished);
  // 440Hz サイン波の先頭サンプルは sin(0) == 0
  REQUIRE_THAT(first, WithinAbs(0.0f, 0.01f));
  REQUIRE_FALSE(finished);

  file.deleteFile();
}

// 全サンプルを読み終えると finished==true になることを確認する
TEST_CASE("SamplePlayer: readNext sets finished at end of buffer",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  sp->resetPlayhead();
  bool finished = false;
  for (int i = 0; i < kTestLen + 10; ++i) {
    sp->readNext(1.0, finished);
    if (finished)
      break;
  }
  REQUIRE(finished);

  file.deleteFile();
}

// 未ロード状態で readNext を呼ぶと即 finished==true かつ 0.0f
// を返すことを確認する
TEST_CASE("SamplePlayer: readNext on unloaded returns 0 and finished",
          "[sample_player]") {
  auto sp = makePrepared();
  bool finished = false;
  float val = sp->readNext(1.0, finished);
  REQUIRE(finished);
  REQUIRE(val == 0.0f);
}

// ─── resetPlayhead ──────────────────────────────────────────────

// resetPlayhead 後に再び先頭から読めることを確認する
TEST_CASE("SamplePlayer: resetPlayhead allows re-read from start",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  // 途中まで進める
  sp->resetPlayhead();
  bool finished = false;
  for (int i = 0; i < kTestLen / 2; ++i)
    sp->readNext(1.0, finished);

  // リセットして再読
  sp->resetPlayhead();
  finished = false;
  float first = sp->readNext(1.0, finished);
  REQUIRE_THAT(first, WithinAbs(0.0f, 0.01f));
  REQUIRE_FALSE(finished);

  file.deleteFile();
}

// ─── readInterpolated 補間 ──────────────────────────────────────

// playRate=0.5 のとき実サンプル間を線形補間していることを確認する
TEST_CASE("SamplePlayer: readInterpolated does linear interpolation",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  sp->resetPlayhead();
  auto view = sp->lock();
  REQUIRE(static_cast<bool>(view));

  // playRate=0.5 で 2 サンプル読む → s[0] と lerp(s[0], s[1], 0.5)
  bool fin = false;
  float v0 = sp->readInterpolated(view.data, view.length, 0.5, fin);
  REQUIRE_FALSE(fin);
  REQUIRE_THAT(v0, WithinAbs(view.data[0], 1e-5f)); // first call returns data[0]
  float v1 = sp->readInterpolated(view.data, view.length, 0.5, fin);
  REQUIRE_FALSE(fin);

  // second call returns the midpoint interpolation between the first two samples
  float expected = view.data[0] * 0.5f + view.data[1] * 0.5f;
  REQUIRE_THAT(v1, WithinAbs(expected, 1e-5f));

  file.deleteFile();
}

// ─── lock() ─────────────────────────────────────────────────────

// 未ロード時に lock() が無効な LockedView を返すことを確認する
TEST_CASE("SamplePlayer: lock returns invalid view when not loaded",
          "[sample_player]") {
  auto sp = makePrepared();
  auto view = sp->lock();
  REQUIRE_FALSE(static_cast<bool>(view));
  REQUIRE(view.data == nullptr);
  REQUIRE(view.length == 0);
}

// ロード後に lock() が有効なビューを返し、長さが一致することを確認する
TEST_CASE("SamplePlayer: lock returns valid view after load",
          "[sample_player]") {
  auto sp = makePrepared();
  auto file = writeTestWav(kTestLen);
  sp->loadSample(file);

  auto view = sp->lock();
  REQUIRE(static_cast<bool>(view));
  REQUIRE(view.data != nullptr);
  REQUIRE(view.length == kTestLen);

  file.deleteFile();
}

// ─── 30 秒制限 ─────────────────────────────────────────────────

// 30 秒を超えるサンプルをロードした場合 30 秒に切り詰められることを確認する
TEST_CASE("SamplePlayer: samples longer than 30s are truncated",
          "[sample_player]") {
  auto sp = makePrepared();
  const auto longLen = static_cast<int>(kSampleRate * 35.0); // 35 秒
  auto file = writeTestWav(longLen);
  sp->loadSample(file);
  REQUIRE(sp->isLoaded());

  auto view = sp->lock();
  REQUIRE(static_cast<bool>(view));
  const auto expected30s = static_cast<int>(kSampleRate * 30.0);
  REQUIRE(view.length == expected30s);
  REQUIRE_THAT(sp->durationSec(), WithinAbs(30.0, 0.1));

  file.deleteFile();
}
