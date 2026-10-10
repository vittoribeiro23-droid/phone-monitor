#include "PluginProcessor.h"
#include "PluginEditor.h"

PhoneMonitorProcessor::PhoneMonitorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void PhoneMonitorProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    server.setSampleRate (sampleRate);
    server.start();   // idempotente
}

bool PhoneMonitorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void PhoneMonitorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pass-through: o buffer já contém o áudio de entrada e sai inalterado.
    // Em render/bounce offline não transmitimos (o celular receberia áudio acelerado).
    if (isNonRealtime())
        return;

    server.pushAudio (buffer);
}

juce::AudioProcessorEditor* PhoneMonitorProcessor::createEditor()
{
    return new PhoneMonitorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhoneMonitorProcessor();
}
