cmake_minimum_required(VERSION 3.22)

project(BabySquatch VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SonarQube用のコンパイルコマンド出力
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCEのパスを指定
add_subdirectory(/Volumes/AUDITIVE/development/JUCE JUCE)

juce_add_plugin(BabySquatch
    COMPANY_NAME "Auditive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE AudI
    PLUGIN_CODE BbSq
    BUNDLE_ID "com.auditive.babysquatch"
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "BabySquatch"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "BabySquatch needs microphone access for audio input."
)

target_sources(BabySquatch
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/GUI/SubParams.cpp
        Source/GUI/ClickParams.cpp
        Source/GUI/DirectParams.cpp
        Source/GUI/UIConstants.h
        Source/GUI/CustomSliderLAF.h
        Source/GUI/PanelComponent.h
        Source/GUI/PanelComponent.cpp
        Source/GUI/KeyboardComponent.h
        Source/GUI/KeyboardComponent.cpp
        Source/GUI/EnvelopeCurveEditor.h
        Source/GUI/EnvelopeCurveEditor.cpp
        Source/GUI/LevelMeter.h
        Source/GUI/LevelMeter.cpp
        Source/DSP/ChannelState.h
        Source/DSP/ClickEngine.h
        Source/DSP/ClickEngine.cpp
        Source/DSP/DirectEngine.h
        Source/DSP/DirectEngine.cpp
        Source/DSP/EnvelopeData.h
        Source/DSP/EnvelopeLutManager.h
        Source/DSP/LevelDetector.h
        Source/DSP/SamplePlayer.h
        Source/DSP/SamplePlayer.cpp
        Source/DSP/SubEngine.h
        Source/DSP/SubEngine.cpp
        Source/DSP/SubOscillator.h
        Source/DSP/SubOscillator.cpp
)

target_compile_definitions(BabySquatch
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(BabySquatch
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(BabySquatch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
