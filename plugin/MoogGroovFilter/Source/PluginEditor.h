#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MoogGroovFilterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit MoogGroovFilterAudioProcessorEditor (MoogGroovFilterAudioProcessor&);
    ~MoogGroovFilterAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MoogGroovFilterAudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogGroovFilterAudioProcessorEditor)
};
