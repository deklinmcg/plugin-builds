#include "PluginProcessor.h"
#include "PluginEditor.h"

MyPluginAudioProcessor::MyPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(gainParam = new juce::AudioParameterFloat(
        "gain", "Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.0f, 1.0f),
        1.0f));
}

MyPluginAudioProcessor::~MyPluginAudioProcessor() {}

void MyPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate);
    workBuffer.setSize(2, samplesPerBlock);
    workBuffer.clear();
}

void MyPluginAudioProcessor::releaseResources() {}

bool MyPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() &&
        mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void MyPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const float gain = gainParam->get();
    const int totalOut = getTotalNumOutputChannels();
    const int totalIn  = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear any output channels that have no corresponding input
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (totalOut == 1)
    {
        // Mono branch
        float* dest = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            dest[i] *= gain;
    }
    else
    {
        // Stereo (or more) branch
        for (int ch = 0; ch < juce::jmin(totalOut, 2); ++ch)
        {
            float* dest = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                dest[i] *= gain;
        }
    }
}

juce::AudioProcessorEditor* MyPluginAudioProcessor::createEditor()
{
    return new MyPluginAudioProcessorEditor(*this);
}

void MyPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(gainParam->get());
}

void MyPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getDataSize() >= sizeof(float))
        gainParam->setValueNotifyingHost(gainParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyPluginAudioProcessor();
}
