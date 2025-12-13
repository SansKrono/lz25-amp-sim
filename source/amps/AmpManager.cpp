#include "AmpManager.h"
#include "ModernMetalAmp.h"

namespace {
    // A very simple clean amp stub to demonstrate extensibility.
    class CleanAmp : public Amplifier
    {
    public:
        void prepare (const juce::dsp::ProcessSpec& s) override { spec = s; }
        void process (juce::dsp::AudioBlock<float>&) override {}
        void reset () override {}
        void updateParameters (juce::AudioProcessorValueTreeState&) override {}
        juce::String getName () const override { return "Clean"; }
        juce::String getDescription () const override { return "Transparent clean amp"; }
        void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>&) override {}
    };
}

AmpManager::AmpManager()
{
    currentAmp = createAmp (currentModel);
}

void AmpManager::prepare (const juce::dsp::ProcessSpec& specIn)
{
    lastSpec = specIn;

    // Prepare current amp
    if (currentAmp)
        currentAmp->prepare (lastSpec);

    // Prepare safety dry buffer for passthrough fallback
    const int numCh = (int) juce::jmax<juce::uint32> (1u, lastSpec.numChannels);
    const int maxSmps = (int) juce::jmax<juce::uint32> (1u, lastSpec.maximumBlockSize);
    dryBuffer.setSize (numCh, maxSmps);
    dryBuffer.clear();
}

void AmpManager::process (juce::dsp::AudioBlock<float>& block)
{
    if (! currentAmp)
        return;

    const auto numCh = (int) block.getNumChannels();
    const auto numSmps = (int) block.getNumSamples();

    // Ensure dry buffer is large enough for this block
    if (dryBuffer.getNumChannels() < numCh || dryBuffer.getNumSamples() < numSmps)
        dryBuffer.setSize (numCh, numSmps, false, false, true);

    // Copy dry input for potential fallback
    for (int ch = 0; ch < numCh; ++ch)
        std::memcpy (dryBuffer.getWritePointer (ch), block.getChannelPointer ((size_t) ch), (size_t) numSmps * sizeof (float));

    // Compute simple input energy on channel 0
    const float* in0 = block.getChannelPointer (0);
    double preEnergy = 0.0;
    for (int i = 0; i < numSmps; ++i)
        preEnergy += (double) in0[i] * (double) in0[i];

    // Process through the current amplifier
    currentAmp->process (block);

    // Compute output energy and sanity-check for NaN/Inf
    const float* out0 = block.getChannelPointer (0);
    double postEnergy = 0.0;
    bool badSample = false;
    for (int i = 0; i < numSmps; ++i)
    {
        const float s = out0[i];
        if (! std::isfinite (s)) { badSample = true; break; }
        postEnergy += (double) s * (double) s;
    }

    const bool lostSignal = (preEnergy > 1.0e-12 && postEnergy < 1.0e-20);

    if (badSample || lostSignal)
    {
        // Restore dry to prevent breaking the signal chain
        for (int ch = 0; ch < numCh; ++ch)
            std::memcpy (block.getChannelPointer ((size_t) ch), dryBuffer.getReadPointer (ch), (size_t) numSmps * sizeof (float));
    }
}

void AmpManager::reset ()
{
    if (currentAmp)
        currentAmp->reset();
}

void AmpManager::updateParameters (juce::AudioProcessorValueTreeState& apvts)
{
    // Handle model switching parameter if present
    if (auto* raw = apvts.getRawParameterValue ("AMP_MODEL"))
    {
        const int idx = static_cast<int> (raw->load()); // raw value holds the choice index as a float
        AmpModel desired = AmpModel::ModernMetal;
        switch (idx)
        {
            case 0: desired = AmpModel::ModernMetal; break;
            case 1: desired = AmpModel::CleanJazz; break; // mapped to Clean stub for now
            default: desired = AmpModel::ModernMetal; break;
        }

        if (desired != currentModel)
            setCurrentAmp (desired);
    }

    if (currentAmp)
        currentAmp->updateParameters (apvts);
}

void AmpManager::setCurrentAmp (AmpModel model)
{
    currentModel = model;
    currentAmp = createAmp (currentModel);
    if (currentAmp && lastSpec.sampleRate > 0.0)
        currentAmp->prepare (lastSpec);
}

std::unique_ptr<Amplifier> AmpManager::createAmp (AmpModel model)
{
    switch (model)
    {
        case AmpModel::ModernMetal:   return std::make_unique<ModernMetalAmp>();
        case AmpModel::CleanJazz:     return std::make_unique<CleanAmp>();
        case AmpModel::BritishCrunch: return std::make_unique<CleanAmp>();
        case AmpModel::VintageBlues:  return std::make_unique<CleanAmp>();
    }
    return std::make_unique<ModernMetalAmp>();
}

void AmpManager::addParametersForAllAmps (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
{
    // Only ModernMetal has specific parameters right now
    ModernMetalAmp metal;
    metal.addParameters (params);
}

juce::StringArray AmpManager::getAvailableAmpNames () const
{
    return juce::StringArray { "Modern Metal", "Clean" };
}
