// LFOToolV1AudioProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

const double LFOToolV1AudioProcessor::syncDivisions[10] = {
    1.0/8.0,   // 1/32  (in beats, where 1 beat = 1 quarter note)
    1.0/4.0,   // 1/16
    1.0/3.0,   // 1/8T  (triplet eighth = 1/3 of a beat)
    1.0/2.0,   // 1/8
    2.0/3.0,   // 1/4T  (triplet quarter = 2/3 of a beat)
    1.0,       // 1/4
    2.0,       // 1/2
    4.0,       // 1/1
    8.0,       // 2/1
    16.0       // 4/1
};

LFOToolV1AudioProcessor::LFOToolV1AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      currentSampleRate(44100.0),
      currentBlockSize(512),
      lfoPhase(0.0),
      smoothedLFO(0.0),
      svfA1(0.0), svfA2(0.0), svfA3(0.0), svfG(0.0), svfK(0.0),
      cachedModCutoff(-1.0),
      svfUpdateCounter(0),
      wasPlaying(false),
      lastBPM(120.0)
{
    svfIc1[0] = svfIc1[1] = 0.0;
    svfIc2[0] = svfIc2[1] = 0.0;

    addParameter(paramRate        = new juce::AudioParameterFloat("Rate",        "Rate",
        juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.3f), 1.0f));
    addParameter(paramRateSync    = new juce::AudioParameterFloat("RateSync",    "RateSync",
        juce::NormalisableRange<float>(0.0f, 10.0f, 1.0f), 3.0f));
    addParameter(paramSyncMode    = new juce::AudioParameterFloat("SyncMode",    "SyncMode",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f));
    addParameter(paramShape       = new juce::AudioParameterFloat("Shape",       "Shape",
        juce::NormalisableRange<float>(0.0f, 5.0f, 1.0f), 5.0f));
    addParameter(paramPhase       = new juce::AudioParameterFloat("Phase",       "Phase",
        0.0f, 360.0f, 0.0f));
    addParameter(paramVolumeDepth = new juce::AudioParameterFloat("VolumeDepth", "VolumeDepth",
        0.0f, 1.0f, 0.8f));
    addParameter(paramFilterDepth = new juce::AudioParameterFloat("FilterDepth", "FilterDepth",
        0.0f, 1.0f, 0.0f));
    addParameter(paramPanDepth    = new juce::AudioParameterFloat("PanDepth",    "PanDepth",
        0.0f, 1.0f, 0.0f));
    addParameter(paramFilterCutoff= new juce::AudioParameterFloat("FilterCutoff","FilterCutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f));
    addParameter(paramFilterRes   = new juce::AudioParameterFloat("FilterRes",   "FilterRes",
        0.0f, 1.0f, 0.1f));
    addParameter(paramFilterType  = new juce::AudioParameterFloat("FilterType",  "FilterType",
        juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f));
    addParameter(paramSmooth      = new juce::AudioParameterFloat("Smooth",      "Smooth",
        0.0f, 1.0f, 0.1f));
    addParameter(paramMix         = new juce::AudioParameterFloat("Mix",         "Mix",
        0.0f, 1.0f, 1.0f));
    addParameter(paramOutputGain  = new juce::AudioParameterFloat("OutputGain",  "OutputGain",
        -12.0f, 12.0f, 0.0f));
}

LFOToolV1AudioProcessor::~LFOToolV1AudioProcessor() {}

void LFOToolV1AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    lfoPhase   = 0.0;
    smoothedLFO = 0.0;

    svfIc1[0] = svfIc1[1] = 0.0;
    svfIc2[0] = svfIc2[1] = 0.0;
    svfUpdateCounter = 0;
    cachedModCutoff  = -1.0;
    wasPlaying = false;

    // Pre-compute initial SVF coefficients
    double initCutoff  = (double)paramFilterCutoff->get();
    double initRes     = (double)paramFilterRes->get();
    double mappedQ     = 0.5 + initRes * 9.5;
    computeSVFCoeffs(initCutoff, mappedQ);
}

void LFOToolV1AudioProcessor::releaseResources() {}

bool LFOToolV1AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() &&
        mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;

    return (mainIn == mainOut);
}

void LFOToolV1AudioProcessor::computeSVFCoeffs(double modCutoff, double mappedQ)
{
    double sr = currentSampleRate;
    modCutoff = juce::jlimit(20.0, 20000.0, modCutoff);
    double w  = std::tan(juce::MathConstants<double>::pi * modCutoff / sr);
    svfG  = w;
    svfK  = 1.0 / mappedQ;
    svfA1 = 1.0 / (1.0 + svfG * (svfG + svfK));
    svfA2 = svfG * svfA1;
    svfA3 = svfG * svfA2;
    cachedModCutoff = modCutoff;
}

