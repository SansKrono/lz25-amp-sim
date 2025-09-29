#include "Pitch.h"

Pitch::Pitch()
: EffectPedal("Pitch", "PITCH", EffectPedal::EnclosureType::Enclosure1590B)
{
}

void Pitch::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    maxBlock = (int) spec.maximumBlockSize;

    maxDelaySamples = (int) std::round(maxDelayMs * 0.001f * sampleRate);
    maxDelaySamples = juce::jmax(maxDelaySamples, 1);

    // Circular buffer: allow 1 second to be safe
    bufferSize = (int) juce::jmax<int>((int) sampleRate, maxDelaySamples * 4 + maxBlock + 8);

    channels.resize((size_t) spec.numChannels);
    for (auto& ch : channels)
    {
        ch.buffer.assign((size_t) bufferSize, 0.0f);
        ch.writePos = 0;
        ch.phase = 0.0f;
    }
}

void Pitch::reset()
{
    for (auto& ch : channels)
    {
        std::fill(ch.buffer.begin(), ch.buffer.end(), 0.0f);
        ch.writePos = 0;
        ch.phase = 0.0f;
    }
}

/**
 * Fetches and returns the set of parameters for use in configuration or execution.
 *
 * This method provides access to a collection of parameters that are relevant
 * to the current operation or setup.
 *
 * @return A collection containing the relevant parameters.
 */
std::vector<EffectPedal::Parameter> Pitch::getParameters() const
{
    // Expose three knobs: PITCH (quantised semis), RANGE (discrete index), SHIFT (0..1 morph)
    return {
        Parameter{"PITCH_PITCH", "PITCH", -12.0f, 12.0f, 0.0f},  // integer semitones
        Parameter{"PITCH_RANGE", "RANGE", 0.0f, 11.0f, 6.0f},    // discrete selector index (12 positions, Off at center)
        Parameter{"PITCH_SHIFT", "SHIFT", 0.0f, 1.0f, 0.0f}      // continuous morph 0..1
    };
}

/**
 * Updates the existing parameters with the provided values.
 *
 * This method modifies the current set of parameters by incorporating
 * the given values, allowing for dynamic reconfiguration or adjustment.
 *
 * @param newParameters A collection containing the new parameter values to be updated.
 */
void Pitch::updateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // Quantised pitch knob (-12..+12 integer)
    const float pitchParam = *apvts.getRawParameterValue("PITCH_PITCH");
    currentPitchSemis = clampRoundToInt(pitchParam, -12, 12);

    // Discrete range index (0..11) with Off in the middle (index 6)
    const float rangeParam = *apvts.getRawParameterValue("PITCH_RANGE");
    currentRangeIndex = clampRoundToInt(rangeParam, 0, 11);

    // Continuous morph 0..1
    currentShiftValue = *apvts.getRawParameterValue("PITCH_SHIFT");

    // Determine target semitones from range index
    const float targetSemis = indexToTargetSemis(currentRangeIndex);

    // Effective shift = base pitch (integer) + linear interpolation to target
    const float shiftSemis = juce::jlimit(0.0f, 1.0f, currentShiftValue) * targetSemis; // linear in knob space
    currentShiftSemis = (float) currentPitchSemis + shiftSemis;
}

/**
 * Reads and processes a linear data structure for further use or computation.
 *
 * This method handles the input of linear data, processes it as necessary,
 * and prepares it for the required operations.
 *
 * @param input The linear data structure to be read and processed.
 * @return The result or processed form of the linear data.
 */
float Pitch::indexToTargetSemis(int rangeIndex) const
{
    // Map discrete range index to target semitone intervals (includes up/down variants and Dive Bomb)
    switch (rangeIndex)
    {
        case 0:  return (float) diveBombSemis; // Dive Bomb (extreme down)
        case 1:  return -24.0f;   // 2 Oct Down
        case 2:  return -12.0f;   // Oct Down
        case 3:  return  -7.0f;   // 5th Down
        case 4:  return  -5.0f;   // 4th Down
        case 5:  return  -2.0f;   // 2nd Down
        case 6:  return   0.0f;   // Off
        case 7:  return  +2.0f;   // 2nd Up
        case 8:  return  +5.0f;   // 4th Up
        case 9:  return  +7.0f;   // 5th Up
        case 10: return +12.0f;   // Oct Up
        case 11: return +24.0f;   // 2 Oct Up
        default: return 0.0f;
    }
}

float Pitch::readLinear(const std::vector<float>& buf, float index) const
{
    // index may be outside [0, bufferSize), wrap and interpolate
    int size = (int) buf.size();
    while (index < 0.0f) index += (float) size;
    while (index >= (float) size) index -= (float) size;

    int i0 = (int) index;
    int i1 = (i0 + 1) % size;
    float frac = index - (float) i0;
    return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
}

/**
 * Executes the main processing logic of the application or component.
 *
 * This method is responsible for performing the core functionality, which
 * may involve data transformation, task execution, or handling operational logic.
 *
 * @param input The input data required to perform the processing.
 * @param context Additional context or configuration necessary for the process.
 * @return The result of the processing operation.
 */
void Pitch::process(juce::dsp::AudioBlock<float>& block)
{
    if (! isEnabled())
        return;

    const int numCh = (int) block.getNumChannels();
    const int numSmps = (int) block.getNumSamples();

    // Bypass at/near zero shift to avoid combing artifacts
    if (std::abs(currentShiftSemis) < 0.01f)
        return;

    const float ratio = std::pow(2.0f, currentShiftSemis / 12.0f);
    const float dd = 1.0f / juce::jmax(1.0e-6f, ratio) - 1.0f; // desired delay slope in samples/sample
    const float slope = std::abs(dd);
    const float phaseInc = juce::jlimit(0.0f, 0.5f, slope / (float) juce::jmax(1, maxDelaySamples)); // advance phase in 0..1

    for (int chIdx = 0; chIdx < numCh; ++chIdx)
    {
        auto* channelData = block.getChannelPointer((size_t) chIdx);
        auto& ch = channels[(size_t) chIdx % channels.size()];

        for (int n = 0; n < numSmps; ++n)
        {
            const float in = channelData[n];

            // Write input to delay buffer
            ch.buffer[(size_t) ch.writePos] = in;

            // Phases for two read heads, 0..1
            float t1 = ch.phase;
            float t2 = t1 + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;

            // Compute delay in samples depending on pitch direction
            float D1 = (dd < 0.0f ? (1.0f - t1) : t1) * (float) maxDelaySamples;
            float D2 = (dd < 0.0f ? (1.0f - t2) : t2) * (float) maxDelaySamples;

            // Read positions
            float rp1 = (float) ch.writePos - D1;
            float rp2 = (float) ch.writePos - D2;

            float s1 = readLinear(ch.buffer, rp1);
            float s2 = readLinear(ch.buffer, rp2);

            // Crossfade windows (Hann), normalize to avoid dips/peaks
            float w1 = hann(t1);
            float w2 = hann(t2);
            float wsum = juce::jmax(1.0e-6f, w1 + w2);
            float out = (w1 * s1 + w2 * s2) / wsum;

            channelData[n] = out;

            // Advance write and phase
            if (++ch.writePos >= bufferSize) ch.writePos = 0;
            ch.phase += phaseInc;
            if (ch.phase >= 1.0f) ch.phase -= 1.0f;
        }
    }
}
