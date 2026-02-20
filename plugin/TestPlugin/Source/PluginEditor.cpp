#include "PluginProcessor.h"
#include "PluginEditor.h"

TestPluginAudioProcessorEditor::TestPluginAudioProcessorEditor (TestPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

TestPluginAudioProcessorEditor::~TestPluginAudioProcessorEditor() {}
void TestPluginAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void TestPluginAudioProcessorEditor::resized() {}
