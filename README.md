# BoomBaby Project

**Free VST3/AU Plugin!**

[![Open Source](https://img.shields.io/badge/Open%20Source-Yes-22c55e)](https://github.com/auditive-tokyo/BoomBaby)
[![License](https://img.shields.io/badge/License-GPLv3%20or%20Commercial-2563eb)](LICENSE.md)
[![Platform](https://img.shields.io/badge/macOS-Apple%20Silicon-111111?logo=apple)](docs/compatibility.md)
[![Formats](https://img.shields.io/badge/Formats-AUv2%20%7C%20VST3-0ea5e9)](#supported-formats)
[![Discussions](https://img.shields.io/github/discussions/auditive-tokyo/BoomBaby)](https://github.com/auditive-tokyo/BoomBaby/discussions)
[![Issues](https://img.shields.io/github/issues/auditive-tokyo/BoomBaby)](https://github.com/auditive-tokyo/BoomBaby/issues)

## Overview

![BoomBaby plugin screenshot](docs/product_in_work.png)

BoomBaby is a 3-channel kick enhancement and synthesis plugin for both audio-track processing and MIDI-triggered kick workflows.

You can insert it directly on an audio track, or load samples and trigger kicks from MIDI.

- Sub: oscillator-based synthesis — pitch/amp envelopes, drive, and waveform/harmonic blending
- Click: transient layer from white noise or sample playback, with amp envelope and filter
- Direct: original input passthrough or sample playback

## Supported Formats

| Format | Platform             |
| ------ | -------------------- |
| AUv2   | macOS, Apple Silicon |
| VST3   | macOS, Apple Silicon |

macOS on Apple Silicon only. Intel Macs and Windows/Linux are not supported at this time.

For DAW-specific test results and full compatibility details, see [docs/compatibility.md](docs/compatibility.md).

## Installation

Download the latest release from the [GitHub Releases page](https://github.com/auditive-tokyo/BoomBaby/releases/latest). Installation instructions are included on the release page.

## Background

I built BoomBaby for one specific use case. I was a longtime fan of Sasquatch V1 (Boz Labs) — until I switched to an Apple M4 Pro Mac in mid-2025 and found it required Rosetta 2.

Kick Ninja (Him DSP) came up as an alternative — and honestly, it's a fantastic plugin. I especially love its envelopes; that's what gives it the freedom to shape really expressive sub movement. The only catch: it's a VST Instrument, not a VST Effect. As someone who frequently works with pre-bounced kicks in collabs and mixing sessions, routing through MIDI just to process an audio file was too much friction.

Rather than compromising, I analyzed both plugins' functionality, extracted the features that actually mattered for my workflow, and built something streamlined around exactly that.

If you need something more versatile, both are excellent commercial options. BoomBaby is intentionally focused — but it covers almost everything I need, and I hope it does the same for you.

I'm planning a commercial version with a broader feature set and a redesigned GUI. This free version is released as open source, with the hope that it might be useful to talented beatmakers and producers around the world.

## Community and Feedback

Detailed contribution guidelines will be documented in CONTRIBUTING.md (coming soon).

- Compatibility reports: Please post your DAW/OS/CPU test results in [GitHub Discussions](https://github.com/auditive-tokyo/BoomBaby/discussions/1). See [docs/compatibility.md](docs/compatibility.md) for the reporting format.
- Preset contributions: If you want to share presets, please use [GitHub Discussions](https://github.com/auditive-tokyo/BoomBaby/discussions) to propose and discuss them.
- Bug reports: Please open them in [GitHub Issues](https://github.com/auditive-tokyo/BoomBaby/issues).
- Feature requests: This project is open source, but implementation scope is selective. Features that strongly improve the developer and producer workflows are more likely to be implemented.
