#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WornEchoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit WornEchoAudioProcessorEditor (WornEchoAudioProcessor&);
    ~WornEchoAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    WornEchoAudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WornEchoAudioProcessorEditor)
};
