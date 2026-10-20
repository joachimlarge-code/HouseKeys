cmake_minimum_required(VERSION 3.22)

# Universal binary on Mac: runs on both Apple Silicon (M1/M2/M3/M4) and Intel.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(HouseKeys VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local copy if you pass -DHK_JUCE_PATH=..., otherwise download it.
set(HK_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(HK_JUCE_PATH)
    add_subdirectory(${HK_JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(HouseKeys
    COMPANY_NAME "Hvasser Audio"
    BUNDLE_ID com.hvasseraudio.housekeys
    PLUGIN_MANUFACTURER_CODE Hvas
    PLUGIN_CODE Hkey
    PRODUCT_NAME "HouseKeys"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE          # installs into ~/Library/Audio/Plug-Ins automatically
    FORMATS AU VST3 Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth)

target_sources(HouseKeys PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(HouseKeys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HouseKeys
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: offline demo renderer (no JUCE needed)
option(HK_BUILD_DEMO "Build the offline demo renderer" OFF)
if(HK_BUILD_DEMO)
    add_executable(render_demo tools/render_demo.cpp)
endif()
