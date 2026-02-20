#include "PluginProcessor.h"
#include <cmath>

// Prime-ish delay lengths for plate reverb (in samples at 44100)
static const int kAllpassLengths[8] = { 142, 107, 379, 277, 573, 419, 751, 563 };
static const int kDelayLengths[4] = { 4453, 3720, 4217, 3163 };
static const float kAllpassCoeffs[8] = { 0.7f, 0.7f, 0.6f, 0.6f, 0.55f, 0.55f, 0.5f, 0.5f };

PurplePlateReverbAudioProcessor::PurplePlateReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(sizeParam       = new juce::AudioParameterFloat("size",       "Size",              0.1f, 1.0f, 0.5f));
    addParameter(decayParam      = new juce::AudioParameterFloat("decay",      "Decay",             0.1f, 10.0f, 2.0f));
    addParameter(brightnessParam = new juce::AudioParameterFloat("brightness", "Brightness",        0.0f, 1.0f, 0.5f));
    addParameter(mixParam        = new juce::AudioParameterFloat("mix",        "Mix",               0.0f, 1.0f, 0.5f));
    addParameter(modDepthParam   = new juce::AudioParameterFloat("mod_depth",  "Modulation Depth",  0.0f, 1.0f, 0.2f));
    addParameter(gateParam       = new juce::AudioParameterFloat("gate",       "Gating",            0.0f, 1.0f, 0.0f));
    addParameter(gateRateParam   = new juce::AudioParameterFloat("gate_rate",  "Gate Rate",         0.1f, 8.0f, 1.0f));
}

PurplePlateReverbAudioProcessor::~PurplePlateReverbAudioProcessor() {}

const juce::String PurplePlateReverbAudioProcessor::getName() const { return "PurplePlateReverb"; }
bool PurplePlateReverbAudioProcessor::acceptsMidi() const { return false; }
bool PurplePlateReverbAudioProcessor::producesMidi() const { return false; }
bool PurplePlateReverbAudioProcessor::isMidiEffect() const { return false; }
double PurplePlateReverbAudioProcessor::getTailLengthSeconds() const { return 10.0; }
int PurplePlateReverbAudioProcessor::getNumPrograms() { return 1; }
int PurplePlateReverbAudioProcessor::getCurrentProgram() { return 0; }
void PurplePlateReverbAudioProcessor::setCurrentProgram(int) {}
const juce::String PurplePlateReverbAudioProcessor::getProgramName(int) { return {}; }
void PurplePlateReverbAudioProcessor::changeProgramName(int, const juce::String&) {}

bool PurplePlateReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PurplePlateReverbAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    double sampleRateRatio = sampleRate / 44100.0;

    // Allocate allpass buffers
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < kNumAllpasses; ++i)
        {
            int len = (int)(kAllpassLengths[i] * sampleRateRatio);
            if (len < 1) len = 1;
            // Slightly different lengths for second channel for stereo decorrelation
            if (ch == 1) len = (int)(len * 1.07f);
            allpasses[ch][i].length = len;
            allpasses[ch][i].buffer.setSize(1, len + 16);
            allpasses[ch][i].buffer.clear();
            allpasses[ch][i].writeIndex = 0;
            allpasses[ch][i].feedback = kAllpassCoeffs[i];
        }

        for (int i = 0; i < kNumDelays; ++i)
        {
            int len = (int)(kDelayLengths[i] * sampleRateRatio);
            if (len < 1) len = 1;
            if (ch == 1) len = (int)(len * 1.11f);
            delays[ch][i].length = len;
            delays[ch][i].buffer.setSize(1, len + 64);
            delays[ch][i].buffer.clear();
            delays[ch][i].writeIndex = 0;
        }

        feedbackState[ch] = 0.0f;
        lpState[ch] = 0.0f;
    }

    // Pre-delay buffer
    preDelayLength = (int)(0.05 * sampleRate); // 50ms max predelay
    if (preDelayLength < 1) preDelayLength = 1;
    preDelayBuffer.setSize(2, preDelayLength + 16);
    preDelayBuffer.clear();
    preDelayWriteIndex = 0;

    modPhase = 0.0f;
    gatePhase = 0.0f;
}

void PurplePlateReverbAudioProcessor::releaseResources() {}

