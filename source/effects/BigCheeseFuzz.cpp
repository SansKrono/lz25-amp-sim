#include "BigCheeseFuzz.h"
#include <cmath>

//==============================================================================
BigCheeseFuzz::BigCheeseFuzz()
    : EffectPedal("FUZZ", "BIGCHEESE", EnclosureType::Enclosure1590BB)
{
}

//==============================================================================
std::vector<EffectPedal::Parameter> BigCheeseFuzz::getParameters() const
{
    return {
        EffectPedal::Parameter("BIGCHEESE_FUZZ",   "FUZZ",   0.0f, 1.0f, 0.5f),
        EffectPedal::Parameter("BIGCHEESE_TONE",   "TONE",   0.0f, 1.0f, 0.5f),
        EffectPedal::Parameter("BIGCHEESE_VOLUME", "VOLUME", 0.0f, 1.0f, 0.7f),
        EffectPedal::Parameter("BIGCHEESE_TRIM",   "TRIM",   0.0f, 1.0f, 0.5f),
        EffectPedal::Parameter("BIGCHEESE_SWITCH", "MODE",   0.0f, 3.0f, 1.0f)
    };
}

//==============================================================================
void BigCheeseFuzz::updateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    currentFuzz   = apvts.getRawParameterValue("BIGCHEESE_FUZZ")->load();
    currentTone   = apvts.getRawParameterValue("BIGCHEESE_TONE")->load();
    currentVolume = apvts.getRawParameterValue("BIGCHEESE_VOLUME")->load();
    currentTrim   = apvts.getRawParameterValue("BIGCHEESE_TRIM")->load();
    currentSwitch = static_cast<int>(apvts.getRawParameterValue("BIGCHEESE_SWITCH")->load());
}

//==============================================================================
void BigCheeseFuzz::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    dt = 1.0f / sampleRate;

    channelStates.clear();
    channelStates.resize(spec.numChannels);

    reset();
    updateFilterCoefficients();
    
    // Initialize tone filter coefficients
    previousTone = -1.0f; // Force initial calculation
    previousSwitch = -1;
    updateToneFilterCoefficients(currentTone, currentSwitch);
}

//==============================================================================
void BigCheeseFuzz::reset()
{
    for (auto& state : channelStates)
    {
        state = CircuitState{};
        state.q1_bias = 4.5f;
        state.q2_bias = 4.5f;
        state.q3_bias = 4.5f;
        state.noiseEnv = 0.0f;
        state.gateGain = 1.0f;
    }
}

//==============================================================================
void BigCheeseFuzz::process(juce::dsp::AudioBlock<float>& block)
{
    if (!enabled)
        return;

    // Lightweight noise gate config (account for interface noise floor)
    const float openThresh = 0.0010f;   // ~ -60 dBFS
    const float closeThresh = 0.0003f;  // ~ -70.5 dBFS
    const float envAtkA = std::exp(-1.0f / (0.005f * sampleRate));   // 5 ms
    const float envRelA = std::exp(-1.0f / (0.050f * sampleRate));   // 50 ms
    const float gateOpenA = std::exp(-1.0f / (0.002f * sampleRate)); // 2 ms open
    const float gateCloseA = std::exp(-1.0f / (0.080f * sampleRate)); // 80 ms close

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* data = block.getChannelPointer(ch);
        auto& state = channelStates[ch];

        updateSwitchMatrix(state, currentSwitch);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float x = data[i];

            // Update envelope from input
            float ax = std::abs(x);
            float aEnv = (ax > state.noiseEnv ? envAtkA : envRelA);
            state.noiseEnv = aEnv * state.noiseEnv + (1.0f - aEnv) * ax;

            // Hysteresis gate control
            if (state.noiseEnv < closeThresh)
                state.gateGain = gateCloseA * state.gateGain; // decay toward 0
            else if (state.noiseEnv > openThresh)
                state.gateGain = gateOpenA * state.gateGain + (1.0f - gateOpenA); // rise toward 1

            float s1 = processInputStage(x, state);
            float s2 = processTransistorQ1(s1, state, currentFuzz);
            float s3 = processTransistorQ2(s2, state, currentFuzz);
            float s4 = processTransistorQ3(s3, state);
            float s5 = processOpAmpStage(s4, state);
            float s6 = processToneCircuit(s5, state, currentTone, currentSwitch);
            float y  = processOutputStage(s6, state, currentVolume);

            data[i] = y * state.gateGain;
        }
    }
}

