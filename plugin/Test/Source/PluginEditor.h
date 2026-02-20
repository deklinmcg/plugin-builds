#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TestAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    TestAudioProcessorEditor (TestAudioProcessor&);
    ~TestAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TestAudioProcessor& audioProcessor;
};
