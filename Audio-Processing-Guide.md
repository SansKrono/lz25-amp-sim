# Audio Processing Guide
Detailed breakdown of the real-time processing chain used by the LZ25 Amp Simulator.

## Prerequisites
- Basic DSP concepts (filters, shelving/peaks, waveshaping)
- Familiarity with JUCE’s AudioProcessor lifecycle and juce::dsp utilities
- Understanding of real-time constraints (no allocation, no locks, constant-time behavior)

## Overview
Processing is centralized in LZ25AudioProcessor::processBlock(). The core amp tone is implemented with a juce::dsp::ProcessorChain and a custom waveshaper. Pre-FX (pitch, gate, transient shaper, compressor) run before the amp chain. Optional IR convolution runs after the amp chain. Gain staging occurs at the input and the output, and an RMS meter is updated smoothly.

## Step-by-Step: processBlock()
1) Housekeeping and channel clearing
- Clear any output channels that exceed the number of input channels.

```cpp
for (auto i = inputChannels; i < outputChannels; ++i)
    buffer.clear (i, 0, buffer.getNumSamples());
```

2) Update ProcessorChain parameters from APVTS
- PreGain from PREGAIN
- Resonance ladder LPF cutoff from RESONANCE (scaled to Hz)
- Bottom End, Treble, Mid, Presence IIR coefficients recalculated from BASS/TREBLE/MID/PRESENCE

```cpp
auto& preGain = processorChain.get<preGainIndex>();
preGain.setGainDecibels (*apvts.getRawParameterValue ("PREGAIN"));

auto& resonanceFilter = processorChain.get<resonanceIndex>();
resonanceFilter.setCutoffFrequencyHz (*apvts.getRawParameterValue ("RESONANCE") * 1000.0f);

auto& bottomEndFilter = processorChain.get<bottomEndIndex>();
*bottomEndFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
    getSampleRate(), 400.0f, 0.4f, *apvts.getRawParameterValue ("BASS"));
// ... similar for Treble, Mid, Presence
```

3) Input stage
- Apply INPUT gain via juce::dsp::Gain

```cpp
_input.setGainDecibels (*apvts.getRawParameterValue ("INPUT"));
juce::dsp::AudioBlock<float> inputGainBlock (buffer);
juce::dsp::ProcessContextReplacing<float> inputContextReplacing (inputGainBlock);
_input.process (inputContextReplacing);
```

4) Pre-FX (Pitch/Dynamics panel)
- Conditioned by boolean PITCH_DYN_PANEL_ENABLE
- Each module has an ENABLE parameter and an optional MIX parameter
- Pitch and Transient Shaper use a local dry copy to perform dry/wet mixing

Processing order:
- Pitch (PITCH_ENABLE, PITCH_MIX)
- SmartGate (SMARTGATE_ENABLE)
- TransientShaper (TRANSIENT_ENABLE, TRANSIENT_MIX, optional soft limiter)
- MxrDynaComp (COMP_ENABLE)

```cpp
if (*apvts.getRawParameterValue ("PITCH_DYN_PANEL_ENABLE") > 0.5f) {
    // Pitch (mix)
    // SmartGate (in-place)
    // Transient Shaper (mix)
    // Compressor (in-place)
}
```

5) Core Amp Chain
- Implemented in a ProcessorChain with indices:
  - resonanceIndex (LadderFilter LPF12)
  - bottomEndIndex (low shelf)
  - highPassFilterIndex (HPF ~700 Hz pre-distortion)
  - preGainIndex (gain in dB)
  - waveshaperIndex (custom function modernMetalClip)
  - presenceIndex (high shelf)
  - bassIndex, midIndex, trebleIndex (post-shape EQ peaks)

The processorChain is initialized in updateProcessorChain(), which:
- Ensures valid sampleRate, then assigns HPF, presence shelf, mid scoop, and default EQs
- Programs the waveshaper with modernMetalClip combining tanh soft clip and limited hard clip

```cpp
auto modernMetalClip = [](float x)
{
    float softClipped = std::tanh (5.0f * x);
    float limit = 0.8f;
    return juce::jlimit (-limit, limit, softClipped * 1.5f);
};
processorChain.get<waveshaperIndex>().functionToUse = modernMetalClip;
```

6) Cab IR Convolution (optional)
- IR_ENABLE parameter toggles whether the convolver (juce::dsp::Convolution) is used
- IR files can be loaded individually or from a folder; navigation via prev/next controls in the GUI

7) Output stage and Metering
- POSTGAIN applied to set final level
- A LinearSmoothedValue is used to smooth the RMS meter towards the measured level

## Buffer Management & Threading Considerations
- No dynamic allocations in processBlock
- Dry/wet mixes create a stack-local AudioBuffer copy for that block only; avoid per-sample allocations
- Parameter fetches use APVTS’s atomic float access (getRawParameterValue->load/read)
- Convolution IR loading happens off the audio thread (triggered by UI), but processing the loaded IR is real-time safe once loaded

## Parameter Smoothing & Automation
- Gains (INPUT/PREGAIN/POSTGAIN) are applied sample-accurately via juce::dsp::Gain, which internally smooths changes to avoid clicks
- EQ coefficient updates are per-block; the chosen filter structures are stable for these update rates
- The RMS meter uses LinearSmoothedValue to provide a visually pleasing decay

## DSP Algorithms Notes
- Waveshaper: mix of soft clip (tanh) and hard limiting for tight modern metal response
- High-Pass (pre): 700 Hz IIR HPF to tighten lows pre-distortion (overdrive-like boost)
- Tone Stack: post-distortion IIR shelves/peaks; Presence is a high shelf; Bass/Mid/Treble are tuneable peaks/shelves
- LadderFilter for Resonance (LPF12) to emulate power-amp/cab low resonance
- Pre-FX:
  - Pitch: dual-window crossfading delay with Hann windows
  - SmartGate: adaptive close time based on envelope decay slope; Djent mode accelerates clamp
  - TransientShaper: dual envelope (fast/slow) difference to emphasize pick attack
  - MxrDynaComp: envelope/OTA-inspired compression with soft knee
  - TubeScreamer808 / BigCheeseFuzz: circuit-inspired stages with filters and asymmetric/nonlinear elements

## Performance Tips
- Avoid calling updateProcessorChain() per-sample; it’s called at construction and after sampleRate changes; per-block we update only what’s needed.
- Keep any heavy UI inspection facilities (melatonin_inspector) disabled in release builds.
- When adding new filters, prefer juce::dsp::IIR::Coefficients::makeX helpers and keep Q/gains in sane ranges to avoid denormals or instability.

## Implementation Examples
- See source/PluginProcessor.cpp: processBlock() and updateProcessorChain()
- See source/effects/* for individual effect algorithm structures

## Common Issues & Solutions
- Silent output after loading IR: ensure IR_ENABLE is on and an IR file is successfully loaded.
- Harsh highs/fizz: reduce PRESENCE or TREBLE; consider lowering PREGAIN or raising INPUT to change drive into waveshaper.
- Muddy lows: increase pre HPF frequency (in code) or reduce BASS/MID; enable TubeScreamer for additional tightening.

## Next Steps
- Read Architecture-Overview.md to understand how components fit together.
- Use Customization-Guide.md to add new DSP modules or parameters.

## References
- JUCE DSP module: https://docs.juce.com/master/group__juce__dsp.html
- AudioProcessor: https://docs.juce.com/master/classAudioProcessor.html