//==============================================================================
// Filter coefficient setup
void BigCheeseFuzz::updateFilterCoefficients()
{
    const float pi = juce::MathConstants<float>::pi;

    // Input HPF (~34 Hz)
    float wc = 2.0f * pi * 34.0f / sampleRate;
    float k = std::tan(wc / 2.0f);
    float norm = 1.0f / (1.0f + k);
    inputHPF_b0 = norm;
    inputHPF_b1 = -norm;
    inputHPF_a1 = (k - 1.0f) * norm;

    // Pre-gain LPF (~3.4 kHz)
    wc = 2.0f * pi * 3400.0f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    preGainLPF_b0 = k * norm;
    preGainLPF_b1 = k * norm;
    preGainLPF_a1 = (k - 1.0f) * norm;

    // Output HPF (~3.4 Hz)
    wc = 2.0f * pi * 3.4f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    outputHPF_b0 = norm;
    outputHPF_b1 = -norm;
    outputHPF_a1 = (k - 1.0f) * norm;

    // DC Block (~0.1 Hz)
    wc = 2.0f * pi * 0.1f / sampleRate;
    k = std::tan(wc / 2.0f);
    norm = 1.0f / (1.0f + k);
    dcBlock_b0 = norm;
    dcBlock_b1 = -norm;
    dcBlock_a1 = (k - 1.0f) * norm;
}

//==============================================================================
// Tone filter coefficient update
void BigCheeseFuzz::updateToneFilterCoefficients(float tone, int switchPos)
{
    // Only update if parameters have changed significantly
    const float threshold = 0.001f;
    if (std::abs(tone - previousTone) < threshold && switchPos == previousSwitch)
        return;
    
    previousTone = tone;
    previousSwitch = switchPos;
    
    const float pi = juce::MathConstants<float>::pi;
    float bassCutoff = 200.0f + tone * 800.0f;
    float trebleCutoff = 2000.0f + (1.0f - tone) * 3000.0f;

    // Apply switch position modifiers
    switch (switchPos)
    {
        case 0: bassCutoff *= 1.2f; trebleCutoff *= 0.8f; break;
        case 2: bassCutoff *= 0.8f; trebleCutoff *= 1.2f; break;
        case 3: bassCutoff *= 0.6f; trebleCutoff *= 1.5f; break;
    }

    // Clamp frequencies to reasonable ranges to prevent instability
    bassCutoff = juce::jlimit(50.0f, sampleRate * 0.4f, bassCutoff);
    trebleCutoff = juce::jlimit(100.0f, sampleRate * 0.4f, trebleCutoff);

    // Calculate bass (low-pass) filter coefficients
    float wc_bass = 2.0f * pi * bassCutoff / sampleRate;
    float k_bass = std::tan(wc_bass / 2.0f);
    float norm_bass = 1.0f / (1.0f + k_bass);
    toneBass_b0 = k_bass * norm_bass;
    toneBass_b1 = k_bass * norm_bass;
    toneBass_a1 = (k_bass - 1.0f) * norm_bass;

    // Calculate treble (high-pass) filter coefficients  
    float wc_treble = 2.0f * pi * trebleCutoff / sampleRate;
    float k_treble = std::tan(wc_treble / 2.0f);
    float norm_treble = 1.0f / (1.0f + k_treble);
    toneTreble_b0 = norm_treble;
    toneTreble_b1 = -norm_treble;
    toneTreble_a1 = (k_treble - 1.0f) * norm_treble;
}

//==============================================================================
// Switch matrix
void BigCheeseFuzz::updateSwitchMatrix(CircuitState& state, int pos)
{
    switch (pos)
    {
        case 0: state.switchA_closed=false; state.switchB_closed=false; state.switchC_closed=true;  break;
        case 1: state.switchA_closed=false; state.switchB_closed=false; state.switchC_closed=true;  break;
        case 2: state.switchA_closed=false; state.switchB_closed=true;  state.switchC_closed=true;  break;
        case 3: state.switchA_closed=true;  state.switchB_closed=true;  state.switchC_closed=true;  break;
    }
}

//==============================================================================
// Processing stages
float BigCheeseFuzz::processInputStage(float input, CircuitState& s)
{
    float hpf = inputHPF_b0 * input + inputHPF_b1 * s.inputHPF_x1 - inputHPF_a1 * s.inputHPF_y1;
    s.inputHPF_x1 = input; s.inputHPF_y1 = hpf;

    float lpf = preGainLPF_b0 * hpf + preGainLPF_b1 * s.preGainLPF_x1 - preGainLPF_a1 * s.preGainLPF_y1;
    s.preGainLPF_x1 = hpf; s.preGainLPF_y1 = lpf;

    return lpf;
}

