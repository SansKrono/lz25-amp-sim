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
// Gain Stage Types Enum
enum class GainStageType
{
    // Classic Tube Amp Emulations
    Fender_12AX7_Clean = 0,      // Fender Bassman/Twin - Clean & articulate
    Marshall_ECC83_Crunch,        // Marshall JCM800 - British crunch
    Mesa_12AX7_HighGain,          // Mesa Boogie - Modern high gain
    Vox_EF86_Bright,              // Vox AC30 - Bright & chimey

    // Vintage Tube Types
    RCA_12AY7_Vintage,            // Lower gain vintage warmth
    GE_12AU7_Jazz,                // Jazz/clean headroom
    Mullard_ECC83_British,        // Classic British tone

    // Modern High Gain
    Peavey_5150_Lead,             // Peavey 5150/6505 - Brutal gain
    Engl_Savage_Modern,           // ENGL Savage - German precision
    Diezel_VH4_Tight,             // Diezel VH4 - Ultra tight

    // Boutique/Specialty
    Dumble_ODS_Smooth,            // Dumble Overdrive Special - Smooth saturation
    Soldano_SLO_Cascade,          // Soldano SLO-100 - Cascading gain
    Bogner_Ecstasy_Warm,          // Bogner Ecstasy - Warm & musical

    // Solid State Emulations
    RolandJC_FET_Clean,           // Roland Jazz Chorus FET - Ultra clean
    Sunn_Transistor_Heavy,        // Sunn Model T - Heavy transistor

    // Hybrid
    Hughes_Kettner_Tube_SS,       // H&K TriAmp - Hybrid character

    NumTypes
};

//==============================================================================
// Gain Stage Configuration Structure
struct GainStageConfig
{
    // Tube/Device characteristics
    float gainFactor;              // Base gain multiplier (mu/amplification factor)
    float saturationCurve;         // How quickly it saturates (0.5-3.0)
    float asymmetry;               // Push-pull asymmetry (0.0-1.0)
    float compressionRatio;        // Dynamic compression (1.0-10.0)
    float harmonicContent;         // Even vs odd harmonics (0.0-1.0)

    // Frequency response
    float couplingCapFreq;         // High-pass coupling cap cutoff (20-200 Hz)
    float gridStopperFreq;         // Low-pass grid stopper (5k-20k Hz)
    float millerCapacitance;       // High-frequency rolloff (0.0-1.0)

    // Bias characteristics
    float biasPoint;               // Operating point (-0.5 to 0.5)
    float biasShift;               // Dynamic bias shift amount (0.0-1.0)

    // Plate/drain characteristics
    float plateResistance;         // Affects headroom (0.5-2.0)
    float sag;                     // Power supply sag simulation (0.0-1.0)

    GainStageConfig() = default;

    // Factory method to create configs for each type
    static GainStageConfig createConfig(GainStageType type);
};

//==============================================================================
// Enhanced Gain Stage Class with Hardware Emulation
class GainStage
{
public:
    GainStage() = default;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;

        // Coupling capacitor (high-pass filter)
        couplingCap.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        updateCouplingCapFrequency();

        // Grid stopper (low-pass filter)
        gridStopper.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        updateGridStopperFrequency();

        // Miller capacitance (additional HF rolloff)
        millerCap.prepare({ sampleRate, (uint32_t)samplesPerBlock, 1 });
        updateMillerCapFrequency();

