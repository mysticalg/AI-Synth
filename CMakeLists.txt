cmake_minimum_required(VERSION 3.22)
project(AISynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.2
)
FetchContent_MakeAvailable(juce)

# Optional legacy VST2 build support (for hosts like Cubase 5).
#
# IMPORTANT:
# JUCE can only build VST2 when a compatible VST2 SDK is provided locally.
# We do not vendor or distribute that SDK in this repository.
option(AISYNTH_ENABLE_VST2 "Build a legacy VST2 (.dll) target when a VST2 SDK is available" OFF)

set(AISYNTH_PLUGIN_FORMATS VST3 Standalone)
if(AISYNTH_ENABLE_VST2)
  if(DEFINED ENV{VST2_SDK_DIR})
    # JUCE checks this variable to locate VST2 headers.
    set(VST2_SDK_DIR "$ENV{VST2_SDK_DIR}")
    list(APPEND AISYNTH_PLUGIN_FORMATS VST)
    message(STATUS "AISynth: VST2 enabled using VST2_SDK_DIR='${VST2_SDK_DIR}'")
  else()
    message(WARNING "AISynth: AISYNTH_ENABLE_VST2=ON but VST2_SDK_DIR is not set. Building VST3/Standalone only.")
  endif()
endif()

juce_add_plugin(AISynth
  BUNDLE_ID "com.aisynth.app"
  COMPANY_NAME "AI Synth"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE AIsy
  PLUGIN_CODE AIs1
  FORMATS ${AISYNTH_PLUGIN_FORMATS}
  PRODUCT_NAME "AI Synth")

# Generate JuceHeader.h so source files can include <JuceHeader.h> on all CI platforms.
juce_generate_juce_header(AISynth)

target_sources(AISynth PRIVATE
  Source/PluginProcessor.h
  Source/PluginProcessor.cpp
  Source/PluginEditor.h
  Source/PluginEditor.cpp
  Source/SynthVoice.h
  Source/SynthVoice.cpp)

target_compile_definitions(AISynth
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AISynth
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(AISynthStressTest
  PRODUCT_NAME "AI Synth Stress Test")

target_sources(AISynthStressTest PRIVATE
  Tests/ProcessorStressTest.cpp)

target_include_directories(AISynthStressTest
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_BINARY_DIR}/AISynth_artefacts/JuceLibraryCode")

target_compile_definitions(AISynthStressTest
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AISynthStressTest
  PRIVATE
    AISynth
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
