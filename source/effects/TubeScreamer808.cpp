#include "TubeScreamer808.h"
#include <cmath>

//==============================================================================
TubeScreamer808::TubeScreamer808()
    : EffectPedal("TS808", "TS808", EnclosureType::Enclosure1590BB)  // 3 knobs - wide pedal
{
}

//==============================================================================
std::vector<EffectPedal::Parameter> TubeScreamer808::getParameters() const
{
    return {
        Parameter("TS808_DRIVE", "DRIVE", 0.0f, 1.0f, 0.5f),
        Parameter("TS808_TONE", "TONE", 0.0f, 1.0f, 0.5f),
        Parameter("TS808_LEVEL", "LEVEL", 0.0f, 1.0f, 0.7f)
    };
}

void TubeScreamer808::updateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    currentDrive = *apvts.getRawParameterValue("TS808_DRIVE");
    currentTone = *apvts.getRawParameterValue("TS808_TONE");
    currentLevel = *apvts.getRawParameterValue("TS808_LEVEL");
}

//==============================================================================
void TubeScreamer808::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    dt = 1.0f / sampleRate;

    channelStates.clear();
    channelStates.resize(spec.numChannels);

    updateFilterCoefficients();
}

void TubeScreamer808::reset()
{
    for (auto& state : channelStates)
    {
        state.inputHPF_z1 = 0.0f;
        state.preClipLPF_z1 = 0.0f;
        state.postClipHPF_z1 = 0.0f;
        state.clipperFeedback_z1 = 0.0f;
        state.toneHPF_z1 = 0.0f;
        state.toneLPF_z1 = 0.0f;
        state.outputHPF_z1 = 0.0f;
        state.noiseEnv = 0.0f;
        state.gateGain = 1.0f;
    }
}

void TubeScreamer808::process(juce::dsp::AudioBlock<float>& block)
{
    if (!enabled)
        return;

    // Lightweight noise gate config (account for interface noise floor)
    const float openThresh = 0.0010f;   // ~ -60 dBFS: signal must exceed this to open
    const float closeThresh = 0.0003f;  // ~ -70.5 dBFS: close when below this
    const float envAtkA = std::exp(-1.0f / (0.005f * sampleRate));   // 5 ms attack for envelope
    const float envRelA = std::exp(-1.0f / (0.050f * sampleRate));   // 50 ms release for envelope
    const float gateOpenA = std::exp(-1.0f / (0.002f * sampleRate)); // 2 ms to open (fast)
    const float gateCloseA = std::exp(-1.0f / (0.080f * sampleRate)); // 80 ms to close (slow)

    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* channelData = block.getChannelPointer(channel);
        auto& state = channelStates[channel];

        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // Update input envelope (abs) with separate attack/release
            float ax = std::abs(input);
            float aEnv = (ax > state.noiseEnv ? envAtkA : envRelA);
            state.noiseEnv = aEnv * state.noiseEnv + (1.0f - aEnv) * ax;

            // Hysteresis gate control
            if (state.noiseEnv < closeThresh)
            {
                state.gateGain = gateCloseA * state.gateGain; // decay toward 0
            }
            else if (state.noiseEnv > openThresh)
            {
                state.gateGain = gateOpenA * state.gateGain + (1.0f - gateOpenA); // rise toward 1
            }
            // else: hold current gateGain

            // Process through the four stages of the TS808
            float stage1 = processInputBuffer(input, state);
            float stage2 = processClippingStage(stage1, state, currentDrive);
            float stage3 = processToneStage(stage2, state, currentTone);
            float stage4 = processOutputBuffer(stage3, state, currentLevel);

            channelData[sample] = stage4 * state.gateGain;
        }
    }
}

//==============================================================================
void TubeScreamer808::updateFilterCoefficients()
{
    const float pi = juce::MathConstants<float>::pi;

    // Input buffer HPF (removes DC, ~20Hz cutoff)
    float wc = 2.0f * pi * 20.0f / sampleRate;
    float k = std::tan(wc / 2.0f);
    float norm = 1.0f / (1.0f + k);
    inputHPF_b0 = norm;
    inputHPF_b1 = -norm;
    inputHPF_a1 = (k - 1.0f) * norm;

    // Pre-clipping LPF (~3.4kHz, from R6||R7 and C5)
    wc = 2.0f * pi * 3400.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    preClipLPF_b0 = k * norm;
    preClipLPF_b1 = k * norm;
    preClipLPF_a1 = (k - 1.0f) * norm;

    // Post-clipping HPF (~34Hz, from C4 and feedback network)
    wc = 2.0f * pi * 34.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    postClipHPF_b0 = norm;
    postClipHPF_b1 = -norm;
    postClipHPF_a1 = (k - 1.0f) * norm;

    // Output buffer HPF (~1.6Hz, from C8)
    wc = 2.0f * pi * 1.6f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    outputHPF_b0 = norm;
    outputHPF_b1 = -norm;
    outputHPF_a1 = (k - 1.0f) * norm;
}