double LFOToolV1AudioProcessor::processSVFSample(double x, int channel, int filterType)
{
    double& ic1 = svfIc1[channel];
    double& ic2 = svfIc2[channel];

    // Simper TPT SVF
    double hp = (x - svfK * ic1 - ic2) * svfA1;
    // Using the correct Simper formulation:
    // Actually the correct form:
    // v3 = x - ic2
    // v1 = a1*ic1 + a2*v3  -- but let's use the verified version below
    double v3 = x - ic2;
    double v1 = svfA1 * ic1 + svfA2 * v3;
    double v2 = ic2 + svfA2 * ic1 + svfA3 * v3;
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;

    // v1 = bandpass, v2 = lowpass
    double lp = v2;
    double bp = v1;
    hp = x - svfK * v1 - v2;

    switch (filterType)
    {
        case 0:  return lp;
        case 1:  return hp;
        case 2:  return bp;
        default: return lp;
    }
}

void LFOToolV1AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    // Clear unused output channels
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const bool isStereo = (totalNumOutputChannels >= 2);

    // Read parameters
    const float rateHz       = paramRate->get();
    const int   rateSyncIdx  = juce::jlimit(0, 9, (int)paramRateSync->get());
    const bool  syncMode     = (paramSyncMode->get() >= 0.5f);
    const int   shape        = juce::jlimit(0, 5, (int)paramShape->get());
    const float phaseOffsetDeg = paramPhase->get();
    const float volumeDepth  = paramVolumeDepth->get();
    const float filterDepth  = paramFilterDepth->get();
    const float panDepth     = paramPanDepth->get();
    const float filterCutoff = paramFilterCutoff->get();
    const float filterRes    = paramFilterRes->get();
    const int   filterType   = juce::jlimit(0, 2, (int)paramFilterType->get());
    const float smooth       = paramSmooth->get();
    const float mix          = paramMix->get();
    const float outputGainDB = paramOutputGain->get();

    const double sr          = currentSampleRate;
    const double outputGainLin = std::pow(10.0, (double)outputGainDB / 20.0);
    const double mappedQ     = 0.5 + (double)filterRes * 9.5;
    const double phaseOffsetNorm = (double)phaseOffsetDeg / 360.0;

    // Smoothing coefficient
    // smoothCoeff = exp(-1.0 / (sampleRate * Smooth * 0.05 + 0.0001))
    const double smoothCoeff = std::exp(-1.0 / (sr * (double)smooth * 0.05 + 0.0001));

    // Determine LFO frequency
    double lfoHz = (double)rateHz;
    if (syncMode)
    {
        // Get host BPM
        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
        {
            juce::AudioPlayHead::CurrentPositionInfo posInfo;
            if (playHead->getCurrentPosition(posInfo))
            {
                if (posInfo.bpm > 0.0)
                    bpm = posInfo.bpm;
            }
        }
        double divBeats = syncDivisions[rateSyncIdx];
        lfoHz = bpm / 60.0 * (1.0 / divBeats);
    }

    // Host sync / phase reset
    {
        bool isCurrentlyPlaying = false;
        bool shouldReset = false;
        if (auto* playHead = getPlayHead())
        {
            juce::AudioPlayHead::CurrentPositionInfo posInfo;
            if (playHead->getCurrentPosition(posInfo))
            {
                isCurrentlyPlaying = posInfo.isPlaying;
                // Reset on playback start
                if (posInfo.isPlaying && !wasPlaying)
                    shouldReset = true;
                // Reset on loop restart (timeInSamples near 0 or jumped back)
                if (posInfo.isLooping && posInfo.isPlaying && posInfo.timeInSamples < (juce::int64)currentBlockSize)
                    shouldReset = true;
            }
        }
        wasPlaying = isCurrentlyPlaying;
        if (shouldReset)
        {
            lfoPhase   = phaseOffsetNorm;
            smoothedLFO = 0.0;
        }
    }

    const double phaseInc = lfoHz / sr;
    const double twoPi    = 2.0 * juce::MathConstants<double>::pi;

    // Get channel pointers
    float* leftData  = buffer.getWritePointer(0);
    float* rightData = isStereo ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // --- LFO phase with phase offset applied
        double phaseWithOffset = lfoPhase + phaseOffsetNorm;
        while (phaseWithOffset >= 1.0) phaseWithOffset -= 1.0;
        while (phaseWithOffset <  0.0) phaseWithOffset += 1.0;

        // --- Waveform generation
        double raw = 0.0;
        switch (shape)
        {
            case 0: // Sine
                raw = std::sin(phaseWithOffset * twoPi);
                break;
            case 1: // Triangle
                raw = 1.0 - std::abs(phaseWithOffset * 4.0 - 2.0);
                raw = juce::jlimit(-1.0, 1.0, raw);
                break;
            case 2: // Saw Down
                raw = 1.0 - 2.0 * phaseWithOffset;
                break;
            case 3: // Saw Up
                raw = 2.0 * phaseWithOffset - 1.0;
                break;
            case 4: // Square
                raw = (phaseWithOffset < 0.5) ? 1.0 : -1.0;
                break;
            case 5: // S-Curve
            {
                double sineVal = std::sin(phaseWithOffset * twoPi);
                const double tanh3 = std::tanh(3.0);
                raw = std::tanh(3.0 * sineVal) / tanh3;
                break;
            }
            default:
                raw = std::sin(phaseWithOffset * twoPi);
                break;
        }

        // --- Smoothing
        smoothedLFO = smoothedLFO + (1.0 - smoothCoeff) * (raw - smoothedLFO);

        // --- Normalise to [0,1]
        const double lfoUnipolar = (smoothedLFO + 1.0) * 0.5;

        // --- SVF coefficient update every 16 samples
        if (svfUpdateCounter == 0)
        {
            double modCutoff = (double)filterCutoff
                * std::pow(2.0, (double)filterDepth * (lfoUnipolar - 0.5) * 4.0);
            modCutoff = juce::jlimit(20.0, 20000.0, modCutoff);
            computeSVFCoeffs(modCutoff, mappedQ);
        }
        svfUpdateCounter = (svfUpdateCounter + 1) & 15;

        // --- Volume modulation
        const double volumeGain = 1.0 - ((double)volumeDepth * (1.0 - lfoUnipolar));

        // --- Pan modulation
        const double panAmount = (lfoUnipolar - 0.5) * 2.0 * (double)panDepth;
        const double leftGain  = std::cos((panAmount + 1.0) * juce::MathConstants<double>::pi / 4.0);
        const double rightGain = std::sin((panAmount + 1.0) * juce::MathConstants<double>::pi / 4.0);

        // --- Advance LFO phase
        lfoPhase += phaseInc;
        while (lfoPhase >= 1.0) lfoPhase -= 1.0;
        while (lfoPhase <  0.0) lfoPhase += 1.0;

        if (!isStereo)
        {
            // Mono path
            const double drySample = (double)leftData[i];
            double wet = drySample;

            // Volume mod
            wet *= volumeGain;

            // Filter
            wet = processSVFSample(wet, 0, filterType);

            // Pan (no effect on mono, skip pan)
            // Mix
            double out = (double)mix * wet + (1.0 - (double)mix) * drySample;

            // Output gain
            out *= outputGainLin;

            leftData[i] = (float)out;
        }
        else
        {
            // Stereo path
            const double dryL = (double)leftData[i];
            const double dryR = (double)rightData[i];

            double wetL = dryL;
            double wetR = dryR;

            // Volume mod
            wetL *= volumeGain;
            wetR *= volumeGain;

            // Filter
            wetL = processSVFSample(wetL, 0, filterType);
            wetR = processSVFSample(wetR, 1, filterType);

            // Pan
            wetL *= leftGain;
            wetR *= rightGain;

            // Mix
            double outL = (double)mix * wetL + (1.0 - (double)mix) * dryL;
            double outR = (double)mix * wetR + (1.0 - (double)mix) * dryR;

            // Output gain
            outL *= outputGainLin;
            outR *= outputGainLin;

            leftData[i]  = (float)outL;
            rightData[i] = (float)outR;
        }
    }
}

