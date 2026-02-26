#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
LFOFoolAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    static const char* targetNames[]   = { "Volume", "Pan", "Filter", "Reverb", "Delay" };
    static const char* shapeNames[]    = { "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Random", "Custom" };
    static const char* syncRateNames[] = { "1/16", "1/8", "1/4", "1/2", "1/1", "2/1" };

    for (int i = 1; i <= 4; ++i)
    {
        juce::String n (i);

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "lfo" + n + "_rate",  "LFO " + n + " Rate",
            juce::NormalisableRange<float> (0.01f, 20.0f, 0.0f, 0.35f), 1.0f, "Hz"));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            "lfo" + n + "_rate_sync", "LFO " + n + " Rate Sync",
            juce::StringArray (syncRateNames, 6), 2));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            "lfo" + n + "_sync_enabled", "LFO " + n + " BPM Sync", false));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "lfo" + n + "_depth", "LFO " + n + " Depth",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            "lfo" + n + "_shape", "LFO " + n + " Shape",
            juce::StringArray (shapeNames, 7), 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "lfo" + n + "_phase", "LFO " + n + " Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f), 0.0f, "deg"));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "lfo" + n + "_grit", "LFO " + n + " Grit",
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            "lfo" + n + "_target", "LFO " + n + " Target",
            juce::StringArray (targetNames, 5), 0));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            "lfo" + n + "_enabled", "LFO " + n + " Enabled", true));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "chaos", "Chaos",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "master_depth", "Master Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "bpm_sync", "BPM Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filter_cutoff_base", "Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "filter_resonance", "Filter Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverb_base", "Reverb Send",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delay_base", "Delay Send",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
LFOFoolAudioProcessor::LFOFoolAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LFOFool", createParameterLayout())
{
    for (int i = 0; i < 4; ++i)
    {
        lfos[i].rng.setSeedRandomly();
        lfos[i].holdValue = lfos[i].rng.nextFloat() * 2.0f - 1.0f;
        lfos[i].nextHold  = lfos[i].rng.nextFloat() * 2.0f - 1.0f;
    }
    chaosRng.setSeedRandomly();
}

LFOFoolAudioProcessor::~LFOFoolAudioProcessor() {}

//==============================================================================
const juce::String LFOFoolAudioProcessor::getName() const { return JucePlugin_Name; }
bool LFOFoolAudioProcessor::acceptsMidi()  const { return false; }
bool LFOFoolAudioProcessor::producesMidi() const { return false; }
bool LFOFoolAudioProcessor::isMidiEffect() const { return false; }
double LFOFoolAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int  LFOFoolAudioProcessor::getNumPrograms()                          { return 1; }
int  LFOFoolAudioProcessor::getCurrentProgram()                       { return 0; }
void LFOFoolAudioProcessor::setCurrentProgram (int)                   {}
const juce::String LFOFoolAudioProcessor::getProgramName (int)        { return {}; }
void LFOFoolAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void LFOFoolAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency (2000.0f);
    filter.setResonance (0.707f);  // Butterworth

    reverb.prepare (spec);

    delayLenSamples = juce::jlimit (1, kMaxDelaySamples - 1,
                                    (int) (sampleRate * 0.5));  // 500ms
    delayBuffer.setSize (2, kMaxDelaySamples);
    delayBuffer.clear();
    delayWritePos = 0;

    gainSmooth.reset (sampleRate, 0.020);
    panSmooth .reset (sampleRate, 0.020);
    gainSmooth.setCurrentAndTargetValue (1.0f);
    panSmooth .setCurrentAndTargetValue (0.0f);
}

void LFOFoolAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LFOFoolAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    return true;
}
#endif

