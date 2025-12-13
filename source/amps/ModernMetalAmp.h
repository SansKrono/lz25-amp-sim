#pragma once

#include "Amplifier.h"
#include "AmpComponents.h"

/**
 * Modern metal high-gain amplifier implementation.
 *
 * This reproduces the existing LZ25 amp chain behaviour exactly:
 * Order: Resonance -> BottomEnd -> HPF700 -> PreGain -> Waveshaper -> Presence -> Bass -> Mid -> Treble
 */
class ModernMetalAmp : public Amplifier
{
public:
    ModernMetalAmp() = default;
    ~ModernMetalAmp() override = default;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset () override;
    void updateParameters (juce::AudioProcessorValueTreeState& apvts) override;

    juce::String getName () const override { return "Modern Metal"; }
    juce::String getDescription () const override { return "High-gain modern metal amp"; }

    void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) override;

private:
    PreDistortionFilter preFilter; // HPF 700 Hz
    Waveshaper distortion;         // modern metal clipper with pre-gain
    ToneStack toneStack;           // resonance + EQ stages

    void initializeModernMetalVoicing();
    static float modernMetalClipFunction (float x);

    // Cached parameter pointers for RT safety
    std::atomic<float>* preGainParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr; // in kHz (compat with existing)
    std::atomic<float>* presenceParam = nullptr;  // 0.5..1.5 multiplier
    std::atomic<float>* trebleParam = nullptr;    // 0..2 gain
    std::atomic<float>* midParam = nullptr;       // 0..2 gain
    std::atomic<float>* bassParam = nullptr;      // 0..2 gain

    // Parameter IDs (must match existing for backward compatibility)
    static inline const juce::String preGainID     = "PREGAIN";
    static inline const juce::String resonanceID   = "RESONANCE";
    static inline const juce::String presenceID    = "PRESENCE";
    static inline const juce::String trebleID      = "TREBLE";
    static inline const juce::String midID         = "MID";
    static inline const juce::String bassID        = "BASS";

    // Unused legacy shaper params kept for preset compatibility
    static inline const juce::String driveID       = "DRIVE";
    static inline const juce::String asymID        = "ASYMMETRY";
    static inline const juce::String harmID        = "HARMONIC_CHARACTER";
    static inline const juce::String shapeID       = "SATURATION_SHAPE";
    static inline const juce::String sagID         = "TUBE_SAG";
};
