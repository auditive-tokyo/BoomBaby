#pragma once

/// InfoBox に表示するパラメーター説明テキストを一元管理する。
/// 追加・修正はこのファイルだけで行えば全 UI に反映される。
namespace InfoText {

// ── Master ──
inline constexpr const char *masterGain =
    "Master output level (-60 ... +12 dB)";

// ── Sub ──
inline constexpr const char *subLength = "Sub note duration (10-2000 ms)";
inline constexpr const char *subAmp =
    "Sub amplitude (0-200%)\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *subFreq =
    "Sub frequency (20-20k Hz)\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *subMix =
    "Waveform mix: 0 = Sine\n-100 = Tri/SQR/SAW mix\n+100 = Tone mix"
    "\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *subSaturate =
    "Saturation drive (0-24 dB)\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *subClipType =
    "Saturation mode: Soft / Hard / Tube";
inline constexpr const char *subTone1 = "1st additive sine (fundamental)";
inline constexpr const char *subTone2 = "2nd additive sine (+1 oct)";
inline constexpr const char *subTone3 = "3rd additive sine (+1 oct + 5th)";
inline constexpr const char *subTone4 = "4th additive sine (+2 oct)";
inline constexpr const char *subWave =
    "Sub oscillator waveform: Tri / SQR / SAW";

// ── Click ──
inline constexpr const char *clickMode = "Click source: Noise or Sample";
inline constexpr const char *clickNoiseDecay =
    "Noise envelope decay (1-2000 ms)";
inline constexpr const char *clickBpfFreq =
    "Bandpass centre frequency (20-20k Hz)";
inline constexpr const char *clickBpfQ = "Bandpass resonance (Q 0.1-18)";
inline constexpr const char *clickBpfSlope =
    "Bandpass slope: 12 / 24 / 48 dB/oct";
inline constexpr const char *clickDrive = "Saturator drive (0-24 dB)";
inline constexpr const char *clickClipType =
    "Saturation mode: Soft / Hard / Tube";
inline constexpr const char *clickHpfFreq = "High-pass cutoff (20-20k Hz)";
inline constexpr const char *clickHpfQ = "High-pass resonance (Q 0.1-18)";
inline constexpr const char *clickHpfSlope = "HPF slope: 12 / 24 / 48 dB/oct";
inline constexpr const char *clickLpfFreq = "Low-pass cutoff (20-20k Hz)";
inline constexpr const char *clickLpfQ = "Low-pass resonance (Q 0.1-18)";
inline constexpr const char *clickLpfSlope = "LPF slope: 12 / 24 / 48 dB/oct";
inline constexpr const char *clickSamplePitch =
    "Sample pitch shift (+/-24 semitones)";
inline constexpr const char *clickSampleAmp =
    "Sample amplitude (0-200%)\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *clickSampleDecay =
    "Sample envelope duration (10-2000 ms)";
inline constexpr const char *clickSampleLoad =
    "Load audio sample (drop or click)";

// ── Direct ──
inline constexpr const char *directMode =
    "Direct source: passthrough or sample";
inline constexpr const char *directPitch =
    "Sample pitch shift (+/-24 semitones)";
inline constexpr const char *directAmp =
    "Direct amplitude (0-200%)\nEnvelope available - click label to draw."
    "\nKnob greys out when envelope is active.";
inline constexpr const char *directDrive = "Saturator drive (0-24 dB)";
inline constexpr const char *directClipType =
    "Saturation mode: Soft / Hard / Tube";
inline constexpr const char *directDecay = "Envelope duration (10-2000 ms)";
inline constexpr const char *directHpfFreq = "High-pass cutoff (20-20k Hz)";
inline constexpr const char *directHpfQ = "High-pass resonance (Q 0.1-18)";
inline constexpr const char *directHpfSlope = "HPF slope: 12 / 24 / 48 dB/oct";
inline constexpr const char *directLpfFreq = "Low-pass cutoff (20-20k Hz)";
inline constexpr const char *directLpfQ = "Low-pass resonance (Q 0.1-18)";
inline constexpr const char *directLpfSlope = "LPF slope: 12 / 24 / 48 dB/oct";
inline constexpr const char *directThreshold =
    "Transient detect threshold (-60-0 dB)";
inline constexpr const char *directHold =
    "Transient gate hold time (20-500 ms)\n"
    "Set it long enough to avoid double-triggering.\n"
    "For fast kick rolls, use sample mode with MIDI trigger instead.";
inline constexpr const char *directSampleLoad =
    "Load audio sample (drop or click)";

// ── Envelope Editor ──
inline constexpr const char *envelopePoint =
    "Right-click: enter value / set position";
inline constexpr const char *envelopeCurve = "Shift-drag on line to draw curve";

} // namespace InfoText
