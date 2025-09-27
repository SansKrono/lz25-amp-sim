
#pragma once

#include "PluginProcessor.h"

namespace Gui {

    class VerticalGradientMeter : public juce::Component, public juce::Timer
    {
    public:
        VerticalGradientMeter(std::function<float()>&& valueFunction) : _valueSupplier(std::move(valueFunction))
        {
            startTimerHz(24);
        }

        void paint(juce::Graphics& g) override
        {
            const auto meterLevel = _valueSupplier();
            auto bounds = getLocalBounds().toFloat();
            
            // Modern flat background - light with subtle border
            g.setColour(juce::Colour::fromRGB(240, 245, 250));  // PRIMARY_BG
            g.fillRoundedRectangle(bounds, 3.0f);
            
            g.setColour(juce::Colour::fromRGB(180, 190, 200));  // KNOB_OUTLINE
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

            // Calculate meter fill height
            const auto scaledY = juce::jmap(meterLevel, -60.0f, 6.0f, 0.0f, static_cast<float>(getHeight()));
            auto fillBounds = bounds.removeFromBottom(scaledY);
            fillBounds = fillBounds.reduced(2.0f);  // Padding inside border
            
            if (fillBounds.getHeight() > 0)
            {
                // Modern flat color scheme - single accent color with opacity variation
                auto fillColor = juce::Colour::fromRGB(64, 150, 255);  // ACCENT_COLOR
                
                // Vary intensity based on level - more opaque for higher levels
                auto normalizedLevel = juce::jmap(meterLevel, -60.0f, 6.0f, 0.3f, 1.0f);
                g.setColour(fillColor.withAlpha(normalizedLevel));
                g.fillRoundedRectangle(fillBounds, 2.0f);
                
                // Add subtle highlight for sci-fi effect
                auto highlightBounds = fillBounds.removeFromTop (_gradient.getColourPosition (juce::jmap (meterLevel, -60.0f, 6.0f, 0.0f, 1.0f)) * fillBounds.getHeight());
                g.setColour(fillColor.brighter(0.3f).withAlpha(0.5f));
                g.fillRoundedRectangle(highlightBounds, 2.0f);
            }
        }

        void resized() override
        {
            const auto bounds = getLocalBounds().toFloat();
            _gradient = juce::ColourGradient{
                juce::Colours::green,
                bounds.getBottomLeft(),
                juce::Colours::red,
                bounds.getTopLeft(),
                false
            };
            _gradient.addColour(0.5f, juce::Colours::yellow);
        }


        void setLevel(const float value)
        {
            level = value;
        }

        void timerCallback() override
        {
            repaint();
        }

    private:
        std::function<float()> _valueSupplier;
        juce::ColourGradient _gradient{};
        float level = -60.0f;

    };
}
