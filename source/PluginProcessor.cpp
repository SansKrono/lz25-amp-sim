#include "PluginProcessor.h"
#include "PluginEditor.h"

/**
 * This method processes audio data through the LZ25AudioProcessor.
 *
 * The method applies various effects and transformations to the input audio buffer,
 * modifies it as per the internal configuration, and outputs the processed audio.
 *
 * @return Processed audio data.
 */
LZ25AudioProcessor::LZ25AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
    #if !JucePlugin_IsMidiEffect
        #if !JucePlugin_IsSynth
              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        #endif
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
    #endif
              ),
      apvts (*this, nullptr, juce::Identifier ("PARAMETERS"), createParameterLayout()),
      valueTree ("Variables", {}, { { "Group", { { "name", "IR Vars" } }, { { "Parameter", { { "id", "file1" }, { "value", "/" } } }, { "Parameter", { { "id", "root" }, { "value", "/" } } } } } }),
      input (true)
{
    apvts.state.addListener (this);

    // Initialize pre-fx effects
    pitch = std::make_unique<Pitch>();
    smartGate = std::make_unique<SmartGate>();
    compressor = std::make_unique<MxrDynaComp>();
    transientShaper = std::make_unique<TransientShaper>();
    tubeScreamer = std::make_unique<TubeScreamer808>();
    bigCheese = std::make_unique<BigCheeseFuzz>();

    // Force IR to be enabled on startup regardless of any previous saved state
    if (auto* irEnableParam = apvts.getParameter ("IR_ENABLE"))
        irEnableParam->setValueNotifyingHost (1.0f);

    // Initial setup for the chain. Coefficients will be set in prepareToPlay.
    updateProcessorChain();
    
    // Initialize new amp components
    masterGainSmooth.reset (getSampleRate() > 0 ? getSampleRate() : 44100.0, 0.05);
}
#endif

LZ25AudioProcessor::~LZ25AudioProcessor() = default;

/**
 * This method lists all impulse response (IR) files in the specified directory.
 *
 * @param directory The directory to search for IR files.
 * @return A list of file paths to the impulse response files found in the directory.
 */
static juce::Array<juce::File> listIRFilesInDirectory (const juce::File& dir)
{
    juce::Array<juce::File> files;
    if (!dir.isDirectory())
        return files;

    // **FIXED: Corrected file listing**
    auto children = dir.findChildFiles (juce::File::TypesOfFileToFind::findFiles, false);
    for (auto& f : children)
    {
        auto ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac")
            files.add (f);
    }

    struct FileNameComparator
    {
        int compareElements (const juce::File& a, const juce::File& b) const
        {
            return a.getFileName().compareIgnoreCase (b.getFileName());
        }
    } cmp;
    files.sort (cmp);
    return files;
}

/**
 * Processes and updates the internal processor chain configuration.
 *
 * This method adjusts the chain of audio processors by applying updates
 * or changes to the processing modules based on the current settings or parameters.
 *
 * @return void
 */
void LZ25AudioProcessor::updateProcessorChain()
{
    // Avoid creating IIR coefficients before a valid sample rate is available
    const double sr = getSampleRate();
    if (sr <= 0.0)
        return;

    // --- CORE DISTORTION FUNCTION FOR MODERN METAL ---
    // This custom waveshaper emulates the saturated, aggressive, and tightly compressed
    // feel of a high-gain preamp (like Fortin Nameless) by combining soft and hard clipping.
    auto modernMetalClip = [](float x)
    {
        // 1. Soft Clipping (Approximates complex tube saturation)
        float softClipped = std::tanh (5.0f * x);

        // 2. Harder Clipping/Limit (Gives the tone its "chug" and aggressive character)
        float limit = 0.8f;
        return juce::jlimit (-limit, limit, softClipped * 1.5f);
    };

    processorChain.get<waveshaperIndex>().functionToUse = modernMetalClip;

    // --- PRE-DISTORTION HIGH-PASS FILTER (THE "BOOST" EFFECT) ---
    // This high-pass filter is crucial for modern metal tone, acting like an overdrive
    // pedal with drive at zero to remove low-end mud *before* distortion.
    // Cutoff: 700.0Hz is a classic value for tightening low-tuned guitars.
    juce::dsp::IIR::Coefficients<float>::Ptr hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass((float) sr, 700.0f);
    *processorChain.get<highPassFilterIndex>().state = *hpfCoeffs;

    // --- MODERN TONE STACK TUNING ---
    // Tune the post-distortion EQ filters for a classic modern metal scoop/boost.

    // Mid Scoop: Aggressive cut around 400Hz to remove "mud" and "boxiness."
    juce::dsp::IIR::Coefficients<float>::Ptr midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter((float) sr,
                                                                  400.0f, // Center Frequency (The "mud" zone)
                                                                  1.0f,   // Q (Width of the cut)
                                                                  juce::Decibels::decibelsToGain(-10.0f)); // Gain (Deep cut)
    *processorChain.get<midIndex>().state = *midCoeffs;

    // Presence/Treble Boost: High-frequency boost for attack, pick clarity, and "fizz."
    juce::dsp::IIR::Coefficients<float>::Ptr presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf((float) sr,
                                                                         4500.0f, // Shelf Frequency
                                                                         0.5f,    // Q
                                                                         juce::Decibels::decibelsToGain(6.0f)); // Gain (Aggressive boost)
    *processorChain.get<presenceIndex>().state = *presenceCoeffs;

    // Set other fixed chain coefficients that were previously handled by placeholder functions
    // Resonance: Set to a reasonable default and controlled by parameter in processBlock
    auto& resonanceFilter = processorChain.get<resonanceIndex>();
    resonanceFilter.setMode (juce::dsp::LadderFilterMode::LPF12);
    resonanceFilter.setResonance (0.2f);
    resonanceFilter.setCutoffFrequencyHz (3000.0f); // Default value

    // Bottom End: Set to a reasonable default and controlled by parameter in processBlock
    auto& bottomEndFilter = processorChain.get<bottomEndIndex>();
    *bottomEndFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf ((float) sr, 400.0f, 0.4f, 0.4f);

    // Treble/Bass are typically set up as filters in the chain and adjusted by the EQ knobs.
    // They are left with flat defaults here, assuming the final EQ stage in processBlock handles the actual knob values.
    auto& trebleFilter = processorChain.get<trebleIndex>();
    *trebleFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter ((float) sr, 5000.0f, 0.6f, 1.0f);

    auto& bassFilter = processorChain.get<bassIndex>();
    *bassFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter ((float) sr, 100.0f, 0.6f, 1.0f);
}

