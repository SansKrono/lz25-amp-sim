#include "PreFXPanel.h"

PreFXPanel::PreFXPanel(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    // Create effect instances for GUI
    smartGate = std::make_unique<SmartGate>();
    compressor = std::make_unique<MxrDynaComp>();
    tubeScreamer = std::make_unique<TubeScreamer808>();
    bigCheese = std::make_unique<BigCheeseFuzz>();
    
    // Create pedal components
    smartGateComponent = std::make_unique<EffectPedalComponent>(*smartGate, apvts);
    compressorComponent = std::make_unique<EffectPedalComponent>(*compressor, apvts);
    tubeScreamerComponent = std::make_unique<EffectPedalComponent>(*tubeScreamer, apvts);
    bigCheeseComponent = std::make_unique<EffectPedalComponent>(*bigCheese, apvts);
    
    // Add and make visible
    addAndMakeVisible(*smartGateComponent);
    addAndMakeVisible(*compressorComponent);
    addAndMakeVisible(*tubeScreamerComponent);
    addAndMakeVisible(*bigCheeseComponent);
}

PreFXPanel::~PreFXPanel() = default;

void PreFXPanel::paint(juce::Graphics& g)
{
    // Transparent background to inherit parent's styling
    g.fillAll(juce::Colours::transparentBlack);
    
    // Draw section title
    g.setColour(juce::Colour::fromRGB(45, 55, 70));
    g.setFont(juce::FontOptions("Arial", 16.0f, juce::Font::bold));
    
    auto bounds = getLocalBounds();
    auto titleArea = bounds.removeFromTop(30);
}

void PreFXPanel::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for title/header
    bounds.removeFromTop(35);

    // Collect visible pedals in order
    struct PedalEntry { EffectPedal* pedal; EffectPedalComponent* comp; };
    std::vector<PedalEntry> pedals = {
        { smartGate.get(),      smartGateComponent.get() },
        { compressor.get(),     compressorComponent.get() },
        { tubeScreamer.get(),   tubeScreamerComponent.get() },
        { bigCheese.get(),      bigCheeseComponent.get() }
    };

    // Base (unscaled) realistic sizes for each pedal
    std::vector<juce::Rectangle<int>> baseSizes;
    baseSizes.reserve(pedals.size());
    int sumBaseW = 0;
    int maxBaseH = 0;
    for (auto& p : pedals)
    {
        auto s = p.pedal->getRealisticSize(1.0f); // use scale 1.0 as base
        baseSizes.push_back(s);
        sumBaseW += s.getWidth();
        maxBaseH = std::max(maxBaseH, s.getHeight());
    }

    const int n = (int) pedals.size();
    const int availableW = bounds.getWidth();
    const int availableH = bounds.getHeight();

    // Compute global scale limited by height and width
    const int minGap = 16; // desired minimum spacing including edges

    float scaleH = maxBaseH > 0 ? (float) availableH / (float) maxBaseH : 1.0f;
    float scaleW = 1.0f;
    if (sumBaseW > 0)
    {
        const int gaps = n + 1;
        const int minGapsWidth = minGap * gaps;
        scaleW = (float) juce::jmax(0, availableW - minGapsWidth) / (float) sumBaseW;
    }

    // Constrain scale to a sensible range
    float scale = juce::jlimit(0.4f, 2.5f, std::min(scaleH, scaleW));

    // With the chosen scale, recompute spacing to evenly distribute
    const int usedW = (int) std::round(scale * (float) sumBaseW);
    const int totalGaps = n + 1;
    const int leftover = juce::jmax(0, availableW - usedW);
    int spacing = leftover / totalGaps;
    int extra = leftover % totalGaps; // distribute remainder to leftmost gaps

    int x = bounds.getX();

    // Position each pedal, vertically centered
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