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

    void  setParameter (int index, float value);
    float getParameter (int index) const;

    void getStateInformation (juce::MemoryBlock& d) override;
    void setStateInformation (const void* d, int s) override;

    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    const juce::String getName() const override { return "PurplePlateReverb"; }
    double getTailLengthSeconds() const override { return 10.0; }

private:
    // Parameters
    float size, decay, brightness, mix, modDepth, gate, gateRate;

    double currentSampleRate = 44100.0;

    // Core reverb
    juce::Reverb reverb;
    void updateReverbParams();

    // Modulation: chorus-style delay on wet tail
    static constexpr int kModBufSize = 4096; // power of 2, ~93ms @ 44.1k
    float modBufL[kModBufSize] = {};
    float modBufR[kModBufSize] = {};
    int   modWritePos = 0;
    float lfoPhase    = 0.0f;
    static constexpr float kLfoRateHz = 0.4f;

    // Gate
    float gatePhase     = 0.0f;
    float gateSlewState = 1.0f;

    // Brightness: 1-pole LP filter state per channel
    float bFilterL = 0.0f;
    float bFilterR = 0.0f;

    JUCE_LEAK_DETECTOR (PurplePlateReverbAudioProcessor)
};