//==============================================================================
// Gain Stage Configuration Implementation
//==============================================================================
GainStageConfig GainStageConfig::createConfig(GainStageType type)
{
    GainStageConfig cfg;

    switch (type)
    {
        case GainStageType::Fender_12AX7_Clean:
            // Modeled after classic blackface/silverface Fender clean channel.
            cfg.gainFactor = 1.2f;              // Low gain, aiming for max headroom
            cfg.saturationCurve = 0.8f;         // Very smooth, soft clipping
            cfg.asymmetry = 0.1f;               // Near symmetric clipping (Class A bias in early stages)
            cfg.compressionRatio = 1.5f;        // Minimal compression
            cfg.harmonicContent = 0.7f;         // Rich in even harmonics (warm, musical)
            cfg.couplingCapFreq = 90.0f;        // Low HPF corner, wide open low-end (full bass)
            cfg.gridStopperFreq = 12000.0f;     // High LPF corner, bright top end
            cfg.millerCapacitance = 0.3f;       // Low HF rolloff
            cfg.biasPoint = 0.0f;               // Near center bias
            cfg.biasShift = 0.1f;               // Minimal bias shift
            cfg.plateResistance = 1.0f;         // Neutral plate load
            cfg.sag = 0.1f;                     // Low sag (typically stiff power supply)
            break;

        case GainStageType::Marshall_ECC83_Crunch:
            // Modeled after classic JCM800/Plexi crunch stages.
            cfg.gainFactor = 2.2f;              // Healthy stage gain for overdrive
            cfg.saturationCurve = 1.7f;         // Crunchy, classic tube breakup
            cfg.asymmetry = 0.32f;              // Moderate asymmetry from biasing
            cfg.compressionRatio = 3.2f;        // Moderate compression from cascading stages
            cfg.harmonicContent = 0.35f;        // Skews toward odd for bite and edge
            cfg.couplingCapFreq = 170.0f;       // Tighter low end than Fender (0.0022uF style)
            cfg.gridStopperFreq = 11000.0f;     // Bright, lets more top end through vs modern high-gain
            cfg.millerCapacitance = 0.4f;       // Less HF rolloff
            cfg.biasPoint = -0.08f;             // Slightly cold, near-center for punch
            cfg.biasShift = 0.28f;              // Some grid-blocking feel on hard picking
            cfg.plateResistance = 1.15f;        // Higher plate resistance for gain (e.g., 100k)
            cfg.sag = 0.16f;                    // Moderate preamp sag/dynamic response
            break;

        case GainStageType::Mesa_12AX7_HighGain:
            // Modeled after Mesa Mark/Rectifier high-gain cascading stages.
            cfg.gainFactor = 2.9f;              // High intrinsic stage gain
            cfg.saturationCurve = 2.0f;         // Aggressive but avoids extreme square wave
            cfg.asymmetry = 0.38f;              // Colder bias -> more asymmetry/aggressiveness
            cfg.compressionRatio = 5.5f;        // High compression from many cascaded stages
            cfg.harmonicContent = 0.25f;        // Skews toward odd harmonics (modern sound)
            cfg.couplingCapFreq = 115.0f;       // Tighter bass than vintage amps
            cfg.gridStopperFreq = 5200.0f;      // Strong LPF to tame high-end fizz
            cfg.millerCapacitance = 0.7f;       // Extra HF rolloff
            cfg.biasPoint = -0.20f;             // Slightly cold bias
            cfg.biasShift = 0.5f;               // Pronounced grid-blocking under hit
            cfg.plateResistance = 1.45f;        // Less headroom, saturates earlier
            cfg.sag = 0.08f;                    // Stiff preamp supply (low sag)
            break;

        case GainStageType::Vox_EF86_Bright:
            // Modeled after a Vox AC30/AC15 Top Boost channel using the EF86 pentode.
            cfg.gainFactor = 1.5f;              // Medium gain, EF86 is powerful
            cfg.saturationCurve = 1.0f;         // Smooth clipping characteristic
            cfg.asymmetry = 0.15f;              // Moderate, warm asymmetry
            cfg.compressionRatio = 1.8f;        // Low-medium compression
            cfg.harmonicContent = 0.8f;         // Very warm, focused on even harmonics
            cfg.couplingCapFreq = 100.0f;       // Full, vintage low end
            cfg.gridStopperFreq = 15000.0f;     // Very bright/open top end
            cfg.millerCapacitance = 0.2f;       // Minimal HF rolloff
            cfg.biasPoint = 0.05f;              // Slightly hot bias
            cfg.biasShift = 0.2f;               // Moderate bias shift
            cfg.plateResistance = 0.9f;         // Neutral plate load
            cfg.sag = 0.25f;                    // Higher sag (AC-style rectifier/supply)
            break;

        case GainStageType::RCA_12AY7_Vintage:
            // Modeled after early/vintage Fender using the low-mu 12AY7 tube.
            cfg.gainFactor = 0.9f;              // Low gain (12AY7 mu is low)
            cfg.saturationCurve = 0.6f;         // Smooth, early distortion
            cfg.asymmetry = 0.05f;              // Very slight asymmetry
            cfg.compressionRatio = 1.3f;        // Minimal compression
            cfg.harmonicContent = 0.85f;        // Very warm, vintage-style tone
            cfg.couplingCapFreq = 120.0f;       // Moderate low-end filtering
            cfg.gridStopperFreq = 10000.0f;     // Open top end
            cfg.millerCapacitance = 0.4f;       // Moderate HF rolloff
            cfg.biasPoint = 0.0f;               // Center bias
            cfg.biasShift = 0.05f;              // Minimal bias shift
            cfg.plateResistance = 0.8f;         // Lower plate resistance
            cfg.sag = 0.3f;                     // High sag (tweed/vintage rectifier feel)
            break;

        case GainStageType::GE_12AU7_Jazz:
            // Modeled for ultra-clean/jazz applications (very low-mu tube).
            cfg.gainFactor = 0.7f;              // Lowest mu for maximum clean headroom
            cfg.saturationCurve = 0.5f;         // Ultra linear clipping
            cfg.asymmetry = 0.02f;              // Near perfect symmetry
            cfg.compressionRatio = 1.2f;        // Negligible compression
            cfg.harmonicContent = 0.9f;         // Ultra clean, almost purely even harmonics
            cfg.couplingCapFreq = 130.0f;       // Tighter low end (often for bass)
            cfg.gridStopperFreq = 16000.0f;     // Max open top end
            cfg.millerCapacitance = 0.1f;       // Minimal HF rolloff
            cfg.biasPoint = 0.02f;              // Center bias
            cfg.biasShift = 0.03f;              // Minimal bias shift
            cfg.plateResistance = 0.7f;         // Lowest plate resistance
            cfg.sag = 0.05f;                    // Very stiff supply (solid-state rectifier)
            break;

        case GainStageType::Mullard_ECC83_British:
            // A more generic "British" crunch, smoother than Marshall JCM800.
            cfg.gainFactor = 1.7f;              // Moderate gain
            cfg.saturationCurve = 1.3f;         // Smooth tube clipping
            cfg.asymmetry = 0.25f;              // Moderate asymmetry
            cfg.compressionRatio = 2.2f;        // Medium compression
            cfg.harmonicContent = 0.6f;         // Balanced even/odd harmonics (rich)
            cfg.couplingCapFreq = 85.0f;        // Full low end
            cfg.gridStopperFreq = 9000.0f;      // Moderate top end filtering
            cfg.millerCapacitance = 0.45f;      // Moderate HF rolloff
            cfg.biasPoint = -0.05f;             // Slightly cold
            cfg.biasShift = 0.25f;              // Some dynamic feel
            cfg.plateResistance = 1.1f;         // Higher plate resistance for gain
            cfg.sag = 0.18f;                    // Medium sag
            break;

        case GainStageType::Peavey_5150_Lead:
            // Modeled after the extreme high-gain, scooped, and tight voicing of the 5150/6505.
            cfg.gainFactor = 3.2f;              // Highest stage gain for extreme saturation
            cfg.saturationCurve = 3.0f;         // Hard, aggressive clipping (approaching square wave)
            cfg.asymmetry = 0.45f;              // Very cold bias for aggressive attack
            cfg.compressionRatio = 7.0f;        // Extreme cascaded compression (5+ stages)
            cfg.harmonicContent = 0.15f;        // Max odd harmonics for sizzle and bite
            cfg.couplingCapFreq = 200.0f;       // Smallest caps -> Ultra-tight bass (aggressively filtered)
            cfg.gridStopperFreq = 4500.0f;      // Very low LPF to aggressively tame fizz (darker top)
            cfg.millerCapacitance = 0.85f;      // Max HF rolloff
            cfg.biasPoint = -0.25f;             // Very cold bias point
            cfg.biasShift = 0.55f;              // Pronounced grid-blocking under hard pick
            cfg.plateResistance = 1.6f;         // Max saturation
            cfg.sag = 0.02f;                    // Stiffest supply for max punch and tightness
            break;

        case GainStageType::Engl_Savage_Modern:
            // Modeled after the German high-gain style (tight, saturated, articulate).
            cfg.gainFactor = 3.1f;              // High stage gain
            cfg.saturationCurve = 2.8f;         // Hard clipping, slightly smoother than 5150
            cfg.asymmetry = 0.40f;              // Aggressive asymmetry
            cfg.compressionRatio = 6.5f;        // Very heavy cascaded compression
            cfg.harmonicContent = 0.20f;        // Very odd-harmonic focus
            cfg.couplingCapFreq = 180.0f;       // Ultra-tight low end
            cfg.gridStopperFreq = 4800.0f;      // Aggressive fizz taming, slightly brighter/more open than 5150
            cfg.millerCapacitance = 0.8f;       // High HF rolloff
            cfg.biasPoint = -0.20f;             // Cold bias
            cfg.biasShift = 0.5f;               // Pronounced grid-blocking
            cfg.plateResistance = 1.55f;        // High saturation
            cfg.sag = 0.05f;                    // Very stiff supply
            break;

        case GainStageType::Diezel_VH4_Tight:
            // Modeled after the VH4 Mega/Lead channel (German high-gain, articulated).
            cfg.gainFactor = 3.0f;              // Strong stage gain
            cfg.saturationCurve = 2.3f;         // Aggressive, but well-defined clipping
            cfg.asymmetry = 0.33f;              // Moderate asymmetry for palm-mute bite
            cfg.compressionRatio = 6.2f;        // Heavy cascaded compression
            cfg.harmonicContent = 0.28f;        // Skews toward odd for bite, but smoother than 5150/ENGL
            cfg.couplingCapFreq = 160.0f;       // Very tight low end
            cfg.gridStopperFreq = 6000.0f;      // Tames fizz but keeps top-end presence clear
            cfg.millerCapacitance = 0.6f;       // Moderate HF rolloff
            cfg.biasPoint = -0.18f;             // Slightly cold
            cfg.biasShift = 0.42f;              // Some grid-blocking feel
            cfg.plateResistance = 1.5f;         // Slight headroom reduction for density
            cfg.sag = 0.04f;                    // Ultra-stiff preamp supply feel
            break;

        case GainStageType::Dumble_ODS_Smooth:
            // Modeled after the Overdrive Special's smooth, harmonically rich lead tone.
            cfg.gainFactor = 1.6f;              // Moderate gain
            cfg.saturationCurve = 1.2f;         // Very smooth, musical clipping
            cfg.asymmetry = 0.2f;               // Low asymmetry
            cfg.compressionRatio = 3.0f;        // Medium compression, highly dynamic
            cfg.harmonicContent = 0.75f;        // Highly rich in even harmonics (smooth, musical)
            cfg.couplingCapFreq = 95.0f;        // Full low end
            cfg.gridStopperFreq = 11000.0f;     // Open top end
            cfg.millerCapacitance = 0.35f;      // Low HF rolloff
            cfg.biasPoint = 0.0f;               // Center bias
            cfg.biasShift = 0.2f;               // Moderate bias shift
            cfg.plateResistance = 1.05f;        // Neutral plate load
            cfg.sag = 0.22f;                    // Medium sag
            break;

        case GainStageType::Soldano_SLO_Cascade:
            // Modeled after the 4-stage cascaded gain structure of the SLO-100.
            cfg.gainFactor = 2.4f;              // High gain
            cfg.saturationCurve = 2.0f;         // Aggressive clipping
            cfg.asymmetry = 0.4f;               // Pronounced asymmetry
            cfg.compressionRatio = 3.5f;        // Medium-high compression (4 stages)
            cfg.harmonicContent = 0.35f;        // Focused on odd harmonics (sizzle/clarity)
            cfg.couplingCapFreq = 72.0f;        // Full, but controlled low end
            cfg.gridStopperFreq = 7000.0f;      // Moderate LPF to control fizz
            cfg.millerCapacitance = 0.55f;      // Moderate HF rolloff
            cfg.biasPoint = -0.14f;             // Cold bias
            cfg.biasShift = 0.38f;              // Pronounced grid-blocking feel
            cfg.plateResistance = 1.25f;        // Higher plate resistance
            cfg.sag = 0.16f;                    // Moderate sag
            break;

        case GainStageType::Bogner_Ecstasy_Warm:
            // Modeled after the Ecstasy's Red/Blue channels (warm, thick high-gain).
            cfg.gainFactor = 2.0f;              // Moderate high gain
            cfg.saturationCurve = 1.7f;         // Smooth, thick clipping
            cfg.asymmetry = 0.22f;              // Moderate asymmetry
            cfg.compressionRatio = 2.8f;        // Medium compression
            cfg.harmonicContent = 0.65f;        // Rich in even harmonics (warm, thick)
            cfg.couplingCapFreq = 88.0f;        // Full low end
            cfg.gridStopperFreq = 9500.0f;      // Moderate LPF, open top end
            cfg.millerCapacitance = 0.4f;       // Moderate HF rolloff
            cfg.biasPoint = -0.08f;             // Slightly cold
            cfg.biasShift = 0.28f;              // Some dynamic feel
            cfg.plateResistance = 1.15f;        // Higher plate resistance
            cfg.sag = 0.19f;                    // Medium sag
            break;

        case GainStageType::RolandJC_FET_Clean:
            // Modeled after the solid-state, ultra-clean preamps of the Jazz Chorus.
            cfg.gainFactor = 1.0f;              // Unity gain (preamp)
            cfg.saturationCurve = 0.4f;         // Ultra linear/zero clipping
            cfg.asymmetry = 0.0f;               // Perfect symmetry (solid state)
            cfg.compressionRatio = 1.1f;        // Negligible compression
            cfg.harmonicContent = 0.95f;        // Ultra clean
            cfg.couplingCapFreq = 150.0f;       // Tighter low end (typical SS voicing)
            cfg.gridStopperFreq = 20000.0f;     // Max open top end
            cfg.millerCapacitance = 0.05f;      // Minimal HF rolloff (not a tube)
            cfg.biasPoint = 0.0f;               // Center bias
            cfg.biasShift = 0.0f;               // No bias shift
            cfg.plateResistance = 0.9f;         // Neutral
            cfg.sag = 0.0f;                     // Zero sag (solid state)
            break;

        case GainStageType::Sunn_Transistor_Heavy:
            // Modeled after a heavy, hard-clipping transistor/solid-state distortion.
            cfg.gainFactor = 2.2f;              // High gain transistor
            cfg.saturationCurve = 2.6f;         // Hard clipping (transistor distortion)
            cfg.asymmetry = 0.1f;               // Slight asymmetry
            cfg.compressionRatio = 2.0f;        // Medium compression
            cfg.harmonicContent = 0.15f;        // Harsh, buzzy odd harmonics (transistor character)
            cfg.couplingCapFreq = 55.0f;        // Very low HPF for massive low end
            cfg.gridStopperFreq = 8000.0f;      // Moderate LPF
            cfg.millerCapacitance = 0.3f;       // Low HF rolloff
            cfg.biasPoint = 0.0f;               // Center bias
            cfg.biasShift = 0.1f;               // Low bias shift
            cfg.plateResistance = 1.6f;         // High saturation
            cfg.sag = 0.0f;                     // Zero sag (solid state)
            break;

        case GainStageType::Hughes_Kettner_Tube_SS:
            // Modeled after a hybrid design, balancing tube gain with SS components.
            cfg.gainFactor = 1.9f;              // Moderate tube gain
            cfg.saturationCurve = 1.6f;         // Blended clipping (tube saturation + SS clipping)
            cfg.asymmetry = 0.18f;              // Moderate tube asymmetry
            cfg.compressionRatio = 2.3f;        // Medium compression
            cfg.harmonicContent = 0.5f;         // Balanced hybrid tone
            cfg.couplingCapFreq = 82.0f;        // Full low end
            cfg.gridStopperFreq = 10000.0f;     // Open top end
            cfg.millerCapacitance = 0.42f;      // Moderate HF rolloff
            cfg.biasPoint = -0.06f;             // Slightly cold
            cfg.biasShift = 0.22f;              // Some dynamic feel
            cfg.plateResistance = 1.12f;        // Higher plate resistance
            cfg.sag = 0.12f;                    // Low sag (stiff SS components)
            break;

        default:
            // Default to Marshall characteristics if type is unknown
            return createConfig(GainStageType::Marshall_ECC83_Crunch);
    }

    return cfg;
}
//==============================================================================
// Helper function to get stage type names for UI
//==============================================================================
juce::String getGainStageTypeName(GainStageType type)
{
    switch (type)
    {
        case GainStageType::Fender_12AX7_Clean:        return "Fender 12AX7 (Clean)";
        case GainStageType::Marshall_ECC83_Crunch:     return "Marshall ECC83 (Crunch)";
        case GainStageType::Mesa_12AX7_HighGain:       return "Mesa 12AX7 (High Gain)";
        case GainStageType::Vox_EF86_Bright:           return "Vox EF86 (Bright)";
        case GainStageType::RCA_12AY7_Vintage:         return "RCA 12AY7 (Vintage)";
        case GainStageType::GE_12AU7_Jazz:             return "GE 12AU7 (Jazz/Clean)";
        case GainStageType::Mullard_ECC83_British:     return "Mullard ECC83 (British)";
        case GainStageType::Peavey_5150_Lead:          return "Peavey 5150 (Lead)";
        case GainStageType::Engl_Savage_Modern:        return "ENGL Savage (Modern)";
        case GainStageType::Diezel_VH4_Tight:          return "Diezel VH4 (Tight)";
        case GainStageType::Dumble_ODS_Smooth:         return "Dumble ODS (Smooth)";
        case GainStageType::Soldano_SLO_Cascade:       return "Soldano SLO-100 (Cascade)";
        case GainStageType::Bogner_Ecstasy_Warm:       return "Bogner Ecstasy (Warm)";
        case GainStageType::RolandJC_FET_Clean:        return "Roland JC-120 FET (Clean)";
        case GainStageType::Sunn_Transistor_Heavy:     return "Sunn Model T (Heavy)";
        case GainStageType::Hughes_Kettner_Tube_SS:    return "Hughes & Kettner (Hybrid)";
        default:                                        return "Unknown";
    }
}

