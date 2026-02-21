#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpacePingAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    SpacePingAudioProcessorEditor (SpacePingAudioProcessor&);
    ~SpacePingAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    SpacePingAudioProcessor& audioProcessor;
};
