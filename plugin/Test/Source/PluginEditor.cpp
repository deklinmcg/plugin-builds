#include "PluginProcessor.h"
#include "PluginEditor.h"

TestAudioProcessorEditor::TestAudioProcessorEditor (TestAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) { setSize (400, 300); }

TestAudioProcessorEditor::~TestAudioProcessorEditor() {}
void TestAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void TestAudioProcessorEditor::resized() {}
