#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TestPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    TestPluginAudioProcessorEditor (TestPluginAudioProcessor&);
    ~TestPluginAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TestPluginAudioProcessor& audioProcessor;
};
