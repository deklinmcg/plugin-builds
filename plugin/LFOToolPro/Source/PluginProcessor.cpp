#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LFOToolProAudioProcessor::LFOToolProAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input",   juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // LFO1
    addParameter(lfo1Shape       = new juce::AudioParameterFloat("LFO1_Shape",       "LFO1 Shape",       0.f, 3.f,   0.f));
    addParameter(lfo1Rate        = new juce::AudioParameterFloat("LFO1_Rate",         "LFO1 Rate",        0.0625f, 32.f, 1.f));
    addParameter(lfo1Sync        = new juce::AudioParameterFloat("LFO1_Sync",         "LFO1 Sync",        0.f, 1.f,   1.f));
    addParameter(lfo1Depth       = new juce::AudioParameterFloat("LFO1_Depth",        "LFO1 Depth",       0.f, 1.f,   0.5f));
    addParameter(lfo1Phase       = new juce::AudioParameterFloat("LFO1_Phase",        "LFO1 Phase",       0.f, 360.f, 0.f));
    addParameter(lfo1GlobalDepth = new juce::AudioParameterFloat("LFO1_GlobalDepth",  "LFO1 Global Depth",0.f, 1.f,   1.f));

    // LFO2
    addParameter(lfo2Shape       = new juce::AudioParameterFloat("LFO2_Shape",       "LFO2 Shape",       0.f, 3.f,   0.f));
    addParameter(lfo2Rate        = new juce::AudioParameterFloat("LFO2_Rate",         "LFO2 Rate",        0.0625f, 32.f, 1.f));
    addParameter(lfo2Sync        = new juce::AudioParameterFloat("LFO2_Sync",         "LFO2 Sync",        0.f, 1.f,   1.f));
    addParameter(lfo2Depth       = new juce::AudioParameterFloat("LFO2_Depth",        "LFO2 Depth",       0.f, 1.f,   0.5f));
    addParameter(lfo2Phase       = new juce::AudioParameterFloat("LFO2_Phase",        "LFO2 Phase",       0.f, 360.f, 0.f));
    addParameter(lfo2GlobalDepth = new juce::AudioParameterFloat("LFO2_GlobalDepth",  "LFO2 Global Depth",0.f, 1.f,   1.f));
    addParameter(filterBase      = new juce::AudioParameterFloat("FilterBase",        "Filter Base",      200.f, 18000.f, 8000.f));

    // LFO3
    addParameter(lfo3Shape       = new juce::AudioParameterFloat("LFO3_Shape",       "LFO3 Shape",       0.f, 3.f,   0.f));
    addParameter(lfo3Rate        = new juce::AudioParameterFloat("LFO3_Rate",         "LFO3 Rate",        0.0625f, 32.f, 1.f));
    addParameter(lfo3Sync        = new juce::AudioParameterFloat("LFO3_Sync",         "LFO3 Sync",        0.f, 1.f,   1.f));
    addParameter(lfo3Depth       = new juce::AudioParameterFloat("LFO3_Depth",        "LFO3 Depth",       0.f, 1.f,   0.5f));
    addParameter(lfo3Phase       = new juce::AudioParameterFloat("LFO3_Phase",        "LFO3 Phase",       0.f, 360.f, 0.f));
    addParameter(lfo3GlobalDepth = new juce::AudioParameterFloat("LFO3_GlobalDepth",  "LFO3 Global Depth",0.f, 1.f,   1.f));

    // LFO4
    addParameter(lfo4Shape       = new juce::AudioParameterFloat("LFO4_Shape",       "LFO4 Shape",       0.f, 3.f,   0.f));
    addParameter(lfo4Rate        = new juce::AudioParameterFloat("LFO4_Rate",         "LFO4 Rate",        0.0625f, 32.f, 1.f));
    addParameter(lfo4Sync        = new juce::AudioParameterFloat("LFO4_Sync",         "LFO4 Sync",        0.f, 1.f,   1.f));
    addParameter(lfo4Depth       = new juce::AudioParameterFloat("LFO4_Depth",        "LFO4 Depth",       0.f, 1.f,   0.5f));
    addParameter(lfo4Phase       = new juce::AudioParameterFloat("LFO4_Phase",        "LFO4 Phase",       0.f, 360.f, 0.f));
    addParameter(lfo4GlobalDepth = new juce::AudioParameterFloat("LFO4_GlobalDepth",  "LFO4 Global Depth",0.f, 1.f,   1.f));
    addParameter(reverbBaseSize  = new juce::AudioParameterFloat("ReverbBaseSize",    "Reverb Base Size", 0.1f, 1.f,   0.5f));
    addParameter(reverbDamping   = new juce::AudioParameterFloat("ReverbDamping",     "Reverb Damping",   0.f, 1.f,   0.5f));

    // Output
    addParameter(outputTrim = new juce::AudioParameterFloat("OutputTrim", "Output Trim", -12.f, 12.f, 0.f));

    // Init biquad state
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 4; ++s)
            biquadState[ch][s] = 0.0;

    bqA0 = 1.0; bqA1 = 0.0; bqA2 = 0.0; bqB1 = 0.0; bqB2 = 0.0;

    // Init comb/allpass write positions and damp states
    for (int f = 0; f < 4; ++f)
        for (int c = 0; c < 2; ++c)
        {
            combWritePos[f][c] = 0;
            combDampState[f][c] = 0.f;
            combDelayLength[f] = 0;
        }
    for (int f = 0; f < 2; ++f)
        for (int c = 0; c < 2; ++c)
        {
            allpassWritePos[f][c] = 0;
            allpassDelayLength[f] = 0;
        }
}

