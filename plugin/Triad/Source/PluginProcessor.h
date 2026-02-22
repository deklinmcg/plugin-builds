/*
  ==============================================================================
    TriadAudioProcessor.h
    Three-layer shimmer reverb with 16-step sequencer and LFO modulation.
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

// Forward declaration
class TriadAudioProcessorEditor;

//==============================================================================
// Simple all-pass filter for diffusion
class AllPassFilter
{
public:
    AllPassFilter() {}
    void prepare(int maxDelayInSamples)
    {
        buffer.assign(maxDelayInSamples + 1, 0.f);
        bufferSize = maxDelayInSamples + 1;
        writePos = 0;
    }
    void setDelay(int d) { delayInSamples = juce::jlimit(1, (int)buffer.size() - 1, d); }
    float process(float input, float g = 0.7f)
    {
        float delayed = buffer[(writePos + bufferSize - delayInSamples) % bufferSize];
        float w = input - g * delayed;
        buffer[writePos] = w;
        writePos = (writePos + 1) % bufferSize;
        return delayed + g * w;
    }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.f); }
private:
    std::vector<float> buffer;
    int bufferSize = 1, writePos = 0, delayInSamples = 1;
};

//==============================================================================
// Simple comb filter
class CombFilter
{
public:
    CombFilter() {}
    void prepare(int maxDelay)
    {
        buffer.assign(maxDelay + 1, 0.f);
        bufferSize = maxDelay + 1;
        writePos = 0;
    }
    void setDelay(int d) { delayInSamples = juce::jlimit(1, (int)buffer.size() - 1, d); }
    float process(float input, float feedback)
    {
        float delayed = buffer[(writePos + bufferSize - delayInSamples) % bufferSize];
        float out = delayed;
        buffer[writePos] = input + delayed * feedback;
        writePos = (writePos + 1) % bufferSize;
        return out;
    }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.f); }
private:
    std::vector<float> buffer;
    int bufferSize = 1, writePos = 0, delayInSamples = 1;
};

//==============================================================================
// Pitch shifter using two overlapping windows (simple granular)
class PitchShifter
{
public:
    static constexpr int kMaxBuf = 65536;
    PitchShifter() {}
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        buffer.assign(kMaxBuf, 0.f);
        writeHead = 0;
        readHead1 = 0.0;
        readHead2 = 0.0;
        windowSize = (int)(0.08 * sr); // 80ms window
        hopSize = windowSize / 2;
        windowPhase1 = 0.0;
        windowPhase2 = 0.5;
        pitchRatio = 1.0;
    }
    void setSemitones(float semitones)
    {
        pitchRatio = std::pow(2.0, (double)semitones / 12.0);
    }
    float process(float input)
    {
        buffer[writeHead % kMaxBuf] = input;
        writeHead++;

        double speed = 1.0 - pitchRatio;

        // Advance read heads
        readHead1 += pitchRatio;
        readHead2 += pitchRatio;

        // Window phases advance at hop rate
        windowPhase1 += 1.0 / (double)windowSize;
        windowPhase2 += 1.0 / (double)windowSize;
        if (windowPhase1 >= 1.0) { windowPhase1 -= 1.0; readHead1 = readHead2 + hopSize; }
        if (windowPhase2 >= 1.0) { windowPhase2 -= 1.0; readHead2 = readHead1 + hopSize; }

        auto readSample = [&](double rh) -> float {
            int i0 = (int)rh;
            float frac = (float)(rh - i0);
            int idx0 = ((writeHead - 1 - i0) % kMaxBuf + kMaxBuf) % kMaxBuf;
            int idx1 = ((writeHead - 1 - i0 - 1) % kMaxBuf + kMaxBuf) % kMaxBuf;
            return buffer[idx0] * (1.f - frac) + buffer[idx1] * frac;
        };

        float win1 = (float)(0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * windowPhase1));
        float win2 = (float)(0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * windowPhase2));

        double rh1 = std::fmod(readHead1, (double)(kMaxBuf / 2));
        double rh2 = std::fmod(readHead2, (double)(kMaxBuf / 2));
        if (rh1 < 0) rh1 += kMaxBuf / 2;
        if (rh2 < 0) rh2 += kMaxBuf / 2;

        return readSample(rh1) * win1 + readSample(rh2) * win2;
    }
    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.f);
        writeHead = 0;
        readHead1 = readHead2 = 0.0;
        windowPhase1 = 0.0;
        windowPhase2 = 0.5;
    }
private:
    std::vector<float> buffer;
    int writeHead = 0;
    double readHead1 = 0.0, readHead2 = 0.0;
    double windowPhase1 = 0.0, windowPhase2 = 0.5;
    double pitchRatio = 1.0;
    double sr = 44100.0;
    int windowSize = 3528, hopSize = 1764;
};

//==============================================================================
// One shimmer reverb layer (stereo)
struct ShimmerLayer
{
    static constexpr int kNumAllPass = 4;
    static constexpr int kNumComb   = 4;

    AllPassFilter apL[kNumAllPass], apR[kNumAllPass];
    CombFilter    cbL[kNumComb],   cbR[kNumComb];
    PitchShifter  pitchL, pitchR;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setSize(float size);
    void setDecay(float decay, float size);
    void setSemitones(float s) { pitchL.setSemitones(s); pitchR.setSemitones(s); }
    // returns {outL, outR}
    std::pair<float,float> process(float inL, float inR, float shimmerMix);

    double sr = 44100.0;
    float feedback = 0.7f;

    // comb delays in samples
    static const int combDelaysL[kNumComb];
    static const int combDelaysR[kNumComb];
    static const int apDelays[kNumAllPass];
};

//==============================================================================
class TriadAudioProcessor  : public juce::AudioProcessor
{
public:
    TriadAudioProcessor();
    ~TriadAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Triad"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    //==============================================================================
    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameters
    juce::AudioParameterFloat* layer1_pitch  = nullptr;
    juce::AudioParameterFloat* layer2_pitch  = nullptr;
    juce::AudioParameterFloat* layer3_pitch  = nullptr;
    juce::AudioParameterFloat* size          = nullptr;
    juce::AudioParameterFloat* decay         = nullptr;
    juce::AudioParameterFloat* shimmer_mix   = nullptr;
    juce::AudioParameterFloat* lfo_rate      = nullptr;
    juce::AudioParameterFloat* lfo_depth     = nullptr;
    juce::AudioParameterFloat* lfo_target    = nullptr;
    juce::AudioParameterFloat* seq_steps     = nullptr;
    juce::AudioParameterFloat* seq_rate      = nullptr;
    juce::AudioParameterFloat* seq_target    = nullptr;
    juce::AudioParameterFloat* mix           = nullptr;

    // Sequencer step values (16 steps, publicly accessible for UI)
    float seqStepValues[16];
    std::atomic<int> currentSeqStep { 0 };

    // Current modulated parameter values (for UI metering)
    std::atomic<float> modValue { 0.f };

private:
    //==============================================================================
    void updateModulation(double bpm);
    float applyModulation(float baseValue, float lfoMod, float seqMod, int target,
                          float p1, float p2, float p3, float sz, float dc, float mx);

    //==============================================================================
    ShimmerLayer layers[3];

    // LFO state
    double lfoPhase = 0.0;

    // Sequencer state
    double seqPhaseAccum = 0.0;  // in beats
    int    seqStep       = 0;
    double sampleRate    = 44100.0;

    // Smoothed layer pitches for pitch shifters
    juce::SmoothedValue<float> pitch1Smooth, pitch2Smooth, pitch3Smooth;
    juce::SmoothedValue<float> sizeSmooth, decaySmooth, shimmerSmooth, mixSmooth;

    // Pre-allocated temp buffer
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> layerBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriadAudioProcessor)
};
