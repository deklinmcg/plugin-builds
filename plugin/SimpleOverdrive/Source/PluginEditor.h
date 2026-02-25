#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SimpleOverdriveAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit SimpleOverdriveAudioProcessorEditor (SimpleOverdriveAudioProcessor&);
    ~SimpleOverdriveAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SimpleOverdriveAudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleOverdriveAudioProcessorEditor)
};
