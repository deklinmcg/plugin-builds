#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WebViewTestAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::Timer
{
public:
    explicit WebViewTestAudioProcessorEditor (WebViewTestAudioProcessor&);
    ~WebViewTestAudioProcessorEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override { webView.setBounds (getLocalBounds()); }
    void timerCallback() override;

private:
    WebViewTestAudioProcessor& audioProcessor;
    juce::WebBrowserComponent webView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewTestAudioProcessorEditor)
};
