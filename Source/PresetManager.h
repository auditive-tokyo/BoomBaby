#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

/// .bbpreset フォルダ形式のプリセット保存 / 読み込み / ナビゲーション。
class PresetManager {
public:
  explicit PresetManager(juce::AudioProcessorValueTreeState &apvts);

  // ── ディレクトリ ──
  juce::File getPresetsDirectory() const;
  juce::File getFactoryDirectory() const;

  // ── 保存 / 読み込み ──
  bool savePreset(const juce::String &name);
  bool loadPreset(const juce::File &presetFolder);

  // ── ナビゲーション ──
  void loadNextPreset();
  void loadPreviousPreset();

  // ── 一覧 ──
  juce::Array<juce::File> getFactoryPresets() const;
  juce::Array<juce::File> getUserPresets() const;

  // ── 現在のプリセット名 ──
  const juce::String &getCurrentPresetName() const noexcept {
    return currentPresetName_;
  }

  /// APVTS state 差し替え後に呼ばれるコールバック。
  /// Processor 側で parameterChanged / bakeAllLuts /
  /// サンプルリロードを実行する。
  std::function<void()> onStateReplaced;

private:
  juce::AudioProcessorValueTreeState &apvts_;
  juce::String currentPresetName_{"Init"};

  juce::Array<juce::File> getAllPresetFolders() const;
  void ensureDirectoryExists() const;
  void expandFactoryPresets() const;
};
