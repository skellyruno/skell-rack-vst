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

# SkellCore guided onboarding; pinned shared UI only.
FetchContent_Declare(SkellCoreTour
  GIT_REPOSITORY https://github.com/skellyruno/skell-vst-template.git
  GIT_TAG 3306ad495c93e062e39b8abbf079534b0b53da4e
  GIT_SHALLOW FALSE
  SOURCE_SUBDIR SkellCore)
FetchContent_MakeAvailable(SkellCoreTour)
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
        Source/Modules/ExtraEffects.h
        Source/Modules/BandLabFx.h
        Source/Presets.h
        Source/MasterPanels.h
        Source/FxCatalog.h
        Source/EffectBrowser.h
        Source/Meters.h
        Source/PresetBrowser.h
)

target_compile_definitions(ModularFXSuite
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ModularFXSuite
    PRIVATE
        SkellCore::UI
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(SKELL_RACK_UI_TOUR_SCREENSHOTS "Build headless real-editor GuidedTour screenshots" OFF)
if(SKELL_RACK_UI_TOUR_SCREENSHOTS)
    add_executable(SkellRackTourScreenshots tests/ui_tour_renderer.cpp)
    target_include_directories(SkellRackTourScreenshots PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/ModularFXSuite_artefacts/JuceLibraryCode")
    target_link_libraries(SkellRackTourScreenshots PRIVATE
      ModularFXSuite SkellCore::UIRenderer
      juce::juce_audio_utils juce::juce_gui_extra)
    target_compile_features(SkellRackTourScreenshots PRIVATE cxx_std_17)
    target_compile_definitions(SkellRackTourScreenshots PRIVATE
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()