//==============================================================================
void LFOFoolAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels < 2)
        return;

    // Clear any extra output channels
    for (int ch = getTotalNumInputChannels(); ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ── Read global parameters ────────────────────────────────────────────────
    const float chaos       = apvts.getRawParameterValue ("chaos")->load();
    const float masterDepth = apvts.getRawParameterValue ("master_depth")->load();
    const bool  bpmSync     = apvts.getRawParameterValue ("bpm_sync")->load() > 0.5f;
    const float filterBase  = apvts.getRawParameterValue ("filter_cutoff_base")->load();
    const float filterRes   = apvts.getRawParameterValue ("filter_resonance")->load();
    const float reverbBase  = apvts.getRawParameterValue ("reverb_base")->load();
    const float delayBase   = apvts.getRawParameterValue ("delay_base")->load();

    // Get host BPM (fallback 120)
    float hostBpm = 120.0f;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = juce::jlimit (20.0f, 300.0f, (float) *bpm);

    // ── Read per-LFO parameters ───────────────────────────────────────────────
    float lfoRate[4], lfoDepth[4], lfoPhaseOff[4], lfoGrit[4];
    int   lfoShape[4], lfoTarget[4], lfoRateSync[4];
    bool  lfoEnabled[4], lfoSyncEn[4];

    for (int i = 0; i < 4; ++i)
    {
        juce::String n (i + 1);
        lfoRate[i]     = apvts.getRawParameterValue ("lfo" + n + "_rate")->load();
        lfoDepth[i]    = apvts.getRawParameterValue ("lfo" + n + "_depth")->load();
        lfoPhaseOff[i] = apvts.getRawParameterValue ("lfo" + n + "_phase")->load();
        lfoGrit[i]     = apvts.getRawParameterValue ("lfo" + n + "_grit")->load();
        lfoShape[i]    = (int) apvts.getRawParameterValue ("lfo" + n + "_shape")->load();
        lfoTarget[i]   = (int) apvts.getRawParameterValue ("lfo" + n + "_target")->load();
        lfoRateSync[i] = (int) apvts.getRawParameterValue ("lfo" + n + "_rate_sync")->load();
        lfoEnabled[i]  = apvts.getRawParameterValue ("lfo" + n + "_enabled")->load() > 0.5f;
        lfoSyncEn[i]   = apvts.getRawParameterValue ("lfo" + n + "_sync_enabled")->load() > 0.5f;
    }

    // ── Chaos noise generator (smoothed random, ~10 Hz LPF) ──────────────────
    const float rawNoise = chaosRng.nextFloat() * 2.0f - 1.0f;
    chaosNoise = chaosNoise * 0.92f + rawNoise * 0.08f;
    const float chaosDrift = chaosNoise * chaos * 0.04f;
    const float chaosJitter = chaosNoise * chaos * 0.015f;

    // ── Tick each LFO by one block ────────────────────────────────────────────
    float lfoVal[4] = {};

    for (int i = 0; i < 4; ++i)
    {
        if (!lfoEnabled[i])
        {
            lfoPhaseOut[i].store (0.0f);
            lfoValueOut[i].store (0.0f);
            continue;
        }

        // Effective rate in Hz
        float rateHz;
        if (bpmSync || lfoSyncEn[i])
        {
            const int idx = juce::jlimit (0, kNumSyncRates - 1, lfoRateSync[i]);
            rateHz = (hostBpm / 60.0f) / kSyncBeatsPerCycle[idx];
        }
        else
        {
            rateHz = lfoRate[i];
        }

        rateHz *= (1.0f + chaosDrift);
        rateHz  = std::max (rateHz, 0.001f);

        const double phaseInc = (double) rateHz * numSamples / currentSampleRate
                              + (double) chaosJitter;

        lfos[i].advance (phaseInc);
        lfoVal[i] = lfos[i].evaluate (lfoPhaseOff[i], lfoShape[i], lfoGrit[i]);

        lfoPhaseOut[i].store ((float) lfos[i].phase);
        lfoValueOut[i].store (lfoVal[i]);
    }

    // ── Sum modulation per target ─────────────────────────────────────────────
    float volumeMod = 0.0f, panMod = 0.0f, filterMod = 0.0f,
          reverbMod = 0.0f, delayMod = 0.0f;

    for (int i = 0; i < 4; ++i)
    {
        if (!lfoEnabled[i]) continue;
        const float c = lfoVal[i] * lfoDepth[i] * masterDepth;
        switch (lfoTarget[i])
        {
            case 0: volumeMod += c; break;
            case 1: panMod    += c; break;
            case 2: filterMod += c; break;
            case 3: reverbMod += c; break;
            case 4: delayMod  += c; break;
        }
    }

    // ── Volume modulation ─────────────────────────────────────────────────────
    // lfo=+1 → full volume, lfo=-1 → silence  (standard tremolo)
    {
        const float norm = (juce::jlimit (-1.0f, 1.0f, volumeMod) + 1.0f) * 0.5f;
        gainSmooth.setTargetValue (norm);
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = gainSmooth.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[s] *= g;
        }
    }

    // ── Pan modulation ────────────────────────────────────────────────────────
    {
        panSmooth.setTargetValue (juce::jlimit (-1.0f, 1.0f, panMod));
        for (int s = 0; s < numSamples; ++s)
        {
            const float p = panSmooth.getNextValue();  // -1 = full left, +1 = full right
            const float lG = juce::jlimit (0.0f, 1.0f, 1.0f - std::max (0.0f,  p));
            const float rG = juce::jlimit (0.0f, 1.0f, 1.0f - std::max (0.0f, -p));
            buffer.getWritePointer (0)[s] *= lG;
            buffer.getWritePointer (1)[s] *= rG;
        }
    }

    // ── Filter ────────────────────────────────────────────────────────────────
    {
        const float cutoff = juce::jlimit (20.0f, 20000.0f,
                                           filterBase + filterMod * 10000.0f);
        const float q      = 0.5f + filterRes * 7.5f;  // 0.5–8.0

        filter.setCutoffFrequency (cutoff);
        filter.setResonance (q);

        juce::dsp::AudioBlock<float>         block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
    }

    // ── Reverb send ───────────────────────────────────────────────────────────
    {
        const float wet = juce::jlimit (0.0f, 1.0f,
                                        reverbBase + reverbMod * 0.5f);
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = 0.7f;
        rp.damping    = 0.5f;
        rp.wetLevel   = wet;
        rp.dryLevel   = 1.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        juce::dsp::AudioBlock<float>         block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    // ── Delay send ────────────────────────────────────────────────────────────
    {
        const float wet = juce::jlimit (0.0f, 1.0f,
                                        delayBase + delayMod * 0.5f);
        if (wet > 0.001f)
        {
            constexpr float feedback = 0.4f;

            for (int ch = 0; ch < 2; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float* dly  = delayBuffer.getWritePointer (ch);

                for (int s = 0; s < numSamples; ++s)
                {
                    const int readPos = (delayWritePos + s - delayLenSamples
                                         + kMaxDelaySamples) % kMaxDelaySamples;
                    const float delayed = dly[readPos];
                    dly[(delayWritePos + s) % kMaxDelaySamples] =
                        data[s] + delayed * feedback;
                    data[s] = data[s] * (1.0f - wet) + delayed * wet;
                }
            }
            delayWritePos = (delayWritePos + numSamples) % kMaxDelaySamples;
        }
    }
}

//==============================================================================
bool LFOFoolAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* LFOFoolAudioProcessor::createEditor()
{
    return new LFOFoolAudioProcessorEditor (*this);
}

//==============================================================================
void LFOFoolAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void LFOFoolAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LFOFoolAudioProcessor();
}
