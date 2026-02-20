#include "PluginProcessor.h"

TestAudioProcessor::TestAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    gainParameter = new juce::AudioParameterFloat("gain", "Gain", 0.0f, 1.0f, 1.0f);
    addParameter(gainParameter);
}

TestAudioProcessor::~TestAudioProcessor()
{
}

const juce::String TestAudioProcessor::getName() const
{
    return "Test";
}

bool TestAudioProcessor::acceptsMidi() const { return false; }
bool TestAudioProcessor::producesMidi() const { return false; }
bool TestAudioProcessor::isMidiEffect() const { return false; }
double TestAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int TestAudioProcessor::getNumPrograms() { return 1; }
int TestAudioProcessor::getCurrentProgram() { return 0; }
void TestAudioProcessor::setCurrentProgram(int) {}
const juce::String TestAudioProcessor::getProgramName(int) { return {}; }
void TestAudioProcessor::changeProgramName(int, const juce::String&) {}

void TestAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void TestAudioProcessor::releaseResources()
{
}

bool TestAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();

    if (mainOutput != mainInput)
        return false;

    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void TestAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float gain = gainParameter->get();

    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= gain;
        }
    }
}

juce::AudioProcessorEditor* TestAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void TestAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    stream.writeFloat(gainParameter->get());
}

void TestAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= (int)sizeof(float))
        gainParameter->setValueNotifyingHost(gainParameter->getNormalisableRange().convertTo0to1(stream.readFloat()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestAudioProcessor();
}
