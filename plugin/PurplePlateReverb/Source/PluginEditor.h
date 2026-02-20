#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PurplePlateReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    PurplePlateReverbAudioProcessorEditor (PurplePlateReverbAudioProcessor&);
    ~PurplePlateReverbAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    PurplePlateReverbAudioProcessor& audioProcessor;
};
