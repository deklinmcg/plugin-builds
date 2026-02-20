#include "PluginProcessor.h"
#include "PluginEditor.h"

PurplePlateReverbAudioProcessor::PurplePlateReverbAudioProcessor()
    : size(0.5), decay(2.0), brightness(0.5), mix(0.5), modDepth(0.2), gate(0.0), gateRate(1.0) {}

PurplePlateReverbAudioProcessor::~PurplePlateReverbAudioProcessor() {}

void PurplePlateReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Clearing, initializing and preparing resources upon playback start
}

void PurplePlateReverbAudioProcessor::releaseResources() {
    // Free any resources allocated during preparation
}

void PurplePlateReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    // Processing audio
    // Here the DSP processing of the audio will take place
}

void PurplePlateReverbAudioProcessor::getStateInformation(juce::MemoryBlock& d) {}
void PurplePlateReverbAudioProcessor::setStateInformation(const void* d, int s) {}
juce::AudioProcessorEditor* PurplePlateReverbAudioProcessor::createEditor() {
    return nullptr; // UI code to be developed further
}

bool PurplePlateReverbAudioProcessor::hasEditor() const {
    return false; // Change to true when editor is available
}

void PurplePlateReverbAudioProcessor::setParameter(int parameterIndex, float newValue) {
    switch (parameterIndex) {
        case 0: size = newValue; break;
        case 1: decay = newValue; break;
        case 2: brightness = newValue; break;
        case 3: mix = newValue; break;
        case 4: modDepth = newValue; break;
        case 5: gate = newValue; break;
        case 6: gateRate = newValue; break;
        default: break;
    }
}

float PurplePlateReverbAudioProcessor::getParameter(int parameterIndex) const {
    switch (parameterIndex) {
        case 0: return size;
        case 1: return decay;
        case 2: return brightness;
        case 3: return mix;
        case 4: return modDepth;
        case 5: return gate;
        case 6: return gateRate;
        default: return 0.0f;
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurplePlateReverbAudioProcessor();
}
