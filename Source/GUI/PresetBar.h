#pragma once

#include "../PresetManager.h"
#include "UIConstants.h"
#include <juce_gui_basics/juce_gui_basics.h>

/// プラグイン最上部に表示するプリセットバー: [◀] 名前 [▶] [Save]
class PresetBar : public juce::Component {
public:
  explicit PresetBar(PresetManager &pm) : presetManager_(pm) {
    addAndMakeVisible(prevButton_);
    addAndMakeVisible(nextButton_);
    addAndMakeVisible(saveButton_);
    addAndMakeVisible(nameLabel_);

    nameLabel_.setJustificationType(juce::Justification::centred);
    nameLabel_.setColour(juce::Label::textColourId, UIConstants::Colours::text);
    nameLabel_.setInterceptsMouseClicks(true, false);
    nameLabel_.addMouseListener(this, false);

    auto btnColour = [](juce::TextButton &b) {
      b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
      b.setColour(juce::TextButton::textColourOffId,
                  UIConstants::Colours::text);
    };
    btnColour(prevButton_);
    btnColour(nextButton_);
    btnColour(saveButton_);

    prevButton_.onClick = [this] {
      presetManager_.loadPreviousPreset();
      refreshPresetName();
    };
    nextButton_.onClick = [this] {
      presetManager_.loadNextPreset();
      refreshPresetName();
    };
    saveButton_.onClick = [this] { onSaveClicked(); };

    refreshPresetName();
  }

  void resized() override {
    auto area = getLocalBounds().reduced(4, 2);
    prevButton_.setBounds(area.removeFromLeft(28));
    area.removeFromLeft(4);
    saveButton_.setBounds(area.removeFromRight(50));
    area.removeFromRight(4);
    nextButton_.setBounds(area.removeFromRight(28));
    area.removeFromRight(4);
    nameLabel_.setBounds(area);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xFF222222));
    // 下端に薄いセパレーターライン
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
  }

  void refreshPresetName() {
    nameLabel_.setText(presetManager_.getCurrentPresetName(),
                       juce::dontSendNotification);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.eventComponent == &nameLabel_)
      showPresetMenu();
  }

private:
  PresetManager &presetManager_;
  juce::TextButton prevButton_{juce::String::charToString(0x25C0)};
  juce::TextButton nextButton_{juce::String::charToString(0x25B6)};
  juce::TextButton saveButton_{"Save"};
  juce::Label nameLabel_;

  void showPresetMenu() {
    juce::PopupMenu menu;

    auto factoryPresets = presetManager_.getFactoryPresets();
    if (!factoryPresets.isEmpty()) {
      juce::PopupMenu factorySub;
      for (int i = 0; i < factoryPresets.size(); ++i)
        factorySub.addItem(1000 + i,
                           factoryPresets[i].getFileNameWithoutExtension());
      menu.addSubMenu("Factory", factorySub);
      menu.addSeparator();
    }

    auto userPresets = presetManager_.getUserPresets();
    if (!userPresets.isEmpty()) {
      for (int i = 0; i < userPresets.size(); ++i)
        menu.addItem(2000 + i, userPresets[i].getFileNameWithoutExtension());
    }

    if (menu.getNumItems() == 0)
      menu.addItem(-1, "(No presets)", false, false);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&nameLabel_),
        [this, factoryPresets, userPresets](int result) {
          if (result >= 2000 && result - 2000 < userPresets.size()) {
            presetManager_.loadPreset(userPresets[result - 2000]);
            refreshPresetName();
          } else if (result >= 1000 && result - 1000 < factoryPresets.size()) {
            presetManager_.loadPreset(factoryPresets[result - 1000]);
            refreshPresetName();
          }
        });
  }

  void onSaveClicked() {
    // NOSONAR: JUCE の AlertWindow は enterModalState(deleteWhenDismissed)
    // で安全に管理される
    auto *aw = new juce::AlertWindow( // NOSONAR
        "Save Preset", "Enter preset name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", presetManager_.getCurrentPresetName());
    aw->addButton("Save", 1);
    aw->addButton("Cancel", 0);

    aw->enterModalState(
        true, juce::ModalCallbackFunction::create([this, aw](int result) {
          if (result == 1) {
            auto name = aw->getTextEditorContents("name").trim();
            if (name.isNotEmpty()) {
              presetManager_.savePreset(name);
              refreshPresetName();
            }
          }
        }),
        true); // deleteWhenDismissed = true
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
