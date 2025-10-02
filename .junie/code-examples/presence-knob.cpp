//==============================================================================
// Alternative: State-Variable Implementation for More Authentic Negative Feedback
//==============================================================================

/**
 * Advanced Presence Control using negative feedback topology
 *
 * This version more closely models the actual negative feedback circuit
 * by implementing a feedback path with frequency-dependent attenuation.
 */
class PresenceControlAdvanced
{
public:
    PresenceControlAdvanced() = default;

    void prepare (double sampleRate, int samplesPerBlock)
    {
        fs = sampleRate;

        // Feedback path filters
        feedbackHPF.prepare ({ sampleRate, (uint32) samplesPerBlock, 1 });
        feedbackLPF.prepare ({ sampleRate, (uint32) samplesPerBlock, 1 });

        // Output shaping
        outputShelf.prepare ({ sampleRate, (uint32) samplesPerBlock, 1 });

        presenceSmooth.reset (sampleRate, 0.05);

        updateFilters();
    }

    void reset()
    {
        feedbackHPF.reset();
        feedbackLPF.reset();
        outputShelf.reset();
        feedbackMemory = 0.0f;
        presenceSmooth.setCurrentAndTargetValue (0.0f);
    }

    void setPresence (float value)
    {
        value = juce::jlimit (-1.0f, 1.0f, value);
        if (std::abs (presence - value) > 1.0e-6f)
        {
            presence = value;
            presenceSmooth.setTargetValue (value);
            updateFilters();
        }
    }

    float processSample (float input)
    {
        float p = presenceSmooth.getNextValue();

        // Calculate feedback amount (inverted for presence control)
        // High presence = low feedback amount
        float feedbackAmount = 0.15f * (1.0f - (p * 0.5f + 0.5f));  // 0.075..0.15

        // Mix input with attenuated feedback
        float mixed = input - (feedbackMemory * feedbackAmount);

        // Process through amp simulation (simplified here)
        float amplified = std::tanh (mixed * 2.0f);

        // Create feedback signal (band-passed)
        float feedback = feedbackHPF.processSample (amplified);
        feedback = feedbackLPF.processSample (feedback);

        // Store for next sample
        feedbackMemory = feedback;

        // Apply output shaping
        float output = outputShelf.processSample (amplified);

        return output;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                channelData[sample] = processSample (channelData[sample]);
            }
        }
    }

private:
    void updateFilters()
    {
        if (fs <= 0.0)
            return;

        // Feedback path is typically band-limited
        // High-pass: Remove subsonic content from feedback
        feedbackHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            fs,
            200.0f,
            0.707f
        );

        // Low-pass: Variable cutoff based on presence
        // More presence = higher cutoff = more high frequencies fed back
        float fbCutoff = 3000.0f + presence * 4000.0f;  // 3kHz..7kHz
        feedbackLPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            fs,
            fbCutoff,
            0.707f
        );

        // Output shaping to compensate for feedback effects
        float shelfGain = presence * 4.0f;  // ±4dB
        outputShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            fs,
            4000.0f,
            0.707f,
            juce::Decibels::decibelsToGain (shelfGain)
        );
    }

    float presence = 0.0f;
    double fs = 0.0;
    float feedbackMemory = 0.0f;

    juce::dsp::IIR::Filter<float> feedbackHPF;
    juce::dsp::IIR::Filter<float> feedbackLPF;
    juce::dsp::IIR::Filter<float> outputShelf;

    juce::SmoothedValue<float> presenceSmooth { 0.0f };
};


//==============================================================================
// Integration Example with Your Existing Code
//==============================================================================

/**
 * How to integrate presence control into your ToneStack class
 */
class ToneStackWithPresence
{
public:
    void prepare (double sampleRate, int samplesPerBlock)
    {
        toneStack.prepare (sampleRate, samplesPerBlock);
        presence.prepare (sampleRate, samplesPerBlock);
    }

    void reset()
    {
        toneStack.reset();
        presence.reset();
    }

    // Tone stack controls (affect mids and overall tone)
    void setTreble (float value) { toneStack.setTreble (value); }
    void setMid (float value) { toneStack.setMid (value); }
    void setBass (float value) { toneStack.setBass (value); }

    // Presence control (separate from tone stack, affects upper highs only)
    void setPresence (float value) { presence.setPresence (value); }

    float processSample (float sample)
    {
        // Process through tone stack first (bass, mid, treble)
        float output = toneStack.processSample (sample);

        // Then apply presence (negative feedback simulation)
        // This is typically done AFTER the tone stack in real amps
        output = presence.processSample (output);

        return output;
    }

private:
    // Your existing tone stack (placeholder - use your actual ToneStack class)
    struct ToneStack {
        void prepare(double, int) {}
        void reset() {}
        void setTreble(float) {}
        void setMid(float) {}
        void setBass(float) {}
        float processSample(float x) { return x; }
    } toneStack;

    PresenceControl presence;
};