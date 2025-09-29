#pragma once

#include "EffectPedal.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include "../../modules/TS-808-Ultra/Source/dsp/ClippingStage.h"

//==============================================================================
/**
 * TS808 Tube Screamer effect pedal implementation
 * Based on the classic Ibanez TS808 circuit
 */
class TubeScreamer808 : public EffectPedal
{
public:
    TubeScreamer808();
    ~TubeScreamer808() override = default;

    //==============================================================================
    // EffectPedal interface
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    
    std::vector<Parameter> getParameters() const override;
    void updateParameters(juce::AudioProcessorValueTreeState& apvts) override;

private:
    //==============================================================================
    // Circuit modeling components
    struct CircuitState
    {
        // Input buffer stage
        float inputHPF_z1 = 0.0f;

        // Clipping stage
        float preClipLPF_z1 = 0.0f;
        float postClipHPF_z1 = 0.0f;
        float clipperFeedback_z1 = 0.0f;

        // Tone stage
        float toneHPF_z1 = 0.0f;
        float toneLPF_z1 = 0.0f;

        // Output buffer
        float outputHPF_z1 = 0.0f;

        // Lightweight noise gate state
        float noiseEnv = 0.0f;   // input envelope follower
        float gateGain = 1.0f;   // smoothed gate gain (0=closed, 1=open)

        // TS-808-Ultra clipping stage (per-channel)
        std::unique_ptr<ClippingStage> clipper;
    };

    std::vector<CircuitState> channelStates;

    //==============================================================================
    // Circuit parameters
    float sampleRate = 44100.0f;
    float dt = 1.0f / 44100.0f;

    // Filter coefficients (calculated in prepare)
    float inputHPF_a1, inputHPF_b0, inputHPF_b1;
    float preClipLPF_a1, preClipLPF_b0, preClipLPF_b1;
    float postClipHPF_a1, postClipHPF_b0, postClipHPF_b1;
    float toneHPF_a1, toneHPF_b0, toneHPF_b1;
    float toneLPF_a1, toneLPF_b0, toneLPF_b1;
    float outputHPF_a1, outputHPF_b0, outputHPF_b1;

    //==============================================================================
    // Current parameter values
    float currentDrive = 0.5f;
    float currentTone = 0.5f;
    float currentLevel = 0.7f;

    //==============================================================================
    // Circuit simulation methods
    float processInputBuffer(float input, CircuitState& state);
    float processClippingStage(float input, CircuitState& state, float drive);
    float processToneStage(float input, CircuitState& state, float tone);
    float processOutputBuffer(float input, CircuitState& state, float level);
    float softClipper(float input);
    void updateFilterCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TubeScreamer808)
};
