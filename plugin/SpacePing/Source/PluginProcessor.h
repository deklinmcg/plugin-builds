#pragma once
#include <JuceHeader.h>

class SpacePingAudioProcessor : public juce::AudioProcessor
{
public:
    SpacePingAudioProcessor();
    ~SpacePingAudioProcessor() override;

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
    juce::AudioParameterFloat* timeParam;
    juce::AudioParameterFloat* feedbackParam;
    juce::AudioParameterFloat* wowFlutterParam;
    juce::AudioParameterFloat* toneParam;
    juce::AudioParameterFloat* spreadParam;
    juce::AudioParameterFloat* mixParam;

    // Delay buffers for ping pong (Left and Right)
    juce::AudioBuffer<float> delayBufferL;
    juce::AudioBuffer<float> delayBufferR;
    int delayBufferSize = 0;
    int writePositionL = 0;
    int writePositionR = 0;

    // Tone filter state (simple one-pole low-pass per channel)
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;

    // Wow & Flutter LFO phases
    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;

    // Saturation/tape character
    float tapeWarmth(float sample);

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpacePingAudioProcessor)
};
