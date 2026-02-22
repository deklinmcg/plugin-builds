#include "PluginProcessor.h"
#include "PluginEditor.h"

// ------------------------------------------------------------
JunoBBDAudioProcessor::JunoBBDAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(modeParam  = new juce::AudioParameterFloat({"mode",  1}, "Mode",  0.0f,  1.0f, 0.0f));
    addParameter(rateParam  = new juce::AudioParameterFloat({"rate",  1}, "Rate",  0.1f, 10.0f, 0.5f));
    addParameter(depthParam = new juce::AudioParameterFloat({"depth", 1}, "Depth", 0.0f,  1.0f, 0.5f));
    addParameter(driftParam = new juce::AudioParameterFloat({"drift", 1}, "Drift", 0.0f,  1.0f, 0.2f));
    addParameter(toneParam  = new juce::AudioParameterFloat({"tone",  1}, "Tone",  0.0f,  1.0f, 0.5f));
    addParameter(widthParam = new juce::AudioParameterFloat({"width", 1}, "Width", 0.0f,  1.0f, 1.0f));
    addParameter(mixParam   = new juce::AudioParameterFloat({"mix",   1}, "Mix",   0.0f,  1.0f, 0.5f));
}

JunoBBDAudioProcessor::~JunoBBDAudioProcessor() {}

// ------------------------------------------------------------
double JunoBBDAudioProcessor::getTailLengthSeconds() const
{
    // Maximum delay is kBaseDelayMs + kModDepthMs ~ 3.3 ms; negligible tail
    return 0.05;
}

// ------------------------------------------------------------
bool JunoBBDAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo() &&
        mainOut != juce::AudioChannelSet::mono())
        return false;

    if (!mainIn.isDisabled() &&
        mainIn != juce::AudioChannelSet::stereo() &&
        mainIn != juce::AudioChannelSet::mono())
        return false;

    return true;
}

// ------------------------------------------------------------
void JunoBBDAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    delayBufL1.assign(kMaxDelaySamples, 0.f);
    delayBufR1.assign(kMaxDelaySamples, 0.f);
    delayBufL2.assign(kMaxDelaySamples, 0.f);
    delayBufR2.assign(kMaxDelaySamples, 0.f);

    writeIdxL1 = writeIdxR1 = 0;
    writeIdxL2 = writeIdxR2 = 0;

    lfoPhase1 = 0.0;
    lfoPhase2 = 0.25; // 90-deg offset for Mode II second LFO

    toneZL = toneZR = toneZL2 = toneZR2 = 0.f;

    for (int i = 0; i < 4; ++i) { dcBlockX[i] = 0.f; dcBlockY[i] = 0.f; }

    driftValL1 = driftValR1 = 0.f;
    driftValL2 = driftValR2 = 0.f;
}

void JunoBBDAudioProcessor::releaseResources() {}

// ------------------------------------------------------------
float JunoBBDAudioProcessor::readDelayLinear(
    const std::vector<float>& buf, int writeIdx, float delaySamples) const
{
    const int bufSize = (int)buf.size();
    float rd = (float)writeIdx - delaySamples;
    while (rd < 0.f) rd += (float)bufSize;

    int   i0 = (int)rd;
    float f  = rd - (float)i0;
    int   i1 = (i0 + 1) % bufSize;
    i0 = ((i0 % bufSize) + bufSize) % bufSize;

    return buf[i0] * (1.f - f) + buf[i1] * f;
}

