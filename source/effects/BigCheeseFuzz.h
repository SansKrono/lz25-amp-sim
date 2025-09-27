#pragma once

#include "EffectPedal.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

//==============================================================================
/**
 * Lovetone Big Cheese fuzz pedal implementation
 * Based on the classic Lovetone Big Cheese fuzz circuit
 */
class BigCheeseFuzz : public EffectPedal
{
public:
    BigCheeseFuzz();
    ~BigCheeseFuzz() override = default;

    //==============================================================================
    // EffectPedal interface
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::dsp::AudioBlock<float>& block) override;

    std::vector<EffectPedal::Parameter> getParameters() const override;
    void updateParameters(juce::AudioProcessorValueTreeState& apvts) override;

private:
    //==============================================================================
    // Circuit modeling components
    struct CircuitState
    {
        // Input stage filtering
        float inputHPF_x1 = 0.0f, inputHPF_y1 = 0.0f;
        float preGainLPF_x1 = 0.0f, preGainLPF_y1 = 0.0f;

        float dcBlock_x1 = 0.0f, dcBlock_y1 = 0.0f;
        float outputHPF_x1 = 0.0f, outputHPF_y1 = 0.0f;

        float toneLPF_x1 = 0.0f, toneLPF_y1 = 0.0f;
        float toneHPF_x1 = 0.0f, toneHPF_y1 = 0.0f;

        // Transistor gain stages
        float q1_collector = 0.0f;
        float q1_base_z1 = 0.0f;
        float q2_collector = 0.0f;
        float q2_base_z1 = 0.0f;
        float q3_collector = 0.0f;
        float q3_base_z1 = 0.0f;

        // Op-amp stage
        float opampOut_z1 = 0.0f;

        // Switch matrix states
        bool switchA_closed = false;
        bool switchB_closed = false;
        bool switchC_closed = false;

        // Bias points
        float q1_bias = 4.5f;
        float q2_bias = 4.5f;
        float q3_bias = 4.5f;

        // Lightweight noise gate state
        float noiseEnv = 0.0f;   // input envelope follower
        float gateGain = 1.0f;   // smoothed gate gain (0=closed, 1=open)
    };

    std::vector<CircuitState> channelStates;

    //==============================================================================
    // Circuit parameters
    float sampleRate = 44100.0f;
    float dt = 1.0f / 44100.0f;

    // Filter coefficients
    float inputHPF_a1 = 0.0f, inputHPF_b0 = 0.0f, inputHPF_b1 = 0.0f;
    float preGainLPF_a1 = 0.0f, preGainLPF_b0 = 0.0f, preGainLPF_b1 = 0.0f;
    float outputHPF_a1 = 0.0f, outputHPF_b0 = 0.0f, outputHPF_b1 = 0.0f;
    float dcBlock_a1 = 0.0f, dcBlock_b0 = 0.0f, dcBlock_b1 = 0.0f;

    //==============================================================================
    // Current parameter values
    float currentFuzz   = 0.5f;
    float currentTone   = 0.5f;
    float currentVolume = 0.7f;
    float currentTrim   = 0.5f;
    int currentSwitch   = 1; // Medium position
    
    // Previous tone value for coefficient update detection
    float previousTone = -1.0f;
    int previousSwitch = -1;
    
    // Pre-calculated tone filter coefficients
    float toneBass_a1 = 0.0f, toneBass_b0 = 0.0f, toneBass_b1 = 0.0f;
    float toneTreble_a1 = 0.0f, toneTreble_b0 = 0.0f, toneTreble_b1 = 0.0f;

    //==============================================================================
    // Circuit simulation methods
    float processInputStage(float input, CircuitState& state);
    float processTransistorQ1(float input, CircuitState& state, float fuzzAmount);
    float processTransistorQ2(float input, CircuitState& state, float fuzzAmount);
    float processTransistorQ3(float input, CircuitState& state);
    float processOpAmpStage(float input, CircuitState& state);
    float processToneCircuit(float input, CircuitState& state, float tone, int switchPos);
    float processOutputStage(float input, CircuitState& state, float volume);

    float transistorDistortion(float input, float bias, float gain, float saturation);
    float asymmetricClipping(float input, float threshold);

    void updateFilterCoefficients();
    void updateToneFilterCoefficients(float tone, int switchPos);
    void updateSwitchMatrix(CircuitState& state, int switchPosition);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigCheeseFuzz)
};
