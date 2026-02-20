#pragma once
#include <JuceHeader.h>

class PurpleplatereverbeAudioProcessor : public juce::AudioProcessor
{
public:
    PurpleplatereverbeAudioProcessor();
    ~PurpleplatereverbeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int dataSize) override;

private:
    juce::AudioParameterFloat* decayParam;
    juce::AudioParameterFloat* dampingParam;
    juce::AudioParameterFloat* predelayParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* modRateParam;
    juce::AudioParameterFloat* modDepthParam;
    juce::AudioParameterFloat* gateThreshParam;
    juce::AudioParameterFloat* gateReleaseParam;
    juce::AudioParameterFloat* widthParam;
    juce::AudioParameterFloat* highCutParam;

    double currentSampleRate = 44100.0;

    // Plate reverb diffusion/delay lines
    static const int kNumAPF = 4;
    static const int kNumDelays = 4;
    static const int kMaxDelaySamples = 192000; // enough for long tails

    // All-pass filters for diffusion
    std::array<juce::AudioBuffer<float>, kNumAPF> apfBuffers;
    std::array<int, kNumAPF> apfLengths;
    std::array<int, kNumAPF> apfIndex;
    float apfGain = 0.6f;

    // Delay lines for tank
    std::array<juce::AudioBuffer<float>, kNumDelays> delayBuffers;
    std::array<int, kNumDelays> delayLengths;
    std::array<int, kNumDelays> delayIndex;

    // Pre-delay
    juce::AudioBuffer<float> predelayBuffer;
    int predelayIndex = 0;
    int maxPredelaySamples = 0;

    // Damping / low-pass state
    std::array<float, kNumDelays> dampState;

    // Modulation LFO
    float lfoPhase = 0.0f;

    // Gate envelope follower
    float gateEnv = 0.0f;

    // Tank feedback state
    float tankL = 0.0f;
    float tankR = 0.0f;

    // High-cut filter state
    float highCutStateL = 0.0f;
    float highCutStateR = 0.0f;

    float readDelayInterp(juce::AudioBuffer<float>& buf, int writeIdx, float delaySamples, int length);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PurpleplatereverbeAudioProcessor)
};