LFOToolProAudioProcessor::~LFOToolProAudioProcessor() {}

void LFOToolProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Reset LFO phases
    for (int i = 0; i < 4; ++i)
        lfoPhase[i] = 0.f;

    // Reset biquad state
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 4; ++s)
            biquadState[ch][s] = 0.0;

    // Comb filter base delays at 44100 Hz
    const int combDelaysBase[4] = {1557, 1617, 1491, 1422};
    const double sizeScale = 0.5 + 1.0 * 0.5; // max scale for buffer allocation

    for (int f = 0; f < 4; ++f)
    {
        // Scale delay to current sample rate and apply max size scale
        int maxLen = (int)std::ceil((double)combDelaysBase[f] * (sampleRate / 44100.0) * sizeScale) + 4;
        combDelayLength[f] = maxLen;

        for (int c = 0; c < 2; ++c)
        {
            combBuffer[f][c].assign(maxLen, 0.f);
            combWritePos[f][c] = 0;
            combDampState[f][c] = 0.f;
        }
    }

    // Allpass filter base delays at 44100 Hz
    const int allpassDelaysBase[2] = {225, 341};

    for (int f = 0; f < 2; ++f)
    {
        int maxLen = (int)std::ceil((double)allpassDelaysBase[f] * (sampleRate / 44100.0) * sizeScale) + 4;
        allpassDelayLength[f] = maxLen;

        for (int c = 0; c < 2; ++c)
        {
            allpassBuffer[f][c].assign(maxLen, 0.f);
            allpassWritePos[f][c] = 0;
        }
    }

    // Prepare stereo work buffer
    stereoBuffer.setSize(2, samplesPerBlock, false, true, false);

    // Initial biquad coefficients
    updateBiquadCoefficients(8000.0);
}

void LFOToolProAudioProcessor::releaseResources() {}

bool LFOToolProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo() &&
        mainOut != juce::AudioChannelSet::mono())
        return false;

    if (mainIn != juce::AudioChannelSet::stereo() &&
        mainIn != juce::AudioChannelSet::mono())
        return false;

    return true;
}

float LFOToolProAudioProcessor::computeLFO(float phase, int shape)
{
    // phase in [0, 1], returns value in [-1, 1]
    float raw = 0.f;
    switch (shape)
    {
        case 0: // Sine
            raw = std::sin(phase * 2.f * (float)M_PI);
            break;
        case 1: // Square
            raw = (phase < 0.5f) ? 1.f : -1.f;
            break;
        case 2: // Saw
            raw = 2.f * phase - 1.f;
            break;
        case 3: // Triangle
            raw = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
            break;
        default:
            raw = 0.f;
            break;
    }
    return raw;
}

void LFOToolProAudioProcessor::updateBiquadCoefficients(double cutoffHz)
{
    // 2-pole Butterworth lowpass via bilinear transform
    cutoffHz = juce::jlimit(80.0, 20000.0, cutoffHz);
    double omega = 2.0 * M_PI * cutoffHz / currentSampleRate;
    double cosw  = std::cos(omega);
    double sinw  = std::sin(omega);
    // Butterworth Q = 1/sqrt(2)
    double alpha = sinw / (2.0 * (1.0 / std::sqrt(2.0)));

    double b0 = (1.0 - cosw) / 2.0;
    double b1 = (1.0 - cosw);
    double b2 = (1.0 - cosw) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosw;
    double a2 = 1.0 - alpha;

    bqA0 = b0 / a0;
    bqA1 = b1 / a0;
    bqA2 = b2 / a0;
    bqB1 = a1 / a0;
    bqB2 = a2 / a0;
}