//==============================================================================
// Helper to populate ComboBox with all stage types
//==============================================================================
void populateGainStageComboBox(juce::ComboBox& comboBox)
{
    comboBox.clear();

    comboBox.addSectionHeading("Classic Tube Amps");
    comboBox.addItem(getGainStageTypeName(GainStageType::Fender_12AX7_Clean),
                     static_cast<int>(GainStageType::Fender_12AX7_Clean) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Marshall_ECC83_Crunch),
                     static_cast<int>(GainStageType::Marshall_ECC83_Crunch) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Mesa_12AX7_HighGain),
                     static_cast<int>(GainStageType::Mesa_12AX7_HighGain) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Vox_EF86_Bright),
                     static_cast<int>(GainStageType::Vox_EF86_Bright) + 1);

    comboBox.addSeparator();
    comboBox.addSectionHeading("Vintage Tubes");
    comboBox.addItem(getGainStageTypeName(GainStageType::RCA_12AY7_Vintage),
                     static_cast<int>(GainStageType::RCA_12AY7_Vintage) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::GE_12AU7_Jazz),
                     static_cast<int>(GainStageType::GE_12AU7_Jazz) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Mullard_ECC83_British),
                     static_cast<int>(GainStageType::Mullard_ECC83_British) + 1);

    comboBox.addSeparator();
    comboBox.addSectionHeading("Modern High Gain");
    comboBox.addItem(getGainStageTypeName(GainStageType::Peavey_5150_Lead),
                     static_cast<int>(GainStageType::Peavey_5150_Lead) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Engl_Savage_Modern),
                     static_cast<int>(GainStageType::Engl_Savage_Modern) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Diezel_VH4_Tight),
                     static_cast<int>(GainStageType::Diezel_VH4_Tight) + 1);

    comboBox.addSeparator();
    comboBox.addSectionHeading("Boutique/Specialty");
    comboBox.addItem(getGainStageTypeName(GainStageType::Dumble_ODS_Smooth),
                     static_cast<int>(GainStageType::Dumble_ODS_Smooth) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Soldano_SLO_Cascade),
                     static_cast<int>(GainStageType::Soldano_SLO_Cascade) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Bogner_Ecstasy_Warm),
                     static_cast<int>(GainStageType::Bogner_Ecstasy_Warm) + 1);

    comboBox.addSeparator();
    comboBox.addSectionHeading("Solid State & Hybrid");
    comboBox.addItem(getGainStageTypeName(GainStageType::RolandJC_FET_Clean),
                     static_cast<int>(GainStageType::RolandJC_FET_Clean) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Sunn_Transistor_Heavy),
                     static_cast<int>(GainStageType::Sunn_Transistor_Heavy) + 1);
    comboBox.addItem(getGainStageTypeName(GainStageType::Hughes_Kettner_Tube_SS),
                     static_cast<int>(GainStageType::Hughes_Kettner_Tube_SS) + 1);
}


void LZ25AudioProcessor::setIRFolder (const juce::File& dir)
{
    if (!dir.isDirectory())
        return;

    root = dir;
    irFiles = listIRFilesInDirectory (dir);

    if (savedFile.existsAsFile())
    {
        currentIRIndex = irFiles.indexOf (savedFile);
        if (currentIRIndex < 0)
            currentIRIndex = 0;
    }
    else
    {
        currentIRIndex = irFiles.isEmpty() ? -1 : 0;
    }

    if (currentIRIndex >= 0 && currentIRIndex < irFiles.size())
        loadIRAtIndex (currentIRIndex);
}

