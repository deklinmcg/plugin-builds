#pragma once

#include <JuceHeader.h>

class TapeTopAudioProcessor : public juce::AudioProcessor
{
public:
    TapeTopAudioProcessor();
    ~TapeTopAudioProcessor() override;

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

    // Parameters
    juce::AudioParameterFloat* driveParam;
    juce::AudioParameterFloat* hpfParam;
    juce::AudioParameterFloat* mixParam;

private:
    // Biquad state for 2 channels
    struct BiquadState
    {
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };

    BiquadState biquadState[2];

    // Biquad coefficients
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    double currentSampleRate = 44100.0;
    float lastHPF = -1.0f;

    void calculateCoefficients(float hpfHz);

    static constexpr double tanhNorm = 0.6043677771; // tanh(0.7)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeTopAudioProcessor)
};