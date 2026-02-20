#include "PluginProcessor.h"
#include "PluginEditor.h"

PurplePlateReverbAudioProcessor::PurplePlateReverbAudioProcessor()
{
    addParameter (sizeParam       = new juce::AudioParameterFloat ("size",      "Size",       0.0f,  1.0f,  0.5f));
    addParameter (decayParam      = new juce::AudioParameterFloat ("decay",     "Decay",      0.0f,  10.0f, 3.0f));
    addParameter (brightnessParam = new juce::AudioParameterFloat ("bright",    "Brightness", 0.0f,  1.0f,  0.7f));
    addParameter (mixParam        = new juce::AudioParameterFloat ("mix",       "Mix",        0.0f,  1.0f,  0.5f));
    addParameter (modDepthParam   = new juce::AudioParameterFloat ("moddepth",  "Mod Depth",  0.0f,  1.0f,  0.2f));
    addParameter (gateParam       = new juce::AudioParameterFloat ("gate",      "Gate",       0.0f,  1.0f,  0.0f));
    addParameter (gateRateParam   = new juce::AudioParameterFloat ("gaterate",  "Gate Rate",  0.1f,  8.0f,  1.0f));
}

PurplePlateReverbAudioProcessor::~PurplePlateReverbAudioProcessor() {}

// -----------------------------------------------------------------------

void PurplePlateReverbAudioProcessor::updateReverbParams (float size, float decay)
{
    juce::Reverb::Parameters p;
    p.roomSize   = 0.3f + size * 0.7f;
    p.damping    = 1.0f - (decay / 10.0f) * 0.92f;
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
}

void PurplePlateReverbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    updateReverbParams (sizeParam->get(), decayParam->get());

    memset (modBufL, 0, sizeof (modBufL));
    memset (modBufR, 0, sizeof (modBufR));
    modWritePos   = 0;
    lfoPhase      = 0.0f;
    gatePhase     = 0.0f;
    gateSlewState = 1.0f;
    bFilterL = bFilterR = 0.0f;
}

void PurplePlateReverbAudioProcessor::releaseResources() {}

// -----------------------------------------------------------------------

void PurplePlateReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn    = getTotalNumInputChannels();
    const int totalOut   = getTotalNumOutputChannels();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, numSamples);

    if (totalOut < 2) return;

    // Read parameters
    const float size       = sizeParam->get();
    const float decay      = decayParam->get();
    const float brightness = brightnessParam->get();
    const float mix        = mixParam->get();
    const float modDepth   = modDepthParam->get();
    const float gate       = gateParam->get();
    const float gateRate   = gateRateParam->get();

    // Capture dry signal
    juce::AudioBuffer<float> dry (2, numSamples);
    dry.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dry.copyFrom (1, 0, buffer, (totalIn > 1) ? 1 : 0, 0, numSamples);

    if (totalIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Apply reverb (wet only)
    updateReverbParams (size, decay);
    reverb.processStereo (L, R, numSamples);

    // Pre-compute constants
    const float lfoInc  = juce::MathConstants<float>::twoPi * kLfoRateHz
                          / (float)currentSampleRate;
    const float gateInc = juce::MathConstants<float>::twoPi
                          * juce::jlimit (0.1f, 8.0f, gateRate)
                          / (float)currentSampleRate;
    const float slewRate  = 1.0f / (0.008f * (float)currentSampleRate);
    const float cutoffHz  = 500.0f + brightness * 19500.0f;
    const float bAlpha    = 1.0f / (1.0f + (float)currentSampleRate
                                          / (juce::MathConstants<float>::twoPi * cutoffHz));

    const float* dryL = dry.getReadPointer (0);
    const float* dryR = dry.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        // Modulation
        const float lfo = std::sin (lfoPhase);
        lfoPhase += lfoInc;
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        modBufL[modWritePos] = L[i];
        modBufR[modWritePos] = R[i];

        const float maxDelay    = 0.007f * (float)currentSampleRate;
        const float delaySamples = 1.0f + maxDelay * modDepth * (lfo * 0.5f + 0.5f);
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
            const float gateOsc    = std::sin (gatePhase);
            gatePhase += gateInc;
            if (gatePhase >= juce::MathConstants<float>::twoPi)
                gatePhase -= juce::MathConstants<float>::twoPi;

            const float target = (gateOsc > 0.0f) ? 1.0f : 0.0f;
            const float delta  = target - gateSlewState;
            gateSlewState += juce::jlimit (-slewRate, slewRate, delta);

            const float gateGain = 1.0f - gate * (1.0f - gateSlewState);
            wetL *= gateGain;
            wetR *= gateGain;
        }
        else
        {
            gateSlewState = 1.0f;
            gatePhase     = 0.0f;
        }

        // Brightness
        bFilterL += bAlpha * (wetL - bFilterL);
        bFilterR += bAlpha * (wetR - bFilterR);
        wetL = bFilterL;
        wetR = bFilterR;

        // Wet/dry mix
        L[i] = dryL[i] * (1.0f - mix) + wetL * mix;
        R[i] = dryR[i] * (1.0f - mix) + wetR * mix;
    }
}

// -----------------------------------------------------------------------

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
