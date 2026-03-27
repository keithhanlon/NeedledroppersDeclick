cmake_minimum_required(VERSION 3.22)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
project(NeedledroppersDeclick VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/JUCE)

juce_add_gui_app(NeedledroppersDeclick
    PRODUCT_NAME "Needledropper's Declicker"
    VERSION "0.1.0"
    COMPANY_NAME "citizenkeith"
    BUNDLE_ID "com.citizenkeith.needledroppersdeclicker")

target_sources(NeedledroppersDeclick PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    src/dsp/WaveletEngine.cpp
    src/dsp/ClickDetector.cpp
    src/dsp/RepairEngine.cpp
    src/threading/DetectionThread.cpp
    src/threading/ProcessingThread.cpp
    src/ui/WaveformView.cpp
    src/ui/BatchQueue.cpp
    src/ui/ParameterPanel.cpp
    src/ui/TransportBar.cpp
    src/ui/StatusBar.cpp)

target_include_directories(NeedledroppersDeclick PRIVATE
    include)

target_compile_definitions(NeedledroppersDeclick PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:NeedledroppersDeclick,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:NeedledroppersDeclick,JUCE_VERSION>")

target_link_libraries(NeedledroppersDeclick PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra)
