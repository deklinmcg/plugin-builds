#pragma once
#include <JuceHeader.h>

class TremoloAudioProcessor : public juce::AudioProcessor
{
public:
    TremoloAudioProcessor();
    ~TremoloAudioProcessor() override;

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
    juce::AudioParameterFloat* speedParam;   // LFO rate in Hz (0.1 - 20 Hz)
    juce::AudioParameterFloat* depthParam;   // Modulation depth (0 - 1)
    juce::AudioParameterFloat* mixParam;     // Dry/Wet mix (0 - 1)
    juce::AudioParameterFloat* shapeParam;   // Waveform shape: 0=sine, 1=triangle

    double currentSampleRate = 44100.0;
    double lfoPhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TremoloAudioProcessor)
};
