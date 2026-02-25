#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class LFOToolV1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit LFOToolV1AudioProcessorEditor (LFOToolV1AudioProcessor&);
    ~LFOToolV1AudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LFOToolV1AudioProcessor& audioProcessor;
    static juce::String getHTML();
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOToolV1AudioProcessorEditor)
};
