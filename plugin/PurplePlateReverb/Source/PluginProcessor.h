#pragma once
#include <JuceHeader.h>

class PurplePlateReverbAudioProcessor : public juce::AudioProcessor
{
public:
    PurplePlateReverbAudioProcessor();
    ~PurplePlateReverbAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    void getStateInformation (juce::MemoryBlock& d) override;
    void setStateInformation (const void* d, int s) override;

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    const juce::String getName() const override { return "PurplePlateReverb"; }
    double getTailLengthSeconds() const override { return 10.0; }

private:
    // Parameters — registered with addParameter() so DAW sees them
    juce::AudioParameterFloat* sizeParam      = nullptr;
    juce::AudioParameterFloat* decayParam     = nullptr;
    juce::AudioParameterFloat* brightnessParam = nullptr;
    juce::AudioParameterFloat* mixParam       = nullptr;
    juce::AudioParameterFloat* modDepthParam  = nullptr;
    juce::AudioParameterFloat* gateParam      = nullptr;
    juce::AudioParameterFloat* gateRateParam  = nullptr;

    double currentSampleRate = 44100.0;

    // Core reverb
    juce::Reverb reverb;
    void updateReverbParams (float size, float decay);

    // Modulation: chorus-style delay on wet tail
    static constexpr int kModBufSize = 4096;
    float modBufL[kModBufSize] = {};
    float modBufR[kModBufSize] = {};
    int   modWritePos = 0;
    float lfoPhase    = 0.0f;
    static constexpr float kLfoRateHz = 0.4f;

    // Gate
    float gatePhase     = 0.0f;
    float gateSlewState = 1.0f;

    // Brightness: 1-pole LP filter state
    float bFilterL = 0.0f;
    float bFilterR = 0.0f;

    JUCE_LEAK_DETECTOR (PurplePlateReverbAudioProcessor)
};
