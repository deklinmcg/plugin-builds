#pragma once
#include <JuceHeader.h>

class PurplePlateReverbAudioProcessor : public juce::AudioProcessor
{
public:
    PurplePlateReverbAudioProcessor();
    ~PurplePlateReverbAudioProcessor() override;

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
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    // Parameters
    juce::AudioParameterFloat* sizeParam;
    juce::AudioParameterFloat* decayParam;
    juce::AudioParameterFloat* brightnessParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* modDepthParam;
    juce::AudioParameterFloat* gateParam;
    juce::AudioParameterFloat* gateRateParam;

    // Internal state
    double currentSampleRate = 44100.0;

    // Plate reverb implemented as a network of allpass filters and delay lines
    static const int kNumAllpasses = 8;
    static const int kNumDelays = 4;
    static const int kMaxDelayLength = 131072;

    // Allpass filters
    struct AllpassFilter {
        juce::AudioBuffer<float> buffer;
        int writeIndex = 0;
        int length = 1;
        float feedback = 0.5f;
    };

    AllpassFilter allpasses[2][kNumAllpasses]; // [channel][index]

    // Delay lines for plate network
    struct DelayLine {
        juce::AudioBuffer<float> buffer;
        int writeIndex = 0;
        int length = 1;
    };

    DelayLine delays[2][kNumDelays]; // [channel][index]

    // Feedback state per channel
    float feedbackState[2] = {0.0f, 0.0f};

    // Lowpass filter state for brightness / damping
    float lpState[2] = {0.0f, 0.0f};

    // Modulation LFO
    float modPhase = 0.0f;

    // Gate LFO
    float gatePhase = 0.0f;

    // Pre-delay buffer
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWriteIndex = 0;
    int preDelayLength = 1;

    // Helper
    float readAllpass(AllpassFilter& ap, float input);
    float readDelay(DelayLine& dl, int offset);
    void writeDelay(DelayLine& dl, float sample);
    float readDelayInterp(DelayLine& dl, float offset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PurplePlateReverbAudioProcessor)
};
