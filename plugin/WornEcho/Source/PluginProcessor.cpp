#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

WornEchoAudioProcessor::WornEchoAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    timeParam = new juce::AudioParameterFloat("time", "Time",
        juce::NormalisableRange<float>(50.0f, 2000.0f, 0.1f, 0.4f), 400.0f, "ms");
    addParameter(timeParam);

    feedbackParam = new juce::AudioParameterFloat("feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f), 40.0f, "%");
    addParameter(feedbackParam);

    wowParam = new juce::AudioParameterFloat("wow", "Wow",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f);
    addParameter(wowParam);

    flutterParam = new juce::AudioParameterFloat("flutter", "Flutter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f);
    addParameter(flutterParam);

    saturationParam = new juce::AudioParameterFloat("saturation", "Saturation",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f);
    addParameter(saturationParam);

    mixParam = new juce::AudioParameterFloat("mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f);
    addParameter(mixParam);
}

WornEchoAudioProcessor::~WornEchoAudioProcessor() {}

const juce::String WornEchoAudioProcessor::getName() const { return "WornEcho"; }
bool WornEchoAudioProcessor::acceptsMidi() const { return false; }
bool WornEchoAudioProcessor::producesMidi() const { return false; }
bool WornEchoAudioProcessor::isMidiEffect() const { return false; }

double WornEchoAudioProcessor::getTailLengthSeconds() const
{
    // Worst case: 2000ms delay with 95% feedback => very long tail
    // With 95% feedback, signal drops 60dB in about 60 repeats => 60 * 2s = 120s
    // Practically cap at a reasonable value
    return 40.0;
}

int WornEchoAudioProcessor::getNumPrograms() { return 1; }
int WornEchoAudioProcessor::getCurrentProgram() { return 0; }
void WornEchoAudioProcessor::setCurrentProgram(int) {}
const juce::String WornEchoAudioProcessor::getProgramName(int) { return {}; }
void WornEchoAudioProcessor::changeProgramName(int, const juce::String&) {}

void WornEchoAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    // Max delay 2000ms + extra for wow/flutter modulation
    maxDelayInSamples = static_cast<int>((2.5 * sampleRate) + 4);

    delayBufferL.assign(static_cast<size_t>(maxDelayInSamples), 0.0f);
    delayBufferR.assign(static_cast<size_t>(maxDelayInSamples), 0.0f);

    writePos = 0;
    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    feedbackL = 0.0f;
    feedbackR = 0.0f;
    lpStateL = 0.0f;
    lpStateR = 0.0f;
}

void WornEchoAudioProcessor::releaseResources()
{
    delayBufferL.clear();
    delayBufferR.clear();
}

bool WornEchoAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != mainIn)
        return false;

    if (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

float WornEchoAudioProcessor::tapeDistort(float x, float drive)
{
    if (drive <= 0.0f)
        return x;
    float amount = 1.0f + drive * 10.0f;
    return std::tanh(x * amount) / std::tanh(amount);
}

float WornEchoAudioProcessor::hermiteInterpolate(const std::vector<float>& buffer, float readPos)
{
    int bufSize = static_cast<int>(buffer.size());
    int idx1 = static_cast<int>(std::floor(readPos));
    float frac = readPos - static_cast<float>(idx1);

    auto wrap = [bufSize](int i) -> int {
        return ((i % bufSize) + bufSize) % bufSize;
    };

    float y0 = buffer[static_cast<size_t>(wrap(idx1 - 1))];
    float y1 = buffer[static_cast<size_t>(wrap(idx1))];
    float y2 = buffer[static_cast<size_t>(wrap(idx1 + 1))];
    float y3 = buffer[static_cast<size_t>(wrap(idx1 + 2))];

    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void WornEchoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear extra output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    const float delayMs     = timeParam->get();
    const float feedback     = feedbackParam->get() / 100.0f;
    const float wowAmount    = wowParam->get();
    const float flutterAmount = flutterParam->get();
    const float saturation   = saturationParam->get();
    const float mix          = mixParam->get();

    const float delaySamples = static_cast<float>(delayMs * 0.001 * currentSampleRate);

    // Wow: slow LFO ~0.5 Hz
    const float wowFreq = 0.5f;
    // Flutter: faster LFO ~6 Hz
    const float flutterFreq = 6.0f;

    const float wowPhaseInc = static_cast<float>(wowFreq / currentSampleRate);
    const float flutterPhaseInc = static_cast<float>(flutterFreq / currentSampleRate);

    // Max modulation depth in samples
    const float wowMaxDepth = static_cast<float>(0.003 * currentSampleRate); // 3ms
    const float flutterMaxDepth = static_cast<float>(0.0005 * currentSampleRate); // 0.5ms

    // Low-pass coefficient for tape warmth (around 4kHz)
    float lpCoeff = 1.0f - std::exp(static_cast<float>(-2.0 * juce::MathConstants<double>::pi * 4000.0 / currentSampleRate));

    float* channelL = buffer.getWritePointer(0);
    float* channelR = (totalOut >= 2) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float inputL = channelL[i];
        float inputR = (channelR != nullptr && totalIn >= 2) ? channelR[i] : inputL;

        // Wow and flutter modulation
        float wowMod = std::sin(2.0f * juce::MathConstants<float>::pi * wowPhase) * wowAmount * wowMaxDepth;
        float flutterMod = std::sin(2.0f * juce::MathConstants<float>::pi * flutterPhase) * flutterAmount * flutterMaxDepth;

        float totalMod = wowMod + flutterMod;
        float modulatedDelay = delaySamples + totalMod;

        // Clamp to valid range
        if (modulatedDelay < 1.0f) modulatedDelay = 1.0f;
        if (modulatedDelay > static_cast<float>(maxDelayInSamples - 4)) modulatedDelay = static_cast<float>(maxDelayInSamples - 4);

        // Read from delay buffer using hermite interpolation
        float readPosL = static_cast<float>(writePos) - modulatedDelay;
        if (readPosL < 0.0f) readPosL += static_cast<float>(maxDelayInSamples);

        float delayedL = hermiteInterpolate(delayBufferL, readPosL);
        float delayedR = hermiteInterpolate(delayBufferR, readPosL);

        // Apply low-pass filter for tape warmth
        lpStateL += lpCoeff * (delayedL - lpStateL);
        lpStateR += lpCoeff * (delayedR - lpStateR);
        delayedL = lpStateL;
        delayedR = lpStateR;

        // Apply tape saturation to the delayed/feedback signal
        delayedL = tapeDistort(delayedL, saturation);
        delayedR = tapeDistort(delayedR, saturation);

        // Write to delay buffer: input + feedback
        delayBufferL[static_cast<size_t>(writePos)] = inputL + feedbackL;
        delayBufferR[static_cast<size_t>(writePos)] = inputR + feedbackR;

        // Store feedback for next sample
        feedbackL = delayedL * feedback;
        feedbackR = delayedR * feedback;

        // Soft-clip feedback to prevent runaway
        feedbackL = std::tanh(feedbackL);
        feedbackR = std::tanh(feedbackR);

        // Mix dry/wet
        float outL = inputL * (1.0f - mix) + delayedL * mix;
        float outR = inputR * (1.0f - mix) + delayedR * mix;

        channelL[i] = outL;
        if (channelR != nullptr)
            channelR[i] = outR;

        // Advance write position
        writePos++;
        if (writePos >= maxDelayInSamples)
            writePos = 0;

        // Advance LFO phases
        wowPhase += wowPhaseInc;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += flutterPhaseInc;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
    }

    // If mono output, we already wrote to channelL
    // If stereo output with mono input, copy L to R handled above
}

juce::AudioProcessorEditor* WornEchoAudioProcessor::createEditor()
{
    return new WornEchoAudioProcessorEditor(*this);
}

void WornEchoAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(timeParam->get());
    stream.writeFloat(feedbackParam->get());
    stream.writeFloat(wowParam->get());
    stream.writeFloat(flutterParam->get());
    stream.writeFloat(saturationParam->get());
    stream.writeFloat(mixParam->get());
}

void WornEchoAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getDataSize() >= 6 * sizeof(float))
    {
        *timeParam       = stream.readFloat();
        *feedbackParam   = stream.readFloat();
        *wowParam        = stream.readFloat();
        *flutterParam    = stream.readFloat();
        *saturationParam = stream.readFloat();
        *mixParam        = stream.readFloat();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WornEchoAudioProcessor();
}
