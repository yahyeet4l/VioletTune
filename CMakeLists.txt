cmake_minimum_required(VERSION 3.22)
project(VioletTune VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_plugin(VioletTune
    COMPANY_NAME "YourName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Yurn
    PLUGIN_CODE Vtun
    FORMATS VST3
    PRODUCT_NAME "VioletTune")

juce_generate_juce_header(VioletTune)

target_sources(VioletTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VioletTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VioletTune
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
