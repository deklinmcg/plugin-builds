#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class TriadAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit TriadAudioProcessorEditor (TriadAudioProcessor&);
    ~TriadAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TriadAudioProcessor& audioProcessor;

    // 1. RELAYS FIRST (no dependencies — destroyed last)
    juce::WebSliderRelay layer1_pitchRelay { ParameterIDs::layer1_pitch };
    juce::WebSliderRelay layer2_pitchRelay { ParameterIDs::layer2_pitch };
    juce::WebSliderRelay layer3_pitchRelay { ParameterIDs::layer3_pitch };
    juce::WebSliderRelay sizeRelay { ParameterIDs::size };
    juce::WebSliderRelay decayRelay { ParameterIDs::decay };
    juce::WebSliderRelay shimmer_mixRelay { ParameterIDs::shimmer_mix };
    juce::WebSliderRelay lfo_rateRelay { ParameterIDs::lfo_rate };
    juce::WebSliderRelay lfo_depthRelay { ParameterIDs::lfo_depth };
    juce::WebComboBoxRelay lfo_targetRelay { ParameterIDs::lfo_target };
    juce::WebSliderRelay seq_stepsRelay { ParameterIDs::seq_steps };
    juce::WebSliderRelay seq_rateRelay { ParameterIDs::seq_rate };
    juce::WebComboBoxRelay seq_targetRelay { ParameterIDs::seq_target };
    juce::WebSliderRelay mixRelay { ParameterIDs::mix };

    // 2. WEBVIEW SECOND (depends on relays via withOptionsFrom)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (depend on relays + parameters — destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> layer1_pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> layer2_pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> layer3_pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> decayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> shimmer_mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfo_rateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfo_depthAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> lfo_targetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> seq_stepsAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> seq_rateAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> seq_targetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    static juce::String getHTML();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriadAudioProcessorEditor)
};
