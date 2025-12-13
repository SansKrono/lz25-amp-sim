#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
 * Abstract interface for swappable amplifier models.
 *
 * Implementations must be real-time safe: avoid allocations in process(),
 * use prepare()/reset() for resource management, and updateParameters() for parameter reads.
 */
class Amplifier
{
public:
    virtual ~Amplifier() = default;

    // Core methods
    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void process (juce::dsp::AudioBlock<float>& block) = 0;
    virtual void reset () = 0;
    virtual void updateParameters (juce::AudioProcessorValueTreeState& apvts) = 0;

    // Metadata
    virtual juce::String getName () const = 0;
    virtual juce::String getDescription () const = 0;

    // Parameter creation for APVTS
    virtual void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) = 0;

protected:
    juce::dsp::ProcessSpec spec {};
    bool isBypassed = false;
};
