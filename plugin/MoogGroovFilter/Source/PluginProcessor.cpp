#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

static const float kDivisionValues[] = { 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f };
static const int kNumDivisions = 6;

MoogGroovFilterAudioProcessor::MoogGroovFilterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(cutoffParam     = new juce::AudioParameterFloat("cutoff",      "Cutoff",      juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f));
    addParameter(resonanceParam  = new juce::AudioParameterFloat("resonance",   "Resonance",   0.0f,  0.95f,  0.3f));
    addParameter(lfoRateParam    = new juce::AudioParameterFloat("lfoRate",     "LFO Rate",    juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.35f), 1.0f));
    addParameter(lfoSyncParam    = new juce::AudioParameterFloat("lfoSync",     "LFO Sync",    0.0f,  5.0f,   2.0f));
    addParameter(syncModeParam   = new juce::AudioParameterFloat("syncMode",    "SyncMode",    0.0f,  1.0f,   0.0f));
    addParameter(lfoShapeParam   = new juce::AudioParameterFloat("lfoShape",    "LFO Shape",   0.0f,  1.0f,   0.0f));
    addParameter(lfoDepthParam   = new juce::AudioParameterFloat("lfoDepth",    "LFO Depth",   0.0f,  1.0f,   0.5f));
    addParameter(lfoPolarityParam= new juce::AudioParameterFloat("lfoPolarity", "LFO Polarity",0.0f,  1.0f,   0.0f));
    addParameter(driveParam      = new juce::AudioParameterFloat("drive",       "Drive",       0.0f,  1.0f,   0.2f));
    addParameter(mixParam        = new juce::AudioParameterFloat("mix",         "Mix",         0.0f,  1.0f,   1.0f));
    addParameter(outputParam     = new juce::AudioParameterFloat("output",      "Output",      -12.0f, 6.0f,  0.0f));

    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 4; ++s)
            stage[ch][s] = 0.0;
}

MoogGroovFilterAudioProcessor::~MoogGroovFilterAudioProcessor() {}

void MoogGroovFilterAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    lfoPhase = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 4; ++s)
            stage[ch][s] = 0.0;
}

void MoogGroovFilterAudioProcessor::releaseResources() {}

bool MoogGroovFilterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto outSet = layouts.getMainOutputChannelSet();
    auto inSet  = layouts.getMainInputChannelSet();
    if (outSet == juce::AudioChannelSet::mono()   && inSet == juce::AudioChannelSet::mono())   return true;
    if (outSet == juce::AudioChannelSet::stereo()  && inSet == juce::AudioChannelSet::stereo()) return true;
    if (outSet == juce::AudioChannelSet::stereo()  && inSet == juce::AudioChannelSet::mono())   return true;
    return false;
}

void MoogGroovFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalOut   = getTotalNumOutputChannels();
    const int totalIn    = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Read parameters
    const float cutoff      = cutoffParam->get();
    const float resonance   = resonanceParam->get();
    const float lfoRateHz   = lfoRateParam->get();
    const int   syncDiv     = juce::jlimit(0, kNumDivisions - 1, (int)std::round(lfoSyncParam->get()));
    const bool  syncMode    = syncModeParam->get() >= 0.5f;
    const bool  shapeTriangle = lfoShapeParam->get() >= 0.5f;
    const float lfoDepth    = lfoDepthParam->get();
    const bool  unipolar    = lfoPolarityParam->get() >= 0.5f;
    const float drive       = driveParam->get();
    const float mix         = mixParam->get();
    const float outputGain  = std::pow(10.0f, outputParam->get() / 20.0f);

    // Determine effective LFO rate
    double effectiveLfoRate = (double)lfoRateHz;
    if (syncMode)
    {
        double bpm = 120.0;
        auto* ph = getPlayHead();
        if (ph != nullptr)
        {
            juce::AudioPlayHead::CurrentPositionInfo posInfo;
            if (ph->getCurrentPosition(posInfo))
                bpm = posInfo.bpm;
        }
        double divVal = (double)kDivisionValues[syncDiv];
        effectiveLfoRate = bpm / 60.0 / divVal;
    }

    const double sr       = currentSampleRate;
    const double twoPi    = 2.0 * juce::MathConstants<double>::pi;
    const double phaseInc = effectiveLfoRate / sr;

    if (totalOut == 1)
    {
        // Mono processing
        auto* channelData = buffer.getWritePointer(0);

        for (int n = 0; n < numSamples; ++n)
        {
            // LFO
            lfoPhase += phaseInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            double lfoOut;
            if (!shapeTriangle)
                lfoOut = std::sin(twoPi * lfoPhase);
            else
                lfoOut = 1.0 - 4.0 * std::abs(lfoPhase - 0.5);

            if (unipolar)
                lfoOut = (lfoOut + 1.0) * 0.5;

            // Cutoff modulation
            double modAmount = (double)lfoDepth * lfoOut;
            double modFreq   = (double)cutoff * std::pow(2.0, modAmount * 4.0);
            if (modFreq < 20.0)    modFreq = 20.0;
            if (modFreq > 20000.0) modFreq = 20000.0;
            double wc = twoPi * modFreq / sr;
            if (wc > juce::MathConstants<double>::pi) wc = juce::MathConstants<double>::pi;

            // Ladder filter (Huovilainen style)
            double inputSample = (double)channelData[n];
            double driveGain   = 1.0 + (double)drive * 3.0;
            double fb          = (double)resonance * 4.0 * stage[0][3];
            double x           = std::tanh(inputSample * driveGain - fb);

            stage[0][0] += wc * (std::tanh(x)           - std::tanh(stage[0][0]));
            stage[0][1] += wc * (std::tanh(stage[0][0]) - std::tanh(stage[0][1]));
            stage[0][2] += wc * (std::tanh(stage[0][1]) - std::tanh(stage[0][2]));
            stage[0][3] += wc * (std::tanh(stage[0][2]) - std::tanh(stage[0][3]));

            double filtered = stage[0][3];
            double out = inputSample * (1.0 - (double)mix) + filtered * (double)mix;
            out *= (double)outputGain;

            channelData[n] = (float)out;
        }
    }
    else
    {
        // Stereo (dual-mono) processing
        auto* leftData  = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);

        for (int n = 0; n < numSamples; ++n)
        {
            // LFO (shared phase)
            lfoPhase += phaseInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            double lfoOut;
            if (!shapeTriangle)
                lfoOut = std::sin(twoPi * lfoPhase);
            else
                lfoOut = 1.0 - 4.0 * std::abs(lfoPhase - 0.5);

            if (unipolar)
                lfoOut = (lfoOut + 1.0) * 0.5;

            // Cutoff modulation
            double modAmount = (double)lfoDepth * lfoOut;
            double modFreq   = (double)cutoff * std::pow(2.0, modAmount * 4.0);
            if (modFreq < 20.0)    modFreq = 20.0;
            if (modFreq > 20000.0) modFreq = 20000.0;
            double wc = twoPi * modFreq / sr;
            if (wc > juce::MathConstants<double>::pi) wc = juce::MathConstants<double>::pi;

            double driveGain = 1.0 + (double)drive * 3.0;

            // Left channel
            {
                double inputSample = (double)leftData[n];
                double fb = (double)resonance * 4.0 * stage[0][3];
                double x  = std::tanh(inputSample * driveGain - fb);

                stage[0][0] += wc * (std::tanh(x)           - std::tanh(stage[0][0]));
                stage[0][1] += wc * (std::tanh(stage[0][0]) - std::tanh(stage[0][1]));
                stage[0][2] += wc * (std::tanh(stage[0][1]) - std::tanh(stage[0][2]));
                stage[0][3] += wc * (std::tanh(stage[0][2]) - std::tanh(stage[0][3]));

                double filtered = stage[0][3];
                double out = inputSample * (1.0 - (double)mix) + filtered * (double)mix;
                out *= (double)outputGain;
                leftData[n] = (float)out;
            }

            // Right channel
            {
                double inputSample = (double)rightData[n];
                double fb = (double)resonance * 4.0 * stage[1][3];
                double x  = std::tanh(inputSample * driveGain - fb);

                stage[1][0] += wc * (std::tanh(x)           - std::tanh(stage[1][0]));
                stage[1][1] += wc * (std::tanh(stage[1][0]) - std::tanh(stage[1][1]));
                stage[1][2] += wc * (std::tanh(stage[1][1]) - std::tanh(stage[1][2]));
                stage[1][3] += wc * (std::tanh(stage[1][2]) - std::tanh(stage[1][3]));

                double filtered = stage[1][3];
                double out = inputSample * (1.0 - (double)mix) + filtered * (double)mix;
                out *= (double)outputGain;
                rightData[n] = (float)out;
            }
        }
    }
}

juce::AudioProcessorEditor* MoogGroovFilterAudioProcessor::createEditor()
{
    return new MoogGroovFilterAudioProcessorEditor(*this);
}

void MoogGroovFilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(cutoffParam->get());
    stream.writeFloat(resonanceParam->get());
    stream.writeFloat(lfoRateParam->get());
    stream.writeFloat(lfoSyncParam->get());
    stream.writeFloat(syncModeParam->get());
    stream.writeFloat(lfoShapeParam->get());
    stream.writeFloat(lfoDepthParam->get());
    stream.writeFloat(lfoPolarityParam->get());
    stream.writeFloat(driveParam->get());
    stream.writeFloat(mixParam->get());
    stream.writeFloat(outputParam->get());
}

void MoogGroovFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    if (stream.getNumBytesRemaining() < 11 * 4) return;
    *cutoffParam      = stream.readFloat();
    *resonanceParam   = stream.readFloat();
    *lfoRateParam     = stream.readFloat();
    *lfoSyncParam     = stream.readFloat();
    *syncModeParam    = stream.readFloat();
    *lfoShapeParam    = stream.readFloat();
    *lfoDepthParam    = stream.readFloat();
    *lfoPolarityParam = stream.readFloat();
    *driveParam       = stream.readFloat();
    *mixParam         = stream.readFloat();
    *outputParam      = stream.readFloat();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoogGroovFilterAudioProcessor();
}
