#include "PluginProcessor.h"
#include <cmath>

static const int apfBaseLengths[4] = { 113, 337, 571, 953 };
static const int delayBaseLengths[4] = { 1553, 2203, 3319, 4457 };

PurpleplatereverbeAudioProcessor::PurpleplatereverbeAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(decayParam = new juce::AudioParameterFloat("decay", "Decay", 0.1f, 10.0f, 3.0f));
    addParameter(dampingParam = new juce::AudioParameterFloat("damping", "Damping", 0.0f, 1.0f, 0.5f));
    addParameter(predelayParam = new juce::AudioParameterFloat("predelay", "Pre-Delay (ms)", 0.0f, 200.0f, 20.0f));
    addParameter(mixParam = new juce::AudioParameterFloat("mix", "Mix", 0.0f, 1.0f, 0.35f));
    addParameter(modRateParam = new juce::AudioParameterFloat("modrate", "Mod Rate (Hz)", 0.05f, 5.0f, 0.8f));
    addParameter(modDepthParam = new juce::AudioParameterFloat("moddepth", "Mod Depth", 0.0f, 1.0f, 0.3f));
    addParameter(gateThreshParam = new juce::AudioParameterFloat("gatethresh", "Gate Threshold (dB)", -80.0f, 0.0f, -80.0f));
    addParameter(gateReleaseParam = new juce::AudioParameterFloat("gaterelease", "Gate Release (ms)", 10.0f, 2000.0f, 200.0f));
    addParameter(widthParam = new juce::AudioParameterFloat("width", "Width", 0.0f, 1.0f, 1.0f));
    addParameter(highCutParam = new juce::AudioParameterFloat("highcut", "High Cut (Hz)", 1000.0f, 20000.0f, 12000.0f));

    apfIndex.fill(0);
    delayIndex.fill(0);
    dampState.fill(0.0f);
}

PurpleplatereverbeAudioProcessor::~PurpleplatereverbeAudioProcessor() {}

const juce::String PurpleplatereverbeAudioProcessor::getName() const { return "PurplePlateReverb"; }
bool PurpleplatereverbeAudioProcessor::acceptsMidi() const { return false; }
bool PurpleplatereverbeAudioProcessor::producesMidi() const { return false; }
bool PurpleplatereverbeAudioProcessor::isMidiEffect() const { return false; }
double PurpleplatereverbeAudioProcessor::getTailLengthSeconds() const { return 10.0; }
int PurpleplatereverbeAudioProcessor::getNumPrograms() { return 1; }
int PurpleplatereverbeAudioProcessor::getCurrentProgram() { return 0; }
void PurpleplatereverbeAudioProcessor::setCurrentProgram(int) {}
const juce::String PurpleplatereverbeAudioProcessor::getProgramName(int) { return {}; }
void PurpleplatereverbeAudioProcessor::changeProgramName(int, const juce::String&) {}

bool PurpleplatereverbeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn = layouts.getMainInputChannelSet();
    if (mainOut.isDisabled()) return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo()) return false;
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo() && !mainIn.isDisabled()) return false;
    return true;
}

void PurpleplatereverbeAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    float srFactor = (float)(sampleRate / 29761.0); // scale lengths relative to 29761 Hz reference

    for (int i = 0; i < kNumAPF; ++i)
    {
        apfLengths[i] = juce::jmax(1, (int)(apfBaseLengths[i] * srFactor));
        apfBuffers[i].setSize(1, apfLengths[i] + 64);
        apfBuffers[i].clear();
        apfIndex[i] = 0;
    }

    for (int i = 0; i < kNumDelays; ++i)
    {
        delayLengths[i] = juce::jmax(1, (int)(delayBaseLengths[i] * srFactor));
        delayBuffers[i].setSize(1, delayLengths[i] + 64);
        delayBuffers[i].clear();
        delayIndex[i] = 0;
    }

    maxPredelaySamples = (int)(0.2 * sampleRate) + 1;
    predelayBuffer.setSize(1, maxPredelaySamples + 1);
    predelayBuffer.clear();
    predelayIndex = 0;

    dampState.fill(0.0f);
    lfoPhase = 0.0f;
    gateEnv = 0.0f;
    tankL = 0.0f;
    tankR = 0.0f;
    highCutStateL = 0.0f;
    highCutStateR = 0.0f;
}

void PurpleplatereverbeAudioProcessor::releaseResources() {}