bool LZ25AudioProcessor::loadIRFile (const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    savedFile = file;
    irLoader.reset();
    irLoader.loadImpulseResponse (savedFile, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, 0);

    // update state
    valueTree.setProperty ("file1", savedFile.getFullPathName(), nullptr);
    valueTree.setProperty ("root", savedFile.getParentDirectory().getFullPathName(), nullptr);

    return true;
}

bool LZ25AudioProcessor::loadIRAtIndex (int idx)
{
    if (idx < 0 || idx >= irFiles.size())
        return false;

    currentIRIndex = idx;
    return loadIRFile (irFiles[(int) currentIRIndex]);
}

bool LZ25AudioProcessor::nextIR()
{
    if (irFiles.isEmpty())
        return false;
    int next = (currentIRIndex + 1 + irFiles.size()) % irFiles.size();
    return loadIRAtIndex (next);
}

bool LZ25AudioProcessor::prevIR()
{
    if (irFiles.isEmpty())
        return false;
    int prev = (currentIRIndex - 1 + irFiles.size()) % irFiles.size();
    return loadIRAtIndex (prev);
}

// **FIXED: Definition for the virtual listener method (Required for linker)**
void LZ25AudioProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // For now, an empty body resolves the linker error.
    juce::ignoreUnused (tree, property);
}

juce::AudioProcessorValueTreeState::ParameterLayout LZ25AudioProcessor::createParameterLayout()
{
    // ... (Parameter definitions remain the same as the previous correct version)
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    // Centered input gain range for better headroom (-24 dB to +24 dB), default at 0 dB
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "INPUT", 1 }, "Input", -24.0f, 24.0f, 0.0f));
    // Output/Post gain now allows trimming and is less aggressive (-24 dB to +24 dB), default at 0 dB
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "POSTGAIN", 1 }, "PostGain", -24.0f, 24.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "RESONANCE", 1 }, "Resonance", 1.0f, 10.0f, 5.0f));
    // Pre-gain centered around a moderate drive (-12 dB to +36 dB), default at +12 dB (center)
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PREGAIN", 1 }, "PreGain", -12.0f, 36.0f, 12.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PRESENCE", 1 }, "Presence", 0.5f, 1.5f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TREBLE", 1 }, "Treble", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MID", 1 }, "Mid", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BASS", 1 }, "Bass", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "LEADCHANNEL", 1 },
        "LeadChannel",
        juce::StringArray { "Clean", "Lead" },
        0));

    // Processing mode: Stereo (process channels independently) or Mono (process one channel for both)
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "PROCESS_MODE", 1 },
        "Process Mode",
        juce::StringArray { "Stereo", "Mono" },
        0));

    // Select which stereo input channel to use as mono source when PROCESS_MODE = Mono
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "INPUTSRC", 1 },
        "Input Source",
        juce::StringArray { "Left", "Right" },
        0));

    // Enhanced Waveshaper Controls (These are ignored by the modernMetalClip, but parameters are kept)
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.1f, 4.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ASYMMETRY", 1 }, "Asymmetry", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "HARMONIC_CHARACTER", 1 }, "Harmonic Character", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SATURATION_SHAPE", 1 }, "Saturation Shape", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TUBE_SAG", 1 }, "Tube Sag", 0.0f, 1.0f, 0.0f));

    // New Tube Amp Parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GAIN1", 1 }, "Gain 1", 0.0f, 1.0f, 0.6f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GAIN2", 1 }, "Gain 2", 0.0f, 1.0f, 0.6f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GAIN3", 1 }, "Gain 3", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "BRIGHTNESS", 1 }, "Bright", false));
    
    // Gain Stage Bypass Parameters (for double-click functionality)
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "GAIN1_BYPASS", 1 }, "Gain 1 Bypass", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "GAIN2_BYPASS", 1 }, "Gain 2 Bypass", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "GAIN3_BYPASS", 1 }, "Gain 3 Bypass", false));

    // Tone Stack position Parameter
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "TONE_STACK_POSITION", 1 },
        "Tone Stack Position",
        juce::StringArray {
            toString(ToneStackPosition::PreGain),
            toString(ToneStackPosition::MidGain),
            toString(ToneStackPosition::PostGain)
        },
        1
    ));

    // Amp Style Preset Parameter - Replaces individual gain stage type selections
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "AMP_STYLE", 1 },
        "Amp Style",
        juce::StringArray { "Brit Crunch Classic", "Tight Modern Stack", "Clean Vintage Glass", "Brutal Lead Stack", "Smooth Boutique Drive", 
                           "Chimey Brit Jangle", "Hot Cascade Lead", "Vintage Glass Clean", "Teutonic Metal Stack", "Hybrid Tube/SS Warmth",
                           "Pure: Fender 12AX7 Clean", "Pure: Marshall ECC83 Crunch", "Pure: Mesa 12AX7 HighGain",
                           "Pure: Vox EF86 Bright", "Pure: RCA 12AY7 Vintage", "Pure: GE 12AU7 Jazz",
                           "Pure: Mullard ECC83 British", "Pure: Peavey 5150 Lead", "Pure: ENGL Savage Modern",
                           "Pure: Diezel VH4 Tight", "Pure: Dumble ODS Smooth", "Pure: Soldano SLO Cascade",
                           "Pure: Bogner Ecstasy Warm", "Pure: RolandJC FET Clean", "Pure: Sunn Transistor Heavy",
                           "Pure: Hughes&Kettner Tube/SS" },
        0)); // Default to Brit Crunch Classic
    
    // Keep individual gain stage type parameters for internal use (hidden from UI)
    juce::StringArray gainStageChoices;
    for (int i = 0; i < static_cast<int>(GainStageType::NumTypes); ++i)
    {
        gainStageChoices.add(getGainStageTypeName(static_cast<GainStageType>(i)));
    }
    
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "GAIN_STAGE_1_TYPE", 1 }, 
        "Gain Stage 1 Type", 
        gainStageChoices, 
        static_cast<int>(GainStageType::Marshall_ECC83_Crunch)));
        
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "GAIN_STAGE_2_TYPE", 1 }, 
        "Gain Stage 2 Type", 
        gainStageChoices, 
        static_cast<int>(GainStageType::Marshall_ECC83_Crunch)));
        
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "GAIN_STAGE_3_TYPE", 1 }, 
        "Gain Stage 3 Type", 
        gainStageChoices, 
        static_cast<int>(GainStageType::Marshall_ECC83_Crunch)));

    // IR Enable parameter
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "IR_ENABLE", 1 }, "IR Enable", true));

    // Panel enable/disable toggles (for tab double-click bypass)
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "PITCH_DYN_PANEL_ENABLE", 1 },  "Pitch/Dynamics Panel Enable",  true));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "PRE_PANEL_ENABLE", 1 },  "Pre Panel Enable",  true));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "AMP_PANEL_ENABLE", 1 },  "Amp Panel Enable",  true));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "POST_PANEL_ENABLE", 1 }, "Post Panel Enable", true));

    // Pre-FX Parameters
    // Pitch parameters (Pitch knob: quantised semitones; Range: discrete selector index; Shift: continuous morph)
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "PITCH_PITCH", 1 }, "Pitch",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "PITCH_RANGE", 1 }, "Pitch Range",
        juce::NormalisableRange<float> (0.0f, 11.0f, 1.0f), 6.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "PITCH_SHIFT", 1 }, "Pitch Shift",
        0.0f, 1.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "PITCH_MIX", 1 }, "Pitch Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "PITCH_ENABLE", 1 }, "Pitch Enable", false));

    // SmartGate parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_INTENSITY", 1 }, "Gate Intensity", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_REDUCTION", 1 }, "Gate Reduction (dB)", 0.0f, 60.0f, 24.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_RELEASE", 1 }, "Gate Release", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_DJENT", 1 }, "Gate Djent", 0.0f, 1.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_MIX", 1 }, "Gate Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "SMARTGATE_ENABLE", 1 }, "Gate Enable", false));

    // Transient Shaper parameters (pick attack booster)
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANSIENT_ATTACK", 1 },  "Transient Attack",  0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANSIENT_SUSTAIN", 1 }, "Transient Sustain", 0.0f, 2.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANSIENT_MIX", 1 },     "Transient Mix",     0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "TRANSIENT_CLIP", 1 },    "Transient Clip Guard", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "TRANSIENT_ENABLE", 1 },  "Transient Enable", false));

    // Compressor parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "COMP_SENSITIVITY", 1 }, "Comp Sensitivity", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "COMP_OUTPUT", 1 }, "Comp Output", 0.0f, 1.0f, 0.7f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "COMP_MIX", 1 }, "Comp Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "COMP_ENABLE", 1 }, "Comp Enable", false));

    // Tube Screamer parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TS808_DRIVE", 1 }, "TS Drive", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TS808_TONE", 1 }, "TS Tone", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TS808_LEVEL", 1 }, "TS Level", 0.0f, 1.0f, 0.7f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TS808_MIX", 1 }, "TS Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "TS808_ENABLE", 1 }, "TS Enable", false));

    // Big Cheese Fuzz parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIGCHEESE_FUZZ", 1 }, "BC Fuzz", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIGCHEESE_TONE", 1 }, "BC Tone", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIGCHEESE_VOLUME", 1 }, "BC Volume", 0.0f, 1.0f, 0.7f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIGCHEESE_TRIM", 1 }, "BC Trim", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "BIGCHEESE_SWITCH", 1 }, "BC Mode", juce::StringArray { "Mild", "Medium", "Hot", "Extreme" }, 1));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "BIGCHEESE_MIX", 1 }, "BC Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "BIGCHEESE_ENABLE", 1 }, "BC Enable", false));

    return { parameters.begin(), parameters.end() };
}

