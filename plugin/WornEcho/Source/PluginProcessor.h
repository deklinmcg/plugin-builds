#pragma once
#include <JuceHeader.h>

class WornEchoAudioProcessor : public juce::AudioProcessor
{
public:
    WornEchoAudioProcessor();
    ~WornEchoAudioProcessor() override;

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

    juce::AudioParameterFloat* timeParam;
    juce::AudioParameterFloat* feedbackParam;
    juce::AudioParameterFloat* wowParam;
    juce::AudioParameterFloat* flutterParam;
    juce::AudioParameterFloat* saturationParam;
    juce::AudioParameterFloat* mixParam;

private:
    double currentSampleRate = 44100.0;
    int maxDelayInSamples = 0;

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePos = 0;

    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;

    float feedbackL = 0.0f;
    float feedbackR = 0.0f;

    // Low-pass filter state for tape warmth
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;

    float tapeDistort(float x, float drive);
    float hermiteInterpolate(const std::vector<float>& buffer, float readPos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WornEchoAudioProcessor)
};
