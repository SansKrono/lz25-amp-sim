#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "effects/EffectPedal.h"
#include "effects/Pitch.h"
#include "effects/MxrDynaComp.h"
#include "effects/TubeScreamer808.h"
#include "effects/BigCheeseFuzz.h"
#include "effects/SmartGate.h"
#include "effects/TransientShaper.h"

class PreFXPanel : public juce::Component
{
public:
    PreFXPanel(juce::AudioProcessorValueTreeState& apvts);
    ~PreFXPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    
    // Effect instances for GUI (Pre-Amp drives)
    std::unique_ptr<TubeScreamer808> tubeScreamer;
    std::unique_ptr<BigCheeseFuzz> bigCheese;
    
    // Effect pedal components
    std::unique_ptr<EffectPedalComponent> tubeScreamerComponent;
    std::unique_ptr<EffectPedalComponent> bigCheeseComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreFXPanel)
};