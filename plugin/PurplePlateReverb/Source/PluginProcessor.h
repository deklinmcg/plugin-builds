#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include <JuceHeader.h>

class PurplePlateReverbAudioProcessor  : public juce::AudioProcessor
{
public:
    PurplePlateReverbAudioProcessor();
    ~PurplePlateReverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    #define NUM_PARAMETERS 7
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // Parameter management
    void setParameter(int parameterIndex, float newValue);
    float getParameter(int parameterIndex) const;

    void getStateInformation (juce::MemoryBlock& d) override;
    void setStateInformation (const void* d, int s) override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    const juce::String getName() const override { return "PurplePlateReverb"; }
    double getTailLengthSeconds() const override { return 0.0; }
private:
    // Parameter variables
    float size, decay, brightness, mix, modDepth, gate, gateRate;

    JUCE_LEAK_DETECTOR(PurplePlateReverbAudioProcessor)
};

#endif // PLUGINPROCESSOR_H_INCLUDED