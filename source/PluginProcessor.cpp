#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
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
    smartGate = std::make_unique<SmartGate>();
    compressor = std::make_unique<MxrDynaComp>();
    tubeScreamer = std::make_unique<TubeScreamer808>();
    bigCheese = std::make_unique<BigCheeseFuzz>();

    // Force IR to be enabled on startup regardless of any previous saved state
    if (auto* irEnableParam = apvts.getParameter ("IR_ENABLE"))
        irEnableParam->setValueNotifyingHost (1.0f);

    // Initial setup for the chain. Coefficients will be set in prepareToPlay.
    updateProcessorChain();
}
#endif

LZ25AudioProcessor::~LZ25AudioProcessor() = default;

// Build a list of IR files in a directory (wav/aif/aiff/flac)
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

// **MODERN METAL ADJUSTMENT: NEW FUNCTION FOR PROCESSING LOGIC**
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

    // Select which stereo input channel to use as mono source
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

    // IR Enable parameter
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "IR_ENABLE", 1 }, "IR Enable", true));

    // Pre-FX Parameters
    // SmartGate parameters
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_INTENSITY", 1 }, "Gate Intensity", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_REDUCTION", 1 }, "Gate Reduction (dB)", 0.0f, 60.0f, 24.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_RELEASE", 1 }, "Gate Release", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_DJENT", 1 }, "Gate Djent", 0.0f, 1.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SMARTGATE_MIX", 1 }, "Gate Mix", 0.0f, 1.0f, 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "SMARTGATE_ENABLE", 1 }, "Gate Enable", false));

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

    // Prepare pre-fx effects
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

void LZ25AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    (void) midiMessages;

    juce::ScopedNoDenormals noDenormals;
    auto inputChannels = getTotalNumInputChannels();
    auto outputChannels = getTotalNumOutputChannels();

    for (auto i = inputChannels; i < outputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

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

    input = true;

    // 1. Pre-FX Processing (input -> smart gate -> compressor -> tube screamer -> big cheese)
    juce::dsp::AudioBlock<float> preFxBlock (buffer);

    // SmartGate (in-place)
    if (smartGate)
    {
        smartGate->setEnabled (*apvts.getRawParameterValue ("SMARTGATE_ENABLE") > 0.5f);
        smartGate->updateParameters (apvts);
        smartGate->process (preFxBlock);
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

    // 2. Main Amp Processor Chain
    juce::dsp::AudioBlock<float> processorChainBlock (buffer);
    processorChain.process (juce::dsp::ProcessContextReplacing<float> (processorChainBlock));

    // 3. IR Loader
    juce::dsp::AudioBlock<float> convolverBlock (buffer);

    if (*apvts.getRawParameterValue ("IR_ENABLE") > 0.5f && irLoader.getCurrentIRSize() > 0)
        irLoader.process (juce::dsp::ProcessContextReplacing<float> (convolverBlock));

    // 4. Output Gain
    _output.setGainDecibels (*apvts.getRawParameterValue ("POSTGAIN"));
    juce::dsp::AudioBlock<float> outputGainBlock (buffer);
    _output.process (juce::dsp::ProcessContextReplacing<float> (outputGainBlock));

    // Ensure mono output by duplicating selected channel to the opposite output (avoid self-copy)
    if (outputChannels == 2)
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
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LZ25AudioProcessor();
}