float LFOToolProAudioProcessor::processBiquad(float input, int channel)
{
    double x0 = (double)input;
    double& x1 = biquadState[channel][0];
    double& x2 = biquadState[channel][1];
    double& y1 = biquadState[channel][2];
    double& y2 = biquadState[channel][3];

    double y0 = bqA0 * x0 + bqA1 * x1 + bqA2 * x2 - bqB1 * y1 - bqB2 * y2;

    x2 = x1; x1 = x0;
    y2 = y1; y1 = y0;

    return (float)y0;
}

float LFOToolProAudioProcessor::processComb(float input, int filterIdx, int channel, float feedback, float dampCoeff)
{
    auto& buf = combBuffer[filterIdx][channel];
    int&  wp  = combWritePos[filterIdx][channel];
    int   len = combDelayLength[filterIdx];
    float& ds = combDampState[filterIdx][channel];

    // Read from buffer (delayed output)
    int readPos = (wp - len + 1 + (int)buf.size()) % (int)buf.size();
    // Clamp readPos
    if (readPos < 0) readPos += (int)buf.size();
    readPos = readPos % (int)buf.size();

    float delayed = buf[readPos];

    // 1-pole lowpass damping inside comb
    ds = (1.f - dampCoeff) * delayed + dampCoeff * ds;

    // Write new value: input + feedback * dampened output
    buf[wp] = input + feedback * ds;

    // Advance write pointer
    wp = (wp + 1) % (int)buf.size();

    return delayed;
}

float LFOToolProAudioProcessor::processAllpass(float input, int filterIdx, int channel, float gain)
{
    auto& buf = allpassBuffer[filterIdx][channel];
    int&  wp  = allpassWritePos[filterIdx][channel];
    int   len = allpassDelayLength[filterIdx];

    int readPos = (wp - len + 1 + (int)buf.size()) % (int)buf.size();
    if (readPos < 0) readPos += (int)buf.size();
    readPos = readPos % (int)buf.size();

    float delayed = buf[readPos];

    float output = -gain * input + delayed + gain * (input - gain * delayed);
    // Standard allpass: output = delayed + gain*(input - gain*delayed) — no, use:
    // v = input - gain*delayed
    // output = delayed + gain*v = delayed + gain*input - gain^2*delayed
    // but standard Schroeder allpass:
    // output = -gain*input + delayed + gain*(input + gain*delayed)
    // Simpler: output = delayed - gain*(input - delayed*gain)
    // Use canonical form:
    float v = input - gain * delayed;
    output = delayed + gain * v;
    buf[wp] = v;

    wp = (wp + 1) % (int)buf.size();

    return output;
}

void LFOToolProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Silence unused output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Copy input to stereo work buffer
    stereoBuffer.setSize(2, numSamples, false, false, true);

    if (totalIn == 1)
    {
        // Mono input: duplicate to both channels
        stereoBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
        stereoBuffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
    else
    {
        stereoBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
        stereoBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);
    }

    float* leftData  = stereoBuffer.getWritePointer(0);
    float* rightData = stereoBuffer.getWritePointer(1);

    // ---- Read parameters ----
    // LFO1
    int   shape1     = (int)std::round(lfo1Shape->get());
    float rate1      = lfo1Rate->get();
    bool  sync1      = (lfo1Sync->get() >= 0.5f);
    float depth1     = lfo1Depth->get();
    float phase1Off  = lfo1Phase->get();
    float gDepth1    = lfo1GlobalDepth->get();

    // LFO2
    int   shape2     = (int)std::round(lfo2Shape->get());
    float rate2      = lfo2Rate->get();
    bool  sync2      = (lfo2Sync->get() >= 0.5f);
    float depth2     = lfo2Depth->get();
    float phase2Off  = lfo2Phase->get();
    float gDepth2    = lfo2GlobalDepth->get();
    float filterBaseHz = filterBase->get();

    // LFO3
    int   shape3     = (int)std::round(lfo3Shape->get());
    float rate3      = lfo3Rate->get();
    bool  sync3      = (lfo3Sync->get() >= 0.5f);
    float depth3     = lfo3Depth->get();
    float phase3Off  = lfo3Phase->get();
    float gDepth3    = lfo3GlobalDepth->get();

    // LFO4
    int   shape4     = (int)std::round(lfo4Shape->get());
    float rate4      = lfo4Rate->get();
    bool  sync4      = (lfo4Sync->get() >= 0.5f);
    float depth4     = lfo4Depth->get();
    float phase4Off  = lfo4Phase->get();
    float gDepth4    = lfo4GlobalDepth->get();
    float revSize    = reverbBaseSize->get();
    float revDamping = reverbDamping->get();

    float outTrim    = outputTrim->get();

    // Get host BPM
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo))
            bpm = posInfo.bpm > 0.0 ? posInfo.bpm : 120.0;
    }

    // Compute phase increments per sample
    // For sync mode: phaseInc = (bpm/60) * (1/rateDivision) / sampleRate
    // rateDivision IS the rate parameter value (in bars, e.g. 0.0625 = 1/16 bar)
    auto computePhaseInc = [&](float rate, bool sync) -> float
    {
        if (sync)
        {
            // rate is in bars (0.0625 to 32.0)
            double beatsPerBar = 4.0; // assuming 4/4
            double barsPerSec  = bpm / (60.0 * beatsPerBar);
            double cyclesPerSec = barsPerSec / (double)rate;
            return (float)(cyclesPerSec / currentSampleRate);
        }
        else
        {
            // rate is in Hz (0.1 to 20)
            return (float)(rate / currentSampleRate);
        }
    };

    float phaseInc1 = computePhaseInc(rate1, sync1);
    float phaseInc2 = computePhaseInc(rate2, sync2);
    float phaseInc3 = computePhaseInc(rate3, sync3);
    float phaseInc4 = computePhaseInc(rate4, sync4);

    // Compute average LFO values for the block (use mid-block phase for filter coefficient)
    // For simplicity, compute LFO at start of block for filter coefficient update
    // then process sample-by-sample for volume, pan and reverb mix

    // We'll compute LFO values per-sample for modulations that need it
    // but update biquad coefficients per-block (spec says per block)

    // Compute filter modulation at start of block for biquad coefficients
    {
        float effPhase2 = lfoPhase[1] + phase2Off / 360.f;
        while (effPhase2 >= 1.f) effPhase2 -= 1.f;
        while (effPhase2 < 0.f)  effPhase2 += 1.f;

        float rawLFO2 = computeLFO(effPhase2, shape2);
        float lfoOut2 = (rawLFO2 + 1.f) * 0.5f;
        float modVal2 = lfoOut2 * depth2 * gDepth2;

        double cutoffHz = (double)filterBaseHz * std::pow(2.0, (double)modVal2 * 4.0 - 2.0);
        cutoffHz = juce::jlimit(80.0, 20000.0, cutoffHz);
        updateBiquadCoefficients(cutoffHz);
    }

    // Compute reverb parameters
    const int combDelaysBase[4] = {1557, 1617, 1491, 1422};
    const int allpassDelaysBase[2] = {225, 341};

    double sizeScaleRev = 0.5 + (double)revSize * 0.5;
    float combFeedback  = 0.84f;
    float dampCoeff     = revDamping * 0.4f;

    // Compute actual (current) delay lengths for combs/allpass based on revSize
    int curCombLen[4], curAllpassLen[2];
    for (int f = 0; f < 4; ++f)
    {
        curCombLen[f] = juce::jlimit(1,
            (int)combBuffer[f][0].size() - 1,
            (int)std::round((double)combDelaysBase[f] * (currentSampleRate / 44100.0) * sizeScaleRev));
    }
    for (int f = 0; f < 2; ++f)
    {
        curAllpassLen[f] = juce::jlimit(1,
            (int)allpassBuffer[f][0].size() - 1,
            (int)std::round((double)allpassDelaysBase[f] * (currentSampleRate / 44100.0) * sizeScaleRev));
    }

    // Update delay lengths used in processing
    for (int f = 0; f < 4; ++f)
        combDelayLength[f] = curCombLen[f];
    for (int f = 0; f < 2; ++f)
        allpassDelayLength[f] = curAllpassLen[f];

    // ---- Per-sample processing ----
    for (int n = 0; n < numSamples; ++n)
    {
        // --- Compute LFO phases and outputs ---

        // LFO1: Volume
        float eff1 = lfoPhase[0] + phase1Off / 360.f;
        while (eff1 >= 1.f) eff1 -= 1.f;
        while (eff1 < 0.f)  eff1 += 1.f;
        float raw1  = computeLFO(eff1, shape1);
        float lfoOut1 = (raw1 + 1.f) * 0.5f;
        float modVal1 = lfoOut1 * depth1 * gDepth1;

        // LFO2: Filter (phase advances but we already updated coefficients per block)
        // (phase still needs to advance per sample)

        // LFO3: Pan
        float eff3 = lfoPhase[2] + phase3Off / 360.f;
        while (eff3 >= 1.f) eff3 -= 1.f;
        while (eff3 < 0.f)  eff3 += 1.f;
        float raw3  = computeLFO(eff3, shape3);
        float lfoOut3 = (raw3 + 1.f) * 0.5f;
        float modVal3 = lfoOut3 * depth3 * gDepth3;

        // LFO4: Reverb mix
        float eff4 = lfoPhase[3] + phase4Off / 360.f;
        while (eff4 >= 1.f) eff4 -= 1.f;
        while (eff4 < 0.f)  eff4 += 1.f;
        float raw4  = computeLFO(eff4, shape4);
        float lfoOut4 = (raw4 + 1.f) * 0.5f;
        float modVal4 = lfoOut4 * depth4 * gDepth4;

        // Advance LFO phases
        lfoPhase[0] += phaseInc1;
        if (lfoPhase[0] >= 1.f) lfoPhase[0] -= 1.f;
        lfoPhase[1] += phaseInc2;
        if (lfoPhase[1] >= 1.f) lfoPhase[1] -= 1.f;
        lfoPhase[2] += phaseInc3;
        if (lfoPhase[2] >= 1.f) lfoPhase[2] -= 1.f;
        lfoPhase[3] += phaseInc4;
        if (lfoPhase[3] >= 1.f) lfoPhase[3] -= 1.f;

        float L = leftData[n];
        float R = rightData[n];

        // --- Step 1: Volume modulation ---
        float volGain = 1.f - modVal1;
        L *= volGain;
        R *= volGain;

        // --- Step 2: Filter modulation (biquad already configured per block) ---
        L = processBiquad(L, 0);
        R = processBiquad(R, 1);

        // --- Step 3: Pan modulation ---
        // panValue in [-1, 1]
        float panValue = (modVal3 * 2.f - 1.f) * depth3 * gDepth3;
        panValue = juce::jlimit(-1.f, 1.f, panValue);
        float panAngle = (panValue + 1.f) * (float)M_PI / 4.f;
        float leftGain  = std::cos(panAngle);
        float rightGain = std::sin(panAngle);
        float monoMix = (L + R) * 0.5f;
        L = monoMix * leftGain;
        R = monoMix * rightGain;

        // --- Step 4: Plate reverb ---
        float reverbMixAmount = modVal4;

        // Process reverb for left and right channels
        float wetL = 0.f, wetR = 0.f;

        // 4 parallel comb filters per channel
        for (int f = 0; f < 4; ++f)
        {
            wetL += processComb(L, f, 0, combFeedback, dampCoeff);
            wetR += processComb(R, f, 1, combFeedback, dampCoeff);
        }
        wetL *= 0.25f;
        wetR *= 0.25f;

        // 2 series allpass filters per channel
        for (int f = 0; f < 2; ++f)
        {
            wetL = processAllpass(wetL, f, 0, 0.5f);
            wetR = processAllpass(wetR, f, 1, 0.5f);
        }

        L = L * (1.f - reverbMixAmount) + wetL * reverbMixAmount;
        R = R * (1.f - reverbMixAmount) + wetR * reverbMixAmount;

        // --- Step 5: Output trim ---
        float finalGain = std::pow(10.f, outTrim / 20.f);
        L *= finalGain;
        R *= finalGain;

        leftData[n]  = L;
        rightData[n] = R;
    }

    // Copy stereo buffer back to output
    if (totalOut == 1)
    {
        // Mix down to mono
        buffer.clear();
        for (int n = 0; n < numSamples; ++n)
            buffer.setSample(0, n, (leftData[n] + rightData[n]) * 0.5f);
    }
    else
    {
        buffer.copyFrom(0, 0, stereoBuffer, 0, 0, numSamples);
        buffer.copyFrom(1, 0, stereoBuffer, 1, 0, numSamples);
    }
}

juce::AudioProcessorEditor* LFOToolProAudioProcessor::createEditor()
{
    return new LFOToolProAudioProcessorEditor(*this);
}

void LFOToolProAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(param))
            stream.writeFloat(p->get());
}

void LFOToolProAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(param))
        {
            if (stream.isExhausted()) break;
            *p = stream.readFloat();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LFOToolProAudioProcessor();
}