float BigCheeseFuzz::processTransistorQ1(float input, CircuitState& s, float fuzzAmount)
{
    // Apply input coupling and base filtering without adding large DC bias
    float baseFiltered = 0.1f * input + 0.9f * s.q1_base_z1; s.q1_base_z1 = baseFiltered;

    // Reasonable gain range for first stage
    float gain = 5.0f + fuzzAmount * 15.0f; // 5x to 20x gain
    float amplified = baseFiltered * gain;
    
    // Apply distortion without problematic bias addition
    float clipped = transistorDistortion(amplified, 0.0f, gain, 0.7f);

    // Output coupling
    float filtered = 0.98f * clipped + 0.02f * s.q1_collector; s.q1_collector = filtered;
    return filtered;
}

float BigCheeseFuzz::processTransistorQ2(float input, CircuitState& s, float fuzzAmount)
{
    // Apply input coupling and base filtering without large DC bias
    float baseFiltered = 0.2f * input + 0.8f * s.q2_base_z1; s.q2_base_z1 = baseFiltered;

    // Fuzz control affects gain - reasonable range
    float emitterResistance = 1.0f + fuzzAmount * 99.0f; // Reduced from 999.0f
    float gain = 8.0f / (1.0f + emitterResistance * 0.01f); // Reduced base gain
    
    float amplified = baseFiltered * gain;
    
    // Apply distortion without problematic bias addition
    float clipped = transistorDistortion(amplified, 0.0f, gain, 0.6f);

    // Output coupling
    float coupled = 0.95f * clipped; s.q2_collector = coupled;
    return coupled;
}

float BigCheeseFuzz::processTransistorQ3(float input, CircuitState& s)
{
    // Apply input coupling and base filtering without large DC bias
    float baseFiltered = 0.3f * input + 0.7f * s.q3_base_z1; s.q3_base_z1 = baseFiltered;

    // Buffer stage with moderate gain
    float gain = 3.0f; // Reduced from 5.0f
    float amplified = baseFiltered * gain;
    
    // Apply distortion without problematic bias addition
    float saturated = transistorDistortion(amplified, 0.0f, gain, 0.8f);

    s.q3_collector = saturated;
    return saturated;
}

float BigCheeseFuzz::processOpAmpStage(float input, CircuitState& s)
{
    float gain = 2.0f;
    float amplified = input * gain;
    float clipped = std::tanh(amplified * 0.5f) * 2.0f;

    float filtered = 0.8f * clipped + 0.2f * s.opampOut_z1; s.opampOut_z1 = filtered;
    return filtered;
}

float BigCheeseFuzz::processToneCircuit(float input, CircuitState& s, float tone, int switchPos)
{
    // Update filter coefficients only when parameters change
    updateToneFilterCoefficients(tone, switchPos);

    // Apply bass (low-pass) filter using pre-calculated coefficients
    float bassOut = toneBass_b0 * input + toneBass_b1 * s.toneLPF_x1 - toneBass_a1 * s.toneLPF_y1;
    s.toneLPF_x1 = input; 
    s.toneLPF_y1 = bassOut;

    // Apply treble (high-pass) filter using pre-calculated coefficients
    float trebleOut = toneTreble_b0 * input + toneTreble_b1 * s.toneHPF_x1 - toneTreble_a1 * s.toneHPF_y1;
    s.toneHPF_x1 = input; 
    s.toneHPF_y1 = trebleOut;

    // Mix bass and treble based on tone control
    return bassOut * (1.0f - tone) + trebleOut * tone;
}

float BigCheeseFuzz::processOutputStage(float input, CircuitState& s, float volume)
{
    float dcBlocked = dcBlock_b0 * input + dcBlock_b1 * s.dcBlock_x1 - dcBlock_a1 * s.dcBlock_y1;
    s.dcBlock_x1 = input; s.dcBlock_y1 = dcBlocked;

    float hpf = outputHPF_b0 * dcBlocked + outputHPF_b1 * s.outputHPF_x1 - outputHPF_a1 * s.outputHPF_y1;
    s.outputHPF_x1 = dcBlocked; s.outputHPF_y1 = hpf;

    return hpf * volume * 2.0f;
}

//==============================================================================
// Nonlinearities
float BigCheeseFuzz::transistorDistortion(float input, float /*bias*/, float gain, float saturation)
{
    // Apply reasonable gain scaling instead of excessive attenuation
    float gained = input * juce::jmin(gain * 0.1f, 2.0f); // Cap the gain multiplication
    float pos = saturation, neg = -saturation * 0.7f;

    if (gained > pos) { 
        float excess = gained - pos; 
        return pos + excess * 0.1f + 0.05f * std::sin(excess * 5.0f); 
    }
    if (gained < neg) { 
        float excess = gained - neg; 
        return neg + excess * 0.05f; 
    }
    return gained;
}

float BigCheeseFuzz::asymmetricClipping(float input, float threshold)
{
    if (input > threshold) return threshold + (input - threshold) * 0.2f;
    if (input < -threshold * 0.8f) return -threshold * 0.8f + (input + threshold * 0.8f) * 0.1f;
    return input;
}