juce::AudioProcessorEditor* LFOToolV1AudioProcessor::createEditor()
{
    return new LFOToolV1AudioProcessorEditor(*this);
}

void LFOToolV1AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(paramRate->get());
    stream.writeFloat(paramRateSync->get());
    stream.writeFloat(paramSyncMode->get());
    stream.writeFloat(paramShape->get());
    stream.writeFloat(paramPhase->get());
    stream.writeFloat(paramVolumeDepth->get());
    stream.writeFloat(paramFilterDepth->get());
    stream.writeFloat(paramPanDepth->get());
    stream.writeFloat(paramFilterCutoff->get());
    stream.writeFloat(paramFilterRes->get());
    stream.writeFloat(paramFilterType->get());
    stream.writeFloat(paramSmooth->get());
    stream.writeFloat(paramMix->get());
    stream.writeFloat(paramOutputGain->get());
}

void LFOToolV1AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    *paramRate         = stream.readFloat();
    *paramRateSync     = stream.readFloat();
    *paramSyncMode     = stream.readFloat();
    *paramShape        = stream.readFloat();
    *paramPhase        = stream.readFloat();
    *paramVolumeDepth  = stream.readFloat();
    *paramFilterDepth  = stream.readFloat();
    *paramPanDepth     = stream.readFloat();
    *paramFilterCutoff = stream.readFloat();
    *paramFilterRes    = stream.readFloat();
    *paramFilterType   = stream.readFloat();
    *paramSmooth       = stream.readFloat();
    *paramMix          = stream.readFloat();
    *paramOutputGain   = stream.readFloat();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LFOToolV1AudioProcessor();
}
