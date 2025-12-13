#pragma once

#include <memory>
#include "Amplifier.h"

class AmpManager
{
public:
    enum class AmpModel
    {
        ModernMetal = 0,
        CleanJazz,      // Future
        BritishCrunch,  // Future
        VintageBlues    // Future
    };

    AmpManager();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::AudioBlock<float>& block);
    void reset ();
    void updateParameters (juce::AudioProcessorValueTreeState& apvts);

    void setCurrentAmp (AmpModel model);
    AmpModel getCurrentAmp () const { return currentModel; }
    Amplifier* getCurrentAmpInstance () { return currentAmp.get(); }

    // Get parameters for APVTS initialization
    void addParametersForAllAmps (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params);

    // Get list of available amp names for UI
    juce::StringArray getAvailableAmpNames () const;

private:
    std::unique_ptr<Amplifier> currentAmp;
    AmpModel currentModel { AmpModel::ModernMetal };

    std::unique_ptr<Amplifier> createAmp (AmpModel model);

    juce::dsp::ProcessSpec lastSpec {};

    // Safety dry buffer to restore signal if an amp processing bug mutes the buffer
    juce::AudioBuffer<float> dryBuffer;
};
