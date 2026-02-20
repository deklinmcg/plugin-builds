#include "PluginProcessor.h"
#include "PluginEditor.h"

PurplePlateReverbAudioProcessor::PurplePlateReverbAudioProcessor()
    : size(0.5f), decay(3.0f), brightness(0.7f), mix(0.5f),
      modDepth(0.2f), gate(0.0f), gateRate(1.0f)
{}

PurplePlateReverbAudioProcessor::~PurplePlateReverbAudioProcessor() {}

// -----------------------------------------------------------------------

void PurplePlateReverbAudioProcessor::updateReverbParams()
{
    juce::Reverb::Parameters p;
    // size  0-1  → roomSize 0.3-1.0 (avoid tiny room)
    p.roomSize   = 0.3f + size * 0.7f;
    // decay 0-10s → damping 1.0-0.08 (high decay = low damping = long bright tail)
    p.damping    = 1.0f - (decay / 10.0f) * 0.92f;
    // We manage wet/dry ourselves so reverb always outputs wet only
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);
}

void PurplePlateReverbAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    reverb.setSampleRate(sampleRate);
    reverb.reset();
    updateReverbParams();

    memset(modBufL, 0, sizeof(modBufL));
    memset(modBufR, 0, sizeof(modBufR));
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

    // Clear any extra output channels
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, numSamples);

    if (totalOut < 2) return;

    // --- Capture dry signal ---
    juce::AudioBuffer<float> dry (2, numSamples);
    dry.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dry.copyFrom (1, 0, buffer, (totalIn > 1) ? 1 : 0, 0, numSamples);

    // Ensure stereo input into reverb (mono → copy to both channels)
    if (totalIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    // --- Apply reverb (wet only) ---
    updateReverbParams();
    reverb.processStereo (L, R, numSamples);

    // --- Pre-compute per-block constants ---
    const float lfoInc  = juce::MathConstants<float>::twoPi * kLfoRateHz
                          / (float)currentSampleRate;

    const float clampedGateRate = juce::jlimit (0.1f, 8.0f, gateRate);
    const float gateInc = juce::MathConstants<float>::twoPi * clampedGateRate
                          / (float)currentSampleRate;

    // Gate slew: 8ms attack/release to smooth the square gate wave
    const float slewRate = 1.0f / (0.008f * (float)currentSampleRate);

    // Brightness LP: fc = 500Hz (dark) … 20kHz (full bright)
    const float cutoffHz = 500.0f + brightness * 19500.0f;
    const float bAlpha   = 1.0f / (1.0f + (float)currentSampleRate
                                         / (juce::MathConstants<float>::twoPi * cutoffHz));

    const float* dryL = dry.getReadPointer(0);
    const float* dryR = dry.getReadPointer(1);

    // --- Per-sample processing ---
    for (int i = 0; i < numSamples; ++i)
    {
        // ---- Modulation (chorus-style shimmer on tail) ----
        const float lfo = std::sin(lfoPhase);
        lfoPhase += lfoInc;
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        modBufL[modWritePos] = L[i];
        modBufR[modWritePos] = R[i];

        // modDepth 0-1 sweeps 0-7ms of delay
        const float maxDelaySamples = 0.007f * (float)currentSampleRate;
        const float delaySamples    = 1.0f + maxDelaySamples * modDepth * (lfo * 0.5f + 0.5f);
        const int   di   = (int)delaySamples;
        const float frac = delaySamples - (float)di;

        const int rpA = (modWritePos - di + kModBufSize) & (kModBufSize - 1);
        const int rpB = (rpA - 1 + kModBufSize) & (kModBufSize - 1);

        const float modL = modBufL[rpA] + frac * (modBufL[rpB] - modBufL[rpA]);
        const float modR = modBufR[rpA] + frac * (modBufR[rpB] - modBufR[rpA]);

        modWritePos = (modWritePos + 1) & (kModBufSize - 1);

        // Blend original + modulated (modDepth=0 → unchanged, =1 → heavy chorus shimmer)
        float wetL = L[i] * (1.0f - modDepth * 0.6f) + modL * modDepth * 0.6f;
        float wetR = R[i] * (1.0f - modDepth * 0.6f) + modR * modDepth * 0.6f;

        // ---- Gate (80s rhythmic gating) ----
        if (gate > 0.001f)
        {
            const float gateOsc = std::sin(gatePhase);
            gatePhase += gateInc;
            if (gatePhase >= juce::MathConstants<float>::twoPi)
                gatePhase -= juce::MathConstants<float>::twoPi;

            // Square gate: open when osc > 0
            const float target = (gateOsc > 0.0f) ? 1.0f : 0.0f;
            // Slew toward target to avoid clicks
            const float delta = target - gateSlewState;
            gateSlewState += juce::jlimit (-slewRate, slewRate, delta);

            // gate=0 → no gating, gate=1 → full gating effect
            const float gateGain = 1.0f - gate * (1.0f - gateSlewState);
            wetL *= gateGain;
            wetR *= gateGain;
        }
        else
        {
            gateSlewState = 1.0f;
            gatePhase     = 0.0f;
        }

        // ---- Brightness (1-pole LP on wet signal) ----
        bFilterL += bAlpha * (wetL - bFilterL);
        bFilterR += bAlpha * (wetR - bFilterR);
        wetL = bFilterL;
        wetR = bFilterR;

        // ---- Wet/dry mix ----
        L[i] = dryL[i] * (1.0f - mix) + wetL * mix;
        R[i] = dryR[i] * (1.0f - mix) + wetR * mix;
    }
}

// -----------------------------------------------------------------------

void PurplePlateReverbAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    juce::MemoryOutputStream stream (d, true);
    stream.writeFloat(size);
    stream.writeFloat(decay);
    stream.writeFloat(brightness);
    stream.writeFloat(mix);
    stream.writeFloat(modDepth);
    stream.writeFloat(gate);
    stream.writeFloat(gateRate);
}

void PurplePlateReverbAudioProcessor::setStateInformation (const void* d, int s)
{
    juce::MemoryInputStream stream (d, (size_t)s, false);
    size       = stream.readFloat();
    decay      = stream.readFloat();
    brightness = stream.readFloat();
    mix        = stream.readFloat();
    modDepth   = stream.readFloat();
    gate       = stream.readFloat();
    gateRate   = stream.readFloat();
}

void PurplePlateReverbAudioProcessor::setParameter (int index, float value)
{
    switch (index)
    {
        case 0: size       = value; break;
        case 1: decay      = value; break;
        case 2: brightness = value; break;
        case 3: mix        = value; break;
        case 4: modDepth   = value; break;
        case 5: gate       = value; break;
        case 6: gateRate   = value; break;
        default: break;
    }
}

float PurplePlateReverbAudioProcessor::getParameter (int index) const
{
    switch (index)
    {
        case 0: return size;
        case 1: return decay;
        case 2: return brightness;
        case 3: return mix;
        case 4: return modDepth;
        case 5: return gate;
        case 6: return gateRate;
        default: return 0.0f;
    }
}

juce::AudioProcessorEditor* PurplePlateReverbAudioProcessor::createEditor()
{
    return nullptr;
}

bool PurplePlateReverbAudioProcessor::hasEditor() const { return false; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurplePlateReverbAudioProcessor();
}