//==============================================================================
void LZ25AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    input = true;

    _rmsOutput.reset (sampleRate, 0.5f);
    _rmsOutput.setCurrentAndTargetValue (-100.0f);

    _rmsInput.reset (sampleRate, 0.5f);
    _rmsInput.setCurrentAndTargetValue (-100.0f);

    _spec.sampleRate = sampleRate;
    _spec.maximumBlockSize = samplesPerBlock;
    // **FIXED: Corrected function call**
    _spec.numChannels = (juce::uint32) getTotalNumOutputChannels();
    reset();

    // Re-initialize the entire amp chain with modern metal settings
    updateProcessorChain();

    _input.setGainDecibels (*apvts.getRawParameterValue ("INPUT"));
    _output.setGainDecibels (*apvts.getRawParameterValue ("POSTGAIN"));

    // Set parameters on the chain elements that change per knob turn
    auto& preGain = processorChain.get<preGainIndex>();
    preGain.setGainDecibels (*apvts.getRawParameterValue ("PREGAIN"));

    // Set resonance and bottom end based on current knobs
    auto& resonanceFilter = processorChain.get<resonanceIndex>();
    resonanceFilter.setCutoffFrequencyHz (*apvts.getRawParameterValue ("RESONANCE") * 1000.0f);

    auto& bottomEndFilter = processorChain.get<bottomEndIndex>();
    *bottomEndFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (getSampleRate(), 400.0f, 0.4f, *apvts.getRawParameterValue ("BASS")); // Bass knob to bottomEnd

    // Set EQ filters with initial knob values
    auto& trebleFilter = processorChain.get<trebleIndex>();
    *trebleFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (getSampleRate(), 5000.0f, 0.6f, *apvts.getRawParameterValue ("TREBLE"));

    auto& midFilter = processorChain.get<midIndex>();
    *midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (getSampleRate(), 500.0f, 0.9f, *apvts.getRawParameterValue ("MID"));

    // The Presence filter is set in updateProcessorChain, but we can update its gain based on the knob here:
    auto& presenceFilter = processorChain.get<presenceIndex>();
    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), 4500.0f, 0.5f, juce::Decibels::decibelsToGain(6.0f * *apvts.getRawParameterValue ("PRESENCE")));

    _input.prepare (_spec);
    processorChain.prepare (_spec);
    _output.prepare (_spec);

    irLoader.reset();
    irLoader.prepare (_spec);

    // Prepare new amp head components
    gainStage1.prepare (sampleRate, samplesPerBlock);
    gainStage2.prepare (sampleRate, samplesPerBlock);
    gainStage3.prepare (sampleRate, samplesPerBlock);
    toneStack.prepare (sampleRate, samplesPerBlock);
    presenceControl.prepare (sampleRate, samplesPerBlock);

    // IO filters
    dcBlocker.prepare ({ sampleRate, (uint32_t) samplesPerBlock, 1 });
    dcBlocker.calcCoefs (10.0f, (float) sampleRate);

    outputFilter.prepare ({ sampleRate, (uint32_t) samplesPerBlock, 1 });
    outputFilter.calcCoefs (12000.0f, (float) sampleRate);

    // Brightness cap
    brightCap.prepare ({ sampleRate, (uint32_t) samplesPerBlock, 1 });
    brightCap.calcCoefs (1500.0f, (float) sampleRate);

    masterGainSmooth.reset (sampleRate, 0.05);

    // Prepare pre-fx effects
    if (pitch)
    {
        pitch->prepare(_spec);
        pitch->setEnabled (*apvts.getRawParameterValue ("PITCH_ENABLE") > 0.5f);
    }
    if (smartGate)
    {
        smartGate->prepare (_spec);
        smartGate->setEnabled (*apvts.getRawParameterValue ("SMARTGATE_ENABLE") > 0.5f);
    }
    if (compressor)
    {
        compressor->prepare (_spec);
        compressor->setEnabled (*apvts.getRawParameterValue ("COMP_ENABLE") > 0.5f);
    }
    if (transientShaper)
    {
        transientShaper->prepare(_spec);
        transientShaper->setEnabled (*apvts.getRawParameterValue ("TRANSIENT_ENABLE") > 0.5f);
    }
    if (tubeScreamer)
    {
        tubeScreamer->prepare (_spec);
        tubeScreamer->setEnabled (*apvts.getRawParameterValue ("TS808_ENABLE") > 0.5f);
    }
    if (bigCheese)
    {
        bigCheese->prepare (_spec);
        bigCheese->setEnabled (*apvts.getRawParameterValue ("BIGCHEESE_ENABLE") > 0.5f);
    }

    if (root.isDirectory())
    {
        setIRFolder (root);
    }
    else if (savedFile.existsAsFile())
    {
        loadIRFile (savedFile);
    }
}

void LZ25AudioProcessor::releaseResources()
{
    // ... (unchanged)
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LZ25AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
    #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
        #endif

    return true;
    #endif
}
#endif

// ============================================================================
// Amp Style Preset Functions
// ============================================================================

// Helper: enable the first N gain stages by clearing their bypass flags
static void setActiveGainStages(LZ25AudioProcessor& processor, int count)
{
    const bool use1 = count >= 1;
    const bool use2 = count >= 2;
    const bool use3 = count >= 3;
    if (auto* p = processor.apvts.getParameter("GAIN1_BYPASS")) p->setValueNotifyingHost(use1 ? 0.0f : 1.0f);
    if (auto* p = processor.apvts.getParameter("GAIN2_BYPASS")) p->setValueNotifyingHost(use2 ? 0.0f : 1.0f);
    if (auto* p = processor.apvts.getParameter("GAIN3_BYPASS")) p->setValueNotifyingHost(use3 ? 0.0f : 1.0f);
}

static int typicalStageCountFor(GainStageType t)
{
    switch (t)
    {
        case GainStageType::Fender_12AX7_Clean: return 2;
        case GainStageType::Marshall_ECC83_Crunch: return 3;
        case GainStageType::Mesa_12AX7_HighGain: return 3; // 5-6 typically, limited to 3 here
        case GainStageType::Vox_EF86_Bright: return 2;
        case GainStageType::RCA_12AY7_Vintage: return 2;
        case GainStageType::GE_12AU7_Jazz: return 2;
        case GainStageType::Mullard_ECC83_British: return 3;
        case GainStageType::Peavey_5150_Lead: return 3; // 5-6 typically, limited to 3 here
        case GainStageType::Engl_Savage_Modern: return 3; // 5-6 typically
        case GainStageType::Diezel_VH4_Tight: return 3; // 5-6 typically
        case GainStageType::Dumble_ODS_Smooth: return 3; // 3-4
        case GainStageType::Soldano_SLO_Cascade: return 3; // 4
        case GainStageType::Bogner_Ecstasy_Warm: return 3; // 4-5
        case GainStageType::RolandJC_FET_Clean: return 2; // 1-2
        case GainStageType::Sunn_Transistor_Heavy: return 3; // 2-4
        case GainStageType::Hughes_Kettner_Tube_SS: return 3; // 3-4
        default: return 3;
    }
}

static void setPureGainStagePreset(LZ25AudioProcessor& processor, GainStageType type)
{
    const int count = juce::jlimit(1, 3, typicalStageCountFor(type));

    auto norm = static_cast<float>(type) / static_cast<float>(GainStageType::NumTypes);
    if (auto* p = processor.apvts.getParameter("GAIN_STAGE_1_TYPE")) p->setValueNotifyingHost(norm);
    if (auto* p = processor.apvts.getParameter("GAIN_STAGE_2_TYPE")) p->setValueNotifyingHost(norm);
    if (auto* p = processor.apvts.getParameter("GAIN_STAGE_3_TYPE")) p->setValueNotifyingHost(norm);

    setActiveGainStages(processor, count);

    // Choose sensible default gains based on gainFactor category
    const auto cfg = GainStageConfig::createConfig(type);
    float g1 = 0.5f, g2 = 0.5f, g3 = 0.5f;
    if (cfg.gainFactor <= 1.0f) { g1 = 0.2f; g2 = 0.15f; g3 = 0.15f; }
    else if (cfg.gainFactor <= 1.9f) { g1 = 0.4f; g2 = 0.35f; g3 = 0.3f; }
    else { g1 = 0.65f; g2 = 0.6f; g3 = 0.55f; }

    if (auto* p = processor.apvts.getParameter("GAIN1")) p->setValueNotifyingHost(g1);
    if (auto* p = processor.apvts.getParameter("GAIN2")) p->setValueNotifyingHost(g2);
    if (auto* p = processor.apvts.getParameter("GAIN3")) p->setValueNotifyingHost(g3);
}

void setBritCrunchClassic(LZ25AudioProcessor& processor)
{
    // Bright input channel configuration (ECC83/ECC83 + Mullard flavor)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Marshall_ECC83_Crunch) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Marshall_ECC83_Crunch) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Mullard_ECC83_British) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages for Marshall-style (CSV: 3 stages)
    setActiveGainStages(processor, 3);

    // Moderate gain settings
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.4f); // ~6
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.35f); // ~5
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.3f); // ~4
}

void setTightModernStack(LZ25AudioProcessor& processor)
{
    // Modern high gain configuration (Mesa/Mesa + Diezel tightness)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Mesa_12AX7_HighGain) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Mesa_12AX7_HighGain) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Diezel_VH4_Tight) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (CSV: 5-6 -> use 3 within plugin)
    setActiveGainStages(processor, 3);

    // High gain settings
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.65f); // ~12
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.6f); // ~10
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.55f); // ~8
}

