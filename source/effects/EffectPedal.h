#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include "BinaryData.h"

//==============================================================================
/**
 * Base class for all effect pedals in the LZ25 amp simulator.
 * Provides common functionality for enable/disable, parameter management,
 * and GUI layout.
 */
class EffectPedal
{
public:
    //==============================================================================
    enum class EnclosureType
    {
        Enclosure1590B,    // Narrow pedal: 159x308 units (good for 2-3 knobs)
        Enclosure1590BB,   // Wide pedal: 255x327 units (good for 3+ knobs)
        Enclosure1590A,    // Small pedal
        Enclosure125B      // Tiny pedal
    };
    
    struct Parameter
    {
        juce::String id;
        juce::String name;
        float minValue;
        float maxValue;
        float defaultValue;
        
        Parameter(const juce::String& paramId, const juce::String& paramName,
                 float min, float max, float defaultVal)
            : id(paramId), name(paramName), minValue(min), maxValue(max), defaultValue(defaultVal) {}
    };

    //==============================================================================
    EffectPedal(const juce::String& pedalName, const juce::String& pedalId, EnclosureType enclosure = EnclosureType::Enclosure1590B);
    virtual ~EffectPedal() = default;

    //==============================================================================
    // Audio processing interface
    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process(juce::dsp::AudioBlock<float>& block) = 0;

    //==============================================================================
    // Parameter management
    virtual std::vector<Parameter> getParameters() const = 0;
    virtual void updateParameters(juce::AudioProcessorValueTreeState& apvts) = 0;

    //==============================================================================
    // Enable/disable functionality
    void setEnabled(bool shouldBeEnabled) { enabled = shouldBeEnabled; }
    bool isEnabled() const { return enabled; }

    //==============================================================================
    // Pedal information
    const juce::String& getName() const { return name; }
    const juce::String& getId() const { return id; }
    int getParameterCount() const { return static_cast<int>(getParameters().size()); }
    EnclosureType getEnclosureType() const { return enclosureType; }
    
    //==============================================================================
    // Realistic dimensions based on SVG references (scaled for UI)
    static juce::Rectangle<int> getEnclosureDimensions(EnclosureType type, float scale = 0.5f);
    static int getKnobSize(float scale = 0.5f);
    juce::Rectangle<int> getRealisticSize(float scale = 0.6f) const;
    
    // Get appropriate enclosure for number of knobs
    static EnclosureType getRecommendedEnclosure(int knobCount);

    //==============================================================================
    // GUI layout helpers
    static juce::Rectangle<int> calculateKnobLayout(int knobIndex, int totalKnobs, 
                                                   const juce::Rectangle<int>& pedalBounds,
                                                   int knobSize = 60);
    
    static juce::Point<int> getKnobPosition(int knobIndex, int totalKnobs, 
                                           const juce::Rectangle<int>& pedalBounds,
                                           int knobSize = 60);

protected:
    juce::String name;
    juce::String id;
    bool enabled = false;
    EnclosureType enclosureType;

    //==============================================================================
    // Common processing utilities
    static float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }
    static float linearToDb(float linear) { return 20.0f * std::log10(std::max(linear, 1e-6f)); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectPedal)
};

//==============================================================================
/**
 * Custom LookAndFeel for circular ON/OFF buttons
 */
class CircularButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

//==============================================================================
// Knob LookAndFeel for pedals: similar to amp knobs but black with white trim
class PedalKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider&) override
    {
        static std::unique_ptr<juce::Drawable> s_knobSurround;
        if (!s_knobSurround)
        {
            auto svgData = juce::String::fromUTF8(BinaryData::knob2svg_svg, BinaryData::knob2svg_svgSize);
            if (auto xml = juce::parseXML(svgData))
                s_knobSurround = juce::Drawable::createFromSVG(*xml);
        }

        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const auto radius = (float) juce::jmin(width / 2, height / 2) - 4.0f;
        const auto centreX = (float) x + (float) width * 0.5f;
        const auto centreY = (float) y + (float) height * 0.5f;

        const auto rx = centreX - radius;
        const auto ry = centreY - radius;
        const auto rw = radius * 2.0f;

        if (s_knobSurround != nullptr)
        {
            const float surroundSize = juce::jmin((float) width, (float) height) - 2.0f;
            juce::Rectangle<float> surroundBounds(centreX - surroundSize * 0.5f,
                                                  centreY - surroundSize * 0.5f,
                                                  surroundSize, surroundSize);
            s_knobSurround->drawWithin(g, surroundBounds, juce::RectanglePlacement::centred, 1.0f);
        }

        // Shadow/glow
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillEllipse(rx + 2, ry + 2, rw, rw);

        // Main body: black
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(rx, ry, rw, rw);

        // Outline: white
        g.setColour(juce::Colours::lightsalmon);
        g.drawEllipse(rx, ry, rw, rw, 1.0f);

        // Value arc: white
        const auto arcRadius = radius - 8.0f;
        const auto arcThickness = 2.0f;

        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.strokePath(valueArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Track arc: faint white
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f,
                               angle, rotaryEndAngle, true);
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.strokePath(trackArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer: white line
        const auto pointerLength = radius * 0.6f;
        const auto pointerThickness = 3.0f;
        juce::Path pointer;
        pointer.startNewSubPath(0, -pointerLength);
        pointer.lineTo(0, -radius * 0.2f);
        auto transform = juce::AffineTransform::rotation(angle).translated(centreX, centreY);
        pointer.applyTransform(transform);
        g.setColour(juce::Colours::white);
        g.strokePath(pointer, juce::PathStrokeType(pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
/**
 * GUI component for displaying effect pedals with flexible knob layouts
 */
class EffectPedalComponent : public juce::Component
{
public:
    EffectPedalComponent(EffectPedal& pedal, juce::AudioProcessorValueTreeState& apvts);
    ~EffectPedalComponent() override = default;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    void setEnabled(bool shouldBeEnabled);
    bool isEnabled() const { return enableButton.getToggleState(); }

private:
    EffectPedal& effectPedal;
    juce::AudioProcessorValueTreeState& apvts;

    //==============================================================================
    // GUI components
    juce::ToggleButton enableButton;
    std::vector<std::unique_ptr<juce::Slider>> parameterSliders;
    std::vector<std::unique_ptr<juce::Label>> parameterLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;

    // Extra UI for Pitch Range knob (shows interval text above the knob)
    int pitchRangeKnobIndex = -1;
    std::unique_ptr<juce::Label> pitchRangeModeLabel; // visible only for Pitch pedal

    //==============================================================================
    void setupComponents();
    void drawPedalBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    
    // Mapping helper for PITCH_RANGE -> human-readable text
    static juce::String rangeIndexToText(int index)
    {
        switch (index)
        {
            case 0:  return "Dive Bomb";
            case 1:  return "2 Oct Down";
            case 2:  return "Octave Down";
            case 3:  return "5th Down";
            case 4:  return "4th Down";
            case 5:  return "2nd Down";
            case 6:  return "Off";
            case 7:  return "2nd Up";
            case 8:  return "4th Up";
            case 9:  return "5th Up";
            case 10: return "Octave Up";
            case 11: return "2 Oct Up";
            default: return {};
        }
    }

    // Custom look and feel for circular button
    CircularButtonLookAndFeel circularButtonLookAndFeel;
    PedalKnobLookAndFeel pedalKnobLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectPedalComponent)
};