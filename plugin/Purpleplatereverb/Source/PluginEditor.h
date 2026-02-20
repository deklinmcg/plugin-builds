#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PurpleplatereverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    PurpleplatereverbAudioProcessorEditor (PurpleplatereverbAudioProcessor&);
    ~PurpleplatereverbAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    PurpleplatereverbAudioProcessor& audioProcessor;
};
