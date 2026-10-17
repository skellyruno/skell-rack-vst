cmake_minimum_required(VERSION 3.22)
project(ModularFXSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ModularFXSuite
    COMPANY_NAME "SkellAudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Skel
    PLUGIN_CODE Mfx1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Modular FX Suite"
)

juce_generate_juce_header(ModularFXSuite)

target_sources(ModularFXSuite
    PRIVATE
        Source/CustomLookAndFeel.h
        Source/FxRegistry.h
        Source/ModuleCards.h
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/Modules/FxBase.h
        Source/Modules/DSPModules.h
        Source/Modules/NewEffects.h
        Source/Modules/MoreEffects.h
        Source/FxCatalog.h
        Source/EffectBrowser.h
)

target_compile_definitions(ModularFXSuite
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ModularFXSuite
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
