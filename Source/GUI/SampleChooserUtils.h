#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

inline void launchSampleChooser(auto &fileChooser, auto onChosen) {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Load Sample",
      juce::File::getSpecialLocation(juce::File::userMusicDirectory),
      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
  fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [cb = std::move(onChosen)](const juce::FileChooser &fc) {
        if (const auto r = fc.getResult(); r.existsAsFile())
          cb(r);
      });
}
