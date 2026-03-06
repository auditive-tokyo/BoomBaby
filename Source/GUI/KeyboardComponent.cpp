#include "KeyboardComponent.h"

KeyboardComponent::KeyboardComponent(juce::MidiKeyboardState &state)
    : keyboardState(state) {
  keyboard.setAvailableRange(12, 84); // C0–C6
  keyboard.setOctaveForMiddleC(4);
  keyboard.setKeyPressBaseOctave(keyPressOctave); // A → C2（低音域向け）
  addAndMakeVisible(keyboard);
}

void KeyboardComponent::resized() {
  keyboard.setBounds(getLocalBounds());
}

bool KeyboardComponent::keyPressed(const juce::KeyPress &key) {
  // Z: オクターブ下げ / X: オクターブ上げ
  if (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z') {
    if (keyPressOctave > 0) {
      --keyPressOctave;
      keyboard.setKeyPressBaseOctave(keyPressOctave);
    }
    return true;
  }
  if (key.getTextCharacter() == 'x' || key.getTextCharacter() == 'X') {
    if (keyPressOctave < 7) {
      ++keyPressOctave;
      keyboard.setKeyPressBaseOctave(keyPressOctave);
    }
    return true;
  }
  return false;
}

void KeyboardComponent::grabFocus() { keyboard.grabKeyboardFocus(); }
