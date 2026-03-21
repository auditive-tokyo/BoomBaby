cmake_minimum_required(VERSION 3.22)

project(BoomBaby VERSION 0.9.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SonarQube用のコンパイルコマンド出力
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCEのパスを指定（CI では -DJUCE_PATH=... で上書き）
set(JUCE_PATH "/Volumes/AUDITIVE/development/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(BoomBaby
    COMPANY_NAME "Auditive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE AudI
    PLUGIN_CODE BmBy
    BUNDLE_ID "com.auditive.boombaby"
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "BoomBaby"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "BoomBaby needs microphone access for audio input."
)

target_sources(BoomBaby
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PresetManager.cpp
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
        Source/GUI/InfoBox.h
        Source/GUI/InfoBox.cpp
        Source/GUI/InfoBoxText.h
        Source/GUI/ChannelFader.h
        Source/GUI/ChannelFader.cpp
        Source/GUI/MasterFader.h
        Source/GUI/MasterFader.cpp
        Source/GUI/WaveformUtils.h
        Source/GUI/SampleChooserUtils.h
        Source/GUI/ClickModeStateUtils.h
        Source/GUI/PresetBar.h
        Source/PresetManager.h
        Source/DSP/ChannelState.h
        Source/DSP/ClickEngine.h
        Source/DSP/ClickEngine.cpp
        Source/DSP/Saturator.h
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
        Source/DSP/TransientDetector.h
)

# ─── Factory Presets (BinaryData) ─────────────────────────────────

juce_add_binary_data(BoomBabyPresets
    SOURCES
        Resources/factory_presets/default_state.xml
        Resources/factory_presets/junglist_state.xml
        Resources/factory_presets/junglist_direct.wav
        Resources/factory_presets/sub_osc_only_state.xml
)

target_compile_definitions(BoomBaby
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(BoomBaby
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(BoomBaby
    PRIVATE
        BoomBabyPresets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─── Unit Tests ───────────────────────────────────────────────

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

enable_testing()
include(CTest)

# テスト実行ファイル（JUCE コンソールアプリとして設定）
# 注意: JUCE の CMake はモジュールをターゲットごとにコンパイルするため、
#       OBJECT ライブラリ経由で共有するとシンボル重複が発生する。
#       DSP ソースをテストターゲットに直接列挙することで回避。
juce_add_console_app(BoomBabyTests PRODUCT_NAME "BoomBabyTests")
target_sources(BoomBabyTests PRIVATE
    Source/DSP/ClickEngine.cpp
    Source/DSP/DirectEngine.cpp
    Source/DSP/SamplePlayer.cpp
    Source/DSP/SubEngine.cpp
    Source/DSP/SubOscillator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/GUI/ChannelFader.cpp
    Source/GUI/ClickParams.cpp
    Source/GUI/DirectParams.cpp
    Source/GUI/SubParams.cpp
    Source/GUI/EnvelopeCurveEditor.cpp
    Source/GUI/InfoBox.cpp
    Source/GUI/KeyboardComponent.cpp
    Source/GUI/MasterFader.cpp
    Source/GUI/PanelComponent.cpp
    Tests/TestSaturator.cpp
    Tests/TestEnvelopeLutManager.cpp
    Tests/TestClickEngine.cpp
    Tests/TestDirectEngine.cpp
    Tests/TestSubEngine.cpp
    Tests/TestLevelDetector.cpp
    Tests/TestSamplePlayer.cpp
    Tests/TestTransientDetector.cpp
    Tests/TestPluginProcessor.cpp
    Tests/TestEnvelopeData.cpp
)
target_compile_definitions(BoomBabyTests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="BoomBaby"
    JucePlugin_VersionString="0.9.0"
)
target_include_directories(BoomBabyTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)
target_link_libraries(BoomBabyTests
    PRIVATE
        BoomBabyPresets
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
)

# ── Coverage instrumentation (llvm source-based) ──
target_compile_options(BoomBabyTests PRIVATE
    -fprofile-instr-generate -fcoverage-mapping)
target_link_options(BoomBabyTests PRIVATE
    -fprofile-instr-generate -fcoverage-mapping)

include(Catch)
catch_discover_tests(BoomBabyTests DISCOVERY_MODE PRE_TEST)
