#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class JunoBBDAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit JunoBBDAudioProcessorEditor (JunoBBDAudioProcessor&);
    ~JunoBBDAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JunoBBDAudioProcessor& audioProcessor;

    // 1. RELAYS FIRST (no dependencies — destroyed last)
    juce::WebSliderRelay modeRelay { ParameterIDs::mode };
    juce::WebSliderRelay rateRelay { ParameterIDs::rate };
    juce::WebSliderRelay depthRelay { ParameterIDs::depth };
    juce::WebSliderRelay driftRelay { ParameterIDs::drift };
    juce::WebSliderRelay toneRelay { ParameterIDs::tone };
    juce::WebSliderRelay widthRelay { ParameterIDs::width };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };

    // 2. WEBVIEW SECOND (depends on relays via withOptionsFrom)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (depend on relays + parameters — destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> modeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> rateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> depthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> driftAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    static juce::String getHTML();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JunoBBDAudioProcessorEditor)
};
