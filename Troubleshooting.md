# Troubleshooting
Common issues and practical fixes when building, running, or extending the LZ25 Amp Simulator.

## Prerequisites
- Basic CMake/JUCE environment set up
- Understanding of plugin formats (VST3/AU/Standalone)

## Build & Setup Issues

### 1) JUCE not found / submodule missing
- Symptom: Compilation fails with missing JUCE headers or modules.
- Fix:
  - Ensure submodule is checked out: `git submodule update --init --recursive`
  - Open the project with CLion’s existing CMake profile (do not create a new build dir).

### 2) Wrong CMake configuration / multiple build dirs
- Symptom: Confusing incremental builds, stale binaries.
- Fix:
  - Use the active CLion CMake profile only (see project instructions). Avoid creating extra build folders.

### 3) Plugin target builds but DAW can’t find it
- Symptom: VST3/AU not visible in DAW.
- Fix:
  - For VST3: ensure the .vst3 lands in the standard directory (JUCE CMake installs to a build artefacts folder). Point your DAW to `LZ25_artefacts/*/VST3/`.
  - For AU (macOS): run `auval -a` or `auval -v aufx LZ25 ...` to validate. On macOS 13+, unsigned AUs may be blocked—prefer VST3 for dev.
  - Try standalone target first (LZ25_Standalone) to validate audio path.

## Runtime Audio Issues

### 4) No sound / silence
- Check input routing: In a mono source scenario, INPUTSRC parameter selects Left/Right.
- Ensure PITCH_DYN_PANEL_ENABLE/other enables are ON as needed.
- IR: If IR_ENABLE is on but no IR is loaded, you’ll still hear the direct amp chain; silence likely means extreme gain staging—verify INPUT/PREGAIN/POSTGAIN.

### 5) Loud/harsh/fizzy output
- Reduce PRESENCE and TREBLE, or the waveshaper drive (lower PREGAIN).
- Use TubeScreamer with lower Tone as a pre-filter, or increase the pre HPF frequency in code.

### 6) Muddy/boomy lows
- Increase pre HPF frequency (code: updateProcessorChain -> makeHighPass).
- Lower BASS/MID, increase PRESENCE a bit, enable SmartGate for tighter stops.

### 7) Crackles/clicks when automating
- Gains are smoothed; EQ coeffs update per block. If your host has tiny buffer sizes, excessive automation can stress CPU.
- Consider smoothing parameter changes you add (LinearSmoothedValue) and avoid updating expensive coefficients per-sample.

### 8) IR loading problems
- Symptom: prev/next does nothing.
- Fix:
  - Verify IR folder contains supported formats (.wav/.aif/.aiff/.flac).
  - Folder listing is non-recursive—place IRs directly in the chosen directory.
  - Name sorting is case-insensitive; ensure current file index stays valid when replacing files.

### 9) Crashes when switching IRs with empty list
- Guard exists (irFiles.isEmpty()). If you extend IR browsing, recheck your bounds logic and currentIRIndex updates.

## Code & Extensibility Issues

### 10) Sample rate is zero in constructor
- updateProcessorChain() guards on getSampleRate() <= 0 to avoid creating IIR coeffs too early. Don’t rely on sampleRate in the constructor; use prepareToPlay.

### 11) Parameter not moving the UI
- Ensure the parameter ID in APVTS matches the attachment ID exactly.
- For pedals, getParameters() must expose the same IDs you registered in APVTS.

### 12) Parameter range feels wrong
- Use NormalisableRange with step/skew if needed. For decibel-style sliders, present values in dB and convert to linear in DSP.

### 13) Performance regressions after adding DSP
- Avoid heap allocations in processBlock. Preallocate in prepare().
- Prefer juce::dsp primitives and limit coefficient recalculation.
- Use Release builds for profiling; Debug builds are slower for JUCE DSP.

### 14) Denormals causing CPU spikes
- JUCE enables ScopedNoDenormals; keep your filters stable and add DC blockers where needed.

### 15) UI overlapping/odd layout
- Panels compute responsive bounds; if you add more knobs, verify layout math in resized().
- For pedals, EffectPedalComponent auto-builds controls—confirm knob counts and spacing.

## Quick Diagnostic Checklist
- Standalone app works? If yes, DAW routing is the likely issue.
- Does toggling IR_ENABLE change tone? If not, verify IR load and that the IR is stereo-compatible.
- Are enables (PITCH_ENABLE, SMARTGATE_ENABLE, etc.) set as expected?
- Is INPUTSRC selecting the correct channel when using a mono input?

## Getting Help
- Use melatonin_inspector (Inspect the UI button) during development to visualize component bounds and debug UI layout.
- Cross-reference: Architecture-Overview.md, Audio-Processing-Guide.md, Implementation-Patterns.md, Customization-Guide.md.