void setCleanVintageGlass(LZ25AudioProcessor& processor)
{
    // Classic clean tone (12AX7 -> 12AU7 -> 12AY7)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Fender_12AX7_Clean) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::GE_12AU7_Jazz) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::RCA_12AY7_Vintage) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 2 gain stages (CSV: 2 stages)
    setActiveGainStages(processor, 2);

    // Low gain settings
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.2f); // ~3
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.15f); // ~2
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.15f); // ~2
}

void setBrutalLeadStack(LZ25AudioProcessor& processor)
{
    // Brutal modern metal tone (5150/5150 + ENGL)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Peavey_5150_Lead) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Peavey_5150_Lead) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Engl_Savage_Modern) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (CSV: 5-6 -> use 3 within plugin)
    setActiveGainStages(processor, 3);

    // Maximum gain
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.75f); // ~15
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.7f); // ~13
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.65f); // ~12
}

void setSmoothBoutiqueDrive(LZ25AudioProcessor& processor)
{
    // Smooth boutique tone (ODS/ODS + Ecstasy warmth)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Dumble_ODS_Smooth) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Dumble_ODS_Smooth) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Bogner_Ecstasy_Warm) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (CSV: 3-4 -> use 3 within plugin)
    setActiveGainStages(processor, 3);

    // Moderate gain for smooth overdrive
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.45f); // ~7
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.4f); // ~6
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.35f); // ~5
}

void setChimeyBritJangle(LZ25AudioProcessor& processor)
{
    // British jangle and chime (EF86 -> ECC83 -> 12AU7)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Vox_EF86_Bright) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Mullard_ECC83_British) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::GE_12AU7_Jazz) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 2 gain stages (CSV: 1-2 -> use 2)
    setActiveGainStages(processor, 2);

    // Moderate gain for classic British crunch
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.5f); // ~8
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.4f); // ~6
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.25f); // ~3.5
}

void setHotCascadeLead(LZ25AudioProcessor& processor)
{
    // High gain cascade configuration - all stages using SLO character
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Soldano_SLO_Cascade) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Soldano_SLO_Cascade) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Soldano_SLO_Cascade) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (CSV: 4 -> use 3 within plugin)
    setActiveGainStages(processor, 3);

    // High gain cascade settings
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.6f); // ~10
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.65f); // ~11
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.7f); // ~13
}

void setVintageGlassClean(LZ25AudioProcessor& processor)
{
    // Ultra-clean vintage jazz configuration (12AY7 -> 12AU7 -> FET)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::RCA_12AY7_Vintage) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::GE_12AU7_Jazz) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::RolandJC_FET_Clean) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 2 gain stages (CSV: 2 stages)
    setActiveGainStages(processor, 2);

    // Very low gain for pristine clean tones
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.1f); // ~1.5
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.12f); // ~1.8
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.15f); // ~2.2
}

void setTeutonicMetalStack(LZ25AudioProcessor& processor)
{
    // Brutal modern metal configuration (ENGL -> Diezel -> Sunn)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Engl_Savage_Modern) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Diezel_VH4_Tight) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Sunn_Transistor_Heavy) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (extreme modern -> use 3)
    setActiveGainStages(processor, 3);

    // Maximum gain for extreme metal
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.8f); // ~16
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.75f); // ~15
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.7f); // ~13
}

void setHybridTubeSSWarmth(LZ25AudioProcessor& processor)
{
    // Creative hybrid configuration combining tube and solid-state warmth (Tube/SS -> Bogner -> ODS)
    processor.apvts.getParameter("GAIN_STAGE_1_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Hughes_Kettner_Tube_SS) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_2_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Bogner_Ecstasy_Warm) /
                                static_cast<float>(GainStageType::NumTypes));
    processor.apvts.getParameter("GAIN_STAGE_3_TYPE")
        ->setValueNotifyingHost(static_cast<float>(GainStageType::Dumble_ODS_Smooth) /
                                static_cast<float>(GainStageType::NumTypes));

    // Activate 3 gain stages (CSV: 3-4 -> use 3)
    setActiveGainStages(processor, 3);

    // Mid-range gain for warm, musical overdrive
    processor.apvts.getParameter("GAIN1")->setValueNotifyingHost(0.35f); // ~5
    processor.apvts.getParameter("GAIN2")->setValueNotifyingHost(0.45f); // ~7
    processor.apvts.getParameter("GAIN3")->setValueNotifyingHost(0.4f); // ~6
}

// ============================================================================
// Process Block
// ============================================================================

// Auto-makeup gain for low-gain presets and settings
// Computes a linear gain factor based on current gain stage types, their configured
// intrinsic gain (gainFactor), user gain knobs, and bypass states. It boosts only
// when the overall estimated drive is low, helping cleans to be audible without
// altering user-visible POSTGAIN.
static float computeAutoMakeupLin(const LZ25AudioProcessor& p)
{
    auto getStage = [&](int idx) -> std::pair<float, GainStageType>
    {
        const char* gainId = (idx == 1 ? "GAIN1" : (idx == 2 ? "GAIN2" : "GAIN3"));
        const char* typeId = (idx == 1 ? "GAIN_STAGE_1_TYPE" : (idx == 2 ? "GAIN_STAGE_2_TYPE" : "GAIN_STAGE_3_TYPE"));
        const float gainNorm = *p.apvts.getRawParameterValue(gainId);
        const float gain = juce::jlimit(0.0f, 4.0f, gainNorm * 4.0f);
        const int typeIndex = static_cast<int>(*p.apvts.getRawParameterValue(typeId));
        return { gain, static_cast<GainStageType>(typeIndex) };
    };

    const bool b1 = *p.apvts.getRawParameterValue("GAIN1_BYPASS") > 0.5f;
    const bool b2 = *p.apvts.getRawParameterValue("GAIN2_BYPASS") > 0.5f;
    const bool b3 = *p.apvts.getRawParameterValue("GAIN3_BYPASS") > 0.5f;

    float sum = 0.0f;
    int active = 0;
    if (!b1) { auto [g, t] = getStage(1); sum += g * GainStageConfig::createConfig(t).gainFactor; active++; }
    if (!b2) { auto [g, t] = getStage(2); sum += g * GainStageConfig::createConfig(t).gainFactor; active++; }
    if (!b3) { auto [g, t] = getStage(3); sum += g * GainStageConfig::createConfig(t).gainFactor; active++; }

    // Reference point: sums above ~3.0 correspond to medium/high-gain stacks
    const float ref = 3.0f;
    float deficit = ref - sum;
    if (deficit <= 0.0f)
        return 1.0f; // no makeup needed

    // More allowance when fewer stages are active
    float stageScale = (active <= 1 ? 1.0f : (active == 2 ? 0.85f : 0.7f));

    // Map deficit [0..ref] to up to +12 dB (scaled by active stage count)
    float t = juce::jlimit(0.0f, 1.0f, deficit / ref);
    float makeupDb = stageScale * (12.0f * t);
    makeupDb = juce::jlimit(0.0f, 12.0f, makeupDb);
    return juce::Decibels::decibelsToGain(makeupDb);
}

