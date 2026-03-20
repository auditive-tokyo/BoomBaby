#include "PresetManager.h"

// ─────────────────────────────────────────────────────────────────
// ctor
// ─────────────────────────────────────────────────────────────────
PresetManager::PresetManager(juce::AudioProcessorValueTreeState &apvts)
    : apvts_(apvts) {
  ensureDirectoryExists();
}

// ─────────────────────────────────────────────────────────────────
// ディレクトリ
// ─────────────────────────────────────────────────────────────────
juce::File PresetManager::getPresetsDirectory() const {
  return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
      .getChildFile("Auditive")
      .getChildFile("BoomBaby")
      .getChildFile("Presets");
}

juce::File PresetManager::getFactoryDirectory() const {
  return getPresetsDirectory().getChildFile("Factory");
}

void PresetManager::ensureDirectoryExists() const {
  getPresetsDirectory().createDirectory();
}

// ─────────────────────────────────────────────────────────────────
// 保存
// ─────────────────────────────────────────────────────────────────
bool PresetManager::savePreset(const juce::String &name) {
  if (name.isEmpty())
    return false;

  auto presetDir = getPresetsDirectory().getChildFile(name + ".bbpreset");

  if (!presetDir.createDirectory())
    return false;

  // 現在の APVTS state をコピー
  auto state = apvts_.copyState();

  // サンプルファイルをプリセットフォルダにコピーし、パスを相対化
  auto copySample = [&](const char *propName, const char *destName) {
    const auto path = state.getProperty(propName).toString();
    if (path.isEmpty())
      return;
    const juce::File src(path);
    if (!src.existsAsFile())
      return;
    auto dest = presetDir.getChildFile(destName);
    src.copyFileTo(dest);
    state.setProperty(propName, juce::String("./" + juce::String(destName)),
                      nullptr);
  };

  copySample("clickSamplePath", "click_sample.wav");
  copySample("directSamplePath", "direct_sample.wav");

  // state.xml 書き出し
  auto xml = state.createXml();
  if (!xml)
    return false;

  auto stateFile = presetDir.getChildFile("state.xml");
  if (!xml->writeTo(stateFile))
    return false;

  currentPresetName_ = name;
  return true;
}

// ─────────────────────────────────────────────────────────────────
// 読み込み
// ─────────────────────────────────────────────────────────────────
bool PresetManager::loadPreset(const juce::File &presetDir) {
  auto stateFile = presetDir.getChildFile("state.xml");
  if (!stateFile.existsAsFile())
    return false;

  auto xml = juce::XmlDocument::parse(stateFile);
  if (!xml)
    return false;

  auto state = juce::ValueTree::fromXml(*xml);
  if (!state.isValid())
    return false;

  // 相対パス (./ 始まり) をプリセットフォルダ基準の絶対パスに解決
  auto resolvePath = [&](const char *propName) {
    const auto path = state.getProperty(propName).toString();
    if (path.startsWith("./")) {
      auto resolved = presetDir.getChildFile(path.substring(2));
      state.setProperty(propName, resolved.getFullPathName(), nullptr);
    }
  };

  resolvePath("clickSamplePath");
  resolvePath("directSamplePath");

  // APVTS state 差し替え
  apvts_.replaceState(state);
  currentPresetName_ = presetDir.getFileNameWithoutExtension();

  if (onStateReplaced)
    onStateReplaced();

  return true;
}

// ─────────────────────────────────────────────────────────────────
// ナビゲーション
// ─────────────────────────────────────────────────────────────────
void PresetManager::loadNextPreset() {
  auto all = getAllPresetFolders();
  if (all.isEmpty())
    return;

  int idx = -1;
  for (int i = 0; i < all.size(); ++i) {
    if (all[i].getFileNameWithoutExtension() == currentPresetName_) {
      idx = i;
      break;
    }
  }

  idx = (idx + 1) % all.size();
  loadPreset(all[idx]);
}

void PresetManager::loadPreviousPreset() {
  auto all = getAllPresetFolders();
  if (all.isEmpty())
    return;

  int idx = 0;
  for (int i = 0; i < all.size(); ++i) {
    if (all[i].getFileNameWithoutExtension() == currentPresetName_) {
      idx = i;
      break;
    }
  }

  idx = (idx - 1 + all.size()) % all.size();
  loadPreset(all[idx]);
}

// ─────────────────────────────────────────────────────────────────
// 一覧
// ─────────────────────────────────────────────────────────────────
juce::Array<juce::File> PresetManager::getFactoryPresets() const {
  juce::Array<juce::File> results;
  auto factoryDir = getFactoryDirectory();
  if (factoryDir.isDirectory()) {
    for (const auto &entry : juce::RangedDirectoryIterator(
             factoryDir, false, "*.bbpreset", juce::File::findDirectories))
      results.add(entry.getFile());
  }
  return results;
}

juce::Array<juce::File> PresetManager::getUserPresets() const {
  juce::Array<juce::File> results;
  auto presetsDir = getPresetsDirectory();
  for (const auto &entry : juce::RangedDirectoryIterator(
           presetsDir, false, "*.bbpreset", juce::File::findDirectories))
    results.add(entry.getFile());
  return results;
}

juce::Array<juce::File> PresetManager::getAllPresetFolders() const {
  juce::Array<juce::File> results;
  results.addArray(getFactoryPresets());
  results.addArray(getUserPresets());
  return results;
}
