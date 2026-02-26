#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TapeTopAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit TapeTopAudioProcessorEditor (TapeTopAudioProcessor&);
    ~TapeTopAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    TapeTopAudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeTopAudioProcessorEditor)
};
