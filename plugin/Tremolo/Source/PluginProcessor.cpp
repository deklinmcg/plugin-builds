#include "PluginProcessor.h"

TremoloAudioProcessor::TremoloAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    speedParam = new juce::AudioParameterFloat("speed", "Speed", juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.5f), 4.0f, "Hz");
    depthParam = new juce::AudioParameterFloat("depth", "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f);
    mixParam = new juce::AudioParameterFloat("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f);
    shapeParam = new juce::AudioParameterFloat("shape", "Shape", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f);

    addParameter(speedParam);
    addParameter(depthParam);
    addParameter(mixParam);
    addParameter(shapeParam);
}

TremoloAudioProcessor::~TremoloAudioProcessor() {}

const juce::String TremoloAudioProcessor::getName() const { return "Tremolo"; }
bool TremoloAudioProcessor::acceptsMidi() const { return false; }
bool TremoloAudioProcessor::producesMidi() const { return false; }
bool TremoloAudioProcessor::isMidiEffect() const { return false; }
double TremoloAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int TremoloAudioProcessor::getNumPrograms() { return 1; }
int TremoloAudioProcessor::getCurrentProgram() { return 0; }
void TremoloAudioProcessor::setCurrentProgram(int) {}
const juce::String TremoloAudioProcessor::getProgramName(int) { return {}; }
void TremoloAudioProcessor::changeProgramName(int, const juce::String&) {}

void TremoloAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    lfoPhase = 0.0;
}

void TremoloAudioProcessor::releaseResources() {}

bool TremoloAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mainIn)
        return false;

    if (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

void TremoloAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear any output channels that don't have corresponding inputs
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    const float speed = speedParam->get();
    const float depth = depthParam->get();
    const float mix = mixParam->get();
    const float shape = shapeParam->get(); // 0 = sine, 1 = triangle

    const double phaseIncrement = speed / currentSampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Compute LFO value based on shape blend
        // Sine component: ranges [0, 1] -> modulation = (1 - depth) + depth * sineLFO
        float sineLFO = 0.5f * (1.0f + (float)std::sin(2.0 * juce::MathConstants<double>::pi * lfoPhase));

        // Triangle component: ranges [0, 1]
        float triPhase = (float)std::fmod(lfoPhase, 1.0);
        float triangleLFO;
        if (triPhase < 0.25f)
            triangleLFO = 0.5f + 2.0f * triPhase;
        else if (triPhase < 0.75f)
            triangleLFO = 1.0f - 2.0f * (triPhase - 0.25f);
        else
            triangleLFO = 0.0f + 2.0f * (triPhase - 0.75f);

        // Blend between sine and triangle
        float lfoValue = sineLFO * (1.0f - shape) + triangleLFO * shape;

        // Compute gain modulation: at depth=0 gain=1 (no effect), at depth=1 gain oscillates 0..1
        float modulatedGain = 1.0f - depth * (1.0f - lfoValue);

        // Apply mix: dry/wet blend
        // wet = modulatedGain * sample, dry = sample
        // output = dry * (1-mix) + wet * mix = sample * ((1-mix) + mix * modulatedGain)
        float gainFactor = (1.0f - mix) + mix * modulatedGain;

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= gainFactor;
        }

        // Advance LFO phase
        lfoPhase += phaseIncrement;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
    }
}

juce::AudioProcessorEditor* TremoloAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void TremoloAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(speedParam->get());
    stream.writeFloat(depthParam->get());
    stream.writeFloat(mixParam->get());
    stream.writeFloat(shapeParam->get());
}

void TremoloAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= 16)
    {
        speedParam->setValueNotifyingHost(speedParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        depthParam->setValueNotifyingHost(depthParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        mixParam->setValueNotifyingHost(mixParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        shapeParam->setValueNotifyingHost(shapeParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TremoloAudioProcessor();
}