// ------------------------------------------------------------
void JunoBBDAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int bufSize = kMaxDelaySamples;

    // Read parameters
    const float mode  = modeParam ->get();   // 0 = Mode I, 1 = Mode II
    const float rate  = rateParam ->get();
    const float depth = depthParam->get();
    const float drift = driftParam->get();
    const float tone  = toneParam ->get();
    const float width = widthParam->get();
    const float mix   = mixParam  ->get();

    const float sr = (float)currentSampleRate;

    // Tone filter coefficient (one-pole LP)
    // tone=0 -> heavy rolloff (~1kHz), tone=1 -> barely any (~18kHz)
    const float toneFreq = 800.f + tone * 17200.f;
    const float toneCoeff = 1.f - std::exp(-2.f * juce::MathConstants<float>::pi * toneFreq / sr);

    // Base delay in samples
    const float baseDelay = kBaseDelayMs * 0.001f * sr;
    const float modDepth  = kModDepthMs  * 0.001f * sr * depth;

    // LFO increment
    const double lfoInc = (double)rate / (double)sr;

    // Drift smoothing
    const float driftSpeed  = 0.0005f;
    const float driftAmount = drift * 3.f; // max ~3 samples drift

    // Mode blend weights
    const float wI  = 1.f - mode;
    const float wII = mode;

    // Gain staging: mix wet/dry
    const float wetGain = mix;
    const float dryGain = 1.f - mix;

    for (int n = 0; n < numSamples; ++n)
    {
        // --- LFO computation ---
        // Phase advances every sample
        const float lfo1 = (float)std::sin(lfoPhase1 * 2.0 * juce::MathConstants<double>::pi);
        // Mode II second LFO: phase2 = phase1 + 0.25 (90 degrees)
        const float lfo2 = (float)std::sin(lfoPhase2 * 2.0 * juce::MathConstants<double>::pi);

        lfoPhase1 += lfoInc;
        if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += lfoInc;
        if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        // --- Drift random walk ---
        driftValL1 += driftSpeed * (rng.nextFloat() * 2.f - 1.f);
        driftValR1 += driftSpeed * (rng.nextFloat() * 2.f - 1.f);
        driftValL2 += driftSpeed * (rng.nextFloat() * 2.f - 1.f);
        driftValR2 += driftSpeed * (rng.nextFloat() * 2.f - 1.f);
        // Soft clamp drift to [-1, 1] then scale
        driftValL1 = juce::jlimit(-1.f, 1.f, driftValL1);
        driftValR1 = juce::jlimit(-1.f, 1.f, driftValR1);
        driftValL2 = juce::jlimit(-1.f, 1.f, driftValL2);
        driftValR2 = juce::jlimit(-1.f, 1.f, driftValR2);

        // --- Input samples ---
        float inL, inR;
        if (buffer.getNumChannels() >= 2)
        {
            inL = buffer.getSample(0, n);
            inR = buffer.getSample(1, n);
        }
        else
        {
            inL = inR = buffer.getSample(0, n);
        }
        const float inMono = (inL + inR) * 0.5f;

        // ================================================================
        // MODE I  — single LFO path
        //   Left BBD:  delay modulated by +lfo1
        //   Right BBD: delay modulated by -lfo1  (classic Juno inversion)
        // ================================================================
        const float delayL1 = baseDelay + modDepth * lfo1 + driftAmount * driftValL1;
        const float delayR1 = baseDelay - modDepth * lfo1 + driftAmount * driftValR1;

        // Write input to Mode I delay lines
        delayBufL1[writeIdxL1] = inMono;
        delayBufR1[writeIdxR1] = inMono;

        // Read with interpolation
        float wetL1 = readDelayLinear(delayBufL1, writeIdxL1, juce::jlimit(1.f, (float)(bufSize - 2), delayL1));
        float wetR1 = readDelayLinear(delayBufR1, writeIdxR1, juce::jlimit(1.f, (float)(bufSize - 2), delayR1));

        writeIdxL1 = (writeIdxL1 + 1) % bufSize;
        writeIdxR1 = (writeIdxR1 + 1) % bufSize;

        // Tone filter on Mode I wet
        toneZL  += toneCoeff * (wetL1 - toneZL);
        toneZR  += toneCoeff * (wetR1 - toneZR);
        wetL1    = toneZL;
        wetR1    = toneZR;

        // ================================================================
        // MODE II  — dual LFO paths (lfo1 on L, lfo2 on R)
        // ================================================================
        const float delayL2 = baseDelay + modDepth * lfo1 + driftAmount * driftValL2;
        const float delayR2 = baseDelay + modDepth * lfo2 + driftAmount * driftValR2;

        delayBufL2[writeIdxL2] = inMono;
        delayBufR2[writeIdxR2] = inMono;

        float wetL2 = readDelayLinear(delayBufL2, writeIdxL2, juce::jlimit(1.f, (float)(bufSize - 2), delayL2));
        float wetR2 = readDelayLinear(delayBufR2, writeIdxR2, juce::jlimit(1.f, (float)(bufSize - 2), delayR2));

        writeIdxL2 = (writeIdxL2 + 1) % bufSize;
        writeIdxR2 = (writeIdxR2 + 1) % bufSize;

        // Tone filter on Mode II wet
        toneZL2 += toneCoeff * (wetL2 - toneZL2);
        toneZR2 += toneCoeff * (wetR2 - toneZR2);
        wetL2    = toneZL2;
        wetR2    = toneZR2;

        // ================================================================
        // Blend Mode I and Mode II
        // ================================================================
        float wetL = wI * wetL1 + wII * wetL2;
        float wetR = wI * wetR1 + wII * wetR2;

        // ================================================================
        // Stereo width processing on wet signal
        //   width=1 -> full stereo, width=0 -> mono wet
        // ================================================================
        const float mid  = (wetL + wetR) * 0.5f;
        const float side = (wetL - wetR) * 0.5f * width;
        wetL = mid + side;
        wetR = mid - side;

        // ================================================================
        // Mix
        // ================================================================
        float outL = dryGain * inL + wetGain * wetL;
        float outR = dryGain * inR + wetGain * wetR;

        // ================================================================
        // Write output
        // ================================================================
        if (totalOut >= 2)
        {
            buffer.setSample(0, n, outL);
            buffer.setSample(1, n, outR);
        }
        else
        {
            buffer.setSample(0, n, (outL + outR) * 0.5f);
        }
    }

    // Clear any extra output channels
    for (int ch = 2; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);
}

// ------------------------------------------------------------
juce::AudioProcessorEditor* JunoBBDAudioProcessor::createEditor()
{
    return new JunoBBDAudioProcessorEditor(*this);
}

// ------------------------------------------------------------
void JunoBBDAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(modeParam ->get());
    stream.writeFloat(rateParam ->get());
    stream.writeFloat(depthParam->get());
    stream.writeFloat(driftParam->get());
    stream.writeFloat(toneParam ->get());
    stream.writeFloat(widthParam->get());
    stream.writeFloat(mixParam  ->get());
}

void JunoBBDAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 7 * 4)
    {
        *modeParam  = stream.readFloat();
        *rateParam  = stream.readFloat();
        *depthParam = stream.readFloat();
        *driftParam = stream.readFloat();
        *toneParam  = stream.readFloat();
        *widthParam = stream.readFloat();
        *mixParam   = stream.readFloat();
    }
}

// ------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JunoBBDAudioProcessor();
}
