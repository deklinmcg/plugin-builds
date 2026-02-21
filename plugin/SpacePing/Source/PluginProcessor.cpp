#include "PluginProcessor.h"
#include <cmath>

static float softClip(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x - (x * x * x) / 3.0f;
}

SpacePingAudioProcessor::SpacePingAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(timeParam = new juce::AudioParameterFloat("time", "Time",
        juce::NormalisableRange<float>(50.0f, 2000.0f, 1.0f, 0.5f), 400.0f));
    addParameter(feedbackParam = new juce::AudioParameterFloat("feedback", "Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f));
    addParameter(wowFlutterParam = new juce::AudioParameterFloat("wow_flutter", "Wow & Flutter",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f));
    addParameter(toneParam = new juce::AudioParameterFloat("tone", "Tone",
        juce::NormalisableRange<float>(200.0f, 8000.0f, 1.0f, 0.4f), 3500.0f));
    addParameter(spreadParam = new juce::AudioParameterFloat("spread", "Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 75.0f));
    addParameter(mixParam = new juce::AudioParameterFloat("mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
}

SpacePingAudioProcessor::~SpacePingAudioProcessor() {}

const juce::String SpacePingAudioProcessor::getName() const { return "SpacePing"; }
bool SpacePingAudioProcessor::acceptsMidi() const { return false; }
bool SpacePingAudioProcessor::producesMidi() const { return false; }
bool SpacePingAudioProcessor::isMidiEffect() const { return false; }

double SpacePingAudioProcessor::getTailLengthSeconds() const
{
    // Max delay 2s, with high feedback can ring for a long time
    return 20.0;
}

int SpacePingAudioProcessor::getNumPrograms() { return 1; }
int SpacePingAudioProcessor::getCurrentProgram() { return 0; }
void SpacePingAudioProcessor::setCurrentProgram(int) {}
const juce::String SpacePingAudioProcessor::getProgramName(int) { return {}; }
void SpacePingAudioProcessor::changeProgramName(int, const juce::String&) {}

void SpacePingAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Max delay = 2000ms + some extra for wow/flutter modulation (up to ~10ms)
    delayBufferSize = (int)(sampleRate * 2.5) + 1;
    delayBufferL.setSize(1, delayBufferSize);
    delayBufferR.setSize(1, delayBufferSize);
    delayBufferL.clear();
    delayBufferR.clear();
    writePositionL = 0;
    writePositionR = 0;

    lpStateL = 0.0f;
    lpStateR = 0.0f;
    wowPhase = 0.0f;
    flutterPhase = 0.0f;
}

void SpacePingAudioProcessor::releaseResources()
{
    delayBufferL.setSize(0, 0);
    delayBufferR.setSize(0, 0);
}

bool SpacePingAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut.isDisabled())
        return false;

    // Accept mono or stereo
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Input can be mono or stereo, or disabled
    if (!mainIn.isDisabled() && mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

float SpacePingAudioProcessor::tapeWarmth(float sample)
{
    // Soft saturation to emulate tape
    return softClip(sample * 1.2f) * 0.85f;
}

void SpacePingAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Read parameters
    const float delayTimeMs = timeParam->get();
    const float feedbackPct = feedbackParam->get() / 100.0f;
    const float wfAmount = wowFlutterParam->get() / 100.0f;
    const float toneCutoff = toneParam->get();
    const float spreadPct = spreadParam->get() / 100.0f;
    const float mixPct = mixParam->get() / 100.0f;

    const float sr = (float)currentSampleRate;

    // One-pole low-pass coefficient
    const float lpCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneCutoff / sr);

    // Wow frequency ~0.5 Hz, flutter ~6 Hz
    const float wowFreq = 0.5f;
    const float flutterFreq = 6.0f;
    // Max modulation depth in samples (at max wow/flutter)
    const float maxWowDepthMs = 3.0f;   // ms
    const float maxFlutterDepthMs = 0.5f; // ms
    const float wowDepthSamples = wfAmount * maxWowDepthMs * 0.001f * sr;
    const float flutterDepthSamples = wfAmount * maxFlutterDepthMs * 0.001f * sr;

    // Base delay in samples
    const float baseDelaySamples = delayTimeMs * 0.001f * sr;

    // Spread: offset left/right delay times
    // At spread=100%, right delay = 1.5x left, left = 0.75x base (ping pong asymmetry)
    // At spread=0%, both identical
    const float spreadFactor = spreadPct;
    const float leftDelayRatio = 1.0f - spreadFactor * 0.25f;  // 0.75 to 1.0
    const float rightDelayRatio = 1.0f + spreadFactor * 0.5f;  // 1.0 to 1.5

    float* delayDataL = delayBufferL.getWritePointer(0);
    float* delayDataR = delayBufferR.getWritePointer(0);

    // Get pointers
    const bool isStereoOut = (totalNumOutputChannels >= 2);
    const bool isStereoIn = (totalNumInputChannels >= 2);

    float* outL = buffer.getWritePointer(0);
    float* outR = isStereoOut ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Input: sum to mono for ping-pong feed
        float inputL = outL[i];
        float inputR = isStereoIn ? outR[i] : inputL;
        float inputMono = (inputL + inputR) * 0.5f;

        // Update wow and flutter LFOs
        float wowMod = std::sin(2.0f * juce::MathConstants<float>::pi * wowPhase);
        float flutterMod = std::sin(2.0f * juce::MathConstants<float>::pi * flutterPhase);
        wowPhase += wowFreq / sr;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += flutterFreq / sr;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

        float modOffsetSamples = wowMod * wowDepthSamples + flutterMod * flutterDepthSamples;

        // Left delay read
        float delayL = baseDelaySamples * leftDelayRatio + modOffsetSamples;
        delayL = juce::jlimit(1.0f, (float)(delayBufferSize - 2), delayL);
        float readPosL = (float)writePositionL - delayL;
        if (readPosL < 0.0f) readPosL += (float)delayBufferSize;
        int idxL0 = (int)readPosL;
        int idxL1 = (idxL0 + 1) % delayBufferSize;
        float fracL = readPosL - (float)idxL0;
        float tapL = delayDataL[idxL0] + fracL * (delayDataL[idxL1] - delayDataL[idxL0]);

        // Right delay read
        float delayR = baseDelaySamples * rightDelayRatio + modOffsetSamples * 0.8f;
        delayR = juce::jlimit(1.0f, (float)(delayBufferSize - 2), delayR);
        float readPosR = (float)writePositionR - delayR;
        if (readPosR < 0.0f) readPosR += (float)delayBufferSize;
        int idxR0 = (int)readPosR;
        int idxR1 = (idxR0 + 1) % delayBufferSize;
        float fracR = readPosR - (float)idxR0;
        float tapR = delayDataR[idxR0] + fracR * (delayDataR[idxR1] - delayDataR[idxR0]);

        // Apply tone filtering (one-pole LP)
        lpStateL += lpCoeff * (tapL - lpStateL);
        lpStateR += lpCoeff * (tapR - lpStateR);
        tapL = lpStateL;
        tapR = lpStateR;

        // Tape warmth/saturation
        tapL = tapeWarmth(tapL);
        tapR = tapeWarmth(tapR);

        // Ping pong: Left feeds right, right feeds left
        // Write into delay buffers
        float writeL = inputMono + tapR * feedbackPct;
        float writeR = inputMono + tapL * feedbackPct;

        // Slight degradation on each pass (reduce high end further via additional gentle LP)
        writeL = writeL * 0.98f;
        writeR = writeR * 0.98f;

        delayDataL[writePositionL] = writeL;
        delayDataR[writePositionR] = writeR;

        writePositionL = (writePositionL + 1) % delayBufferSize;
        writePositionR = (writePositionR + 1) % delayBufferSize;

        // Mix dry/wet
        float dryL = inputL;
        float dryR = inputR;
        float wetL = tapL;
        float wetR = tapR;

        float outSampleL = dryL * (1.0f - mixPct) + wetL * mixPct;
        float outSampleR = dryR * (1.0f - mixPct) + wetR * mixPct;

        outL[i] = outSampleL;
        if (isStereoOut)
        {
            if (outR != nullptr)
                outR[i] = outSampleR;
        }
    }

    // If mono output, we already wrote to channel 0
    if (!isStereoOut && totalNumOutputChannels == 1)
    {
        // Already handled
    }
}

juce::AudioProcessorEditor* SpacePingAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void SpacePingAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(timeParam->get());
    stream.writeFloat(feedbackParam->get());
    stream.writeFloat(wowFlutterParam->get());
    stream.writeFloat(toneParam->get());
    stream.writeFloat(spreadParam->get());
    stream.writeFloat(mixParam->get());
}

void SpacePingAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= (int)(6 * sizeof(float)))
    {
        timeParam->setValueNotifyingHost(timeParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        feedbackParam->setValueNotifyingHost(feedbackParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        wowFlutterParam->setValueNotifyingHost(wowFlutterParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        toneParam->setValueNotifyingHost(toneParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        spreadParam->setValueNotifyingHost(spreadParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
        mixParam->setValueNotifyingHost(mixParam->getNormalisableRange().convertTo0to1(stream.readFloat()));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpacePingAudioProcessor();
}