//==============================================================================
float TubeScreamer808::processInputBuffer(float input, CircuitState& state)
{
    // Input buffer: Unity gain buffer with HPF
    // Simulates Q1 transistor buffer stage
    float output = inputHPF_b0 * input + inputHPF_b1 * state.inputHPF_z1 - inputHPF_a1 * state.inputHPF_z1;
    state.inputHPF_z1 = output;

    return output * 0.99f; // Slight attenuation to model real buffer
}

float TubeScreamer808::processClippingStage(float input, CircuitState& state, float drive)
{
    // Drive control: maps 0-1 to resistance values that affect gain
    // Low drive = high resistance = low gain, High drive = low resistance = high gain
    float driveResistance = 1000.0f + drive * 499000.0f; // 1k to 500k
    float gainFactor = 1.0f + (51000.0f / driveResistance) * 10.0f; // Approximate op-amp gain

    // Apply gain
    float gained = input * gainFactor;

    // Pre-clipping LPF
    float filtered = preClipLPF_b0 * gained + preClipLPF_b1 * state.preClipLPF_z1 - preClipLPF_a1 * state.preClipLPF_z1;
    state.preClipLPF_z1 = filtered;

    // Soft clipping (models the diode clipping)
    float clipped = softClipper(filtered);

    // Post-clipping HPF
    float output = postClipHPF_b0 * clipped + postClipHPF_b1 * state.postClipHPF_z1 - postClipHPF_a1 * state.postClipHPF_z1;
    state.postClipHPF_z1 = output;

    return output;
}

float TubeScreamer808::processToneStage(float input, CircuitState& state, float tone)
{
    // Tone control: 0 = bass (LPF), 1 = treble (HPF)
    // This models the gyrator circuit formed by the op-amp and tone pot

    // Dynamic filter calculation based on tone setting
    float bassCutoff = 200.0f + tone * 2000.0f; // 200Hz to 2.2kHz
    float trebleCutoff = 1000.0f + (1.0f - tone) * 1500.0f; // 1kHz to 2.5kHz

    const float pi = juce::MathConstants<float>::pi;

    // Bass path (LPF)
    float wc_bass = 2.0f * pi * bassCutoff / sampleRate;
    float k_bass = std::tan(wc_bass / 2.0f);
    float norm_bass = 1.0f / (1.0f + k_bass);
    float bass_b0 = k_bass * norm_bass;
    float bass_b1 = k_bass * norm_bass;
    float bass_a1 = (k_bass - 1.0f) * norm_bass;

    float bassOut = bass_b0 * input + bass_b1 * state.toneLPF_z1 - bass_a1 * state.toneLPF_z1;
    state.toneLPF_z1 = bassOut;

    // Treble path (HPF)
    float wc_treble = 2.0f * pi * trebleCutoff / sampleRate;
    float k_treble = std::tan(wc_treble / 2.0f);
    float norm_treble = 1.0f / (1.0f + k_treble);
    float treble_b0 = norm_treble;
    float treble_b1 = -norm_treble;
    float treble_a1 = (k_treble - 1.0f) * norm_treble;

    float trebleOut = treble_b0 * input + treble_b1 * state.toneHPF_z1 - treble_a1 * state.toneHPF_z1;
    state.toneHPF_z1 = trebleOut;

    // Mix bass and treble based on tone control
    return bassOut * (1.0f - tone) + trebleOut * tone;
}

float TubeScreamer808::processOutputBuffer(float input, CircuitState& state, float level)
{
    // Output buffer with level control
    // HPF to remove DC offset
    float filtered = outputHPF_b0 * input + outputHPF_b1 * state.outputHPF_z1 - outputHPF_a1 * state.outputHPF_z1;
    state.outputHPF_z1 = filtered;

    // Apply level control
    return filtered * level;
}

float TubeScreamer808::softClipper(float input)
{
    // Asymmetric soft clipping to model the 1N4148 diodes
    // This creates the characteristic TS808 overdrive sound

    const float threshold = 0.7f; // Diode forward voltage
    const float ratio = 0.3f; // Compression ratio after threshold

    if (input > threshold)
    {
        float excess = input - threshold;
        return threshold + excess * ratio + 0.1f * std::sin(excess * 3.0f);
    }
    else if (input < -threshold * 0.8f) // Asymmetric clipping
    {
        float excess = input + threshold * 0.8f;
        return -threshold * 0.8f + excess * ratio * 0.7f;
    }

    return input;
}
