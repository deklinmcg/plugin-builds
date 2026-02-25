#pragma once

#include <JuceHeader.h>

class TapeBloomAudioProcessor : public juce::AudioProcessor
{
public:
    TapeBloomAudioProcessor();
    ~TapeBloomAudioProcessor() override;

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
    juce::AudioParameterFloat* warmthParam;
    juce::AudioParameterFloat* fluxParam;
    juce::AudioParameterFloat* bloomParam;
    juce::AudioParameterFloat* outputParam;

private:
    // Per-channel state variables
    double hp_prev[2];
    double x_prev[2];
    double bloom_lp_prev[2];

    double currentSampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeBloomAudioProcessor)
};