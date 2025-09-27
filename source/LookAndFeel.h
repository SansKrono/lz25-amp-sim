#pragma once
#include "BinaryData.h"
#include "PluginProcessor.h"
#define M_PI 3.14159265358979323846f

// Modern flat UI color palette - light theme with sci-fi accents
#define PRIMARY_BG juce::Colour::fromRGB (240, 245, 250) // Light blue-grey background
#define SECONDARY_BG juce::Colour::fromRGB (220, 230, 240) // Slightly darker background
#define ACCENT_COLOR juce::Colours::lightsalmon // Bright blue accent
#define TEXT_COLOR juce::Colour::fromRGB (45, 55, 70) // Dark blue-grey text
#define KNOB_FILL juce::Colours::darkgrey // White knob fill
#define KNOB_OUTLINE juce::Colours::transparentBlack // Light grey outline

class MXRLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setKnobSurroundEnabled (bool enabled) { knobSurroundEnabled = enabled; }
    bool isKnobSurroundEnabled() const { return knobSurroundEnabled; }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider&) override
    {
        // Lazy-load the SVG surround from BinaryData (embedded via Assets)
        static std::unique_ptr<juce::Drawable> s_knobSurround;
        if (!s_knobSurround)
        {
            // The resource name is derived from the filename: knob2svg.svg -> knob2svg_svg
            auto svgData = juce::String::fromUTF8 (BinaryData::knob2svg_svg, BinaryData::knob2svg_svgSize);
            if (auto xml = juce::parseXML (svgData))
                s_knobSurround = juce::Drawable::createFromSVG (*xml);
        }

        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f; // Leave margin for outline
        const auto centreX = (float) x + (float) width * 0.5f;
        const auto centreY = (float) y + (float) height * 0.5f;

        const auto rx = centreX - radius;
        const auto ry = centreY - radius;
        const auto rw = radius * 2.0f;

        // Draw knob surround first (behind the knob)
        if (knobSurroundEnabled && s_knobSurround != nullptr)
        {
            // Fit the surround to a square around the knob, slightly larger than the knob itself
            const float surroundSize = juce::jmin ((float) width, (float) height) - 2.0f;
            juce::Rectangle<float> surroundBounds (centreX - surroundSize * 0.5f,
                centreY - surroundSize * 0.5f,
                surroundSize,
                surroundSize);
            s_knobSurround->drawWithin (g, surroundBounds, juce::RectanglePlacement::centred, 1.0f);
        }

        // Draw main knob body - clean white circle with subtle shadow
        g.setColour (KNOB_OUTLINE.withAlpha (0.3f));
        g.fillEllipse (rx + 2, ry + 2, rw, rw); // Shadow

        g.setColour (KNOB_FILL);
        g.fillEllipse (rx, ry, rw, rw);

        g.setColour (KNOB_OUTLINE);
        g.drawEllipse (rx, ry, rw, rw, 2.0f);

        // Draw value arc - modern progress indicator
        const auto arcRadius = radius - 8.0f;
        const auto arcThickness = 4.0f;

        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);

        g.setColour (ACCENT_COLOR);
        g.strokePath (valueArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw subtle track for remaining range
        juce::Path trackArc;
        trackArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, angle, rotaryEndAngle, true);

        g.setColour (KNOB_OUTLINE.withAlpha (0.4f));
        g.strokePath (trackArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw clean pointer - simple line indicator
        const auto pointerLength = radius * 0.6f;
        const auto pointerThickness = 3.0f;

        juce::Path pointer;
        pointer.startNewSubPath (0, -pointerLength);
        pointer.lineTo (0, -radius * 0.2f);

        auto transform = juce::AffineTransform::rotation (angle).translated (centreX, centreY);
        pointer.applyTransform (transform);

        g.setColour (TEXT_COLOR);
        g.strokePath (pointer, juce::PathStrokeType (pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Optional: Add subtle center dot for sci-fi feel
        const auto centerRadius = 3.0f;
        g.setColour (ACCENT_COLOR.withAlpha (0.8f));
        g.fillEllipse (centreX - centerRadius, centreY - centerRadius, centerRadius * 2, centerRadius * 2);
    }

private:
    bool knobSurroundEnabled = true;
};

//==============================================================================
// LookAndFeel for evenly-spread rounded tab headers
class TabsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Reduce spacing and disable overlap so tabs can tile neatly
    int getTabButtonSpaceAroundImage() override { return 4; }
    int getTabButtonOverlap (int) override { return 0; }

    int getTabButtonBestWidth (juce::TabBarButton& button, int /*tabDepth*/) override
    {
        auto& bar = button.getTabbedButtonBar();
        const int numTabs = juce::jmax (1, bar.getNumTabs());
        const bool isVertical = (bar.getOrientation() == juce::TabbedButtonBar::TabsAtLeft
                                 || bar.getOrientation() == juce::TabbedButtonBar::TabsAtRight);
        const int length = isVertical ? bar.getHeight() : bar.getWidth();

        // Evenly divide the available length among tabs
        const int equalLen = juce::jmax (60, length / numTabs); // enforce a sensible minimum
        return equalLen; // TabbedButtonBar will subtract overlap internally
    }

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g) override
    {
        // Transparent/clean background for the bar area
        juce::ignoreUnused (bar);
        g.setColour (juce::Colours::transparentBlack);
        g.fillAll();
    }

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h) override
    {
        juce::ignoreUnused (bar);
        // Draw a subtle separator line under the tabs area to separate from content panel
        // g.setColour(KNOB_OUTLINE.withAlpha(0.6f));
        // g.drawLine(0.0f, (float) h - 0.5f, (float) w, (float) h - 0.5f, 1.0f);
    }

    juce::Font getTabButtonFont (juce::TabBarButton&, float height) override
    {
        return {height * 0.45f, juce::Font::bold};
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        auto area = button.getActiveArea().toFloat();

        const bool isFront = button.isFrontTab();
        constexpr float corner = 8.0f;

        // Colors for different states
        auto base = juce::Colours::darkgrey; // matches overall UI
        auto fill = isFront ? base.brighter (0.1f) : base.darker (0.1f);
        if (isMouseDown)
            fill = fill.darker (0.15f);
        else if (isMouseOver)
            fill = fill.brighter (0.08f);

        // Outer rounded rect (header capsule)
        g.setColour (fill);
        g.fillRoundedRectangle (area.reduced (3.0f), corner);

        // Outline
        // g.setColour(KNOB_OUTLINE);
        // g.drawRoundedRectangle(area.reduced(3.0f), corner, 1.5f);

        // Tab text
        g.setColour (juce::Colours::white);
        const auto f = getTabButtonFont (button, area.getHeight());
        g.setFont (f);
        g.drawFittedText (button.getButtonText(), area.toNearestInt().reduced (8, 4), juce::Justification::centred, 1);
    }
};
