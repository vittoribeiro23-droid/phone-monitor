cmake_minimum_required(VERSION 3.22)
project(PhoneMonitor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# JUCE (ajuste a tag para a versão mais recente se quiser)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)

# Gerador de QR code (Nayuki, arquivo único em C++)
FetchContent_Declare(qrcodegen
    GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
    GIT_TAG v1.8.0
    GIT_SHALLOW ON)

FetchContent_MakeAvailable(JUCE qrcodegen)

add_library(qrcodegen_lib STATIC ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp)
target_include_directories(qrcodegen_lib PUBLIC ${qrcodegen_SOURCE_DIR}/cpp)

juce_add_plugin(PhoneMonitor
    COMPANY_NAME "MeuEstudio"
    PLUGIN_MANUFACTURER_CODE Mstd
    PLUGIN_CODE Pmon
    FORMATS VST3 Standalone
    PRODUCT_NAME "Phone Monitor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PhoneMonitor PRIVATE
    Source/AudioStreamServer.cpp
    Source/AudioStreamServer.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(PhoneMonitor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PhoneMonitor
    PRIVATE
        juce::juce_audio_utils
        qrcodegen_lib
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
