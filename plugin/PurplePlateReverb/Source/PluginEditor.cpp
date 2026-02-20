#include "PluginProcessor.h"
#include "PluginEditor.h"

PurplePlateReverbAudioProcessorEditor::PurplePlateReverbAudioProcessorEditor (PurplePlateReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

PurplePlateReverbAudioProcessorEditor::~PurplePlateReverbAudioProcessorEditor() {}
void PurplePlateReverbAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void PurplePlateReverbAudioProcessorEditor::resized() {}
