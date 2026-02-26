#include "PluginProcessor.h"
#include "PluginEditor.h"

TapeTopAudioProcessor::TapeTopAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(driveParam = new juce::AudioParameterFloat("drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 35.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " %"; }));

    addParameter(hpfParam = new juce::AudioParameterFloat("hpf", "HPF",
        juce::NormalisableRange<float>(20.0f, 500.0f, 0.1f, 0.4f), 120.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " Hz"; }));

    addParameter(mixParam = new juce::AudioParameterFloat("mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 60.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " %"; }));
}

TapeTopAudioProcessor::~TapeTopAudioProcessor() {}

const juce::String TapeTopAudioProcessor::getName() const { return "TapeTop"; }
bool TapeTopAudioProcessor::acceptsMidi() const { return false; }
bool TapeTopAudioProcessor::producesMidi() const { return false; }
bool TapeTopAudioProcessor::isMidiEffect() const { return false; }
double TapeTopAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int TapeTopAudioProcessor::getNumPrograms() { return 1; }
int TapeTopAudioProcessor::getCurrentProgram() { return 0; }
void TapeTopAudioProcessor::setCurrentProgram(int) {}
const juce::String TapeTopAudioProcessor::getProgramName(int) { return {}; }
void TapeTopAudioProcessor::changeProgramName(int, const juce::String&) {}

void TapeTopAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Reset biquad states
    for (int ch = 0; ch < 2; ++ch)
    {
        biquadState[ch].x1 = 0.0;
        biquadState[ch].x2 = 0.0;
        biquadState[ch].y1 = 0.0;
        biquadState[ch].y2 = 0.0;
    }

    lastHPF = hpfParam->get();
    calculateCoefficients(lastHPF);
}

void TapeTopAudioProcessor::releaseResources() {}

bool TapeTopAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut == juce::AudioChannelSet::stereo() && mainIn == juce::AudioChannelSet::stereo())
        return true;
    if (mainOut == juce::AudioChannelSet::mono() && mainIn == juce::AudioChannelSet::mono())
        return true;
    if (mainOut == juce::AudioChannelSet::stereo() && mainIn == juce::AudioChannelSet::mono())
        return true;

    return false;
}

void TapeTopAudioProcessor::calculateCoefficients(float hpfHz)
{
    // 2-pole Butterworth high-pass filter using bilinear transform
    const double fs = currentSampleRate;
    const double fc = juce::jlimit(10.0, fs * 0.49, (double)hpfHz);

    // Pre-warp
    const double omega = 2.0 * juce::MathConstants<double>::pi * fc / fs;
    const double K = std::tan(omega / 2.0);
    const double K2 = K * K;
    // Butterworth Q = 1/sqrt(2)
    const double Q = juce::MathConstants<double>::sqrt2 / 2.0; // 0.7071...
    const double norm = 1.0 / (1.0 + K / Q + K2);

    b0 = 1.0 * norm;
    b1 = -2.0 * norm;
    b2 = 1.0 * norm;
    a1 = 2.0 * (K2 - 1.0) * norm;
    a2 = (1.0 - K / Q + K2) * norm;
}

void TapeTopAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float currentHPF = hpfParam->get();
    if (std::abs(currentHPF - lastHPF) > 0.01f)
    {
        calculateCoefficients(currentHPF);
        lastHPF = currentHPF;
    }

    const float drive = driveParam->get();
    const float mix   = mixParam->get();

    const double inputGainMul = 1.0 + (drive / 100.0) * 4.0;
    const double mixWet  = mix / 100.0;
    const double mixDry  = 1.0 - mixWet;

    // tanh(0.7) for normalization
    const double tanhNormFactor = std::tanh(0.7);

    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Determine channels to process
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        BiquadState& state = biquadState[ch];

        for (int n = 0; n < numSamples; ++n)
        {
            const double dry = (double)channelData[n];

            // Biquad HPF
            const double xn = dry;
            const double yn = b0 * xn + b1 * state.x1 + b2 * state.x2
                              - a1 * state.y1 - a2 * state.y2;

            state.x2 = state.x1;
            state.x1 = xn;
            state.y2 = state.y1;
            state.y1 = yn;

            const double hp_signal = yn;

            // Apply drive
            const double driven = hp_signal * inputGainMul;

            // Tape saturation with normalization
            const double saturated = std::tanh(driven * 0.7) / tanhNormFactor;

            // Mix
            const double output = dry * mixDry + saturated * mixWet;

            channelData[n] = (float)output;
        }
    }

    // If stereo output but mono input, copy left to right
    if (totalOut >= 2 && buffer.getNumChannels() >= 2)
    {
        // Already handled since numChannels covers both channels
    }

    // Zero any extra output channels
    for (int ch = numChannels; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);
}

juce::AudioProcessorEditor* TapeTopAudioProcessor::createEditor()
{
    return new TapeTopAudioProcessorEditor(*this);
}

void TapeTopAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(driveParam->get());
    stream.writeFloat(hpfParam->get());
    stream.writeFloat(mixParam->get());
}

void TapeTopAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 12)
    {
        *driveParam = stream.readFloat();
        *hpfParam   = stream.readFloat();
        *mixParam   = stream.readFloat();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeTopAudioProcessor();
}