#include "TubeScreamer808.h"
#include <cmath>

//==============================================================================
TubeScreamer808::TubeScreamer808()
    : EffectPedal("TS808", "TS808", EnclosureType::Enclosure1590BB)
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

    // Update submodule clipper drive (expects 0..10 range)
    const float clipperDrive = juce::jlimit(0.0f, 10.0f, currentDrive * 10.0f);
    for (auto& state : channelStates)
        if (state.clipper)
            state.clipper->setDrive(clipperDrive);
}

//==============================================================================
void TubeScreamer808::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    dt = 1.0f / sampleRate;

    channelStates.clear();
    channelStates.resize(spec.numChannels);

    // Prepare TS-808-Ultra clippers per channel
    const float clipperDrive = juce::jlimit(0.0f, 10.0f, currentDrive * 10.0f);
    for (auto& state : channelStates)
    {
        if (!state.clipper)
            state.clipper = std::make_unique<ClippingStage>();
        state.clipper->prepare(sampleRate);
        state.clipper->setDrive(clipperDrive);
    }

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

        // Reset TS-808-Ultra clipper state
        if (state.clipper)
            state.clipper->reset();
    }
}

void TubeScreamer808::process(juce::dsp::AudioBlock<float>& block)
{
    if (!enabled)
        return;

    // Lightweight noise gate config
    const float openThresh = 0.0010f;   // ~ -60 dBFS
    const float closeThresh = 0.0003f;  // ~ -70.5 dBFS
    const float envAtkA = std::exp(-1.0f / (0.005f * sampleRate));   // 5 ms attack
    const float envRelA = std::exp(-1.0f / (0.050f * sampleRate));   // 50 ms release
    const float gateOpenA = std::exp(-1.0f / (0.002f * sampleRate)); // 2 ms open
    const float gateCloseA = std::exp(-1.0f / (0.080f * sampleRate)); // 80 ms close

    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* channelData = block.getChannelPointer(channel);
        auto& state = channelStates[channel];

        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // Update input envelope
            float ax = std::abs(input);
            float aEnv = (ax > state.noiseEnv ? envAtkA : envRelA);
            state.noiseEnv = aEnv * state.noiseEnv + (1.0f - aEnv) * ax;

            // Hysteresis gate control
            if (state.noiseEnv < closeThresh)
            {
                state.gateGain = gateCloseA * state.gateGain;
            }
            else if (state.noiseEnv > openThresh)
            {
                state.gateGain = gateOpenA * state.gateGain + (1.0f - gateOpenA);
            }

            // Process through the four stages
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

    // Pre-clipping LPF (~3.4kHz)
    wc = 2.0f * pi * 3400.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    preClipLPF_b0 = k * norm;
    preClipLPF_b1 = k * norm;
    preClipLPF_a1 = (k - 1.0f) * norm;

    // Post-clipping HPF (~34Hz)
    wc = 2.0f * pi * 34.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    postClipHPF_b0 = norm;
    postClipHPF_b1 = -norm;
    postClipHPF_a1 = (k - 1.0f) * norm;

    // Output buffer HPF (~1.6Hz)
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
    // CRITICAL FIX: Use Direct Form II Transposed to work with single z1 state variable
    // This is more efficient and matches your header structure

    // Direct Form II Transposed for HPF: y[n] = b0*x[n] + z1[n-1]
    //                                     z1[n] = b1*x[n] - a1*y[n]

    float output = inputHPF_b0 * input + state.inputHPF_z1;
    state.inputHPF_z1 = inputHPF_b1 * input - inputHPF_a1 * output;

    return output * 0.99f;
}

float TubeScreamer808::processClippingStage(float input, CircuitState& state, float /*drive*/)
{
    // Pre-clipping LPF using Direct Form II Transposed
    float filtered = preClipLPF_b0 * input + state.preClipLPF_z1;
    state.preClipLPF_z1 = preClipLPF_b1 * input - preClipLPF_a1 * filtered;

    // Use TS-808-Ultra clipping stage
    float clipped = state.clipper ? state.clipper->processSample(filtered) : filtered;

    // Post-clipping HPF using Direct Form II Transposed
    float output = postClipHPF_b0 * clipped + state.postClipHPF_z1;
    state.postClipHPF_z1 = postClipHPF_b1 * clipped - postClipHPF_a1 * output;

    return output;
}