float PurpleplatereverbeAudioProcessor::readDelayInterp(juce::AudioBuffer<float>& buf, int writeIdx, float delaySamples, int length)
{
    float readPos = (float)writeIdx - delaySamples;
    while (readPos < 0.0f) readPos += (float)length;
    int idx0 = (int)readPos;
    int idx1 = idx0 + 1;
    if (idx1 >= length) idx1 -= length;
    float frac = readPos - (float)idx0;
    const float* data = buf.getReadPointer(0);
    return data[idx0] + frac * (data[idx1] - data[idx0]);
}

void PurpleplatereverbeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, numSamples);

    const float decay = decayParam->get();
    const float damping = dampingParam->get();
    const float predelayMs = predelayParam->get();
    const float mix = mixParam->get();
    const float modRate = modRateParam->get();
    const float modDepth = modDepthParam->get();
    const float gateThreshDb = gateThreshParam->get();
    const float gateReleaseMs = gateReleaseParam->get();
    const float width = widthParam->get();
    const float highCut = highCutParam->get();

    const float gateThreshLin = juce::Decibels::decibelsToGain(gateThreshDb);
    const float gateReleaseCoeff = std::exp(-1.0f / (float)(currentSampleRate * gateReleaseMs * 0.001));

    // Compute feedback factor from decay time
    // Approximate: feedback = 10^(-3 * avgDelayTime / decay)
    float avgDelay = 0.0f;
    for (int i = 0; i < kNumDelays; ++i)
        avgDelay += (float)delayLengths[i];
    avgDelay /= (float)(kNumDelays * currentSampleRate);
    float feedback = std::pow(10.0f, -3.0f * avgDelay / juce::jmax(decay, 0.1f));
    feedback = juce::jlimit(0.0f, 0.98f, feedback);

    int predelaySamps = juce::jlimit(0, maxPredelaySamples - 1, (int)(predelayMs * 0.001f * (float)currentSampleRate));

    // High-cut coefficient (one-pole)
    float hcCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * highCut / (float)currentSampleRate);
    hcCoeff = juce::jlimit(0.0f, 1.0f, hcCoeff);

    const float lfoInc = modRate / (float)currentSampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        // Sum input to mono
        float inputSample = 0.0f;
        for (int ch = 0; ch < totalIn; ++ch)
            inputSample += buffer.getReadPointer(ch)[n];
        if (totalIn > 1) inputSample *= 0.5f;

        // Gate envelope follower
        float absInput = std::fabs(inputSample);
        if (absInput > gateEnv)
            gateEnv = absInput;
        else
            gateEnv = gateReleaseCoeff * gateEnv;

        float gateGain = (gateEnv > gateThreshLin) ? 1.0f : (gateEnv / juce::jmax(gateThreshLin, 1e-10f));
        gateGain = juce::jlimit(0.0f, 1.0f, gateGain);

        // Pre-delay
        float* pdData = predelayBuffer.getWritePointer(0);
        pdData[predelayIndex] = inputSample;
        int pdReadIdx = predelayIndex - predelaySamps;
        if (pdReadIdx < 0) pdReadIdx += maxPredelaySamples;
        float pdOut = pdData[pdReadIdx];
        predelayIndex = (predelayIndex + 1) % maxPredelaySamples;

        // Input diffusion (all-pass chain)
        float diffused = pdOut;
        for (int a = 0; a < kNumAPF; ++a)
        {
            float* apfData = apfBuffers[a].getWritePointer(0);
            int len = apfLengths[a];
            int rIdx = apfIndex[a] - len;
            if (rIdx < 0) rIdx += (len + 64);
            float delayed = apfData[rIdx % (len + 64)];
            float temp = diffused + apfGain * delayed;
            float out = delayed - apfGain * temp;
            apfData[apfIndex[a] % (len + 64)] = temp;
            apfIndex[a] = (apfIndex[a] + 1) % (len + 64);
            diffused = out;
        }

        // LFO
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = std::sin(2.0f * juce::MathConstants<float>::pi * lfoPhase);
        float modSamples = lfo * modDepth * 30.0f; // max 30 samples modulation

        // Tank processing
        float tankInL = diffused + tankR * feedback;
        float tankInR = diffused + tankL * feedback;

        float outL = 0.0f, outR = 0.0f;

        // Delay 0 and 1 for left
        for (int d = 0; d < 2; ++d)
        {
            float* dData = delayBuffers[d].getWritePointer(0);
            int len = delayLengths[d];
            int totalLen = len + 64;
            dData[delayIndex[d] % totalLen] = (d == 0) ? tankInL : tankInL * 0.8f;
            float effectiveDelay = (float)len + modSamples * ((d == 0) ? 1.0f : -0.7f);
            effectiveDelay = juce::jlimit(1.0f, (float)(totalLen - 2), effectiveDelay);
            float rd = readDelayInterp(delayBuffers[d], delayIndex[d] % totalLen, effectiveDelay, totalLen);

            // Damping
            dampState[d] = dampState[d] + damping * (rd - dampState[d]);
            // Actually: low pass is dampState = (1-damping)*rd + damping*dampState
            dampState[d] = (1.0f - damping) * rd + damping * dampState[d];

            outL += dampState[d];
            delayIndex[d] = (delayIndex[d] + 1) % totalLen;
        }

        // Delay 2 and 3 for right
        for (int d = 2; d < 4; ++d)
        {
            float* dData = delayBuffers[d].getWritePointer(0);
            int len = delayLengths[d];
            int totalLen = len + 64;
            dData[delayIndex[d] % totalLen] = (d == 2) ? tankInR : tankInR * 0.8f;
            float effectiveDelay = (float)len + modSamples * ((d == 2) ? -1.0f : 0.7f);
            effectiveDelay = juce::jlimit(1.0f, (float)(totalLen - 2), effectiveDelay);
            float rd = readDelayInterp(delayBuffers[d], delayIndex[d] % totalLen, effectiveDelay, totalLen);

            dampState[d] = (1.0f - damping) * rd + damping * dampState[d];

            outR += dampState[d];
            delayIndex[d] = (delayIndex[d] + 1) % totalLen;
        }

        outL *= 0.5f;
        outR *= 0.5f;

        tankL = outL;
        tankR = outR;

        // Apply gate
        outL *= gateGain;
        outR *= gateGain;

        // High-cut filter
        highCutStateL += hcCoeff * (outL - highCutStateL);
        highCutStateR += hcCoeff * (outR - highCutStateR);
        outL = highCutStateL;
        outR = highCutStateR;

        // Width processing
        float mid = (outL + outR) * 0.5f;
        float side = (outL - outR) * 0.5f;
        outL = mid + side * width;
        outR = mid - side * width;

        // Mix
        float dryL = (totalIn > 0) ? buffer.getReadPointer(0)[n] : 0.0f;
        float dryR = (totalIn > 1) ? buffer.getReadPointer(1)[n] : dryL;

        if (totalOut >= 2)
        {
            buffer.getWritePointer(0)[n] = dryL * (1.0f - mix) + outL * mix;
            buffer.getWritePointer(1)[n] = dryR * (1.0f - mix) + outR * mix;
        }
        else if (totalOut == 1)
        {
            float monoWet = (outL + outR) * 0.5f;
            buffer.getWritePointer(0)[n] = dryL * (1.0f - mix) + monoWet * mix;
        }
    }
}

