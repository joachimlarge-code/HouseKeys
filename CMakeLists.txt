cmake_minimum_required(VERSION 3.22)

# Universal binary on Mac: runs on both Apple Silicon (M1/M2/M3/M4) and Intel.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(HouseKeys VERSION 3.8.0 LANGUAGES C CXX)

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

# --- Piano samples (Salamander Grand Piano, CC-BY 3.0) compiled into the plugin
file(GLOB HK_PIANO_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Piano/*.ogg")
if(HK_PIANO_FILES)
    juce_add_binary_data(HouseKeysSamples NAMESPACE BinaryData SOURCES ${HK_PIANO_FILES})
    set_target_properties(HouseKeysSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(HK_HAS_SAMPLES 1)
else()
    message(WARNING "No piano samples in Resources/Piano - the plugin will use the modelled piano")
    set(HK_HAS_SAMPLES 0)
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
    Source/PluginEditor.cpp
    Source/SampleLoader.cpp
    Source/PresetAI.cpp)

target_compile_definitions(HouseKeys PUBLIC
    HK_HAS_SAMPLES=${HK_HAS_SAMPLES}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HouseKeys
    PRIVATE
        juce::juce_audio_utils
        $<$<BOOL:${HK_HAS_SAMPLES}>:HouseKeysSamples>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Optional: offline render tool (plays every sound through the real engine to a WAV)
option(HK_BUILD_TOOLS "Build the offline render tool" OFF)
if(HK_BUILD_TOOLS)
    juce_add_console_app(hk_render PRODUCT_NAME "hk_render")
    target_sources(hk_render PRIVATE tools/hk_render.cpp Source/SampleLoader.cpp Source/PresetAI.cpp)
    target_compile_definitions(hk_render PRIVATE HK_HAS_SAMPLES=${HK_HAS_SAMPLES} JUCE_USE_OGGVORBIS=1
                               JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_include_directories(hk_render PRIVATE Source)
    target_link_libraries(hk_render PRIVATE juce::juce_audio_formats juce::juce_events
                          $<$<BOOL:${HK_HAS_SAMPLES}>:HouseKeysSamples>
                          juce::juce_recommended_config_flags)
endif()