void LZ25AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    (void) midiMessages;

    juce::ScopedNoDenormals noDenormals;
    auto inputChannels = getTotalNumInputChannels();
    auto outputChannels = getTotalNumOutputChannels();

    for (auto i = inputChannels; i < outputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Handle Amp Style Preset Changes
    static int lastAmpStyle = -1;
    int currentAmpStyle = static_cast<int>(*apvts.getRawParameterValue("AMP_STYLE"));
    
    if (currentAmpStyle != lastAmpStyle)
    {
        switch (currentAmpStyle)
        {
            case 0: setBritCrunchClassic(*this); break;
            case 1: setTightModernStack(*this); break;
            case 2: setCleanVintageGlass(*this); break;
            case 3: setBrutalLeadStack(*this); break;
            case 4: setSmoothBoutiqueDrive(*this); break;
            case 5: setChimeyBritJangle(*this); break;
            case 6: setHotCascadeLead(*this); break;
            case 7: setVintageGlassClean(*this); break;
            case 8: setTeutonicMetalStack(*this); break;
            case 9: setHybridTubeSSWarmth(*this); break;
            // Pure single-type gain-stage presets
            case 10: setPureGainStagePreset(*this, GainStageType::Fender_12AX7_Clean); break;
            case 11: setPureGainStagePreset(*this, GainStageType::Marshall_ECC83_Crunch); break;
            case 12: setPureGainStagePreset(*this, GainStageType::Mesa_12AX7_HighGain); break;
            case 13: setPureGainStagePreset(*this, GainStageType::Vox_EF86_Bright); break;
            case 14: setPureGainStagePreset(*this, GainStageType::RCA_12AY7_Vintage); break;
            case 15: setPureGainStagePreset(*this, GainStageType::GE_12AU7_Jazz); break;
            case 16: setPureGainStagePreset(*this, GainStageType::Mullard_ECC83_British); break;
            case 17: setPureGainStagePreset(*this, GainStageType::Peavey_5150_Lead); break;
            case 18: setPureGainStagePreset(*this, GainStageType::Engl_Savage_Modern); break;
            case 19: setPureGainStagePreset(*this, GainStageType::Diezel_VH4_Tight); break;
            case 20: setPureGainStagePreset(*this, GainStageType::Dumble_ODS_Smooth); break;
            case 21: setPureGainStagePreset(*this, GainStageType::Soldano_SLO_Cascade); break;
            case 22: setPureGainStagePreset(*this, GainStageType::Bogner_Ecstasy_Warm); break;
            case 23: setPureGainStagePreset(*this, GainStageType::RolandJC_FET_Clean); break;
            case 24: setPureGainStagePreset(*this, GainStageType::Sunn_Transistor_Heavy); break;
            case 25: setPureGainStagePreset(*this, GainStageType::Hughes_Kettner_Tube_SS); break;
            default: break;
        }
        lastAmpStyle = currentAmpStyle;
    }

    // Update main processor chain parameters on every block (Tone Stack, Resonance, Gain)
    auto& preGain = processorChain.get<preGainIndex>();
    preGain.setGainDecibels (*apvts.getRawParameterValue ("PREGAIN"));

    // Resonance
    auto& resonanceFilter = processorChain.get<resonanceIndex>();
    resonanceFilter.setCutoffFrequencyHz (*apvts.getRawParameterValue ("RESONANCE") * 1000.0f);

    // Bottom End/Bass
    auto& bottomEndFilter = processorChain.get<bottomEndIndex>();
    *bottomEndFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (getSampleRate(), 400.0f, 0.4f, *apvts.getRawParameterValue ("BASS"));

    // Treble
    auto& trebleFilter = processorChain.get<trebleIndex>();
    *trebleFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (getSampleRate(), 5000.0f, 0.6f, *apvts.getRawParameterValue ("TREBLE"));

    // Mid
    auto& midFilter = processorChain.get<midIndex>();
    *midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (getSampleRate(), 500.0f, 0.9f, *apvts.getRawParameterValue ("MID"));

    // Presence (updating gain only, as freq/Q are fixed for metal tone)
    auto& presenceFilter = processorChain.get<presenceIndex>();
    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), 4500.0f, 0.5f, juce::Decibels::decibelsToGain(6.0f * *apvts.getRawParameterValue ("PRESENCE")));


    _input.setGainDecibels (*apvts.getRawParameterValue ("INPUT"));

    juce::dsp::AudioBlock<float> inputGainBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> inputContextReplacing (inputGainBlock);

    _input.process (inputContextReplacing);

    // Update input RMS meter (post input gain, pre-processing)
    _rmsInput.skip (buffer.getNumSamples());
    {
        const auto valueIn = juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, 0, buffer.getNumSamples()));
        if (valueIn < _rmsInput.getCurrentValue())
            _rmsInput.setTargetValue (valueIn);
        else
            _rmsInput.setCurrentAndTargetValue (valueIn);
    }

    input = true;

    // 1. PITCH/DYNAMICS Processing (Pitch -> Gate -> Transient -> Compressor)
    {
        juce::dsp::AudioBlock<float> preFxBlock (buffer);

        if (*apvts.getRawParameterValue ("PITCH_DYN_PANEL_ENABLE") > 0.5f)
        {
            // Pitch (with mix) - first in chain
            if (pitch)
                pitch->setEnabled (*apvts.getRawParameterValue ("PITCH_ENABLE") > 0.5f);
            if (pitch && *apvts.getRawParameterValue ("PITCH_ENABLE") > 0.5f)
            {
                pitch->updateParameters (apvts);
                juce::AudioBuffer<float> dryBuffer;
                dryBuffer.makeCopyOf (buffer);
                pitch->process (preFxBlock);

                const float mix = *apvts.getRawParameterValue ("PITCH_MIX");
                const float dry = 1.0f - mix;
                const int numCh2 = buffer.getNumChannels();
                const int numSmps2 = buffer.getNumSamples();
                for (int ch = 0; ch < numCh2; ++ch)
                {
                    float* wetData = buffer.getWritePointer (ch);
                    const float* dryData = dryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSmps2; ++i)
                        wetData[i] = mix * wetData[i] + dry * dryData[i];
                }
            }

            // SmartGate (in-place)
            if (smartGate)
            {
                smartGate->setEnabled (*apvts.getRawParameterValue ("SMARTGATE_ENABLE") > 0.5f);
                smartGate->updateParameters (apvts);
                smartGate->process (preFxBlock);
            }

            // Transient Shaper (with mix) — after gate, before compression
            if (transientShaper)
                transientShaper->setEnabled (*apvts.getRawParameterValue ("TRANSIENT_ENABLE") > 0.5f);
            if (transientShaper && *apvts.getRawParameterValue ("TRANSIENT_ENABLE") > 0.5f)
            {
                transientShaper->updateParameters (apvts);
                juce::AudioBuffer<float> dryBuffer;
                dryBuffer.makeCopyOf (buffer);
                transientShaper->process (preFxBlock);

                const float mixTS = *apvts.getRawParameterValue ("TRANSIENT_MIX");
                const float dryTS = 1.0f - mixTS;
                const int numChTS = buffer.getNumChannels();
                const int numSmpsTS = buffer.getNumSamples();
                for (int ch = 0; ch < numChTS; ++ch)
                {
                    float* wetData = buffer.getWritePointer (ch);
                    const float* dryData = dryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSmpsTS; ++i)
                        wetData[i] = mixTS * wetData[i] + dryTS * dryData[i];
                }
            }

            // Compressor (with mix)
            if (compressor)
                compressor->setEnabled (*apvts.getRawParameterValue ("COMP_ENABLE") > 0.5f);
            if (compressor && *apvts.getRawParameterValue ("COMP_ENABLE") > 0.5f)
            {
                compressor->updateParameters (apvts);
                juce::AudioBuffer<float> dryBuffer;
                dryBuffer.makeCopyOf (buffer);
                compressor->process (preFxBlock);

                const float mix = *apvts.getRawParameterValue ("COMP_MIX");
                const float dry = 1.0f - mix;
                const int numCh = buffer.getNumChannels();
                const int numSmps = buffer.getNumSamples();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* wetData = buffer.getWritePointer (ch);
                    const float* dryData = dryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSmps; ++i)
                        wetData[i] = mix * wetData[i] + dry * dryData[i];
                }
            }
        }

        // 2. PRE-AMP drives (Tube Screamer -> Big Cheese)
        if (*apvts.getRawParameterValue ("PRE_PANEL_ENABLE") > 0.5f)
        {
            // Tube Screamer (with mix)
            if (tubeScreamer)
                tubeScreamer->setEnabled (*apvts.getRawParameterValue ("TS808_ENABLE") > 0.5f);
            if (tubeScreamer && *apvts.getRawParameterValue ("TS808_ENABLE") > 0.5f)
            {
                tubeScreamer->updateParameters (apvts);
                juce::AudioBuffer<float> dryBuffer;
                dryBuffer.makeCopyOf (buffer);
                tubeScreamer->process (preFxBlock);

                const float mix = *apvts.getRawParameterValue ("TS808_MIX");
                const float dry = 1.0f - mix;
                const int numCh = buffer.getNumChannels();
                const int numSmps = buffer.getNumSamples();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* wetData = buffer.getWritePointer (ch);
                    const float* dryData = dryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSmps; ++i)
                        wetData[i] = mix * wetData[i] + dry * dryData[i];
                }
            }

            // Big Cheese Fuzz (with mix)
            if (bigCheese)
                bigCheese->setEnabled (*apvts.getRawParameterValue ("BIGCHEESE_ENABLE") > 0.5f);
            if (bigCheese && *apvts.getRawParameterValue ("BIGCHEESE_ENABLE") > 0.5f)
            {
                bigCheese->updateParameters (apvts);
                juce::AudioBuffer<float> dryBuffer;
                dryBuffer.makeCopyOf (buffer);
                bigCheese->process (preFxBlock);

                const float mix = *apvts.getRawParameterValue ("BIGCHEESE_MIX");
                const float dry = 1.0f - mix;
                const int numCh = buffer.getNumChannels();
                const int numSmps = buffer.getNumSamples();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* wetData = buffer.getWritePointer (ch);
                    const float* dryData = dryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSmps; ++i)
                        wetData[i] = mix * wetData[i] + dry * dryData[i];
                }
            }
        }
    }

    // 2. New High-Gain Tube Amp Head Processing
    if (*apvts.getRawParameterValue ("AMP_PANEL_ENABLE") > 0.5f)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0)
            return;

        // Update gain stage types based on parameter selection
        const int stage1TypeIndex = static_cast<int>(*apvts.getRawParameterValue ("GAIN_STAGE_1_TYPE"));
        const int stage2TypeIndex = static_cast<int>(*apvts.getRawParameterValue ("GAIN_STAGE_2_TYPE"));
        const int stage3TypeIndex = static_cast<int>(*apvts.getRawParameterValue ("GAIN_STAGE_3_TYPE"));

        gainStage1.setStageType(static_cast<GainStageType>(stage1TypeIndex));
        gainStage2.setStageType(static_cast<GainStageType>(stage2TypeIndex));
        gainStage3.setStageType(static_cast<GainStageType>(stage3TypeIndex));

        // Read current parameter values and set targets for smoothing
        const float g1 = juce::jlimit (0.0f, 4.0f, *apvts.getRawParameterValue ("GAIN1") * 4.0f);
        const float g2 = juce::jlimit (0.0f, 4.0f, *apvts.getRawParameterValue ("GAIN2") * 4.0f);
        const float g3 = juce::jlimit (0.0f, 4.0f, *apvts.getRawParameterValue ("GAIN3") * 4.0f);

        gainStage1.setGain (g1);
        gainStage2.setGain (g2);
        gainStage3.setGain (g3);

        // Map 0..2 (unity at 1) to -1..1 (unity at 0) for new tone stack
        toneStack.setBass (*apvts.getRawParameterValue ("BASS") - 1.0f);
        toneStack.setMid (*apvts.getRawParameterValue ("MID") - 1.0f);
        toneStack.setTreble (*apvts.getRawParameterValue ("TREBLE") - 1.0f);
        toneStack.setPresence (*apvts.getRawParameterValue ("PRESENCE")); // no-op inside ToneStack

        // Map Presence from 0.5..1.5 to -1..1 for advanced presence controller
        {
            const float rawP = *apvts.getRawParameterValue ("PRESENCE");
            const float mappedP = juce::jlimit (-1.0f, 1.0f, (rawP - 1.0f) * 2.0f);
            presenceControl.setPresence (mappedP);
        }


        {
                    const float base = juce::jlimit (0.0f, 1.5f, *apvts.getRawParameterValue ("POSTGAIN") * 0.1f + 1.0f);
                    const float autoLin = computeAutoMakeupLin(*this);
                    masterGainSmooth.setTargetValue (juce::jlimit (0.0f, 3.0f, base * autoLin));
                }

        const bool brightOn = *apvts.getRawParameterValue ("BRIGHTNESS") > 0.5f;
        
        // Read bypass states for gain stages
        const bool gain1Bypassed = *apvts.getRawParameterValue ("GAIN1_BYPASS") > 0.5f;
        const bool gain2Bypassed = *apvts.getRawParameterValue ("GAIN2_BYPASS") > 0.5f;
        const bool gain3Bypassed = *apvts.getRawParameterValue ("GAIN3_BYPASS") > 0.5f;

        // Choose input channel: if multiple channels, pick the one with higher RMS
        int inChan = 0;
        if (numChannels >= 2)
        {
            const float* ch0 = buffer.getReadPointer (0);
            const float* ch1 = buffer.getReadPointer (1);
            double sum0 = 0.0, sum1 = 0.0;
            for (int n = 0; n < numSamples; ++n)
            {
                sum0 += (double) ch0[n] * (double) ch0[n];
                sum1 += (double) ch1[n] * (double) ch1[n];
            }
            inChan = (sum1 > sum0 ? 1 : 0);
        }

        const float* inData = buffer.getReadPointer (inChan);
        float* out0 = buffer.getWritePointer (0);

        for (int n = 0; n < numSamples; ++n)
        {
            float x = inData[n];

            x = dcBlocker.processSample (x);
            if (brightOn)
                x = brightCap.processSample (x);

            // Gain stage processing with bypass functionality
            if (!gain1Bypassed)
                x = gainStage1.processSample (x);
            if (!gain2Bypassed)
                x = gainStage2.processSample (x);
            if (!gain3Bypassed)
                x = gainStage3.processSample (x);

            x = toneStack.processSample (x);
            x = presenceControl.processSample (x);

            x *= masterGainSmooth.getNextValue();
            x = outputFilter.processSample (x);

            out0[n] = x; // mono output
        }

        // Mirror mono to any additional channels (dual-mono if needed)
        for (int ch = 1; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    // 3. IR Loader
    juce::dsp::AudioBlock<float> convolverBlock (buffer);

    if (*apvts.getRawParameterValue ("IR_ENABLE") > 0.5f && irLoader.getCurrentIRSize() > 0)
        irLoader.process (juce::dsp::ProcessContextReplacing<float> (convolverBlock));

    // 4. Output Gain
    _output.setGainDecibels (*apvts.getRawParameterValue ("POSTGAIN"));
    juce::dsp::AudioBlock<float> outputGainBlock (buffer);
    _output.process (juce::dsp::ProcessContextReplacing<float> (outputGainBlock));

    // If PROCESS_MODE is Mono, ensure mono output by duplicating selected channel to the opposite output (avoid self-copy)
    const int processMode = static_cast<int> (*apvts.getRawParameterValue ("PROCESS_MODE")); // 0 = Stereo, 1 = Mono
    if (processMode == 1 && outputChannels == 2)
    {
        const int srcIndex = static_cast<int> (*apvts.getRawParameterValue ("INPUTSRC"));
        const int numSamples = buffer.getNumSamples();

        // Determine source channel safely
        const int srcChannel = (srcIndex == 0 && inputChannels >= 1) ? 0 :
                               (srcIndex == 1 && inputChannels >= 2) ? 1 : 0; // Default to 0 if selection is out of bounds

        // Copy only to the other channel to prevent overlapping self-copy (JUCE assert)
        if (srcChannel == 0 && outputChannels >= 2)
        {
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples); // L -> R
        }
        else if (srcChannel == 1 && outputChannels >= 2)
        {
            buffer.copyFrom (0, 0, buffer, 1, 0, numSamples); // R -> L
        }
        // If output is mono or srcChannel is unexpected, do nothing extra.
    }

    // RMS Metering
    _rmsOutput.skip (buffer.getNumSamples());
    input = false;

    const auto valueOut = juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, 0, buffer.getNumSamples()));
    if (valueOut < _rmsOutput.getCurrentValue())
        _rmsOutput.setTargetValue (valueOut);
    else
        _rmsOutput.setCurrentAndTargetValue (valueOut);

    input = true;
}

