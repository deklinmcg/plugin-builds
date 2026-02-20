#include "PluginProcessor.h"

TestPluginAudioProcessor::TestPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(gainParam = new juce::AudioParameterFloat("gain", "Gain", 0.0f, 1.0f, 1.0f));
}

TestPluginAudioProcessor::~TestPluginAudioProcessor() {}

const juce::String TestPluginAudioProcessor::getName() const { return "TestPlugin"; }
bool TestPluginAudioProcessor::acceptsMidi() const { return false; }
bool TestPluginAudioProcessor::producesMidi() const { return false; }
bool TestPluginAudioProcessor::isMidiEffect() const { return false; }
double TestPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TestPluginAudioProcessor::getNumPrograms() { return 1; }
int TestPluginAudioProcessor::getCurrentProgram() { return 0; }
void TestPluginAudioProcessor::setCurrentProgram(int) {}
const juce::String TestPluginAudioProcessor::getProgramName(int) { return {}; }
void TestPluginAudioProcessor::changeProgramName(int, const juce::String&) {}

void TestPluginAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void TestPluginAudioProcessor::releaseResources() {}

bool TestPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mainIn)
        return false;

    if (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

void TestPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float gain = gainParam->get();

    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= gain;
        }
    }
}

juce::AudioProcessorEditor* TestPluginAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void TestPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    stream.writeFloat(gainParam->get());
}

void TestPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= (int)sizeof(float))
        *gainParam = stream.readFloat();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginAudioProcessor();
}
