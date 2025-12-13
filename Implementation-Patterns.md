# Implementation Patterns
JUCE-specific and general design patterns employed in the LZ25 Amp Simulator project.

## Prerequisites
- Familiarity with JUCE widgets, AudioProcessor, and APVTS
- Basic knowledge of RAII and C++17 smart pointers

## Overview
This document catalogs patterns used throughout the codebase to help you understand and extend the plugin safely, especially under real-time audio constraints.

## JUCE Patterns

### AudioProcessorValueTreeState (APVTS)
- Central parameter store: constructed in the processor with createParameterLayout().
- Thread-safe parameter reads in audio thread via getRawParameterValue().
- GUI bindings with SliderAttachment/ButtonAttachment for automatic UI sync.

Example parameter layout fragment:
```cpp
std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
params.push_back (std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{"PREGAIN", 1}, "PreGain", -12.0f, 36.0f, 12.0f));
return { params.begin(), params.end() };
```

### ProcessorChain Indexing
- The amp’s core DSP uses juce::dsp::ProcessorChain with compile-time indices that are mirrored in an enum.
- Pattern: define an enum { resonanceIndex, bottomEndIndex, highPassFilterIndex, preGainIndex, waveshaperIndex, presenceIndex, bassIndex, midIndex, trebleIndex } and use get<index>() for clarity.

```cpp
auto& preGain = processorChain.get<preGainIndex>();
preGain.setGainDecibels(db);
```

### Component Composition
- Editor composes panels (PitchDynamicsPanel, PreFXPanel, AmpPanel, PostFXPanel) with a TabbedComponent.
- Custom PanelTabbedComponent intercepts mouseDoubleClick to toggle panel enable parameters, demonstrating event routing pattern.

### LookAndFeel Customization
- AmpPanel and pedal components use custom LookAndFeel for rotary controls and tab headers.
- Encapsulate drawing code in LookAndFeel subclasses to centralize visual style.

### ValueTree for Non-Parameter State
- A lightweight juce::ValueTree stores IR paths (file1/root) separate from APVTS parameters.
- Pattern: keep file system paths and other non-automatable data out of parameter set.

### BinaryData Asset Embedding
- Images and SVG assets compiled into BinaryData and loaded at runtime (e.g., amp faceplate).
- Pattern: check pointers/sizes for safety before loadFrom.

## Real-Time Safe DSP Patterns
- Avoid allocations/locks in processBlock.
- Precompute filter coefficients in prepareToPlay or in an update function that guards against sr <= 0.
- Use juce::dsp::Gain and AudioBlock for zero-copy gains and processing contexts.
- When wet/dry mixing, copy buffer once per block and do a simple linear mix, not per-sample allocations.

## State and Lifecycle Patterns
- Construct effects as std::unique_ptr and prepare them with juce::dsp::ProcessSpec in prepareToPlay.
- Reset effect internal states in reset() and when sample rate changes.
- Use LinearSmoothedValue for UI meters and other smoothed telemetry.

## GUI Patterns
- Pedal UIs are created by EffectPedalComponent which dynamically builds sliders/labels from the pedal’s parameter descriptors.
- Pattern: data-driven UI creation; adding new pedal parameters automatically yields new sliders/attachments.
- Tooltips: a top-level TooltipWindow with a toggle changes delay (0 vs 2s) for instant vs delayed hints.

## Error Handling & Safety
- Use jlimit/jmax/jmin extensively for range clamping.
- On tab toggles and IR navigation, guard against empty lists (irFiles.isEmpty()).
- In listIRFilesInDirectory(), filter extensions and sort case-insensitively.

## Extensibility Patterns
- Add parameters in APVTS layout; expose them via panels/sliders through attachments.
- Encapsulate new DSP modules as EffectPedal subclasses; leverage EffectPedalComponent for UI.
- Keep IR handling modular (setIRFolder/loadIRFile/loadIRAtIndex/prevIR/nextIR) to swap or extend easily.

## Implementation Examples
- ProcessorChain setup: source/PluginProcessor.cpp::updateProcessorChain()
- Panel composition and tab toggling: source/PluginEditor.h/.cpp
- Pedal UI builder: source/effects/EffectPedal.cpp

## Next Steps
- See Customization-Guide.md for hands-on instructions to add parameters and effects.
- See Audio-Processing-Guide.md for DSP details and performance tips.