// ... (rest of the functions remain the same: getRMSOutputValue, hasEditor, createEditor, getStateInformation, setStateInformation, createPluginFilter)
// The rest of the functions are already present and correct based on the previous fixes.

//==============================================================================
const juce::String LZ25AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LZ25AudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool LZ25AudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool LZ25AudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double LZ25AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LZ25AudioProcessor::getNumPrograms()
{
    return 1;
}

int LZ25AudioProcessor::getCurrentProgram()
{
    return 0;
}

void LZ25AudioProcessor::setCurrentProgram (int index)
{
    (void) index;
}

const juce::String LZ25AudioProcessor::getProgramName (int index)
{
    (void) index;
    return {};
}

void LZ25AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    (void) index;
    (void) newName;
}


float LZ25AudioProcessor::getRMSOutputValue (const int channel) const
{
    jassert (juce::isPositiveAndBelow (channel, getTotalNumOutputChannels()));
    return _rmsOutput.getCurrentValue();
}

float LZ25AudioProcessor::getRMSInputValue (const int channel) const
{
    jassert (juce::isPositiveAndBelow (channel, getTotalNumInputChannels()));
    return _rmsInput.getCurrentValue();
}


bool LZ25AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* LZ25AudioProcessor::createEditor()
{
    return new LZ25AudioProcessorEditor (*this);
}

//==============================================================================
void LZ25AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.appendChild (valueTree, nullptr);
    juce::MemoryOutputStream stream (destData, false);
    apvts.state.writeToStream (stream);
}

void LZ25AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, size_t (sizeInBytes));
    valueTree = tree.getChildWithName ("Variables");

    if (tree.isValid())
    {
        apvts.state = tree;
        savedFile = juce::File (valueTree.getProperty ("file1"));
        root = juce::File (valueTree.getProperty ("root"));

        irLoader.reset();
        if (root.isDirectory())
        {
            setIRFolder (root);
            if (savedFile.existsAsFile())
            {
                auto idx = irFiles.indexOf (savedFile);
                if (idx >= 0)
                    loadIRAtIndex (idx);
            }
        }
        else if (savedFile.existsAsFile())
        {
            loadIRFile (savedFile);
        }
    }
    
    // Ensure IR is enabled after restoring any saved state
    if (auto* irEnableParam = apvts.getParameter ("IR_ENABLE"))
        irEnableParam->setValueNotifyingHost (1.0f);
}

void LZ25AudioProcessor::reset()
{
    processorChain.reset();
    irLoader.reset();
    // Reset new components
    presenceControl.reset();
}

juce::File LZ25AudioProcessor::getDefaultPresetDirectory()
{
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto dir = docs.getChildFile ("LZ25 Presets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

bool LZ25AudioProcessor::savePreset (const juce::File& fileToSave) const
{
    // Root preset tree wraps parameters and variables for future compatibility
    juce::ValueTree rootTree { "LZ25Preset" };

    // Copy parameter state
    rootTree.addChild (apvts.state.createCopy(), -1, nullptr);

    // Copy auxiliary variables (IR paths etc.)
    if (valueTree.isValid())
        rootTree.addChild (valueTree.createCopy(), -1, nullptr);

    if (auto xml = rootTree.createXml())
        return xml->writeTo (fileToSave);

    return false;
}

bool LZ25AudioProcessor::loadPreset (const juce::File& fileToLoad)
{
    if (! fileToLoad.existsAsFile())
        return false;

    auto xml = juce::parseXML (fileToLoad);
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
        return false;

    // We expect either a wrapped preset or a raw PARAMETERS tree
    juce::ValueTree paramsTree;
    juce::ValueTree varsTree;

    if (tree.hasType ("LZ25Preset"))
    {
        paramsTree = tree.getChildWithName (apvts.state.getType()); // "PARAMETERS"
        varsTree = tree.getChildWithName ("Variables");
    }
    else if (tree.hasType (apvts.state.getType()))
    {
        paramsTree = tree;
    }

    if (! paramsTree.isValid())
        return false;

    // Apply parameter state
    apvts.state = paramsTree;

    // Restore auxiliary variables and IRs
    if (varsTree.isValid())
    {
        valueTree = varsTree;
        savedFile = juce::File (valueTree.getProperty ("file1"));
        root = juce::File (valueTree.getProperty ("root"));

        irLoader.reset();
        if (root.isDirectory())
        {
            setIRFolder (root);
            if (savedFile.existsAsFile())
            {
                auto idx = irFiles.indexOf (savedFile);
                if (idx >= 0)
                    loadIRAtIndex (idx);
            }
        }
        else if (savedFile.existsAsFile())
        {
            loadIRFile (savedFile);
        }
    }

    // Ensure IR is enabled after preset load
    if (auto* irEnableParam = apvts.getParameter ("IR_ENABLE"))
        irEnableParam->setValueNotifyingHost (1.0f);

    return true;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LZ25AudioProcessor();
}