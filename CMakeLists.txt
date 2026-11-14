cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(ToiletVocals VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ToiletVocals
    COMPANY_NAME             "Porcelain Audio"
    COMPANY_WEBSITE          ""
    BUNDLE_ID                com.porcelainaudio.toiletvocals
    PLUGIN_MANUFACTURER_CODE Tlta
    PLUGIN_CODE              TlVc
    PRODUCT_NAME             "Toilet Vocals"
    DESCRIPTION              "One-plugin vocal chain."
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE             kAudioUnitType_Effect
    VST3_CATEGORIES          Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD  TRUE)

target_sources(ToiletVocals PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Artwork.cpp)

target_include_directories(ToiletVocals PRIVATE Source)

target_compile_definitions(ToiletVocals PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    TOILET_VOCALS_VERSION="${PROJECT_VERSION}")

target_link_libraries(ToiletVocals
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
