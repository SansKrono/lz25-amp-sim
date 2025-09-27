#include "SmartGate.h"
#include <cmath>

//==============================================================================
SmartGate::SmartGate()
    : EffectPedal("SMART GATE", "SMARTGATE", EnclosureType::Enclosure1590BB)
{
}

//==============================================================================
std::vector<EffectPedal::Parameter> SmartGate::getParameters() const
{
    return {
        Parameter("SMARTGATE_INTENSITY", "INTENSITY", 0.0f, 1.0f, 0.5f),
        Parameter("SMARTGATE_REDUCTION", "REDUCTION", 0.0f, 60.0f, 24.0f),
        Parameter("SMARTGATE_RELEASE", "RELEASE", 0.0f, 1.0f, 0.5f),
        // Represent Djent as a 0/1 slider in the generic pedal UI
        Parameter("SMARTGATE_DJENT", "DJENT", 0.0f, 1.0f, 0.0f)
    };
}

void SmartGate::updateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    currentIntensity = *apvts.getRawParameterValue("SMARTGATE_INTENSITY");
    currentReductionDb = *apvts.getRawParameterValue("SMARTGATE_REDUCTION");
    currentRelease = *apvts.getRawParameterValue("SMARTGATE_RELEASE");
    currentDjent = (*apvts.getRawParameterValue("SMARTGATE_DJENT")) > 0.5f;

    // Adjust envelope follower and timing ranges in Djent mode
    if (currentDjent)
    {
        envAttackTime = 0.0015f;  // faster envelope
        envReleaseTime = 0.050f;
        minCloseTime = 0.0015f;
        maxCloseTime = 0.080f;

        // In djent mode, ensure at least 40 dB of reduction for tight muting
        currentReductionDb = juce::jmax(currentReductionDb, 40.0f);
    }
    else
    {
        envAttackTime = 0.003f;
        envReleaseTime = 0.080f;
        minCloseTime = 0.005f;
        maxCloseTime = 0.400f;
    }
}

//==============================================================================
void SmartGate::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);

    channelStates.clear();
    channelStates.resize(spec.numChannels);

    reset();
}

void SmartGate::reset()
{
    for (auto& s : channelStates)
    {
        s.envelope = 0.0f;
        s.prevEnvelope = 0.0f;
        s.gain = 1.0f;
    }
}

void SmartGate::process(juce::dsp::AudioBlock<float>& block)
{
    if (!enabled)
        return;

    const float minGain = juce::jmax( dbToLinear(-currentReductionDb), 1.0e-4f );

    // Opening (removing reduction) speed derived from Release parameter
    // 0 -> slow open (200ms), 1 -> fast open (5ms)
    const float openTime = 0.200f - 0.195f * juce::jlimit(0.0f, 1.0f, currentRelease);

    // Envelope follower coefficients
    const float aEnvAtk = calcAlphaFromTau(envAttackTime);
    const float aEnvRel = calcAlphaFromTau(envReleaseTime);

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* data = block.getChannelPointer(ch);
        auto& st = channelStates[ch];

        for (size_t n = 0; n < block.getNumSamples(); ++n)
        {
            const float x = data[n];
            const float ax = std::abs(x);

            // Envelope follower with separate attack/release
            const float a = (ax > st.envelope) ? aEnvAtk : aEnvRel;
            st.prevEnvelope = st.envelope;
            st.envelope = a * st.envelope + (1.0f - a) * ax;

            // How fast is the envelope falling? (per-sample decrease)
            const float slope = juce::jmax(0.0f, st.prevEnvelope - st.envelope);

            // Map slope to [0,1] with some scaling, then shape with intensity
            const float slopeScaled = juce::jlimit(0.0f, 1.0f, slope * 200.0f);
            const float curveExp = 0.5f + 3.5f * juce::jlimit(0.0f, 1.0f, currentIntensity);
            const float speedFactor = std::pow(slopeScaled, curveExp); // 0 = slow close, 1 = fastest close

            // Compute dynamic closing time constant
            const float closeTime = maxCloseTime - (maxCloseTime - minCloseTime) * speedFactor;
            const float aClose = calcAlphaFromTau(closeTime);
            const float aOpen = calcAlphaFromTau(openTime);

            // If envelope is decaying (slope > 0), move gain towards minGain quickly depending on speed
            if (slope > 0.0f)
                st.gain = aClose * st.gain + (1.0f - aClose) * minGain;
            else
                st.gain = aOpen * st.gain + (1.0f - aOpen) * 1.0f;

            data[n] = x * st.gain;
        }
    }
}
