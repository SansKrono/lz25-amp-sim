#pragma once

#include "EffectPedal.h"
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
 * SmartGate effect pedal
 *
 * Adaptive noise gate that adjusts its closing speed based on how quickly
 * the input signal is decaying. Fast mutes close quickly; slow ring-outs
 * close gently to preserve sustain.
 */
class SmartGate : public EffectPedal
{
public:
    SmartGate();
    ~SmartGate() override = default;

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
        float envelope = 0.0f;
        float prevEnvelope = 0.0f;
        float gain = 1.0f;
    };

    std::vector<ChannelState> channelStates;

    //==============================================================================
    // Parameters (current/effective)
    float sampleRate = 44100.0f;

    float currentIntensity = 0.5f;   // 0..1
    float currentReductionDb = 24.0f; // 0..60 dB
    float currentRelease = 0.5f;     // 0..1 (how fast to open)
    bool currentDjent = false;       // aggressive mode

    // Envelope follower time constants
    float envAttackTime = 0.003f;    // 3 ms
    float envReleaseTime = 0.080f;   // 80 ms

    // Derived
    float minCloseTime = 0.005f;     // seconds
    float maxCloseTime = 0.400f;     // seconds

    // Helpers
    float calcAlphaFromTau(float tauSeconds) const { return std::exp(-1.0f / (juce::jmax(tauSeconds, 1.0e-5f) * sampleRate)); }
};
