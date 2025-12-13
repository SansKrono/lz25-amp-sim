# Customization Guide
How to modify and extend the LZ25 Amp Simulator safely as a JUCE/C++ beginner.

## Prerequisites
- CMake + your IDE (CLion recommended) set up for JUCE builds
- C++ basics and familiarity with the project’s Architecture Overview
- JUCE’s AudioProcessor/Component and APVTS concepts

## Overview
This guide walks you through common customization tasks:
- Adding a new parameter to the amp
- Exposing that parameter in the UI
- Creating a new effect pedal
- Adding features to panels or the IR system

Each section includes high-level steps and small code snippets to get you started.

## Quick Start: Where to Look
- Processor DSP & parameters: source/PluginProcessor.{h,cpp}
- GUI editor and layout: source/PluginEditor.{h,cpp}
- Panels: source/*Panel.{h,cpp}
- Pedals (effects): source/effects/*

## Add a New Parameter (APVTS)
1) Define the parameter in createParameterLayout()
```cpp
// In LZ25AudioProcessor::createParameterLayout()
parameters.push_back (std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"NEWPARAM", 1}, "New Param",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
```

2) Read the parameter in processBlock() (or updateProcessorChain())
```cpp
const float newParam = *apvts.getRawParameterValue("NEWPARAM");
// Use it to update filter coeffs, gains, etc.
```

3) Add UI control (e.g., a slider) and attachment
- If it’s an amp control: edit source/AmpPanel.cpp and add a slider + SliderAttachment.
- If it’s a pedal control: add it to the pedal’s getParameters() and the UI appears automatically via EffectPedalComponent.

```cpp
// AmpPanel example (member)
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentNewParam;

// In constructor
addAndMakeVisible(_sliderNew);
_sliderAttachmentNewParam = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "NEWPARAM", _sliderNew);
```

## Create a New Effect Pedal
1) Subclass EffectPedal in source/effects/
```cpp
class MyCoolPedal : public EffectPedal {
public:
    MyCoolPedal() : EffectPedal("MY PEDAL", "MYCOOL", EnclosureType::Enclosure1590B) {}
    void prepare(const juce::dsp::ProcessSpec& spec) override { /* init DSP */ }
    void reset() override { /* reset state */ }
    void process(juce::dsp::AudioBlock<float>& block) override { /* your DSP */ }
    std::vector<Parameter> getParameters() const override {
        return { Parameter{"MYCOOL_GAIN", "GAIN", 0.0f, 1.0f, 0.5f} };
    }
    void updateParameters(juce::AudioProcessorValueTreeState& apvts) override {
        gain = apvts.getRawParameterValue("MYCOOL_GAIN")->load();
    }
private:
    float gain = 0.5f;
};
```

2) Add APVTS parameters to createParameterLayout()
```cpp
parameters.push_back (std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"MYCOOL_GAIN", 1}, "MyCool Gain", 0.0f, 1.0f, 0.5f));
parameters.push_back (std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{"MYCOOL_ENABLE", 1}, "MyCool Enable", false));
parameters.push_back (std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"MYCOOL_MIX", 1}, "MyCool Mix", 0.0f, 1.0f, 1.0f));
```

3) Add it to a panel
- For pre-amp drives: PreFXPanel.
- For pitch/dynamics: PitchDynamicsPanel.
- For future post-fx: PostFXPanel.

```cpp
myCoolPedal = std::make_unique<MyCoolPedal>();
myCoolPedalComponent = std::make_unique<EffectPedalComponent>(*myCoolPedal, apvts);
addAndMakeVisible(*myCoolPedalComponent);
```

4) Wire it into processing (in LZ25AudioProcessor::processBlock)
- Prepare in prepareToPlay()
- In processBlock, call pedal->updateParameters(apvts)
- Process with an in-place or wet/dry strategy like the existing pedals

```cpp
if (myCoolPedal) myCoolPedal->setEnabled(*apvts.getRawParameterValue("MYCOOL_ENABLE") > 0.5f);
if (myCoolPedal && myCoolPedal->isEnabled()) {
    myCoolPedal->updateParameters(apvts);
    juce::AudioBuffer<float> dry; dry.makeCopyOf(buffer);
    juce::dsp::AudioBlock<float> block(buffer);
    myCoolPedal->process(block);
    const float mix = *apvts.getRawParameterValue("MYCOOL_MIX");
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* wet = buffer.getWritePointer(ch);
        auto* d = dry.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            wet[i] = mix * wet[i] + (1.0f - mix) * d[i];
    }
}
```

## Modify the Core Amp Chain
- The ProcessorChain is set up in updateProcessorChain(). You can:
  - Change the pre-distortion HPF frequency
  - Adjust the modernMetalClip function’s shape/limit
  - Retune the mid scoop/presence shelf defaults

```cpp
// Pre HPF
*processorChain.get<highPassFilterIndex>().state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 700.0f);

// Waveshaper
processorChain.get<waveshaperIndex>().functionToUse = [](float x){ return std::tanh(6.0f * x); };
```

## Extend IR Handling
- Add default IR folder on first run using juce::File user directories
- Validate and show messages in the editor if IR load fails
- Consider adding IR mix or stereo/mono options

## Add a New Panel
- Create a new Component subclass in source/, instantiate it in PluginEditor, and add a tab with addTab().
- Expose an enable toggle parameter using APVTS (e.g., NEWPANEL_ENABLE) and follow the existing double-click-to-toggle pattern.

## Version Control Tips
- Keep changes modular (separate commits for DSP vs UI vs parameters)
- Update documentation files alongside code changes
- Use CLion/CTest targets (Tests) to keep regression coverage as you extend

## Next Steps
- Read Implementation-Patterns.md for patterns to emulate
- See Audio-Processing-Guide.md for performance guidelines
- Consult File-Structure-Reference.md to place your code correctly