float TubeScreamer808::processToneStage(float input, CircuitState& state, float tone)
{
    // FIXED: More accurate tone control modeling
    // Real TS808 tone control: 0 = dark/scooped, 1 = bright/present

    const float pi = juce::MathConstants<float>::pi;

    // Bass path (LPF) - becomes more prominent at low tone settings
    float bassCutoff = 500.0f + tone * 1500.0f; // 500Hz to 2kHz
    float wc_bass = 2.0f * pi * bassCutoff / sampleRate;
    float k_bass = std::tan(wc_bass / 2.0f);
    float norm_bass = 1.0f / (1.0f + k_bass);
    float bass_b0 = k_bass * norm_bass;
    float bass_b1 = k_bass * norm_bass;
    float bass_a1 = (k_bass - 1.0f) * norm_bass;

    // Process bass path using Direct Form II Transposed
    float bassOut = bass_b0 * input + state.toneLPF_z1;
    state.toneLPF_z1 = bass_b1 * input - bass_a1 * bassOut;

    // Treble path (HPF) - becomes more prominent at high tone settings
    float trebleCutoff = 400.0f + (1.0f - tone) * 800.0f; // 1.2kHz to 400Hz (inverted)
    float wc_treble = 2.0f * pi * trebleCutoff / sampleRate;
    float k_treble = std::tan(wc_treble / 2.0f);
    float norm_treble = 1.0f / (1.0f + k_treble);
    float treble_b0 = norm_treble;
    float treble_b1 = -norm_treble;
    float treble_a1 = (k_treble - 1.0f) * norm_treble;

    // Process treble path using Direct Form II Transposed
    float trebleOut = treble_b0 * input + state.toneHPF_z1;
    state.toneHPF_z1 = treble_b1 * input - treble_a1 * trebleOut;

    // FIXED: Better mixing curve (equal power crossfade)
    // This prevents the "dip" in volume at middle tone settings
    float toneAngle = tone * 1.5707963f; // 0 to π/2
    float bassGain = std::cos(toneAngle);
    float trebleGain = std::sin(toneAngle);

    return bassOut * bassGain + trebleOut * trebleGain;
}

float TubeScreamer808::processOutputBuffer(float input, CircuitState& state, float level)
{
    // FIXED: Output HPF using Direct Form II Transposed
    float filtered = outputHPF_b0 * input + state.outputHPF_z1;
    state.outputHPF_z1 = outputHPF_b1 * input - outputHPF_a1 * filtered;

    // Apply level control with slight compensation
    return filtered * level * 1.5f; // Boost to compensate for losses
}

float TubeScreamer808::softClipper(float input)
{
    // FIXED: Realistic asymmetric soft clipping based on 1N4148 diodes
    // Real TS808 uses back-to-back diodes with asymmetric clipping

    const float threshold = 0.5f; // Adjusted for better headroom

    // Asymmetric clipping (more compression on positive side)
    // This creates even-order harmonics characteristic of TS808
    if (input > threshold)
    {
        // Positive side: harder clipping (forward-biased diode)
        float x = (input - threshold) / threshold;
        return threshold + threshold * (2.0f / 3.141592f) * std::atan(x * 2.5f);
    }
    else if (input < -threshold * 0.9f)
    {
        // Negative side: slightly softer clipping (asymmetry)
        float x = (input + threshold * 0.9f) / (threshold * 0.9f);
        return -threshold * 0.9f + (threshold * 0.9f) * (2.0f / 3.141592f) * std::atan(x * 2.2f);
    }

    // Linear region with slight soft knee for smoother transition
    return input + 0.05f * input * input * input;
}