float PurplePlateReverbAudioProcessor::readAllpass(AllpassFilter& ap, float input)
{
    float* buf = ap.buffer.getWritePointer(0);
    int readIdx = ap.writeIndex - ap.length;
    if (readIdx < 0) readIdx += ap.buffer.getNumSamples();

    float delayed = buf[readIdx];
    float output = -input + delayed;
    float written = input + delayed * ap.feedback;
    buf[ap.writeIndex] = written;
    ap.writeIndex++;
    if (ap.writeIndex >= ap.buffer.getNumSamples())
        ap.writeIndex = 0;
    return output;
}

float PurplePlateReverbAudioProcessor::readDelay(DelayLine& dl, int offset)
{
    int idx = dl.writeIndex - offset;
    while (idx < 0) idx += dl.buffer.getNumSamples();
    return dl.buffer.getReadPointer(0)[idx];
}

float PurplePlateReverbAudioProcessor::readDelayInterp(DelayLine& dl, float offset)
{
    float fidx = (float)dl.writeIndex - offset;
    while (fidx < 0.0f) fidx += (float)dl.buffer.getNumSamples();
    int i0 = (int)fidx;
    int i1 = i0 + 1;
    float frac = fidx - (float)i0;
    int numSamples = dl.buffer.getNumSamples();
    i0 = i0 % numSamples;
    i1 = i1 % numSamples;
    const float* buf = dl.buffer.getReadPointer(0);
    return buf[i0] + frac * (buf[i1] - buf[i0]);
}

void PurplePlateReverbAudioProcessor::writeDelay(DelayLine& dl, float sample)
{
    dl.buffer.getWritePointer(0)[dl.writeIndex] = sample;
    dl.writeIndex++;
    if (dl.writeIndex >= dl.buffer.getNumSamples())
        dl.writeIndex = 0;
}

void PurplePlateReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear extra output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Grab parameters
    const float size       = sizeParam->get();
    const float decay      = decayParam->get();
    const float brightness = brightnessParam->get();
    const float mix        = mixParam->get();
    const float modDepth   = modDepthParam->get();
    const float gate       = gateParam->get();
    const float gateRate   = gateRateParam->get();

    // Compute feedback coefficient from decay
    // At decay=10s, we want feedback close to 1. At decay=0.1s, close to 0.
    // Average delay loop length ~ sum of delay lengths / sr
    double avgLoopTime = 0.0;
    for (int i = 0; i < kNumDelays; ++i)
        avgLoopTime += (double)delays[0][i].length;
    avgLoopTime /= currentSampleRate;
    if (avgLoopTime < 0.001) avgLoopTime = 0.001;

    // fb^(1/avgLoopTime) = e^(-3/decay) => fb = e^(-3*avgLoopTime/decay)
    float fb = std::exp(-3.0f * (float)avgLoopTime / juce::jmax(decay, 0.1f));
    fb = juce::jlimit(0.0f, 0.99f, fb);

    // Lowpass coefficient for brightness: higher brightness = higher cutoff
    float lpCoeff = 0.05f + brightness * 0.9f; // range [0.05, 0.95]

    // Modulation rate ~0.5 Hz
    float modRate = 0.5f;
    float modPhaseInc = (float)(modRate / currentSampleRate);
    float gatePhaseInc = (float)(gateRate / currentSampleRate);

    // Size scaling for allpass/delay read offsets
    float sizeScale = 0.3f + size * 0.7f; // range [0.3, 1.0]

    // Pre-delay amount scaled by size
    int preDelaySamples = (int)(size * 0.04f * (float)currentSampleRate);
    preDelaySamples = juce::jlimit(1, preDelayLength - 1, preDelaySamples);

    for (int n = 0; n < numSamples; ++n)
    {
        // Get mono input
        float inL = buffer.getReadPointer(0)[n];
        float inR = (totalIn >= 2) ? buffer.getReadPointer(1)[n] : inL;
        float monoIn = (inL + inR) * 0.5f;

        // Write to pre-delay
        if (preDelayWriteIndex >= preDelayBuffer.getNumSamples())
            preDelayWriteIndex = 0;
        preDelayBuffer.getWritePointer(0)[preDelayWriteIndex] = monoIn;
        if (preDelayBuffer.getNumChannels() > 1)
            preDelayBuffer.getWritePointer(1)[preDelayWriteIndex] = monoIn;

        // Read from pre-delay
        int pdReadIdx = preDelayWriteIndex - preDelaySamples;
        if (pdReadIdx < 0) pdReadIdx += preDelayBuffer.getNumSamples();
        float preDelayed = preDelayBuffer.getReadPointer(0)[pdReadIdx];
        preDelayWriteIndex++;

        // Modulation LFO
        float modLFO = std::sin(2.0f * juce::MathConstants<float>::pi * modPhase);
        modPhase += modPhaseInc;
        if (modPhase >= 1.0f) modPhase -= 1.0f;

        // Gate LFO (square-ish wave with smooth transitions)
        float gateLFO = 0.5f + 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * gatePhase);
        gatePhase += gatePhaseInc;
        if (gatePhase >= 1.0f) gatePhase -= 1.0f;

        // Gate amplitude: when gate=0, no gating (mult=1). When gate=1, full gating
        float gateMult = 1.0f - gate * (1.0f - gateLFO);

        float wetL = 0.0f, wetR = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            // Input to reverb network: pre-delayed input + feedback
            float input = preDelayed + feedbackState[ch] * fb;

            // Allpass chain (first 4 = input diffusion)
            float sig = input;
            for (int i = 0; i < 4; ++i)
            {
                sig = readAllpass(allpasses[ch][i], sig);
            }

            // Write into first delay line
            writeDelay(delays[ch][0], sig);

            // Read from first delay with modulation
            float modOffset = modDepth * modLFO * 16.0f; // up to 16 samples pitch drift
            float d0len = (float)delays[ch][0].length * sizeScale;
            float read0 = readDelayInterp(delays[ch][0], d0len + modOffset);

            // Lowpass damping
            lpState[ch] = lpState[ch] + lpCoeff * (read0 - lpState[ch]);
            float damped = lpState[ch];

            // Second allpass pair
            float sig2 = damped;
            for (int i = 4; i < 6; ++i)
            {
                sig2 = readAllpass(allpasses[ch][i], sig2);
            }

            // Second delay
            writeDelay(delays[ch][1], sig2);
            float read1 = readDelay(delays[ch][1], (int)(delays[ch][1].length * sizeScale));

            // Third allpass pair
            float sig3 = read1;
            for (int i = 6; i < 8; ++i)
            {
                sig3 = readAllpass(allpasses[ch][i], sig3);
            }

            // Third delay
            writeDelay(delays[ch][2], sig3);
            float read2 = readDelay(delays[ch][2], (int)(delays[ch][2].length * sizeScale));

            // Fourth delay for extra diffusion
            writeDelay(delays[ch][3], read2);
            float read3 = readDelay(delays[ch][3], (int)(delays[ch][3].length * sizeScale));

            // Feedback state
            feedbackState[ch] = read3;

            // Output taps from various delay lines
            float wet = read0 * 0.3f + read1 * 0.3f + read2 * 0.2f + read3 * 0.2f;

            // Apply gate
            wet *= gateMult;

            if (ch == 0) wetL = wet;
            else         wetR = wet;
        }

        // Soft clip the wet signal to prevent blowups
        wetL = std::tanh(wetL);
        wetR = std::tanh(wetR);

        // Mix
        if (totalOut >= 2)
        {
            buffer.getWritePointer(0)[n] = inL * (1.0f - mix) + wetL * mix;
            buffer.getWritePointer(1)[n] = inR * (1.0f - mix) + wetR * mix;
        }
        else
        {
            float monoWet = (wetL + wetR) * 0.5f;
            float monoOrig = (inL + inR) * 0.5f;
            buffer.getWritePointer(0)[n] = monoOrig * (1.0f - mix) + monoWet * mix;
        }
    }
}

juce::AudioProcessorEditor* PurplePlateReverbAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void PurplePlateReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(sizeParam->get());
    stream.writeFloat(decayParam->get());
    stream.writeFloat(brightnessParam->get());
    stream.writeFloat(mixParam->get());
    stream.writeFloat(modDepthParam->get());
    stream.writeFloat(gateParam->get());
    stream.writeFloat(gateRateParam->get());
}

void PurplePlateReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (sizeInBytes >= (int)(7 * sizeof(float)))
    {
        *sizeParam       = stream.readFloat();
        *decayParam      = stream.readFloat();
        *brightnessParam = stream.readFloat();
        *mixParam        = stream.readFloat();
        *modDepthParam   = stream.readFloat();
        *gateParam       = stream.readFloat();
        *gateRateParam   = stream.readFloat();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurplePlateReverbAudioProcessor();
}
