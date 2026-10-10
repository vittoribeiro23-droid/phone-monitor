#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "qrcodegen.hpp"

#include <optional>

class PhoneMonitorEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PhoneMonitorEditor (PhoneMonitorProcessor&);
    ~PhoneMonitorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateQr();

    PhoneMonitorProcessor& processor;

    juce::TextButton nextIpButton { "Trocar IP" };
    juce::StringArray addresses;
    int addressIndex = 0;

    juce::String url;
    std::optional<qrcodegen::QrCode> qr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhoneMonitorEditor)
};
