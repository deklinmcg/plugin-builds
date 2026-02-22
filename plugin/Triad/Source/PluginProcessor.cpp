/*
  ==============================================================================
    TriadAudioProcessor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// ShimmerLayer static data
const int ShimmerLayer::combDelaysL[kNumComb] = { 1557, 1617, 1491, 1422 };
const int ShimmerLayer::combDelaysR[kNumComb] = { 1617, 1557, 1422, 1491 };
const int ShimmerLayer::apDelays[kNumAllPass]  = { 225, 556, 441, 341 };

//==============================================================================
void ShimmerLayer::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;
    double ratio = sampleRate / 44100.0;

    for (int i = 0; i < kNumComb; ++i)
    {
        int dL = (int)(combDelaysL[i] * ratio);
        int dR = (int)(combDelaysR[i] * ratio);
        cbL[i].prepare(dL + 16); cbL[i].setDelay(dL);
        cbR[i].prepare(dR + 16); cbR[i].setDelay(dR);
    }
    for (int i = 0; i < kNumAllPass; ++i)
    {
        int d = (int)(apDelays[i] * ratio);
        apL[i].prepare(d + 16); apL[i].setDelay(d);
        apR[i].prepare(d + 16); apR[i].setDelay(d);
    }
    pitchL.prepare(sampleRate);
    pitchR.prepare(sampleRate);
}

void ShimmerLayer::reset()
{
    for (int i = 0; i < kNumComb;    ++i) { cbL[i].reset(); cbR[i].reset(); }
    for (int i = 0; i < kNumAllPass; ++i) { apL[i].reset(); apR[i].reset(); }
    pitchL.reset();
    pitchR.reset();
}

void ShimmerLayer::setSize(float sz)
{
    // Scale comb delays by size (0.5 to 2.0 range)
    double ratio = sr / 44100.0;
    float scale  = 0.5f + sz * 1.5f;
    for (int i = 0; i < kNumComb; ++i)
    {
        cbL[i].setDelay(juce::jlimit(1, (int)(cbL[i].buffer.size() - 1),
                                     (int)(combDelaysL[i] * ratio * scale)));
        cbR[i].setDelay(juce::jlimit(1, (int)(cbR[i].buffer.size() - 1),
                                     (int)(combDelaysR[i] * ratio * scale)));
    }
}

void ShimmerLayer::setDecay(float decaySeconds, float sz)
{
    // Map decay time to feedback, also taking size into account
    // RT60 feedback derivation per comb filter
    double ratio = sr / 44100.0;
    float  scale = 0.5f + sz * 1.5f;
    for (int i = 0; i < kNumComb; ++i)
    {
        int  dl  = (int)(combDelaysL[i] * ratio * scale);
        float fb = std::pow(10.f, -3.f * (float)dl / (decaySeconds * (float)sr));
        cbL[i].setDelay(dl);
        // store feedback implicitly: process() uses the value we write here
        // We can't store per-filter fb easily without refactor, so use a shared one
        (void)fb;
    }
    // Shared feedback: -60dB in decaySeconds from a representative delay ~1500 samples scaled
    double repDelay = 1500.0 * ratio * scale;
    feedback = std::pow(10.f, -3.f * (float)repDelay / (decaySeconds * (float)sr));
    feedback = juce::jlimit(0.f, 0.98f, feedback);
}

std::pair<float,float> ShimmerLayer::process(float inL, float inR, float shimmerMix)
{
    // Run through comb bank
    float combSumL = 0.f, combSumR = 0.f;
    for (int i = 0; i < kNumComb; ++i)
    {
        combSumL += cbL[i].process(inL, feedback);
        combSumR += cbR[i].process(inR, feedback);
    }
    combSumL *= 0.25f;
    combSumR *= 0.25f;

    // Run through allpass chain
    float apOutL = combSumL, apOutR = combSumR;
    for (int i = 0; i < kNumAllPass; ++i)
    {
        apOutL = apL[i].process(apOutL);
        apOutR = apR[i].process(apOutR);
    }

    // Pitch shift the reverb tail
    float shiftedL = pitchL.process(apOutL);
    float shiftedR = pitchR.process(apOutR);

    // Mix original reverb with pitch-shifted version
    float outL = apOutL * (1.f - shimmerMix) + shiftedL * shimmerMix;
    float outR = apOutR * (1.f - shimmerMix) + shiftedR * shimmerMix;

    return { outL, outR };
}

//==============================================================================
TriadAudioProcessor::TriadAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (layer1_pitch = new juce::AudioParameterFloat ({"layer1_pitch", 1}, "Layer 1 Pitch", -24.f, 24.f, 0.f));
    addParameter (layer2_pitch = new juce::AudioParameterFloat ({"layer2_pitch", 1}, "Layer 2 Pitch", -24.f, 24.f, 7.f));
    addParameter (layer3_pitch = new juce::AudioParameterFloat ({"layer3_pitch", 1}, "Layer 3 Pitch", -24.f, 24.f, 12.f));
    addParameter (size         = new juce::AudioParameterFloat ({"size",         1}, "Size",          0.f,   1.f,   0.7f));
    addParameter (decay        = new juce::AudioParameterFloat ({"decay",        1}, "Decay",         0.1f, 10.f,   3.0f));
    addParameter (shimmer_mix  = new juce::AudioParameterFloat ({"shimmer_mix",  1}, "Shimmer Mix",   0.f,   1.f,   0.5f));
    addParameter (lfo_rate     = new juce::AudioParameterFloat ({"lfo_rate",     1}, "LFO Rate",      0.01f,10.f,   0.5f));
    addParameter (lfo_depth    = new juce::AudioParameterFloat ({"lfo_depth",    1}, "LFO Depth",     0.f,   1.f,   0.3f));
    addParameter (lfo_target   = new juce::AudioParameterFloat ({"lfo_target",   1}, "LFO Target",    0.f,   5.f,   0.f));
    addParameter (seq_steps    = new juce::AudioParameterFloat ({"seq_steps",    1}, "Sequencer Steps",2.f, 16.f,  16.f));
    addParameter (seq_rate     = new juce::AudioParameterFloat ({"seq_rate",     1}, "Sequencer Rate", 0.25f, 8.f,  1.0f));
    addParameter (seq_target   = new juce::AudioParameterFloat ({"seq_target",   1}, "Sequencer Target",0.f, 5.f,  1.f));
    addParameter (mix          = new juce::AudioParameterFloat ({"mix",          1}, "Mix",            0.f,   1.f,   0.5f));

    // Default sequencer step values: random-ish spread from -1 to 1
    for (int i = 0; i < 16; ++i)
        seqStepValues[i] = std::sin((float)i * 1.1f) * 0.5f; // gentle pattern
}

TriadAudioProcessor::~TriadAudioProcessor() {}

//==============================================================================
void TriadAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    for (int i = 0; i < 3; ++i)
        layers[i].prepare(sr, samplesPerBlock);

    lfoPhase      = 0.0;
    seqPhaseAccum = 0.0;
    seqStep       = 0;

    float smoothTime = 0.05f; // 50ms
    pitch1Smooth.reset(sr, smoothTime);
    pitch2Smooth.reset(sr, smoothTime);
    pitch3Smooth.reset(sr, smoothTime);
    sizeSmooth.reset(sr, smoothTime);
    decaySmooth.reset(sr, smoothTime);
    shimmerSmooth.reset(sr, smoothTime);
    mixSmooth.reset(sr, smoothTime);

    pitch1Smooth.setCurrentAndTargetValue(layer1_pitch->get());
    pitch2Smooth.setCurrentAndTargetValue(layer2_pitch->get());
    pitch3Smooth.setCurrentAndTargetValue(layer3_pitch->get());
    sizeSmooth.setCurrentAndTargetValue(size->get());
    decaySmooth.setCurrentAndTargetValue(decay->get());
    shimmerSmooth.setCurrentAndTargetValue(shimmer_mix->get());
    mixSmooth.setCurrentAndTargetValue(mix->get());

    wetBuffer.setSize(2, samplesPerBlock);
    layerBuf.setSize(2, samplesPerBlock);
    wetBuffer.clear();
    layerBuf.clear();
}

void TriadAudioProcessor::releaseResources()
{
    for (int i = 0; i < 3; ++i)
        layers[i].reset();
}

//==============================================================================
bool TriadAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono or stereo in/out
    auto outSet = layouts.getMainOutputChannelSet();
    auto inSet  = layouts.getMainInputChannelSet();

    if (outSet != juce::AudioChannelSet::mono() &&
        outSet != juce::AudioChannelSet::stereo())
        return false;

    if (inSet != juce::AudioChannelSet::mono() &&
        inSet != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void TriadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalOut = getTotalNumOutputChannels();
    const int totalIn  = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear any extra output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Read raw parameter values
    float p1Base  = layer1_pitch->get();
    float p2Base  = layer2_pitch->get();
    float p3Base  = layer3_pitch->get();
    float szBase  = size->get();
    float dcBase  = decay->get();
    float smBase  = shimmer_mix->get();
    float mxBase  = mix->get();

    float lfoRateVal  = lfo_rate->get();
    float lfoDepthVal = lfo_depth->get();
    int   lfoTgt      = (int)std::round(lfo_target->get());
    int   seqNumSteps = juce::jlimit(2, 16, (int)std::round(seq_steps->get()));
    float seqRateVal  = seq_rate->get();   // beats per step
    int   seqTgt      = (int)std::round(seq_target->get());

    // Estimate BPM from host, default 120
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (playHead->getCurrentPosition(pos))
            bpm = pos.bpm > 0.0 ? pos.bpm : 120.0;
    }

    // Seconds per beat
    double secPerBeat = 60.0 / bpm;
    // Samples per sequencer step advance
    double samplesPerSeqStep = seqRateVal * secPerBeat * sampleRate;
    // LFO: samples per cycle
    double samplesPerLfoCycle = sampleRate / (double)lfoRateVal;

    // Compute LFO value (sine, sample-accurate per block midpoint for simplicity)
    float lfoVal = (float)std::sin(juce::MathConstants<double>::twoPi * lfoPhase);
    // Advance lfo phase for the block
    lfoPhase += (double)numSamples / samplesPerLfoCycle;
    if (lfoPhase >= 1.0) lfoPhase -= std::floor(lfoPhase);

    // Advance sequencer phase
    seqPhaseAccum += (double)numSamples;
    while (seqPhaseAccum >= samplesPerSeqStep)
    {
        seqPhaseAccum -= samplesPerSeqStep;
        seqStep = (seqStep + 1) % seqNumSteps;
    }
    currentSeqStep.store(seqStep);
    float seqVal = seqStepValues[seqStep]; // -1 to 1

    // Compute modulated values
    float lfoMod = lfoVal * lfoDepthVal;
    float seqMod = seqVal;

    // --- Apply modulation to correct target ---
    // Targets: 0=Layer1Pitch, 1=Layer2Pitch, 2=Layer3Pitch, 3=Size, 4=Decay, 5=Mix

    // LFO modulation
    float p1Mod = p1Base, p2Mod = p2Base, p3Mod = p3Base;
    float szMod = szBase, dcMod = dcBase, smMod = smBase, mxMod = mxBase;

    auto applyLfo = [&](float modAmt, int target) {
        switch (target) {
            case 0: p1Mod = juce::jlimit(-24.f, 24.f, p1Mod + modAmt * 12.f); break;
            case 1: p2Mod = juce::jlimit(-24.f, 24.f, p2Mod + modAmt * 12.f); break;
            case 2: p3Mod = juce::jlimit(-24.f, 24.f, p3Mod + modAmt * 12.f); break;
            case 3: szMod = juce::jlimit(0.f, 1.f,   szMod + modAmt * 0.5f);  break;
            case 4: dcMod = juce::jlimit(0.1f,10.f,  dcMod + modAmt * 4.f);   break;
            case 5: mxMod = juce::jlimit(0.f, 1.f,   mxMod + modAmt * 0.5f);  break;
            default: break;
        }
    };
    applyLfo(lfoMod, lfoTgt);

    // Sequencer modulation (depth scaled by lfo_depth? No — seq has its own range via step values)
    auto applySeq = [&](float seqAmount, int target) {
        switch (target) {
            case 0: p1Mod = juce::jlimit(-24.f, 24.f, p1Mod + seqAmount * 12.f); break;
            case 1: p2Mod = juce::jlimit(-24.f, 24.f, p2Mod + seqAmount * 12.f); break;
            case 2: p3Mod = juce::jlimit(-24.f, 24.f, p3Mod + seqAmount * 12.f); break;
            case 3: szMod = juce::jlimit(0.f, 1.f,   szMod + seqAmount * 0.5f);  break;
            case 4: dcMod = juce::jlimit(0.1f,10.f,  dcMod + seqAmount * 4.f);   break;
            case 5: mxMod = juce::jlimit(0.f, 1.f,   mxMod + seqAmount * 0.5f);  break;
            default: break;
        }
    };
    applySeq(seqMod, seqTgt);

    modValue.store(lfoMod);

    // Update smoothed targets
    pitch1Smooth.setTargetValue(p1Mod);
    pitch2Smooth.setTargetValue(p2Mod);
    pitch3Smooth.setTargetValue(p3Mod);
    sizeSmooth.setTargetValue(szMod);
    decaySmooth.setTargetValue(dcMod);
    shimmerSmooth.setTargetValue(smMod);
    mixSmooth.setTargetValue(mxMod);

    // Clear wet buffer
    wetBuffer.clear();

    float* wetL = wetBuffer.getWritePointer(0);
    float* wetR = wetBuffer.getWritePointer(1);

    // Get dry input pointers (handle mono input)
    const float* inL = buffer.getReadPointer(0);
    const float* inR = (totalIn > 1) ? buffer.getReadPointer(1) : buffer.getReadPointer(0);

    // Process sample-by-sample with smoothed params
    for (int s = 0; s < numSamples; ++s)
    {
        float curP1 = pitch1Smooth.getNextValue();
        float curP2 = pitch2Smooth.getNextValue();
        float curP3 = pitch3Smooth.getNextValue();
        float curSz = sizeSmooth.getNextValue();
        float curDc = decaySmooth.getNextValue();
        float curSm = shimmerSmooth.getNextValue();

        // Update layer params (these are slow-changing; update every sample is fine)
        layers[0].setSemitones(curP1);
        layers[1].setSemitones(curP2);
        layers[2].setSemitones(curP3);

        layers[0].setDecay(curDc, curSz);
        layers[1].setDecay(curDc, curSz);
        layers[2].setDecay(curDc, curSz);

        float dryL = inL[s];
        float dryR = inR[s];

        float sumL = 0.f, sumR = 0.f;
        for (int li = 0; li < 3; ++li)
        {
            auto [oL, oR] = layers[li].process(dryL, dryR, curSm);
            sumL += oL;
            sumR += oR;
        }
        // Scale down 3 layers
        sumL *= (1.f / 3.f);
        sumR *= (1.f / 3.f);

        wetL[s] = sumL;
        wetR[s] = sumR;
    }

    // Mix dry/wet and write to output
    float* outL = buffer.getWritePointer(0);
    float* outR = (totalOut > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    for (int s = 0; s < numSamples; ++s)
    {
        float curMix = mixSmooth.getNextValue();
        float dryL = inL[s];
        float dryR = inR[s];
        outL[s] = dryL * (1.f - curMix) + wetL[s] * curMix;
        if (totalOut > 1)
            outR[s] = dryR * (1.f - curMix) + wetR[s] * curMix;
    }
}

//==============================================================================
juce::AudioProcessorEditor* TriadAudioProcessor::createEditor()
{
    return new TriadAudioProcessorEditor (*this);
}

//==============================================================================
void TriadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    // Save parameters
    mos.writeFloat (layer1_pitch->get());
    mos.writeFloat (layer2_pitch->get());
    mos.writeFloat (layer3_pitch->get());
    mos.writeFloat (size->get());
    mos.writeFloat (decay->get());
    mos.writeFloat (shimmer_mix->get());
    mos.writeFloat (lfo_rate->get());
    mos.writeFloat (lfo_depth->get());
    mos.writeFloat (lfo_target->get());
    mos.writeFloat (seq_steps->get());
    mos.writeFloat (seq_rate->get());
    mos.writeFloat (seq_target->get());
    mos.writeFloat (mix->get());
    // Save sequencer step values
    for (int i = 0; i < 16; ++i)
        mos.writeFloat(seqStepValues[i]);
}

void TriadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t)sizeInBytes, false);
    if (mis.getNumBytesRemaining() < 13 * 4) return;
    *layer1_pitch = mis.readFloat();
    *layer2_pitch = mis.readFloat();
    *layer3_pitch = mis.readFloat();
    *size         = mis.readFloat();
    *decay        = mis.readFloat();
    *shimmer_mix  = mis.readFloat();
    *lfo_rate     = mis.readFloat();
    *lfo_depth    = mis.readFloat();
    *lfo_target   = mis.readFloat();
    *seq_steps    = mis.readFloat();
    *seq_rate     = mis.readFloat();
    *seq_target   = mis.readFloat();
    *mix          = mis.readFloat();
    for (int i = 0; i < 16 && mis.getNumBytesRemaining() >= 4; ++i)
        seqStepValues[i] = mis.readFloat();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TriadAudioProcessor();
}
