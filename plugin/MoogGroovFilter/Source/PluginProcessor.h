#pragma once
#include <JuceHeader.h>

class MoogGroovFilterAudioProcessor : public juce::AudioProcessor
{
public:
    MoogGroovFilterAudioProcessor();
    ~MoogGroovFilterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MoogGroovFilter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioParameterFloat* cutoffParam;
    juce::AudioParameterFloat* resonanceParam;
    juce::AudioParameterFloat* lfoRateParam;
    juce::AudioParameterFloat* lfoSyncParam;
    juce::AudioParameterFloat* syncModeParam;
    juce::AudioParameterFloat* lfoShapeParam;
    juce::AudioParameterFloat* lfoDepthParam;
    juce::AudioParameterFloat* lfoPolarityParam;
    juce::AudioParameterFloat* driveParam;
    juce::AudioParameterFloat* mixParam;
    juce::AudioParameterFloat* outputParam;

private:
    double currentSampleRate = 44100.0;

    // LFO state
    double lfoPhase = 0.0;

    // Ladder filter state: 2 channels x 4 stages
    double stage[2][4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoogGroovFilterAudioProcessor)
};
