#pragma once
#include <JuceHeader.h>

class JunoBBDAudioProcessor : public juce::AudioProcessor
{
public:
    JunoBBDAudioProcessor();
    ~JunoBBDAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "JunoBBD"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioParameterFloat* modeParam  = nullptr;
    juce::AudioParameterFloat* rateParam  = nullptr;
    juce::AudioParameterFloat* depthParam = nullptr;
    juce::AudioParameterFloat* driftParam = nullptr;
    juce::AudioParameterFloat* toneParam  = nullptr;
    juce::AudioParameterFloat* widthParam = nullptr;
    juce::AudioParameterFloat* mixParam   = nullptr;

private:
    // BBD delay line helpers
    static constexpr int   kMaxDelaySamples = 8192;
    static constexpr float kBaseDelayMs     = 1.5f;   // centre delay ms
    static constexpr float kModDepthMs      = 1.8f;   // max additional LFO sweep

    // Per-channel delay buffers (left/right BBD paths)
    std::vector<float> delayBufL1, delayBufR1; // Mode I paths
    std::vector<float> delayBufL2, delayBufR2; // Mode II extra paths
    int writeIdxL1 = 0, writeIdxR1 = 0;
    int writeIdxL2 = 0, writeIdxR2 = 0;

    // LFO state
    double lfoPhase1 = 0.0; // Mode I LFO (same phase, opposite sign on R)
    double lfoPhase2 = 0.0; // Mode II second LFO (90-deg offset from phase1)

    // Tone filters (one-pole LP per channel)
    float toneZL = 0.f, toneZR = 0.f;
    float toneZL2= 0.f, toneZR2= 0.f;

    // DC-block / anti-aliasing filters
    float dcBlockX[4]  = {};
    float dcBlockY[4]  = {};

    // Drift random-walk state
    float driftValL1 = 0.f, driftValR1 = 0.f;
    float driftValL2 = 0.f, driftValR2 = 0.f;
    juce::Random rng;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    // Interpolated read from delay buffer
    float readDelayLinear(const std::vector<float>& buf, int writeIdx, float delaySamples) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoBBDAudioProcessor)
};
