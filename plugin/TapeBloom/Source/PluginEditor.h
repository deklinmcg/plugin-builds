#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TapeBloomAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit TapeBloomAudioProcessorEditor (TapeBloomAudioProcessor&);
    ~TapeBloomAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    TapeBloomAudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBloomAudioProcessorEditor)
};
