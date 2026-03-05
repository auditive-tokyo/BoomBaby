#include "ClickEngine.h"
#include <cmath>

void ClickEngine::prepareToPlay(double /*sampleRate*/, int samplesPerBlock) {
  // ── BPF / HPF / LPF 初期化 ──
  using enum juce::dsp::StateVariableTPTFilterType;
  bpf1_.reset();
  bpf1_.setType(bandpass);
  bpf2_.reset();
  bpf2_.setType(bandpass);
  for (auto &f : hpfs_) {
    f.reset();
    f.setType(highpass);
  }
  for (auto &f : lpfs_) {
    f.reset();
    f.setType(lowpass);
  }

  scratchBuffer_.resize(static_cast<size_t>(samplesPerBlock));
  noteTimeSamples_ = 0.0f;
  active_.store(false);
  sampler_.prepare();
}

void ClickEngine::triggerNote() {
  active_.store(true);
  bpf1_.reset();
  bpf2_.reset();
  for (auto &f : hpfs_)
    f.reset();
  for (auto &f : lpfs_)
    f.reset();
  noteTimeSamples_ = 0.0f;
  sampler_.resetPlayhead();
}

auto ClickEngine::setupFilters(float sr) -> FilterFlags {
  const float f1 = juce::jlimit(20.0f, sr * 0.49f, freq1_.load());
  const float q1 = focus1_.load();
  const float f2 = juce::jlimit(20.0f, sr * 0.49f, freq2_.load());
  const float q2 = focus2_.load();
  const float hpfF = juce::jlimit(20.0f, sr * 0.49f, hpfParams_.freq.load());
  const float hpfQv = hpfParams_.q.load();
  const float lpfF = juce::jlimit(20.0f, sr * 0.49f, lpfParams_.freq.load());
  const float lpfQv = lpfParams_.q.load();
  const int hpfStg = hpfParams_.stages.load();
  const int lpfStg = lpfParams_.stages.load();

  const FilterFlags flags{q1 > 0.001f,    q2 > 0.001f, hpfQv > 0.001f,
                          lpfQv > 0.001f, hpfStg,      lpfStg};
  if (flags.bpf1) {
    bpf1_.setCutoffFrequency(f1);
    bpf1_.setResonance(juce::jlimit(0.01f, 30.0f, q1));
  }
  if (flags.bpf2) {
    bpf2_.setCutoffFrequency(f2);
    bpf2_.setResonance(juce::jlimit(0.01f, 30.0f, q2));
  }
  if (flags.hpf) {
    const float qH = juce::jlimit(0.01f, 30.0f, hpfQv);
    for (int i = 0; i < hpfStg; ++i) {
      hpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(hpfF);
      hpfs_[static_cast<std::size_t>(i)].setResonance(qH);
    }
  }
  if (flags.lpf) {
    const float qL = juce::jlimit(0.01f, 30.0f, lpfQv);
    for (int i = 0; i < lpfStg; ++i) {
      lpfs_[static_cast<std::size_t>(i)].setCutoffFrequency(lpfF);
      lpfs_[static_cast<std::size_t>(i)].setResonance(qL);
    }
  }
  return flags;
}

float ClickEngine::synthesizeSample(int mode, const FilterFlags &flags) {
  float s = 0.0f;
  if (mode == 1) {
    // ── Tone: インパルス → BPF リング ──
    s = (noteTimeSamples_ < 1.0f) ? 1.0f : 0.0f;
    if (flags.bpf1)
      s = bpf1_.processSample(0, s);
    if (flags.bpf2)
      s = bpf2_.processSample(0, s);
  } else if (mode == 2) {
    // ── Noise: ホワイトノイズ → BPF 励起 ──
    s = random_.nextFloat() * 2.0f - 1.0f;
    if (flags.bpf1)
      s = bpf1_.processSample(0, s);
    if (flags.bpf2)
      s = bpf2_.processSample(0, s);
  } else if (mode == 3) {
    // ── Sample: SamplePlayer から読み出し ──
    bool finished = false;
    s = sampler_.readNext(1.0, finished);
    if (finished)
      return 0.0f; // render 側で active を落とす
  }
  // ── ポスト HPF / LPF ──
  if (flags.hpf) {
    for (int i = 0; i < flags.hpfStages; ++i)
      s = hpfs_[static_cast<std::size_t>(i)].processSample(0, s);
  }
  if (flags.lpf) {
    for (int i = 0; i < flags.lpfStages; ++i)
      s = lpfs_[static_cast<std::size_t>(i)].processSample(0, s);
  }
  if (flags.hpf || flags.lpf)
    s = std::tanh(s); // 共振ピークのソフトクリップ
  return s;
}

void ClickEngine::render(juce::AudioBuffer<float> &buffer, int numSamples,
                         bool clickPass, double sampleRate) {
  if (!active_.load()) {
    std::fill_n(scratchBuffer_.data(), numSamples, 0.0f);
    return;
  }

  const int numChannels = buffer.getNumChannels();
  const auto sr = static_cast<float>(sampleRate);
  const float clickGain = juce::Decibels::decibelsToGain(gainDb_.load());
  const float decayMs = decayMs_.load();
  const int mode = mode_.load();
  // decayMs = 全体の減衰時間。内部時定数 τ = decayMs/5 で exp(-5) ≈ -43dB
  const float maxTimeSamples = decayMs * sr / 1000.0f;
  const FilterFlags flags = setupFilters(sr);

  for (int sample = 0; sample < numSamples; ++sample) {
    if (noteTimeSamples_ >= maxTimeSamples) {
      active_.store(false);
      std::fill_n(scratchBuffer_.data() + sample,
                  static_cast<size_t>(numSamples - sample), 0.0f);
      break;
    }

    const float amp =
        std::exp(-noteTimeSamples_ * 5000.0f / (decayMs * sr + 1e-6f));
    const float out = synthesizeSample(mode, flags) * amp * clickGain;

    scratchBuffer_[static_cast<size_t>(sample)] = out;
    if (clickPass) {
      for (int ch = 0; ch < numChannels; ++ch)
        buffer.addSample(ch, sample, out);
    }
    noteTimeSamples_ += 1.0f;
  }
}
