#include "PostFXPanel.h"

PostFXPanel::PostFXPanel(juce::AudioProcessorValueTreeState& apvts)
{
    // Empty constructor - all parameters moved to AMP tab
    juce::ignoreUnused(apvts);
}

PostFXPanel::~PostFXPanel() = default;

void PostFXPanel::paint(juce::Graphics& g)
{
    // Transparent background to inherit parent's styling
    g.fillAll(juce::Colours::transparentBlack);
    
    // Draw placeholder text
    g.setColour(juce::Colour::fromRGB(45, 55, 70));
    g.setFont(juce::FontOptions("Arial", 18.0f, juce::Font::plain));
    
    auto bounds = getLocalBounds();
    g.drawText("POST FX", bounds.removeFromTop(40), juce::Justification::centred);
    
    g.setFont(juce::FontOptions("Comic Sans MS", 14.0f, juce::Font::plain));
    g.setColour(juce::Colours::white);
    g.drawText("Future home for post-amp effects:", bounds.removeFromTop(30), juce::Justification::centred);
    g.drawText("- Delay", bounds.removeFromTop(25), juce::Justification::centred);
    g.drawText("- Reverb", bounds.removeFromTop(25), juce::Justification::centred);
    g.drawText("- Chorus", bounds.removeFromTop(25), juce::Justification::centred);
    g.drawText("- Modulation", bounds.removeFromTop(25), juce::Justification::centred);
}

void PostFXPanel::resized()
{
    // No components to resize - placeholder only
}