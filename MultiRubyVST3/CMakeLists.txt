cmake_minimum_required(VERSION 3.22)
project(MultiRuby VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Jos JUCE on jo ladattu, voit ohittaa latauksen:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/JUCE-7.0.12
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MultiRuby
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # Ei kopioida automaattisesti Program Files -kansioon (vaatii admin-oikeudet)
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ycmp
    PLUGIN_CODE Mrby
    FORMATS VST3
    VST3_CATEGORIES Fx
    PRODUCT_NAME "Multi Ruby"
)

target_sources(MultiRuby
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(MultiRuby
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MultiRuby
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Valinnainen DSP-testi (ei tarvita pluginin käännöksessä):
#   cmake -B build -DMULTIRUBY_BUILD_TESTS=ON
option(MULTIRUBY_BUILD_TESTS "Build the offline DSP test" OFF)
if(MULTIRUBY_BUILD_TESTS)
    add_executable(multiruby_test Tests/dsp_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(multiruby_test PRIVATE Source)
    target_compile_definitions(multiruby_test PRIVATE JucePlugin_Name="Multi Ruby" JUCE_STANDALONE_APPLICATION=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(multiruby_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_executable(multiruby_snap Tests/snap.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(multiruby_snap PRIVATE Source)
    target_compile_definitions(multiruby_snap PRIVATE JucePlugin_Name="Multi Ruby" JUCE_STANDALONE_APPLICATION=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(multiruby_snap PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
