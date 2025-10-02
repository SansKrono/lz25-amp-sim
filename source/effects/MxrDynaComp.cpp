#include "MxrDynaComp.h"
#include <cmath>

//==============================================================================
MxrDynaComp::MxrDynaComp()
    : EffectPedal("COMPRESSOR", "COMP", EnclosureType::Enclosure1590BB)  // Increased size to ensure knob fit
{
}

//==============================================================================
std::vector<EffectPedal::Parameter> MxrDynaComp::getParameters() const
{
    return {
        Parameter("COMP_SENSITIVITY", "SENS", 0.0f, 1.0f, 0.5f),
        Parameter("COMP_OUTPUT", "OUTPUT", 0.0f, 1.0f, 0.7f)
    };
}

void MxrDynaComp::updateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    currentSensitivity = *apvts.getRawParameterValue("COMP_SENSITIVITY");
    currentOutput = *apvts.getRawParameterValue("COMP_OUTPUT");
}

//==============================================================================
void MxrDynaComp::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    dt = 1.0f / sampleRate;

    channelStates.clear();
    channelStates.resize(spec.numChannels);

    updateFilterCoefficients();

    // Initialize attack/release coefficients for each channel
    for (auto& state : channelStates)
    {
        updateAttackRelease(state, 0.5f); // Default sensitivity
    }
}

void MxrDynaComp::reset()
{
    for (auto& state : channelStates)
    {
        state.inputHPF_z1 = 0.0f;
        state.inputLPF_z1 = 0.0f;
        state.envelopeFollower = 0.0f;
        state.peakDetector = 0.0f;
        state.envelopeSmooth_z1 = 0.0f;
        state.vcaGain = 1.0f;
        state.gainSmooth_z1 = 0.0f;
        state.outputHPF_z1 = 0.0f;
        state.dcBlocker_z1 = 0.0f;
    }
}

void MxrDynaComp::process(juce::dsp::AudioBlock<float>& block)
{
    if (!enabled)
        return;

    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* channelData = block.getChannelPointer(channel);
        auto& state = channelStates[channel];

        // Update attack/release based on sensitivity
        updateAttackRelease(state, currentSensitivity);

        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // Process through the four stages of the Dyna Comp
            float stage1 = processInputStage(input, state);
            float controlVoltage = processEnvelopeDetector(stage1, state, currentSensitivity);
            float stage2 = processVCA(stage1, controlVoltage, state);
            float output = processOutputStage(stage2, state, currentOutput);

            channelData[sample] = output;
        }
    }
}

//==============================================================================
void MxrDynaComp::updateFilterCoefficients()
{
    const float pi = juce::MathConstants<float>::pi;

    // Input HPF (~16Hz, from C2 and input network)
    float wc = 2.0f * pi * 16.0f / sampleRate;
    float k = std::tan(wc / 2.0f);
    float norm = 1.0f / (1.0f + k);
    inputHPF_b0 = norm;
    inputHPF_b1 = -norm;
    inputHPF_a1 = (k - 1.0f) * norm;

    // Input LPF (~7kHz, from C3 and network)
    wc = 2.0f * pi * 7000.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    inputLPF_b0 = k * norm;
    inputLPF_b1 = k * norm;
    inputLPF_a1 = (k - 1.0f) * norm;

    // Output HPF (~1.6Hz, from C10)
    wc = 2.0f * pi * 1.6f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    outputHPF_b0 = norm;
    outputHPF_b1 = -norm;
    outputHPF_a1 = (k - 1.0f) * norm;

    // DC blocker (~0.5Hz)
    wc = 2.0f * pi * 0.5f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    dcBlock_b0 = norm;
    dcBlock_b1 = -norm;
    dcBlock_a1 = (k - 1.0f) * norm;
}

void MxrDynaComp::updateAttackRelease(CircuitState& state, float sensitivity)
{
    // Attack and release times based on envelope detector circuit
    // Higher sensitivity = faster attack, slower release (more compression)
    float baseAttack = 0.001f; // 1ms base attack
    float baseRelease = 0.1f; // 100ms base release

    float attackTime = baseAttack * (1.0f + sensitivity * 4.0f);
    float releaseTime = baseRelease * (1.0f + (1.0f - sensitivity) * 9.0f); // 100ms to 1s

    state.attackCoeff = std::exp(-1.0f / (attackTime * sampleRate));
    state.releaseCoeff = std::exp(-1.0f / (releaseTime * sampleRate));
}

