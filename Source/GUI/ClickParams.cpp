// ClickParams.cpp
// Click panel UI setup / layout
#include "../PluginEditor.h"

void BabySquatchAudioProcessorEditor::setupClickParams() {
  clickUI.xyPad.setOnChanged([](float /*blend*/, float /*decay*/) {
    // TODO: DSP write (blend -> SQR<->SAW, decay -> Decay ms)
  });
  addAndMakeVisible(clickUI.xyPad);
}

void BabySquatchAudioProcessorEditor::layoutClickParams(
    juce::Rectangle<int> area) {
  clickUI.xyPad.setBounds(area);
}
