cmake_minimum_required(VERSION 3.22)
project(KaraSoft VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically so the app runs without the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(CORE_SOURCES
    src/core/KaraokeSong.cpp
    src/core/KaraokeSong.h)

set(AUDIO_SOURCES
    src/audio/FluidApi.cpp
    src/audio/FluidApi.h
    src/audio/KaraokeEngine.cpp
    src/audio/KaraokeEngine.h)

#==============================================================================
juce_add_gui_app(KaraSoft
    PRODUCT_NAME "KaraSoft"
    COMPANY_NAME "KaraSoft"
    VERSION ${PROJECT_VERSION})

target_sources(KaraSoft PRIVATE
    ${CORE_SOURCES}
    ${AUDIO_SOURCES}
    src/Main.cpp
    src/ui/AppSettings.cpp
    src/ui/AppSettings.h
    src/ui/LyricView.cpp
    src/ui/LyricView.h
    src/ui/ChannelPanel.cpp
    src/ui/ChannelPanel.h
    src/ui/SynthMixer.cpp
    src/ui/SynthMixer.h
    src/ui/MainComponent.cpp
    src/ui/MainComponent.h)

target_compile_definitions(KaraSoft PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:KaraSoft,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:KaraSoft,JUCE_VERSION>"
    JUCE_PLUGINHOST_VST3=1
    JUCE_ASIO=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KaraSoft PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(NOT WIN32)
    target_link_libraries(KaraSoft PRIVATE ${CMAKE_DL_LIBS})
endif()

#==============================================================================
# Command-line checker for song files (no GUI, no audio).
juce_add_console_app(karaparse PRODUCT_NAME "karaparse")
target_sources(karaparse PRIVATE ${CORE_SOURCES} tools/karaparse.cpp)
target_compile_definitions(karaparse PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(karaparse PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags)

# Offline renderer: plays a song through FluidSynth into a WAV, for testing the engine.
juce_add_console_app(kararender PRODUCT_NAME "kararender")
target_sources(kararender PRIVATE ${CORE_SOURCES} ${AUDIO_SOURCES} tools/kararender.cpp)
target_compile_definitions(kararender PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(kararender PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags)
if(NOT WIN32)
    target_link_libraries(kararender PRIVATE ${CMAKE_DL_LIBS})
endif()

# Source files contain Thai UTF-8 string literals.
if(MSVC)
    foreach(t KaraSoft karaparse kararender)
        target_compile_options(${t} PRIVATE /utf-8)
    endforeach()
endif()
