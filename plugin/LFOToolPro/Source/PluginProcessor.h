#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class LFOToolProAudioProcessor : public juce::AudioProcessor
{
public:
    LFOToolProAudioProcessor();
    ~LFOToolProAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LFOToolPro"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters - LFO1
    juce::AudioParameterFloat* lfo1Shape;
    juce::AudioParameterFloat* lfo1Rate;
    juce::AudioParameterFloat* lfo1Sync;
    juce::AudioParameterFloat* lfo1Depth;
    juce::AudioParameterFloat* lfo1Phase;
    juce::AudioParameterFloat* lfo1GlobalDepth;

    // Parameters - LFO2
    juce::AudioParameterFloat* lfo2Shape;
    juce::AudioParameterFloat* lfo2Rate;
    juce::AudioParameterFloat* lfo2Sync;
    juce::AudioParameterFloat* lfo2Depth;
    juce::AudioParameterFloat* lfo2Phase;
    juce::AudioParameterFloat* lfo2GlobalDepth;
    juce::AudioParameterFloat* filterBase;

    // Parameters - LFO3
    juce::AudioParameterFloat* lfo3Shape;
    juce::AudioParameterFloat* lfo3Rate;
    juce::AudioParameterFloat* lfo3Sync;
    juce::AudioParameterFloat* lfo3Depth;
    juce::AudioParameterFloat* lfo3Phase;
    juce::AudioParameterFloat* lfo3GlobalDepth;

    // Parameters - LFO4
    juce::AudioParameterFloat* lfo4Shape;
    juce::AudioParameterFloat* lfo4Rate;
    juce::AudioParameterFloat* lfo4Sync;
    juce::AudioParameterFloat* lfo4Depth;
    juce::AudioParameterFloat* lfo4Phase;
    juce::AudioParameterFloat* lfo4GlobalDepth;
    juce::AudioParameterFloat* reverbBaseSize;
    juce::AudioParameterFloat* reverbDamping;

    // Output
    juce::AudioParameterFloat* outputTrim;

private:
    double currentSampleRate = 44100.0;

    // LFO phases
    float lfoPhase[4] = {0.f, 0.f, 0.f, 0.f};

    // Biquad filter state: [channel][x1,x2,y1,y2]
    double biquadState[2][4];
    // Biquad coefficients
    double bqA0, bqA1, bqA2, bqB1, bqB2;

    // Comb filter buffers [filterIndex][channel]
    std::vector<float> combBuffer[4][2];
    int combWritePos[4][2];
    int combDelayLength[4]; // in samples (scaled)

    // Comb filter damping state (1-pole lowpass inside comb)
    float combDampState[4][2];

    // Allpass filter buffers [filterIndex][channel]
    std::vector<float> allpassBuffer[2][2];
    int allpassWritePos[2][2];
    int allpassDelayLength[2]; // in samples (scaled)

    // Stereo work buffer
    juce::AudioBuffer<float> stereoBuffer;

    // Helper methods
    float computeLFO(float phase, int shape);
    void updateBiquadCoefficients(double cutoffHz);
    float processBiquad(float input, int channel);
    float processComb(float input, int filterIdx, int channel, float feedback, float dampCoeff);
    float processAllpass(float input, int filterIdx, int channel, float gain);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOToolProAudioProcessor)
};