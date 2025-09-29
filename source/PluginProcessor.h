#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include "effects/MxrDynaComp.h"
#include "effects/TubeScreamer808.h"
#include "effects/BigCheeseFuzz.h"
#include "effects/SmartGate.h"
#include "effects/Pitch.h"
#include "effects/TransientShaper.h"

//==============================================================================
/**
*/
class LZ25AudioProcessor : public juce::AudioProcessor, public juce::ValueTree::Listener
#if JucePlugin_Enable_ARA
    , public juce::AudioProcessorARAExtension
#endif
{
public:
    //==============================================================================
    LZ25AudioProcessor();
    ~LZ25AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // **RESTORED: setWaveshaper is no longer needed; updateProcessorChain handles it.**
    // void setWaveshaper();

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void reset() override;

    // **FIXED: Must be static for linker**
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // **FIXED: Required definition for linker**
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    // **Modern metal tone stack tuning functions removed in favor of updateProcessorChain()**
    // void presence();
    // void equalize();
    // void resonance();
    // void bottomEnd();

    float getRMSOutputValue(const int channel) const;
    float getRMSInputValue(const int channel) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree valueTree;

    // Removed unused waveshaper state variables
    // std::string waveshaperFunction;
    // std::string currentWaveshapeFunction;
    // float driveAmount = 1.0f;
    // float asymmetryAmount = 0.5f;
    // float harmonicCharacter = 1.0f;
    // float saturationShapeAmount = 1.0f;
    // float tubeSagAmount = 0.0f;

    bool input;

    juce::File root, savedFile;
    juce::dsp::Convolution irLoader;

    // IR folder management
    juce::Array<juce::File> irFiles;
    int currentIRIndex = -1;

    // IR management API
    void setIRFolder (const juce::File& dir);
    bool loadIRFile (const juce::File& file);
    bool loadIRAtIndex (int idx);
    bool nextIR();
    bool prevIR();
    juce::String getCurrentIRName() const { return savedFile.getFileName(); }

    // Preset management API (shared across Standalone and plugin formats)
    bool savePreset (const juce::File& fileToSave) const;
    bool loadPreset (const juce::File& fileToLoad);
    static juce::File getDefaultPresetDirectory();

private:
    // **MODERN METAL ADJUSTMENT: ADD NEW INDEX FOR PRE-DISTORTION HPF**
    enum
    {
        resonanceIndex,
        bottomEndIndex,
        highPassFilterIndex, // <-- NEW INDEX: For pre-distortion low-cut (Tube Screamer/Grind effect)
        preGainIndex,
        waveshaperIndex,
        presenceIndex,
        bassIndex,
        midIndex,
        trebleIndex
    };

    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoefs = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::Gain<float> _input;
    // **MODERN METAL ADJUSTMENT: INSERT THE NEW HPF INTO THE CHAIN**
    juce::dsp::ProcessorChain<
        juce::dsp::LadderFilter<float>,                              // 0. Resonance
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>,        // 1. Bottom End
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>,        // 2. High Pass Filter (NEW)
        juce::dsp::Gain<float>,                                     // 3. Pre Gain
        juce::dsp::WaveShaper<float>,                               // 4. Distortion
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>,        // 5. Presence
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>,        // 6. Bass
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>,        // 7. Mid
        juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoefs>         // 8. Treble
        > processorChain;
    juce::dsp::Gain<float> _output;

    juce::dsp::ProcessSpec _spec{};
    juce::LinearSmoothedValue<float> _rmsOutput;
    juce::LinearSmoothedValue<float> _rmsInput;

    // **NEW FUNCTION DECLARATION**
    void updateProcessorChain();

    //==============================================================================
    // Pre-FX Effects
    std::unique_ptr<Pitch> pitch;
    std::unique_ptr<SmartGate> smartGate;
    std::unique_ptr<MxrDynaComp> compressor;
    std::unique_ptr<TransientShaper> transientShaper;
    std::unique_ptr<TubeScreamer808> tubeScreamer;
    std::unique_ptr<BigCheeseFuzz> bigCheese;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LZ25AudioProcessor)
};