cmake_minimum_required(VERSION 3.22)
project(AuraSynthBass VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(FORMATS VST3)
if(APPLE)
  list(APPEND FORMATS AU)
endif()

juce_add_plugin(AuraSynthBass
  COMPANY_NAME "Thiago R. Galvao"
  PLUGIN_MANUFACTURER_CODE Trgl
  PLUGIN_CODE Asb1
  FORMATS ${FORMATS}
  PRODUCT_NAME "AURA Synth Bass"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AuraSynthBass PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(AuraSynthBass PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(AuraSynthBass PRIVATE juce::juce_audio_utils juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
