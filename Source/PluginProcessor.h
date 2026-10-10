#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioStreamServer.h"

/**
    Plugin "pass-through": o áudio continua passando normalmente pela DAW,
    e uma cópia é enviada ao celular pelo servidor embutido.
*/
class PhoneMonitorProcessor : public juce::AudioProcessor
{
public:
    PhoneMonitorProcessor();
    ~PhoneMonitorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Phone Monitor"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    AudioStreamServer& getServer() noexcept { return server; }

private:
    AudioStreamServer server;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhoneMonitorProcessor)
};
