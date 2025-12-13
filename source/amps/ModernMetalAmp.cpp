#include "ModernMetalAmp.h"

void ModernMetalAmp::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    preFilter.prepare (spec);
    preFilter.setHighPassCutoff (700.0f);

    distortion.prepare (spec);
    distortion.setFunction ([] (float x) { return modernMetalClipFunction (x); });

    toneStack.prepare (spec);
    initializeModernMetalVoicing();
}

void ModernMetalAmp::process (juce::dsp::AudioBlock<float>& block)
{
    if (isBypassed)
        return;

    // If parameters haven't been cached yet, act as a passthrough to avoid muting
    if (preGainParam == nullptr)
        return;

    // Pre stages
    toneStack.processPre (block); // Resonance + BottomEnd (pre-dist)
    preFilter.process (block);    // HPF 700 Hz

    // Drive
    const float preDB = preGainParam->load();
    distortion.setPreGain (preDB);
    distortion.process (block);

    // Post EQ
    toneStack.process (block);
}

void ModernMetalAmp::reset ()
{
    preFilter.reset();
    distortion.reset();
    toneStack.reset();
}

void ModernMetalAmp::updateParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // Cache parameter pointers on first call (RT safe afterwards)
    if (preGainParam == nullptr)
    {
        preGainParam   = apvts.getRawParameterValue (preGainID);
        resonanceParam = apvts.getRawParameterValue (resonanceID);
        presenceParam  = apvts.getRawParameterValue (presenceID);
        trebleParam    = apvts.getRawParameterValue (trebleID);
        midParam       = apvts.getRawParameterValue (midID);
        bassParam      = apvts.getRawParameterValue (bassID);
        // Legacy/unreferenced params intentionally not cached, but exist for presets.
    }

    // Update tone stack
    if (resonanceParam)
        toneStack.setResonance (*resonanceParam * 1000.0f); // kHz -> Hz, matches previous code

    if (bassParam)
        toneStack.setBass (*bassParam);

    if (trebleParam)
        toneStack.setTreble (*trebleParam);

    if (midParam)
    {
        // Match previous mid filter tuning at 500 Hz, Q 0.9
        toneStack.setMidFilter (500.0f, 0.9f);
        toneStack.setMid (*midParam);
    }

    if (presenceParam)
        toneStack.setPresence (*presenceParam);
}

void ModernMetalAmp::addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
{
    // These IDs/ranges must match existing PluginProcessor to preserve presets
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { preGainID,   1 }, "PreGain",   -12.0f, 36.0f, 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { resonanceID, 1 }, "Resonance", 1.0f,   10.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { presenceID,  1 }, "Presence",  0.5f,   1.5f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { trebleID,    1 }, "Treble",    0.0f,   2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { midID,       1 }, "Mid",       0.0f,   2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { bassID,      1 }, "Bass",      0.0f,   2.0f, 1.0f));

    // Legacy shaper params retained for preset compatibility
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { driveID, 1 }, "Drive",              0.1f, 4.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { asymID,  1 }, "Asymmetry",          0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { harmID,  1 }, "Harmonic Character", 0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { shapeID, 1 }, "Saturation Shape",   0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { sagID,   1 }, "Tube Sag",           0.0f, 1.0f, 0.0f));
}

void ModernMetalAmp::initializeModernMetalVoicing()
{
    // Presence shelves around 4.5 kHz, 6 dB base boost (scaled by Presence 0.5..1.5)
    toneStack.setPresenceFilter (4500.0f, 0.5f);

    // Treble & Bass peak defaults
    toneStack.setTrebleFilter (5000.0f, 0.6f);
    toneStack.setBassFilter (100.0f, 0.6f);

    // Pre bottom-end shelf ~400 Hz
    // Implemented via ToneStack::setBass which also updates bottomEnd low-shelf
    toneStack.setBass (1.0f);

    // Mid control center defaults (user control applied in update)
    toneStack.setMidFilter (500.0f, 0.9f);

    // Default resonance
    toneStack.setResonance (3000.0f);
}

float ModernMetalAmp::modernMetalClipFunction (float x)
{
    // Matches the lambda previously used in PluginProcessor::updateProcessorChain()
    const float softClipped = std::tanh (5.0f * x);
    const float limit = 0.8f;
    return juce::jlimit (-limit, limit, softClipped * 1.5f);
}
