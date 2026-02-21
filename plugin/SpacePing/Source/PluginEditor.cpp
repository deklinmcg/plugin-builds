#include "PluginProcessor.h"
#include "PluginEditor.h"

SpacePingAudioProcessorEditor::SpacePingAudioProcessorEditor (SpacePingAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

SpacePingAudioProcessorEditor::~SpacePingAudioProcessorEditor() {}
void SpacePingAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void SpacePingAudioProcessorEditor::resized() {}
