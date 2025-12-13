# Architecture Overview
Beginner-friendly overview of the LZ25 Amp Simulator plugin’s structure, lifecycle, and data flow.

## Prerequisites
- C++ basics (classes, RAII, smart pointers)
- Intro to real-time audio constraints (no locks/allocations in audio thread)
- JUCE concepts: AudioProcessor, AudioProcessorEditor, AudioProcessorValueTreeState (APVTS), Components, dsp:: classes

## Overview
The LZ25 Amp Simulator is a JUCE-based audio effect plugin (VST3/AU) and standalone app. It models an amp preamp, a modern-metal waveshaper tone core, a tone stack, and integrates pre-/post-effects and an IR loader for cabinet simulation.

- Runtime roles:
  - LZ25AudioProcessor: real-time DSP, parameters, state, IR management
  - LZ25AudioProcessorEditor: GUI, tabbed panels, parameter bindings, tooltips
  - Panels: PitchDynamicsPanel, PreFXPanel, AmpPanel, PostFXPanel (UI composition)
  - Effects (pre-fx): Pitch, SmartGate, TransientShaper, MxrDynaComp, TubeScreamer808, BigCheeseFuzz
  - DSP backbone: juce::dsp::ProcessorChain with filters, gains, and waveshaper
  - State: AudioProcessorValueTreeState (APVTS) + ValueTree for IR persistence

## High-Level Signal Flow
```
Audio In
  └─> Input Gain ("INPUT")
       └─> PITCH/DYNAMICS Panel (if enabled)
             ├─ Pitch (mixable)
             ├─ SmartGate (adaptive noise gate)
             ├─ Transient Shaper (attack emphasis, mixable)
             └─ MXR Dyna Comp (compressor, mixable)
       └─> PRE-AMP chain (ProcessorChain)
             ├─ PreGain ("PREGAIN")
             ├─ High-Pass Filter (~700 Hz, pre-distortion "boost")
             ├─ Waveshaper (modernMetalClip soft+hard hybrid)
             ├─ Resonance (LadderFilter LPF12, cutoff via "RESONANCE")
             ├─ Bottom End (low shelf)
             ├─ Mid (peak around 500 Hz)
             ├─ Treble (peak around 5 kHz)
             └─ Presence (high shelf ~4.5 kHz)
       └─> IR Convolution (if IR_ENABLE)
       └─> Post Gain ("POSTGAIN")
       └─> Meter (RMS)
  └─> Audio Out
```

GUI Structure
```
LZ25AudioProcessorEditor
  ├─ Top bar: IR controls (prev/next/name), Load IR/Folder, IR Enable toggle, Instant Tooltip toggle, Inspector button
  └─ TabbedComponent (TabsAtTop)
      ├─ PITCH/DYNAMICS -> Pitch, Gate, Transient, Compressor (pedal components)
      ├─ PRE-AMP       -> Drive pedals (TubeScreamer808, BigCheese)
      ├─ AMP           -> Core amp controls (Input, PreGain, EQ, Presence, PostGain, Resonance, etc.)
      └─ POST FX       -> Placeholder for future modules
```

## AudioProcessor Lifecycle
- Construction: creates APVTS parameters (createParameterLayout), instantiates pre-fx processors, sets default IR state, primes ProcessorChain via updateProcessorChain().
- prepareToPlay: configures ProcessSpec (sampleRate, block size, channels), prepares pre-fx, resets meters and chain.
- processBlock: runs per audio block; updates chain coefficients/gains from parameters, processes pre-fx (with dry/wet blends), runs core amp chain, optional IR convolution, output level/meter.
- releaseResources/reset: free/reset per-session state; no dynamic allocations in the audio thread.
- State: getStateInformation/setStateInformation serialize APVTS; a ValueTree holds IR file and root dirs.

## Parameter & State Management
- AudioProcessorValueTreeState (apvts): central registry of parameters (gain, EQ, pre-fx, panel enables, IR_ENABLE, etc.).
- GUI attachments: SliderAttachment/ButtonAttachment bind UI widgets to APVTS.
- ValueTree: stores IR path (file1) and root directory for persistence alongside APVTS.

## Memory Management Patterns
- DSP effects (Pitch, SmartGate, TransientShaper, MxrDynaComp, TubeScreamer808, BigCheeseFuzz) are owned via std::unique_ptr in the processor and/or panels; lifetime matches processor/editor.
- juce::dsp::ProcessorChain elements are value members (no per-block heap allocation).
- Audio blocks use juce::dsp::AudioBlock wrappers around existing buffers (no copies unless mixing wet/dry where a stack-local copy is used and reused per block scope).

## Threading & Real-Time Safety
- processBlock avoids locks, file I/O, and heap allocations.
- updateProcessorChain defers coefficient creation until sampleRate > 0 to avoid invalid states during construction.
- GUI runs on the message thread; APVTS ensures thread-safe parameter access in audio thread via atomic floats.

## Modules and Dependencies
- JUCE modules (audio_basics, processors, dsp, gui, etc.)
- melatonin_inspector (optional dev-time UI inspector)
- Convolution via juce::dsp::Convolution for IRs

## Key Classes at a Glance
- LZ25AudioProcessor: processBlock, prepareToPlay, IR management, APVTS layout.
- LZ25AudioProcessorEditor: layout, tabbed panels, tooltips, inspector, IR controls.
- Panels: PitchDynamicsPanel, PreFXPanel, AmpPanel, PostFXPanel (composite UI for effect pedals and amp controls).
- Effects: Pitch (dual-window shifter), SmartGate (adaptive gate), TransientShaper, MxrDynaComp, TubeScreamer808, BigCheeseFuzz.

## Next Steps
- Dive into Audio-Processing-Guide.md for the detailed processing chain and algorithms.
- See File-Structure-Reference.md for a file-by-file map.
- Check Implementation-Patterns.md for JUCE idioms used here.
- Use Customization-Guide.md to add your own parameters or effects.
