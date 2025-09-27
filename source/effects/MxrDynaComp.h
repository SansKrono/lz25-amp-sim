#pragma once

#include "EffectPedal.h"
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
 * MXR Dyna Comp compressor effect pedal implementation
 * Based on the classic MXR Dyna Comp compressor circuit
 */
class MxrDynaComp : public EffectPedal
{
public:
    MxrDynaComp();
    ~MxrDynaComp() override = default;

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
        // Input stage filtering
        float inputHPF_z1 = 0.0f;
        float inputLPF_z1 = 0.0f;

        // Envelope detector
        float envelopeFollower = 0.0f;
        float peakDetector = 0.0f;
        float envelopeSmooth_z1 = 0.0f;

        // VCA/OTA gain control
        float vcaGain = 1.0f;
        float gainSmooth_z1 = 0.0f;

        // Output stage
        float outputHPF_z1 = 0.0f;
        float dcBlocker_z1 = 0.0f;

        // Attack/Release timing
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
    };

    std::vector<CircuitState> channelStates;

    //==============================================================================
    // Circuit parameters
    float sampleRate = 44100.0f;
    float dt = 1.0f / 44100.0f;

    // Filter coefficients
    float inputHPF_a1, inputHPF_b0, inputHPF_b1;
    float inputLPF_a1, inputLPF_b0, inputLPF_b1;
    float outputHPF_a1, outputHPF_b0, outputHPF_b1;
    float dcBlock_a1, dcBlock_b0, dcBlock_b1;

    // Compressor parameters
    static constexpr float threshold = -18.0f; // dB
    static constexpr float ratio = 4.0f; // 4:1 compression
    static constexpr float kneeWidth = 6.0f; // dB
    static constexpr float makeupGain = 12.0f; // dB

    //==============================================================================
    // Current parameter values
    float currentSensitivity = 0.5f;
    float currentOutput = 0.7f;

    //==============================================================================
    // Circuit simulation methods
    float processInputStage(float input, CircuitState& state);
    float processEnvelopeDetector(float input, CircuitState& state, float sensitivity);
    float processVCA(float input, float controlVoltage, CircuitState& state);
    float processOutputStage(float input, CircuitState& state, float level);
    float softKneeCompressor(float inputLevel, float threshold, float ratio, float knee);
    void updateFilterCoefficients();
    void updateAttackRelease(CircuitState& state, float sensitivity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MxrDynaComp)
};