//==============================================================================
float MxrDynaComp::processInputStage(float input, CircuitState& state)
{
    // Input buffer stage with Q1 transistor emulation
    // Apply input filtering
    float hpfOut = inputHPF_b0 * input + inputHPF_b1 * state.inputHPF_z1 - inputHPF_a1 * state.inputHPF_z1;
    state.inputHPF_z1 = hpfOut;

    float lpfOut = inputLPF_b0 * hpfOut + inputLPF_b1 * state.inputLPF_z1 - inputLPF_a1 * state.inputLPF_z1;
    state.inputLPF_z1 = lpfOut;

    // Transistor buffer gain and slight compression
    float bufferGain = 0.95f;
    return lpfOut * bufferGain;
}

float MxrDynaComp::processEnvelopeDetector(float input, CircuitState& state, float sensitivity)
{
    // Envelope detection circuit modeling (Q2, diodes D1/D2, and RC network)
    float inputLevel = std::abs(input);

    // Peak detection with diode rectification
    float rectified = inputLevel;
    if (rectified > state.peakDetector)
        state.peakDetector = rectified;
    else
        state.peakDetector *= 0.9999f; // Slow decay

    // Envelope follower with attack/release
    float target = state.peakDetector;
    if (target > state.envelopeFollower)
    {
        // Attack
        state.envelopeFollower = target + (state.envelopeFollower - target) * state.attackCoeff;
    }
    else
    {
        // Release
        state.envelopeFollower = target + (state.envelopeFollower - target) * state.releaseCoeff;
    }

    // Smooth the envelope
    float smoothed = 0.1f * state.envelopeFollower + 0.9f * state.envelopeSmooth_z1;
    state.envelopeSmooth_z1 = smoothed;

    // Convert to control voltage (sensitivity affects the response curve)
    float sensitivityFactor = 0.1f + sensitivity * 2.0f; // 0.1 to 2.1
    float controlVoltage = smoothed * sensitivityFactor;

    return controlVoltage;
}

float MxrDynaComp::processVCA(float input, float controlVoltage, CircuitState& state)
{
    // LM13700 OTA (Operational Transconductance Amplifier) modeling
    // The control voltage modulates the transconductance

    // Convert control voltage to gain reduction
    float inputDb = linearToDb(std::abs(input) + 1e-6f);
    float gainReductionDb = softKneeCompressor(inputDb, threshold, ratio, kneeWidth);

    // Apply sensitivity to gain reduction
    gainReductionDb *= (0.3f + currentSensitivity * 0.7f); // Scale by sensitivity

    // Convert back to linear and apply
    float gainReduction = dbToLinear(gainReductionDb);

    // Smooth gain changes to avoid zipper noise
    float targetGain = gainReduction * dbToLinear(makeupGain);
    state.vcaGain = targetGain + (state.vcaGain - targetGain) * 0.95f;

    return input * state.vcaGain;
}

float MxrDynaComp::processOutputStage(float input, CircuitState& state, float level)
{
    // Output buffer stage with level control
    // DC blocking
    float dcBlocked = dcBlock_b0 * input + dcBlock_b1 * state.dcBlocker_z1 - dcBlock_a1 * state.dcBlocker_z1;
    state.dcBlocker_z1 = dcBlocked;

    // Output HPF
    float hpfOut = outputHPF_b0 * dcBlocked + outputHPF_b1 * state.outputHPF_z1 - outputHPF_a1 * state.outputHPF_z1;
    state.outputHPF_z1 = hpfOut;

    // Apply output level
    return hpfOut * level * 2.0f; // Additional makeup gain
}

float MxrDynaComp::softKneeCompressor(float inputLevel, float threshold, float ratio, float knee)
{
    float output = 0.0f;

    if (inputLevel <= (threshold - knee / 2.0f))
    {
        // Below knee - no compression
        output = 0.0f;
    }
    else if (inputLevel < (threshold + knee / 2.0f))
    {
        // In knee region - soft transition
        float kneeInput = inputLevel - threshold + knee / 2.0f;
        float kneeRatio = kneeInput / knee;
        float kneeFactor = kneeRatio * kneeRatio;
        output = kneeFactor * (1.0f / ratio - 1.0f) * (inputLevel - threshold);
    }
    else
    {
        // Above knee - full compression
        output = (1.0f / ratio - 1.0f) * (inputLevel - threshold);
    }

    return output;
}