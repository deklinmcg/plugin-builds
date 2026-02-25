#include "PluginProcessor.h"
#include "PluginEditor.h"

TapeBloomAudioProcessor::TapeBloomAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      currentSampleRate(44100.0)
{
    addParameter(driveParam  = new juce::AudioParameterFloat("drive",  "Drive",  0.0f, 1.0f, 0.25f));
    addParameter(warmthParam = new juce::AudioParameterFloat("warmth", "Warmth", 0.0f, 1.0f, 0.5f));
    addParameter(fluxParam   = new juce::AudioParameterFloat("flux",   "Flux",   0.0f, 1.0f, 0.5f));
    addParameter(bloomParam  = new juce::AudioParameterFloat("bloom",  "Bloom",  0.0f, 1.0f, 0.4f));
    addParameter(outputParam = new juce::AudioParameterFloat("output", "Output", -12.0f, 6.0f, 0.0f));

    for (int i = 0; i < 2; ++i)
    {
        hp_prev[i]       = 0.0;
        x_prev[i]        = 0.0;
        bloom_lp_prev[i] = 0.0;
    }
}

TapeBloomAudioProcessor::~TapeBloomAudioProcessor()
{
}

const juce::String TapeBloomAudioProcessor::getName() const
{
    return "TapeBloom";
}

bool TapeBloomAudioProcessor::acceptsMidi() const  { return false; }
bool TapeBloomAudioProcessor::producesMidi() const { return false; }
bool TapeBloomAudioProcessor::isMidiEffect() const { return false; }

double TapeBloomAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TapeBloomAudioProcessor::getNumPrograms()                                    { return 1; }
int TapeBloomAudioProcessor::getCurrentProgram()                                 { return 0; }
void TapeBloomAudioProcessor::setCurrentProgram(int)                             {}
const juce::String TapeBloomAudioProcessor::getProgramName(int)                  { return {}; }
void TapeBloomAudioProcessor::changeProgramName(int, const juce::String&)        {}

void TapeBloomAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    for (int i = 0; i < 2; ++i)
    {
        hp_prev[i]       = 0.0;
        x_prev[i]        = 0.0;
        bloom_lp_prev[i] = 0.0;
    }
}

void TapeBloomAudioProcessor::releaseResources()
{
}

bool TapeBloomAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() &&
        mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void TapeBloomAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalOut     = getTotalNumOutputChannels();
    const int numSamples   = buffer.getNumSamples();
    const double sr        = currentSampleRate;
    const double twoPi     = 2.0 * juce::MathConstants<double>::pi;

    // Read parameters
    const double drive   = driveParam->get();
    const double warmth  = warmthParam->get();
    const double flux    = fluxParam->get();
    const double bloom   = bloomParam->get();
    const double outputdB = outputParam->get();

    // Step 1: driveGain
    const double driveGain = 1.0 + drive * 5.0;

    // Step 2: highpass coefficients
    const double hpCutoff  = 20.0 + flux * 100.0;
    const double alpha_hp  = 1.0 / (1.0 + twoPi * hpCutoff / sr);

    // Step 8: bloom lowpass coefficient
    const double alpha_bloom = (twoPi * 180.0) / (twoPi * 180.0 + sr);

    // Step 9: output gain
    const double outGain = std::pow(10.0, outputdB / 20.0);

    // Normalisation denominator
    const double tanhDriveGain = std::tanh(driveGain);
    const double normFactor = (tanhDriveGain > 1e-9) ? (1.0 / tanhDriveGain) : 1.0;

    // Pre-computed constant for evenSat offset
    const double tanh03 = std::tanh(0.3);

    // Clear any channels beyond what we'll write
    for (int ch = totalOut; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    // Process channels
    const int channelsToProcess = juce::jmin(totalOut, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        double hp_p       = hp_prev[ch];
        double x_p        = x_prev[ch];
        double bloom_lp_p = bloom_lp_prev[ch];

        for (int n = 0; n < numSamples; ++n)
        {
            const double x = static_cast<double>(channelData[n]);

            // Step 2: 1-pole IIR highpass
            const double hp_out = alpha_hp * (hp_p + x - x_p);
            hp_p = hp_out;
            x_p  = x;

            // Step 3: split
            const double lowShelf = x - hp_out;
            const double satInput = hp_out;

            // Step 4: apply drive
            const double driven = satInput * driveGain;

            // Step 5: saturation with warmth blend
            const double oddSat  = std::tanh(driven);
            const double evenSat = 0.5 * (std::tanh(driven + 0.3) - tanh03);
            const double saturated = (1.0 - warmth) * oddSat + warmth * evenSat;

            // Step 6: normalise
            const double satNorm = saturated * normFactor;

            // Step 7: recombine
            const double combined = lowShelf + satNorm;

            // Step 8: bloom lowpass shelf
            bloom_lp_p = bloom_lp_p + alpha_bloom * (combined - bloom_lp_p);
            const double withBloom = combined + bloom * 0.6 * bloom_lp_p;

            // Step 9: output gain
            const double finalSample = withBloom * outGain;

            channelData[n] = static_cast<float>(finalSample);
        }

        hp_prev[ch]       = hp_p;
        x_prev[ch]        = x_p;
        bloom_lp_prev[ch] = bloom_lp_p;
    }

    // If mono input but stereo output, copy left to right
    if (totalOut >= 2 && buffer.getNumChannels() >= 2 && channelsToProcess == 1)
    {
        buffer.copyFrom(1, 0, buffer.getReadPointer(0), numSamples);
    }
}

juce::AudioProcessorEditor* TapeBloomAudioProcessor::createEditor()
{
    return new TapeBloomAudioProcessorEditor(*this);
}

void TapeBloomAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(driveParam->get());
    stream.writeFloat(warmthParam->get());
    stream.writeFloat(fluxParam->get());
    stream.writeFloat(bloomParam->get());
    stream.writeFloat(outputParam->get());
}

void TapeBloomAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 5 * sizeof(float))
    {
        *driveParam  = stream.readFloat();
        *warmthParam = stream.readFloat();
        *fluxParam   = stream.readFloat();
        *bloomParam  = stream.readFloat();
        *outputParam = stream.readFloat();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeBloomAudioProcessor();
}