juce::AudioProcessorEditor* PurpleplatereverbeAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void PurpleplatereverbeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    mos.writeFloat(decayParam->get());
    mos.writeFloat(dampingParam->get());
    mos.writeFloat(predelayParam->get());
    mos.writeFloat(mixParam->get());
    mos.writeFloat(modRateParam->get());
    mos.writeFloat(modDepthParam->get());
    mos.writeFloat(gateThreshParam->get());
    mos.writeFloat(gateReleaseParam->get());
    mos.writeFloat(widthParam->get());
    mos.writeFloat(highCutParam->get());
}

void PurpleplatereverbeAudioProcessor::setStateInformation(const void* data, int dataSize)
{
    juce::MemoryInputStream mis(data, static_cast<size_t>(dataSize), false);
    if (dataSize >= 40)
    {
        *decayParam = mis.readFloat();
        *dampingParam = mis.readFloat();
        *predelayParam = mis.readFloat();
        *mixParam = mis.readFloat();
        *modRateParam = mis.readFloat();
        *modDepthParam = mis.readFloat();
        *gateThreshParam = mis.readFloat();
        *gateReleaseParam = mis.readFloat();
        *widthParam = mis.readFloat();
        *highCutParam = mis.readFloat();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurpleplatereverbeAudioProcessor();
}