        // Smooth parameter changes
        gainSmooth.reset(sampleRate, 0.05);
        biasSmooth.reset(sampleRate, 0.05);
        compressionSmooth.reset(sampleRate, 0.02);
        sagEnvelope.reset(sampleRate, 0.01);
    }

    void reset()
    {
        couplingCap.reset();
        gridStopper.reset();
        millerCap.reset();
        sagEnvelope.setCurrentAndTargetValue(0.0f);
        previousSample = 0.0f;
    }

    void setGain(float newGain)
    {
        gainSmooth.setTargetValue(newGain * config.gainFactor);
    }

    void setStageType(GainStageType type)
    {
        currentType = type;
        config = GainStageConfig::createConfig(type);
        updateFilterFrequencies();
    }

    void setTubeModel(int model)
    {
        // Map legacy tube model to gain stage types for backward compatibility
        switch (model)
        {
            case 0: setStageType(GainStageType::Fender_12AX7_Clean); break;
            case 1: setStageType(GainStageType::Marshall_ECC83_Crunch); break;
            case 2: setStageType(GainStageType::Mesa_12AX7_HighGain); break;
            case 3: setStageType(GainStageType::Peavey_5150_Lead); break;
            default: setStageType(GainStageType::Marshall_ECC83_Crunch); break;
        }
    }

    GainStageType getStageType() const { return currentType; }

    float processSample(float input)
    {
        // Apply coupling capacitor (blocks DC)
        float filtered = couplingCap.processSample(input);

        // Dynamic bias shift based on signal amplitude
        updateDynamicBias(filtered);

        // Apply gain with dynamic compression
        float gained = applyDynamicGain(filtered);

        // Apply tube saturation based on configured characteristics
        float saturated = applyTubeSaturation(gained);

        // Apply grid stopper (tames high frequencies)
        saturated = gridStopper.processSample(saturated);

        // Apply Miller capacitance effect (HF rolloff)
        saturated = millerCap.processSample(saturated);

        // Apply power supply sag if configured
        if (config.sag > 0.01f)
            saturated = applySag(saturated);

        previousSample = saturated;
        return saturated;
    }

private:
    void updateFilterFrequencies()
    {
        updateCouplingCapFrequency();
        updateGridStopperFrequency();
        updateMillerCapFrequency();
    }

    void updateCouplingCapFrequency()
    {
        couplingCap.calcCoefs(config.couplingCapFreq, static_cast<float>(currentSampleRate));
    }

    void updateGridStopperFrequency()
    {
        gridStopper.calcCoefs(config.gridStopperFreq, static_cast<float>(currentSampleRate));
    }

    void updateMillerCapFrequency()
    {
        // Miller capacitance creates frequency-dependent gain reduction
        float millerFreq = 10000.0f * (1.0f - config.millerCapacitance * 0.8f);
        millerCap.calcCoefs(millerFreq, static_cast<float>(currentSampleRate));
    }

    void updateDynamicBias(float input)
    {
        // Simulate grid blocking/bias shift under heavy signal
        float envLevel = std::abs(input);
        float biasShift = envLevel * config.biasShift * 0.3f;
        float targetBias = config.biasPoint + biasShift;
        biasSmooth.setTargetValue(targetBias);
    }

    float applyDynamicGain(float input)
    {
        float gain = gainSmooth.getNextValue();
        float inputLevel = std::abs(input);

        // Dynamic compression based on input level
        if (config.compressionRatio > 1.01f)
        {
            float threshold = 0.3f;
            if (inputLevel > threshold)
            {
                float over = inputLevel - threshold;
                float compressed = over / config.compressionRatio;
                float compGain = (threshold + compressed) / inputLevel;
                compressionSmooth.setTargetValue(compGain);
            }
            else
            {
                compressionSmooth.setTargetValue(1.0f);
            }
            gain *= compressionSmooth.getNextValue();
        }

        return input * gain;
    }

    float applyTubeSaturation(float input)
    {
        const float bias = biasSmooth.getNextValue();
        float biased = input + bias;

        // Apply plate resistance (affects headroom)
        biased *= config.plateResistance;

        // Base saturation curve
        float output = applySaturationCurve(biased);

        // Apply asymmetry for push-pull characteristic
        if (config.asymmetry > 0.01f)
        {
            if (biased > 0.0f)
            {
                // Positive half - more compressed
                output *= (1.0f - config.asymmetry * 0.3f);
            }
            else
            {
                // Negative half - less compressed
                output *= (1.0f + config.asymmetry * 0.2f);
            }
        }

        // Add harmonic content character
        output = applyHarmonicColoration(output, input);

        return output;
    }

    float applySaturationCurve(float input)
    {
        float curve = config.saturationCurve;

        // Adjust saturation characteristic based on curve parameter
        if (curve < 1.0f)
        {
            // Soft saturation (more linear)
            return std::tanh(input * curve * 1.5f) / curve;
        }
        else if (curve < 2.0f)
        {
            // Medium saturation
            float x = juce::jlimit(-2.0f, 2.0f, input * curve);
            return (2.0f / juce::MathConstants<float>::pi) * std::atan(x);
        }
        else
        {
            // Hard saturation (more aggressive)
            float x = juce::jlimit(-1.5f, 1.5f, input * curve * 0.8f);
            float cubic = x - (x * x * x) / 3.0f;
            return cubic * 0.9f;
        }
    }

    float applyHarmonicColoration(float output, float input)
    {
        // Mix even and odd harmonics based on configuration
        // harmonicContent: 0.0 = more odd (harsh), 1.0 = more even (warm)

        if (config.harmonicContent > 0.01f)
        {
            // Add subtle even-order harmonics (warm character)
            float even = output * output * std::copysign(1.0f, output) * 0.1f;
            output += even * config.harmonicContent;
        }

        if (config.harmonicContent < 0.99f)
        {
            // Enhance odd-order harmonics (bite/edge)
            float odd = output * output * output * 0.05f;
            output += odd * (1.0f - config.harmonicContent);
        }

        return output;
    }

    float applySag(float input)
    {
        // Power supply sag: high signal levels reduce available voltage
        float envLevel = std::abs(input);
        float sagAmount = envLevel * config.sag;
        sagEnvelope.setTargetValue(sagAmount);

        float sagReduction = 1.0f - (sagEnvelope.getNextValue() * 0.3f);
        return input * sagReduction;
    }

    chowdsp::FirstOrderHPF<float> couplingCap;
    chowdsp::FirstOrderLPF<float> gridStopper;
    chowdsp::FirstOrderLPF<float> millerCap;

    juce::SmoothedValue<float> gainSmooth { 1.0f };
    juce::SmoothedValue<float> biasSmooth { 0.0f };
    juce::SmoothedValue<float> compressionSmooth { 1.0f };
    juce::SmoothedValue<float> sagEnvelope { 0.0f };

    GainStageType currentType = GainStageType::Marshall_ECC83_Crunch;
    GainStageConfig config;
    double currentSampleRate = 44100.0;
    float previousSample = 0.0f;
};

