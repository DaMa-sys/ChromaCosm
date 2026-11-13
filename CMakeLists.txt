cmake_minimum_required(VERSION 3.22)

project(ChromaCosm VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# JUCE Discovery (Local Path or Automatic Git Fetch for portability)
# -----------------------------------------------------------------------------
set(JUCE_PATH "C:/JUCE/JUCE" CACHE PATH "Path to local JUCE installation")

if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using local JUCE from: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Local JUCE not found at '${JUCE_PATH}'. Fetching JUCE 7 via Git...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ChromaCosm
    COMPANY_NAME "DaMA"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE DaMA
    PLUGIN_CODE ChCs
    FORMATS VST3 Standalone
    PRODUCT_NAME "ChromaCosm"
)

target_sources(ChromaCosm
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ChromaEngine.h
)

target_compile_definitions(ChromaCosm
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ChromaCosm
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
