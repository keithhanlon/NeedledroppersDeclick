cmake_minimum_required(VERSION 3.22)
project(DeClick VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/JUCE)

juce_add_gui_app(DeClick
    PRODUCT_NAME "DeClick"
    VERSION "0.1.0"
    COMPANY_NAME "citizenkeith"
    BUNDLE_ID "com.citizenkeith.declick")

target_sources(DeClick PRIVATE
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

target_include_directories(DeClick PRIVATE
    include)

target_compile_definitions(DeClick PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DeClick,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DeClick,JUCE_VERSION>")

target_link_libraries(DeClick PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra)
