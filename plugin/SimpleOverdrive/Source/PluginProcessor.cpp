// SimpleOverdriveAudioProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

SimpleOverdriveAudioProcessor::SimpleOverdriveAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(driveParam = new juce::AudioParameterFloat(
        "drive", "Drive",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.0f, 1.0f),
        1.0f));
}

SimpleOverdriveAudioProcessor::~SimpleOverdriveAudioProcessor() {}

void SimpleOverdriveAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // No buffers required
}

void SimpleOverdriveAudioProcessor::releaseResources() {}

bool SimpleOverdriveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() &&
        mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SimpleOverdriveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const float drive      = driveParam->get();
    const float tanhDrive  = std::tanh(drive);
    // Avoid division by zero (drive >= 1.0 so tanh(drive) > 0, but guard anyway)
    const float invTanhDrive = (tanhDrive > 1e-9f) ? (1.0f / tanhDrive) : 1.0f;

    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (totalOut == 1)
    {
        // Mono: process single channel
        float* channelData = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = channelData[i] * drive;
            const float y = std::tanh(x);
            channelData[i] = y * invTanhDrive;
        }
    }
    else
    {
        // Stereo: dual-mono — same Drive applied independently to L and R
        float* leftData  = buffer.getWritePointer(0);
        float* rightData = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float xL = leftData[i] * drive;
            leftData[i]    = std::tanh(xL) * invTanhDrive;

            const float xR = rightData[i] * drive;
            rightData[i]   = std::tanh(xR) * invTanhDrive;
        }

        // Clear any extra output channels beyond stereo
        for (int ch = 2; ch < totalOut; ++ch)
            buffer.clear(ch, 0, numSamples);
    }
}

juce::AudioProcessorEditor* SimpleOverdriveAudioProcessor::createEditor()
{
    return new SimpleOverdriveAudioProcessorEditor(*this);
}

void SimpleOverdriveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(driveParam->get());
}

void SimpleOverdriveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 4)
        driveParam->setValueNotifyingHost(driveParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleOverdriveAudioProcessor();
}
