#include "PluginProcessor.h"
#include "PluginEditor.h"

WebViewTestAudioProcessor::WebViewTestAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

WebViewTestAudioProcessor::~WebViewTestAudioProcessor() {}

juce::AudioProcessorEditor* WebViewTestAudioProcessor::createEditor()
{
    return new WebViewTestAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WebViewTestAudioProcessor();
}
