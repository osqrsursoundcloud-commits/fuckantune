cmake_minimum_required(VERSION 3.22)
project(FuckAnTune VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Le dossier "JUCE" doit se trouver a cote de ce fichier
add_subdirectory(JUCE)

juce_add_plugin(FuckAnTune
    COMPANY_NAME "Underground"
    PLUGIN_MANUFACTURER_CODE Undg
    PLUGIN_CODE Fktn
    FORMATS VST3
    PRODUCT_NAME "FUCKANTUNE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Pitch)

target_sources(FuckAnTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FuckAnTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FuckAnTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
