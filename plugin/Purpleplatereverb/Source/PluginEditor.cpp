#include "PluginProcessor.h"
#include "PluginEditor.h"

PurpleplatereverbAudioProcessorEditor::PurpleplatereverbAudioProcessorEditor (PurpleplatereverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

PurpleplatereverbAudioProcessorEditor::~PurpleplatereverbAudioProcessorEditor() {}
void PurpleplatereverbAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void PurpleplatereverbAudioProcessorEditor::resized() {}
