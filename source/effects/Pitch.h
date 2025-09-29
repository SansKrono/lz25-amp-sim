#pragma once

#include "EffectPedal.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

//==============================================================================
/**
 * Basic pitch-shifting pedal inspired by DigiTech Whammy DT behaviour.
 *
 * This is a lightweight real-time shifter implemented using a
 * dual modulated delay with crossfading windows. It's not studio-grade,
 * but provides musical pitch shifting with low latency suitable as a
 * first pedal in the chain.
 */
class Pitch : public EffectPedal
{
public:
    Pitch();
    ~Pitch() override = default;

    //==============================================================================
    // EffectPedal interface
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::dsp::AudioBlock<float>& block) override;

    std::vector<Parameter> getParameters() const override;
    void updateParameters(juce::AudioProcessorValueTreeState& apvts) override;

private:
    struct ChannelState
    {
        std::vector<float> buffer;   // circular buffer per channel
        int writePos = 0;
        float phase = 0.0f;           // 0..1 ramp
    };

    std::vector<ChannelState> channels;

    // Parameters
    float sampleRate = 44100.0f;
    int maxBlock = 512;

    // Delay/pitch settings
    float maxDelayMs = 40.0f;        // Window/grain length (ms)
    int maxDelaySamples = 0;         // Derived from SR
    int bufferSize = 0;              // Circular buffer size per channel

    // Control state
    float currentShiftSemis = 0.0f;  // effective semitone shift (computed)
    int currentPitchSemis = 0;       // quantized pitch knob (-12..+12)
    int currentRangeIndex = 0;       // quantized range selector index
    float currentShiftValue = 0.0f;  // 0..1 continuous morph

    // Configurable dive bomb amount (in semitones, negative)
    int diveBombSemis = -48;         // can be adjusted if needed

    // Helpers
    static inline float hann(float t)
    {
        // t in [0,1)
        return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * t));
    }

    static inline int clampRoundToInt(float v, int minV, int maxV)
    {
        int r = (int) std::round(v);
        return juce::jlimit(minV, maxV, r);
    }

    float indexToTargetSemis(int rangeIndex) const;
    float readLinear(const std::vector<float>& buf, float index) const;
};
