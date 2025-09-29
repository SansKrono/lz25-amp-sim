#pragma once
#include "EffectPedal.h"

// A simple transient shaper tailored to boost guitar pick attack
class TransientShaper : public EffectPedal
{
public:
    TransientShaper()
    : EffectPedal("PICK ATTACK", "TRANSIENT", EffectPedal::EnclosureType::Enclosure1590B)
    {
    }

    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = (float) spec.sampleRate;
        reset();
        // Default time constants tuned for guitar pick detection
        setTimes(0.001f, 0.050f); // attack 1ms, sustain 50ms
    }

    void reset() override
    {
        for (auto& v : envFast) v = 0.0f;
        for (auto& v : envSlow) v = 0.0f;
    }

    std::vector<Parameter> getParameters() const override
    {
        return {
            Parameter{"TRANSIENT_ATTACK",  "ATTACK",  0.0f, 2.0f, 1.0f},
            Parameter{"TRANSIENT_SUSTAIN", "SUSTAIN", 0.0f, 2.0f, 1.0f}
        };
    }

    void updateParameters(juce::AudioProcessorValueTreeState& apvts) override
    {
        // Attack: >1 boosts attack, <1 reduces
        attackGain = apvts.getRawParameterValue("TRANSIENT_ATTACK")->load();
        sustainGain = apvts.getRawParameterValue("TRANSIENT_SUSTAIN")->load();
        mix = apvts.getRawParameterValue("TRANSIENT_MIX")->load();
        clipGuard = apvts.getRawParameterValue("TRANSIENT_CLIP")->load() > 0.5f;
    }

    void process(juce::dsp::AudioBlock<float>& block) override
    {
        const auto numChannels = (int) block.getNumChannels();
        const auto numSamples  = (int) block.getNumSamples();

        // Ensure arrays big enough for stereo
        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* data = block.getChannelPointer((size_t) ch);
            float eFast = envFast[ch];
            float eSlow = envSlow[ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                const float ax = std::abs(x);

                // Leaky integrators for fast and slow envelopes
                eFast += coeffFast * (ax - eFast);
                eSlow += coeffSlow * (ax - eSlow);

                // Transient indicator: difference between fast and slow
                float t = eFast - eSlow;                 // can be negative during decays
                t = juce::jlimit(0.0f, 1.0f, t * 8.0f);  // scale to [0..1] range, emphasize pick edge

                // Crossfade between sustain gain and attack gain based on t
                float g = sustainGain + t * (attackGain - sustainGain);
                float y = x * g;

                if (clipGuard)
                {
                    // gentle limiter as we approach 0 dBFS
                    constexpr float K = 3.5f; // softness
                    y = std::tanh(K * y) / std::tanh(K);
                }

                // Output fully-wet here; host block handles dry/wet mix
                data[i] = y;
            }

            envFast[ch] = eFast;
            envSlow[ch] = eSlow;
        }
    }

private:
    void setTimes(float attackTime, float sustainTime)
    {
        attackTau = juce::jmax(1e-4f, attackTime);
        sustainTau = juce::jmax(1e-3f, sustainTime);
        coeffFast = 1.0f - std::exp(-1.0f / (sampleRate * attackTau));
        coeffSlow = 1.0f - std::exp(-1.0f / (sampleRate * sustainTau));
    }

    float sampleRate { 44100.0f };
    float attackTau { 0.001f }, sustainTau { 0.050f };
    float coeffFast { 0.0f }, coeffSlow { 0.0f };

    float envFast[2] { 0.0f, 0.0f };
    float envSlow[2] { 0.0f, 0.0f };

    float attackGain { 1.0f };
    float sustainGain { 1.0f };
    float mix { 1.0f };
    bool  clipGuard { false };
};
