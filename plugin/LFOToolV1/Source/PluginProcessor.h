// LFOToolV1AudioProcessor.h
#pragma once
#include <JuceHeader.h>

class LFOToolV1AudioProcessor : public juce::AudioProcessor
{
public:
    LFOToolV1AudioProcessor();
    ~LFOToolV1AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LFOToolV1"; }
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
    juce::AudioParameterFloat* paramRate;
    juce::AudioParameterFloat* paramRateSync;
    juce::AudioParameterFloat* paramSyncMode;
    juce::AudioParameterFloat* paramShape;
    juce::AudioParameterFloat* paramPhase;
    juce::AudioParameterFloat* paramVolumeDepth;
    juce::AudioParameterFloat* paramFilterDepth;
    juce::AudioParameterFloat* paramPanDepth;
    juce::AudioParameterFloat* paramFilterCutoff;
    juce::AudioParameterFloat* paramFilterRes;
    juce::AudioParameterFloat* paramFilterType;
    juce::AudioParameterFloat* paramSmooth;
    juce::AudioParameterFloat* paramMix;
    juce::AudioParameterFloat* paramOutputGain;

private:
    double currentSampleRate;
    int currentBlockSize;

    // LFO state
    double lfoPhase;
    double smoothedLFO;

    // SVF state per channel (2 channels)
    double svfIc1[2];
    double svfIc2[2];

    // SVF coefficient cache
    double svfA1, svfA2, svfA3, svfG, svfK;
    double cachedModCutoff;
    int svfUpdateCounter;

    // Playback state tracking
    bool wasPlaying;
    double lastBPM;

    // Division table: beat values for each RateSync index
    // 1/32, 1/16, 1/8T, 1/8, 1/4T, 1/4, 1/2, 1/1, 2/1, 4/1
    static const double syncDivisions[10];

    void computeSVFCoeffs(double modCutoff, double mappedQ);
    double processSVFSample(double x, int channel, int filterType);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOToolV1AudioProcessor)
};
