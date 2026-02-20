#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TremoloAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    TremoloAudioProcessorEditor (TremoloAudioProcessor&);
    ~TremoloAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TremoloAudioProcessor& audioProcessor;
};
