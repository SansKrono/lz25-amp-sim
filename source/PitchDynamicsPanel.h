#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "effects/EffectPedal.h"
#include "effects/Pitch.h"
#include "effects/MxrDynaComp.h"
#include "effects/SmartGate.h"
#include "effects/TransientShaper.h"

class PitchDynamicsPanel : public juce::Component
{
public:
    explicit PitchDynamicsPanel(juce::AudioProcessorValueTreeState& apvts);
    ~PitchDynamicsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Effect instances for GUI (Pitch/Dynamics tools)
    std::unique_ptr<Pitch> pitch;
    std::unique_ptr<SmartGate> smartGate;
    std::unique_ptr<TransientShaper> transientShaper;
    std::unique_ptr<MxrDynaComp> compressor;

    // Effect pedal components
    std::unique_ptr<EffectPedalComponent> pitchComponent;
    std::unique_ptr<EffectPedalComponent> smartGateComponent;
    std::unique_ptr<EffectPedalComponent> transientShaperComponent;
    std::unique_ptr<EffectPedalComponent> compressorComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDynamicsPanel)
};