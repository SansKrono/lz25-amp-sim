#include "PitchDynamicsPanel.h"

PitchDynamicsPanel::PitchDynamicsPanel(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    // Create effect instances for GUI
    pitch = std::make_unique<Pitch>();
    smartGate = std::make_unique<SmartGate>();
    transientShaper = std::make_unique<TransientShaper>();
    compressor = std::make_unique<MxrDynaComp>();

    // Create pedal components
    pitchComponent = std::make_unique<EffectPedalComponent>(*pitch, apvts);
    smartGateComponent = std::make_unique<EffectPedalComponent>(*smartGate, apvts);
    transientShaperComponent = std::make_unique<EffectPedalComponent>(*transientShaper, apvts);
    compressorComponent = std::make_unique<EffectPedalComponent>(*compressor, apvts);

    // Add and make visible in processing order
    addAndMakeVisible(*pitchComponent);
    addAndMakeVisible(*smartGateComponent);
    addAndMakeVisible(*transientShaperComponent);
    addAndMakeVisible(*compressorComponent);
}

PitchDynamicsPanel::~PitchDynamicsPanel() = default;

void PitchDynamicsPanel::paint(juce::Graphics& g)
{
    // Transparent background to inherit parent's styling
    g.fillAll(juce::Colours::transparentBlack);
}

void PitchDynamicsPanel::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for title/header (consistency with PreFXPanel)
    bounds.removeFromTop(35);

    struct PedalEntry { EffectPedal* pedal; EffectPedalComponent* comp; };
    std::vector<PedalEntry> pedals = {
        { pitch.get(),          pitchComponent.get() },
        { smartGate.get(),      smartGateComponent.get() },
        { transientShaper.get(),transientShaperComponent.get() },
        { compressor.get(),     compressorComponent.get() }
    };

    // Base (unscaled) realistic sizes for each pedal
    std::vector<juce::Rectangle<int>> baseSizes;
    baseSizes.reserve(pedals.size());
    int sumBaseW = 0;
    int maxBaseH = 0;
    for (auto& p : pedals)
    {
        auto s = p.pedal->getRealisticSize(1.0f);
        baseSizes.push_back(s);
        sumBaseW += s.getWidth();
        maxBaseH = std::max(maxBaseH, s.getHeight());
    }

    const int n = (int) pedals.size();
    const int availableW = bounds.getWidth();
    const int availableH = bounds.getHeight();

    const int minGap = 16;

    float scaleH = maxBaseH > 0 ? (float) availableH / (float) maxBaseH : 1.0f;
    float scaleW = 1.0f;
    if (sumBaseW > 0)
    {
        const int gaps = n + 1;
        const int minGapsWidth = minGap * gaps;
        scaleW = (float) juce::jmax(0, availableW - minGapsWidth) / (float) sumBaseW;
    }

    float scale = juce::jlimit(0.4f, 2.5f, std::min(scaleH, scaleW));

    const int usedW = (int) std::round(scale * (float) sumBaseW);
    const int totalGaps = n + 1;
    const int leftover = juce::jmax(0, availableW - usedW);
    int spacing = leftover / totalGaps;
    int extra = leftover % totalGaps;

    int x = bounds.getX();
    for (int i = 0; i < n; ++i)
    {
        int thisGap = spacing + (i < extra ? 1 : 0);
        x += thisGap;

        const auto& base = baseSizes[(size_t) i];
        const int w = (int) std::round(base.getWidth() * scale);
        const int h = (int) std::round(base.getHeight() * scale);
        const int y = bounds.getY() + (availableH - h) / 2;

        pedals[(size_t) i].comp->setBounds(x, y, w, h);
        x += w;
    }
}