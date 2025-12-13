#pragma once

#include <functional>
#include <juce_dsp/juce_dsp.h>

/**
 * Shared building blocks for amplifier models.
 * These are intentionally lightweight wrappers around JUCE DSP primitives
 * to standardize naming and parameter updates across amps.
 */
class ToneStack
{
public:
    void prepare (const juce::dsp::ProcessSpec& s);
    void process (juce::dsp::AudioBlock<float>& block); // Post-distortion tone/EQ stage
    void reset();

    // Setters for common controls
    void setTreble (float gain); // 0.0 - 2.0 (linear gain)
    void setMid (float gain); // 0.0 - 2.0 (linear gain)
    void setBass (float gain); // 0.0 - 2.0 (linear gain)
    void setPresence (float gain); // 0.5 - 1.5 (multiplier applied to a base boost)
    void setResonance (float cutoff); // Hz (used by ladderFilter)

    // Configure filter types/frequencies
    void setTrebleFilter (float centerFreq, float q);
    void setMidFilter (float centerFreq, float q);
    void setBassFilter (float centerFreq, float q);
    void setPresenceFilter (float shelfFreq, float q);

    // Pre-distortion helpers (bottom end + resonance before clipper)
    void processPre (juce::dsp::AudioBlock<float>& block);

private:
    using IIRMono = juce::dsp::IIR::Filter<float>;
    using IIRCoeffs = juce::dsp::IIR::Coefficients<float>;
    using IIRDup = juce::dsp::ProcessorDuplicator<IIRMono, IIRCoeffs>;

    IIRDup trebleFilter;
    IIRDup midFilter;
    IIRDup bassFilter;
    IIRDup presenceFilter;
    IIRDup bottomEndFilter;
    juce::dsp::LadderFilter<float> resonanceFilter;

    // Cached params
    float trebleCF = 5000.0f, trebleQ = 0.6f, trebleGain = 1.0f;
    float midCF = 500.0f, midQ = 0.9f, midGain = 1.0f;
    float bassCF = 100.0f, bassQ = 0.6f, bassGain = 1.0f;
    float presenceSF = 4500.0f, presenceQ = 0.5f, presenceBoostDB = 6.0f, presenceMult = 1.0f;
    float bottomEndHz = 400.0f, bottomEndQ = 0.4f, bottomEndGain = 1.0f;

    juce::dsp::ProcessSpec spec { 44100.0, 512, };
};

class PreDistortionFilter
{
public:
    void prepare (const juce::dsp::ProcessSpec& s);
    void process (juce::dsp::AudioBlock<float>& block);
    void reset();
    void setHighPassCutoff (float hz);

private:
    using IIRMono = juce::dsp::IIR::Filter<float>;
    using IIRCoeffs = juce::dsp::IIR::Coefficients<float>;
    using IIRDup = juce::dsp::ProcessorDuplicator<IIRMono, IIRCoeffs>;

    IIRDup hpf;
    float cutoffHz = 700.0f;
    juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
};

class Waveshaper
{
public:
    using WaveshaperFunction = float (*) (float);

    void prepare (const juce::dsp::ProcessSpec& s);
    void process (juce::dsp::AudioBlock<float>& block);
    void reset();
    void setFunction (WaveshaperFunction func);
    void setPreGain (float gainDb);

private:
    juce::dsp::WaveShaper<float> shaper;
    juce::dsp::Gain<float> preGain;
    juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
};