//==============================================================================
// Tone Stack Class - Emulates traditional passive guitar amp tone stack
//==============================================================================
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
        float output = input;
        
        // Bass control (low shelf) - Realistic ±6dB range like real guitar amps
        float bass = bassSmooth.getNextValue();
        if (std::abs(bass - 0.5f) > 0.01f) // Only process if knob is moved from center
        {
            float bassGain = juce::Decibels::decibelsToGain((bass - 0.5f) * 12.0f); // ±6dB
            float bassFiltered = bassFilter.processSample(input);
            output = output + (bassFiltered - input) * (bassGain - 1.0f) * 0.7f; // Gentle mixing
        }
        
        // Mid control (peaking) - Realistic ±4dB range for musical mid control
        float mid = midSmooth.getNextValue();
        if (std::abs(mid - 0.5f) > 0.01f)
        {
            float midGain = juce::Decibels::decibelsToGain((mid - 0.5f) * 8.0f); // ±4dB  
            float midFiltered = midFilter.processSample(input);
            output = output + (midFiltered - input) * (midGain - 1.0f) * 0.5f; // Gentle mixing
        }
        
        // Treble control (high shelf) - Realistic ±6dB range
        float treble = trebleSmooth.getNextValue();
        if (std::abs(treble - 0.5f) > 0.01f)
        {
            float trebleGain = juce::Decibels::decibelsToGain((treble - 0.5f) * 12.0f); // ±6dB
            float trebleFiltered = trebleFilter.processSample(input);
            output = output + (trebleFiltered - input) * (trebleGain - 1.0f) * 0.7f; // Gentle mixing
        }
        
        // Presence boost - Realistic 0 to +3dB boost only (like real amp presence controls)
        float presence = presenceSmooth.getNextValue();
        if (presence > 0.05f) // Only boost, no cut
        {
            float presenceGain = juce::Decibels::decibelsToGain(presence * 6.0f); // 0 to +3dB
            float presenceFiltered = presenceFilter.processSample(input);
            output = output + (presenceFiltered - input) * (presenceGain - 1.0f) * 0.2f; // Very gentle presence
        }
        
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