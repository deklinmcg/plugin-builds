#include "PluginProcessor.h"
#include "PluginEditor.h"

PurplePlateReverbAudioProcessor::PurplePlateReverbAudioProcessor()
{
    addParameter (sizeParam       = new juce::AudioParameterFloat ("size",     "Size",       0.0f,  1.0f,  0.5f));
    addParameter (decayParam      = new juce::AudioParameterFloat ("decay",    "Decay",      0.0f,  10.0f, 3.0f));
    addParameter (brightnessParam = new juce::AudioParameterFloat ("bright",   "Brightness", 0.0f,  1.0f,  0.7f));
    addParameter (mixParam        = new juce::AudioParameterFloat ("mix",      "Mix",        0.0f,  1.0f,  0.5f));
    addParameter (modDepthParam   = new juce::AudioParameterFloat ("moddepth", "Mod Depth",  0.0f,  1.0f,  0.2f));
    addParameter (gateParam       = new juce::AudioParameterFloat ("gate",     "Gate",       0.0f,  1.0f,  0.0f));
    addParameter (gateRateParam   = new juce::AudioParameterFloat ("gaterate", "Gate Rate",  0.1f,  8.0f,  1.0f));
}

PurplePlateReverbAudioProcessor::~PurplePlateReverbAudioProcessor() {}

bool PurplePlateReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output, or be mono feeding stereo
    return (in == out || (in == juce::AudioChannelSet::mono()
                          && out == juce::AudioChannelSet::stereo()));
}

void PurplePlateReverbAudioProcessor::updateReverbParams (float size, float decay)
{
    juce::Reverb::Parameters p;
    p.roomSize   = 0.3f + size * 0.7f;
    p.damping    = 1.0f - (decay / 10.0f) * 0.92f;
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverbL.setParameters (p);
    reverbR.setParameters (p);
}

void PurplePlateReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    reverbL.setSampleRate (sampleRate);
    reverbR.setSampleRate (sampleRate);
    reverbL.reset();
    reverbR.reset();
    updateReverbParams (sizeParam->get(), decayParam->get());

    // Pre-allocate dry buffer — no heap allocation in processBlock
    dryBuf.setSize (2, samplesPerBlock, false, true, false);

    memset (modBufL, 0, sizeof (modBufL));
    memset (modBufR, 0, sizeof (modBufR));
    modWritePos   = 0;
    lfoPhase      = 0.0f;
    gatePhase     = 0.0f;
    gateSlewState = 1.0f;
    bFilterL = bFilterR = 0.0f;
}

void PurplePlateReverbAudioProcessor::releaseResources()
{
    dryBuf.setSize (0, 0);
}

void PurplePlateReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn    = getTotalNumInputChannels();
    const int totalOut   = getTotalNumOutputChannels();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, numSamples);

    // Read params
    const float size       = sizeParam->get();
    const float decay      = decayParam->get();
    const float brightness = brightnessParam->get();
    const float mix        = mixParam->get();
    const float modDepth   = modDepthParam->get();
    const float gate       = gateParam->get();
    const float gateRate   = gateRateParam->get();

    updateReverbParams (size, decay);

    const bool isStereo = (totalOut >= 2);

    if (isStereo)
    {
        // Save dry (no allocation — uses pre-allocated dryBuf)
        dryBuf.copyFrom (0, 0, buffer, 0, 0, numSamples);
        dryBuf.copyFrom (1, 0, buffer, (totalIn > 1) ? 1 : 0, 0, numSamples);

        if (totalIn == 1)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        reverbL.processStereo (L, R, numSamples);

        const float* dryL = dryBuf.getReadPointer (0);
        const float* dryR = dryBuf.getReadPointer (1);

        // Constants
        const float lfoInc   = juce::MathConstants<float>::twoPi * kLfoRateHz / (float)currentSampleRate;
        const float gateInc  = juce::MathConstants<float>::twoPi * juce::jlimit (0.1f, 8.0f, gateRate) / (float)currentSampleRate;
        const float slewRate = 1.0f / (0.008f * (float)currentSampleRate);
        const float cutoff   = 500.0f + brightness * 19500.0f;
        const float bAlpha   = 1.0f / (1.0f + (float)currentSampleRate / (juce::MathConstants<float>::twoPi * cutoff));

        for (int i = 0; i < numSamples; ++i)
        {
            // Modulation
            const float lfo = std::sin (lfoPhase);
            lfoPhase += lfoInc;
            if (lfoPhase >= juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

            modBufL[modWritePos] = L[i];
            modBufR[modWritePos] = R[i];

            const float delaySamples = 1.0f + 0.007f * (float)currentSampleRate * modDepth * (lfo * 0.5f + 0.5f);
            const int   di   = (int)delaySamples;
            const float frac = delaySamples - (float)di;
            const int   rpA  = (modWritePos - di + kModBufSize) & (kModBufSize - 1);
            const int   rpB  = (rpA - 1 + kModBufSize) & (kModBufSize - 1);

            const float modL = modBufL[rpA] + frac * (modBufL[rpB] - modBufL[rpA]);
            const float modR = modBufR[rpA] + frac * (modBufR[rpB] - modBufR[rpA]);
            modWritePos = (modWritePos + 1) & (kModBufSize - 1);

            float wetL = L[i] * (1.0f - modDepth * 0.6f) + modL * modDepth * 0.6f;
            float wetR = R[i] * (1.0f - modDepth * 0.6f) + modR * modDepth * 0.6f;

            // Gate
            if (gate > 0.001f)
            {
                const float gateOsc = std::sin (gatePhase);
                gatePhase += gateInc;
                if (gatePhase >= juce::MathConstants<float>::twoPi) gatePhase -= juce::MathConstants<float>::twoPi;

                const float target = (gateOsc > 0.0f) ? 1.0f : 0.0f;
                gateSlewState += juce::jlimit (-slewRate, slewRate, target - gateSlewState);
                const float gateGain = 1.0f - gate * (1.0f - gateSlewState);
                wetL *= gateGain;
                wetR *= gateGain;
            }
            else { gateSlewState = 1.0f; gatePhase = 0.0f; }

            // Brightness LP
            bFilterL += bAlpha * (wetL - bFilterL);
            bFilterR += bAlpha * (wetR - bFilterR);

            // Mix
            L[i] = dryL[i] * (1.0f - mix) + bFilterL * mix;
            R[i] = dryR[i] * (1.0f - mix) + bFilterR * mix;
        }
    }
    else
    {
        // Mono
        float* M = buffer.getWritePointer (0);
        dryBuf.copyFrom (0, 0, buffer, 0, 0, numSamples);

        reverbL.processMono (M, numSamples);

        const float* dryM = dryBuf.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
            M[i] = dryM[i] * (1.0f - mix) + M[i] * mix;
    }
}

void PurplePlateReverbAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    juce::MemoryOutputStream s (d, true);
    s.writeFloat (sizeParam->get());
    s.writeFloat (decayParam->get());
    s.writeFloat (brightnessParam->get());
    s.writeFloat (mixParam->get());
    s.writeFloat (modDepthParam->get());
    s.writeFloat (gateParam->get());
    s.writeFloat (gateRateParam->get());
}

void PurplePlateReverbAudioProcessor::setStateInformation (const void* d, int size)
{
    juce::MemoryInputStream s (d, (size_t)size, false);
    *sizeParam       = s.readFloat();
    *decayParam      = s.readFloat();
    *brightnessParam = s.readFloat();
    *mixParam        = s.readFloat();
    *modDepthParam   = s.readFloat();
    *gateParam       = s.readFloat();
    *gateRateParam   = s.readFloat();
}

juce::AudioProcessorEditor* PurplePlateReverbAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurplePlateReverbAudioProcessor();
}
