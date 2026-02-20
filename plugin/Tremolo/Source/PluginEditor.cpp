#include "PluginProcessor.h"
#include "PluginEditor.h"

TremoloAudioProcessorEditor::TremoloAudioProcessorEditor (TremoloAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

TremoloAudioProcessorEditor::~TremoloAudioProcessorEditor() {}
void TremoloAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void TremoloAudioProcessorEditor::resized() {}
