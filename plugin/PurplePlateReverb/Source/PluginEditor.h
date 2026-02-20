#ifndef PLUGINEDITOR_H_INCLUDED
#define PLUGINEDITOR_H_INCLUDED

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PurplePlateReverbAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    PurplePlateReverbAudioProcessorEditor(PurplePlateReverbAudioProcessor&);
    ~PurplePlateReverbAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the processor
    PurplePlateReverbAudioProcessor& processor;
};

#endif // PLUGINEDITOR_H_INCLUDED