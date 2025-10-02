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
#include <chowdsp_filters/chowdsp_filters.h>
#include "effects/MxrDynaComp.h"
#include "effects/TubeScreamer808.h"
#include "effects/BigCheeseFuzz.h"
#include "effects/SmartGate.h"
#include "effects/Pitch.h"
#include "effects/TransientShaper.h"

//==============================================================================
// Gain Stage Class
class GainStage
{
public:
    GainStage() = default;
    
    void prepare(double sampleRate, int samplesPerBlock)
    {
        // Coupling capacitor (high-pass filter)
        couplingCap.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        couplingCap.calcCoefs(80.0f, static_cast<float>(sampleRate));
        
        // Grid stopper (low-pass filter)
        gridStopper.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        gridStopper.calcCoefs(8000.0f, static_cast<float>(sampleRate));
        
        // Smooth parameter changes
        gainSmooth.reset(sampleRate, 0.05);
        biasSmooth.reset(sampleRate, 0.05);
    }
    
    void reset()
    {
        couplingCap.reset();
        gridStopper.reset();
    }
    
    void setGain(float newGain)
    {
        gainSmooth.setTargetValue(newGain);
    }
    
    void setTubeModel(int model)
    {
        tubeModelType = model;
    }
    
    float processSample(float input)
    {
        // Apply coupling capacitor (blocks DC)
        float filtered = couplingCap.processSample(input);
        
        // Apply gain
        float gained = filtered * gainSmooth.getNextValue();
        
        // Apply tube saturation based on model
        float saturated = applyTubeSaturation(gained);
        
        // Apply grid stopper (tames high frequencies)
        return gridStopper.processSample(saturated);
    }
    
private:
    float applyTubeSaturation(float input)
    {
        const float bias = biasSmooth.getNextValue();
        float biased = input + bias;
        
        switch (tubeModelType)
        {
            case 0: // Soft (tanh)
                return std::tanh(biased * 1.5f);
                
            case 1: // Medium (arctan)
                return (2.0f / juce::MathConstants<float>::pi) * std::atan(biased * 2.0f);
                
            case 2: // Hard (cubic)
            {
                float x = juce::jlimit(-1.5f, 1.5f, biased * 1.2f);
                return x - (x * x * x) / 3.0f;
            }
                
            case 3: // Asymmetric (push-pull simulation)
            {
                if (biased > 0.0f)
                    return std::tanh(biased * 2.0f) * 0.7f;
                else
                    return std::tanh(biased * 1.5f);
            }
                
            default:
                return std::tanh(biased);
        }
    }
    
    chowdsp::FirstOrderHPF<float> couplingCap;
    chowdsp::FirstOrderLPF<float> gridStopper;
    juce::SmoothedValue<float> gainSmooth { 1.0f };
    juce::SmoothedValue<float> biasSmooth { 0.0f };
    int tubeModelType = 0;
};

//==============================================================================
// Tone Stack Class
class ToneStack
{
public:
    ToneStack() = default;
    
    void prepare(double sampleRate, int samplesPerBlock)
    {
        bassFilter.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        midFilter.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        trebleFilter.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        presenceFilter.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        
        bassFilter.calcCoefs(100.0f, static_cast<float>(sampleRate));
        midFilter.calcCoefs(800.0f, static_cast<float>(sampleRate));
        trebleFilter.calcCoefs(3000.0f, static_cast<float>(sampleRate));
        presenceFilter.calcCoefs(4000.0f, static_cast<float>(sampleRate));
        
        bassSmooth.reset(sampleRate, 0.05);
        midSmooth.reset(sampleRate, 0.05);
        trebleSmooth.reset(sampleRate, 0.05);
        presenceSmooth.reset(sampleRate, 0.05);
    }
    
    void reset()
    {
        bassFilter.reset();
        midFilter.reset();
        trebleFilter.reset();
        presenceFilter.reset();
    }
    
    void setBass(float value) { bassSmooth.setTargetValue(value); }
    void setMid(float value) { midSmooth.setTargetValue(value); }
    void setTreble(float value) { trebleSmooth.setTargetValue(value); }
    void setPresence(float value) { presenceSmooth.setTargetValue(value); }
    
    float processSample(float input)
    {
        // Bass control (low shelf)
        float bass = bassSmooth.getNextValue();
        float bassGain = juce::Decibels::decibelsToGain((bass - 0.5f) * 24.0f);
        float output = input + (bassFilter.processSample(input) - input) * (bassGain - 1.0f);
        
        // Mid control (peaking)
        float mid = midSmooth.getNextValue();
        float midGain = juce::Decibels::decibelsToGain((mid - 0.5f) * 18.0f);
        output += (midFilter.processSample(input) - input) * (midGain - 1.0f);
        
        // Treble control (high shelf)
        float treble = trebleSmooth.getNextValue();
        float trebleGain = juce::Decibels::decibelsToGain((treble - 0.5f) * 20.0f);
        output += (trebleFilter.processSample(input) - input) * (trebleGain - 1.0f);
        
        // Presence boost
        float presence = presenceSmooth.getNextValue();
        float presenceGain = juce::Decibels::decibelsToGain(presence * 12.0f);
        output += presenceFilter.processSample(input) * (presenceGain - 1.0f) * 0.3f;
        
        return output;
    }
    
private:
    chowdsp::FirstOrderLPF<float> bassFilter;
    chowdsp::FirstOrderLPF<float> midFilter;
    chowdsp::FirstOrderHPF<float> trebleFilter;
    chowdsp::FirstOrderHPF<float> presenceFilter;
    
    juce::SmoothedValue<float> bassSmooth { 0.5f };
    juce::SmoothedValue<float> midSmooth { 0.5f };
    juce::SmoothedValue<float> trebleSmooth { 0.5f };
    juce::SmoothedValue<float> presenceSmooth { 0.0f };
};

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
    // New Amp Head Components
    GainStage gainStage1;
    GainStage gainStage2;
    GainStage gainStage3;
    ToneStack toneStack;
    
    // Input/Output filters
    chowdsp::FirstOrderHPF<float> dcBlocker;
    chowdsp::FirstOrderLPF<float> outputFilter;
    
    // Brightness cap simulation
    chowdsp::FirstOrderHPF<float> brightCap;
    
    // Smoothing for master gain
    juce::SmoothedValue<float> masterGainSmooth { 1.0f };

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