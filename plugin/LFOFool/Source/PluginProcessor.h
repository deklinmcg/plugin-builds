#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <atomic>

//==============================================================================
/**
 * LFOFool - Multi-LFO Modulation Plugin
 *
 * 4 independent LFOs with per-LFO grit (wavefolder distortion) and a global
 * Chaos control. Each LFO routes independently to Volume, Pan, Filter,
 * Reverb, or Delay.
 */
class LFOFoolAudioProcessor : public juce::AudioProcessor
{
public:
    // Modulation targets (must match parameter choice list order)
    enum class Target { Volume = 0, Pan, Filter, Reverb, Delay };

    // BPM-sync beat multiples (beats per LFO cycle)
    static constexpr int   kNumSyncRates = 6;
    static constexpr float kSyncBeatsPerCycle[kNumSyncRates] = {
        0.25f,  // 1/16 note
        0.5f,   // 1/8  note
        1.0f,   // 1/4  note (1 beat)
        2.0f,   // 1/2  note
        4.0f,   // 1/1  (1 bar)
        8.0f    // 2/1  (2 bars)
    };

    //==============================================================================
    LFOFoolAudioProcessor();
    ~LFOFoolAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // LFO output data for UI visualisation (written each block)
    std::atomic<float> lfoPhaseOut[4] = {};
    std::atomic<float> lfoValueOut[4] = {};

private:
    //==============================================================================
    // LFO oscillator state
    struct LFOState
    {
        double phase     = 0.0;
        float  holdValue = 0.0f;  // S&H current held value
        float  nextHold  = 0.0f;  // S&H value queued for next cycle
        juce::Random rng;

        void advance (double inc)
        {
            phase += inc;
            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                holdValue = nextHold;
                nextHold  = rng.nextFloat() * 2.0f - 1.0f;
            }
        }

        // Returns value in [-1, +1] with optional grit wavefolder
        float evaluate (float phaseOffsetDeg, int shape, float grit) const
        {
            double p = std::fmod (phase + phaseOffsetDeg / 360.0, 1.0);
            if (p < 0.0) p += 1.0;
            return applyGrit (shapeValue (p, shape), grit);
        }

    private:
        static float shapeValue (double p, int shape)
        {
            switch (shape)
            {
                case 0:  return (float) std::sin (p * juce::MathConstants<double>::twoPi);
                case 1:  return p < 0.5 ? (float) (p * 4.0 - 1.0) : (float) (3.0 - p * 4.0);
                case 2:  return p < 0.5 ? 1.0f : -1.0f;
                case 3:  return (float) (p * 2.0 - 1.0);
                case 4:  return (float) (1.0 - p * 2.0);
                default: return 0.0f;  // placeholder for Random / Custom (handled via holdValue)
            }
        }

        float shapeValue (double p, int shape) const
        {
            if (shape == 5) return holdValue;  // S&H Random
            return shapeValue (p, shape);
        }

        static float applyGrit (float v, float grit)
        {
            if (grit < 0.001f) return v;
            float f = v * (1.0f + grit * 7.0f);  // drive 1x–8x
            for (int i = 0; i < 8; ++i)
            {
                if      (f >  1.0f) f = 2.0f - f;
                else if (f < -1.0f) f = -2.0f - f;
                else break;
            }
            return f;
        }
    };

    std::array<LFOState, 4> lfos;

    // Chaos: smoothed noise applied as phase jitter + rate drift
    float        chaosNoise = 0.0f;
    juce::Random chaosRng;

    // Global DSP
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Reverb                        reverb;

    // Delay (simple stereo circular buffer)
    static constexpr int kMaxDelaySamples = 192001;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos    = 0;
    int delayLenSamples  = 0;

    // Per-block smoothing for volume and pan
    juce::SmoothedValue<float> gainSmooth { 1.0f };
    juce::SmoothedValue<float> panSmooth  { 0.0f };

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOFoolAudioProcessor)
};
