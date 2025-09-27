#include "EffectPedal.h"

//==============================================================================
// CircularButtonLookAndFeel implementation
//==============================================================================
void CircularButtonLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                 bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    
    // Make the button area square and centered
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto circleArea = juce::Rectangle<float>(size, size).withCentre(bounds.getCentre());
    
    // Reduce the circle size slightly for better visual appearance
    circleArea = circleArea.reduced(2.0f);
    
    // Choose color based on toggle state
    juce::Colour fillColour = button.getToggleState() ? juce::Colours::red : juce::Colours::black;
    
    // Add subtle highlight when hovered
    if (shouldDrawButtonAsHighlighted)
        fillColour = fillColour.brighter(0.1f);
    
    // Add pressed state visual feedback
    if (shouldDrawButtonAsDown)
        fillColour = fillColour.darker(0.2f);
    
    // Draw the filled circle
    g.setColour(fillColour);
    g.fillEllipse(circleArea);
    
    // Draw a subtle border for better definition
    g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));
    g.drawEllipse(circleArea, 1.0f);
    
    // Draw the "ON" text below the circle
    auto textArea = bounds.removeFromBottom(bounds.getHeight() * 0.3f);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("ON", textArea, juce::Justification::centred);
}

//==============================================================================
// EffectPedal implementation
//==============================================================================
EffectPedal::EffectPedal(const juce::String& pedalName, const juce::String& pedalId, EnclosureType enclosure)
    : name(pedalName), id(pedalId), enclosureType(enclosure)
{
}

juce::Rectangle<int> EffectPedal::getEnclosureDimensions(EnclosureType type, float scale)
{
    // SVG dimensions converted to UI-appropriate sizes
    switch (type)
    {
        case EnclosureType::Enclosure1590B:
            // SVG: 159x308 -> scaled for UI
            return juce::Rectangle<int>(0, 0, 
                static_cast<int>(159.0f * scale), 
                static_cast<int>(308.0f * scale));
            
        case EnclosureType::Enclosure1590BB:
            // SVG: 255x327 -> scaled for UI  
            return juce::Rectangle<int>(0, 0,
                static_cast<int>(255.0f * scale),
                static_cast<int>(327.0f * scale));
                
        case EnclosureType::Enclosure1590A:
            // Smaller enclosure - estimated dimensions
            return juce::Rectangle<int>(0, 0,
                static_cast<int>(93.0f * scale),
                static_cast<int>(120.0f * scale));
                
        case EnclosureType::Enclosure125B:
            // Tiny enclosure - estimated dimensions  
            return juce::Rectangle<int>(0, 0,
                static_cast<int>(70.0f * scale),
                static_cast<int>(95.0f * scale));
                
        default:
            return getEnclosureDimensions(EnclosureType::Enclosure1590B, scale);
    }
}

int EffectPedal::getKnobSize(float scale)
{
    // Based on knob2svg.svg viewBox: 70.86x66.45 -> use average and scale
    return static_cast<int>((70.86f + 66.45f) / 2.0f * scale);
}

juce::Rectangle<int> EffectPedal::getRealisticSize(float scale) const
{
    return getEnclosureDimensions(enclosureType, scale);
}

EffectPedal::EnclosureType EffectPedal::getRecommendedEnclosure(int knobCount)
{
    if (knobCount <= 2)
        return EnclosureType::Enclosure1590B;  // Narrow for 1-2 knobs
    else if (knobCount <= 3)
        return EnclosureType::Enclosure1590BB; // Wide for 3+ knobs
    else
        return EnclosureType::Enclosure1590BB; // Use wide for many knobs
}

juce::Rectangle<int> EffectPedal::calculateKnobLayout(int knobIndex, int totalKnobs, 
                                                     const juce::Rectangle<int>& pedalBounds,
                                                     int knobSize)
{
    auto position = getKnobPosition(knobIndex, totalKnobs, pedalBounds, knobSize);
    return juce::Rectangle<int>(position.x, position.y, knobSize, knobSize);
}

juce::Point<int> EffectPedal::getKnobPosition(int knobIndex, int totalKnobs, 
                                             const juce::Rectangle<int>& pedalBounds,
                                             int knobSize)
{
    // Leave space for pedal name at top and enable button at bottom
    auto knobArea = pedalBounds.reduced(10);
    knobArea.removeFromTop(30); // Space for pedal name
    knobArea.removeFromBottom(40); // Space for enable button
    
    int x, y;
    
    if (totalKnobs <= 3)
    {
        // Single row layout for 1-3 knobs
        int totalWidth = totalKnobs * knobSize + (totalKnobs - 1) * 10;
        int startX = knobArea.getCentreX() - totalWidth / 2;
        
        x = startX + knobIndex * (knobSize + 10);
        y = knobArea.getCentreY() - knobSize / 2;
    }
    else
    {
        // Two row layout for 4+ knobs
        int knobsPerRow = (totalKnobs + 1) / 2; // Distribute evenly
        int row = knobIndex / knobsPerRow;
        int col = knobIndex % knobsPerRow;
        
        int totalWidth = knobsPerRow * knobSize + (knobsPerRow - 1) * 10;
        int startX = knobArea.getCentreX() - totalWidth / 2;
        
        x = startX + col * (knobSize + 10);
        y = knobArea.getY() + row * (knobSize + 15) + 10;
    }
    
    return juce::Point<int>(x, y);
}

//==============================================================================
// EffectPedalComponent implementation
//==============================================================================
EffectPedalComponent::EffectPedalComponent(EffectPedal& pedal, juce::AudioProcessorValueTreeState& apvts)
    : effectPedal(pedal), apvts(apvts)
{
    setupComponents();
}

void EffectPedalComponent::setupComponents()
{
    // Helper to provide descriptive tooltips per pedal parameter
    auto tooltipFor = [this](const juce::String& paramId) -> juce::String
    {
        // TS808
        if (paramId == "TS808_DRIVE")   return "Tube Screamer drive amount. Turn up for more diode clipping, sustain, and mid push; turn down for a cleaner boost.";
        if (paramId == "TS808_TONE")    return "Post-clip tone tilt. Turn up for a brighter, leaner sound; turn down for darker, thicker lows.";
        if (paramId == "TS808_LEVEL")   return "TS808 output level. Turn up to hit the amp harder (more amp drive); turn down for unity gain.";

        // Dyna Comp
        if (paramId == "COMP_SENSITIVITY") return "Compressor sensitivity/threshold. Turn up for more clamp and sustain (less transient); turn down for more dynamics.";
        if (paramId == "COMP_OUTPUT")     return "Compressor makeup/output level. Turn up to recover volume or push the amp; turn down to match bypass.";

        // Smart Gate
        if (paramId == "SMARTGATE_INTENSITY") return "Gate clamping intensity. Turn up for faster, tighter closing on decays; turn down for more natural sustain.";
        if (paramId == "SMARTGATE_REDUCTION") return "Maximum noise reduction when closed. Turn up for deeper silencing; turn down to leave some room noise.";
        if (paramId == "SMARTGATE_RELEASE")   return "Gate release speed. Turn up for a faster open/snappier feel; turn down for a slower, smoother release.";
        if (paramId == "SMARTGATE_DJENT")     return "Aggressive gate mode. Turn up/on for ultra-tight chugs (faster clamp, higher reduction); turn down/off for natural gating.";

        // Big Cheese
        if (paramId == "BIGCHEESE_FUZZ")   return "Fuzz gain into the transistor/op-amp stages. Turn up for thicker, nastier saturation; turn down for milder bite.";
        if (paramId == "BIGCHEESE_TONE")   return "Tone voicing. Turn up for brighter, cutting highs; turn down for darker, heavier lows.";
        if (paramId == "BIGCHEESE_VOLUME") return "Fuzz output level. Turn up to hit the amp harder; turn down for unity.";
        if (paramId == "BIGCHEESE_TRIM")   return "Input trim (pre-gain) into the fuzz. Turn up to drive harder (more compression/sizzle); turn down for cleaner articulation.";
        if (paramId == "BIGCHEESE_SWITCH") return "Mode selector (0–3) for voicing/clipping. Turn up for tighter/raspier modes; turn down for smoother/softer modes.";

        return {};
    };

    // Setup enable button with custom circular look and feel
    addAndMakeVisible(enableButton);
    enableButton.setButtonText("ON");
    enableButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    enableButton.setToggleState(effectPedal.isEnabled(), juce::dontSendNotification);
    enableButton.setLookAndFeel(&circularButtonLookAndFeel);
    
    // Create enable button attachment
    juce::String enableParamId = effectPedal.getId() + "_ENABLE";
    enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, enableParamId, enableButton);
    
    enableButton.onStateChange = [this]() {
        effectPedal.setEnabled(enableButton.getToggleState());
    };
    
    // Setup parameter sliders and labels
    auto parameters = effectPedal.getParameters();
    
    parameterSliders.clear();
    parameterLabels.clear();
    sliderAttachments.clear();
    
    for (const auto& param : parameters)
    {
        // Create slider
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setRange(param.minValue, param.maxValue);
        slider->setValue(param.defaultValue);
        slider->setDoubleClickReturnValue(true, param.defaultValue);
        
        // Apply pedal knob look and feel (black with white trim)
        slider->setLookAndFeel(&pedalKnobLookAndFeel);
        // Fallback colours (won't be used by our custom LAF draw routine)
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::black);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        
        // Tooltip for parameter
        if (auto tip = tooltipFor(param.id); tip.isNotEmpty())
            slider->setTooltip(tip);
        
        addAndMakeVisible(*slider);
        
        // Create label
        auto label = std::make_unique<juce::Label>();
        label->setText(param.name, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(*label);
        
        // Create parameter attachment
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, param.id, *slider);
        
        // Store components
        parameterSliders.push_back(std::move(slider));
        parameterLabels.push_back(std::move(label));
        sliderAttachments.push_back(std::move(attachment));
    }

    // Add a universal Mix control for all pedals
    {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setRange(0.0f, 1.0f);
        slider->setValue(1.0f);
        slider->setDoubleClickReturnValue(true, 1.0f);
        slider->setLookAndFeel(&pedalKnobLookAndFeel);
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::black);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white);
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        
        // Tooltip for MIX
        slider->setTooltip("Dry/Wet blend for this pedal. Turn up for more effect; turn down for more dry signal.");
        
        addAndMakeVisible(*slider);

        auto label = std::make_unique<juce::Label>();
        label->setText("MIX", juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(*label);

        juce::String mixParamId = effectPedal.getId() + "_MIX";
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, mixParamId, *slider);

        parameterSliders.push_back(std::move(slider));
        parameterLabels.push_back(std::move(label));
        sliderAttachments.push_back(std::move(attachment));
    }
}

void EffectPedalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    drawPedalBackground(g, bounds);
    
    // Draw pedal name
    g.setColour(juce::Colours::white);
    const juce::FontOptions fontOptions(12.0f, juce::Font::bold);
    g.setFont(fontOptions);
    auto nameArea = bounds.removeFromTop(25);
    g.drawText(effectPedal.getName(), nameArea, juce::Justification::centred);
}

void EffectPedalComponent::drawPedalBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    // Draw pedal-style background
    auto pedalBounds = bounds.toFloat().reduced(2.0f);
    
    // Main pedal body
    g.setColour(juce::Colour::fromRGB(80, 80, 80)); // Dark grey pedal body
    g.fillRoundedRectangle(pedalBounds, 8.0f);
    
    // Pedal border/highlight
    g.setColour(juce::Colour::fromRGB(120, 120, 120));
    g.drawRoundedRectangle(pedalBounds, 8.0f, 2.0f);
    
    // Top highlight for 3D effect
    g.setColour(juce::Colour::fromRGB(140, 140, 140).withAlpha(0.5f));
    auto topHighlight = pedalBounds.removeFromTop(pedalBounds.getHeight() * 0.3f);
    g.fillRoundedRectangle(topHighlight, 8.0f);
    
    // Bottom shadow for depth
    g.setColour(juce::Colour::fromRGB(40, 40, 40).withAlpha(0.7f));
    auto bottomShadow = pedalBounds.removeFromBottom(3.0f);
    g.fillRoundedRectangle(bottomShadow, 4.0f);
}

void EffectPedalComponent::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for pedal name
    auto nameArea = bounds.removeFromTop(25);
    juce::ignoreUnused(nameArea);

    // Reserve space for enable button at bottom (slightly larger to avoid overlap)
    auto enableArea = bounds.removeFromBottom(45);
    enableButton.setBounds(enableArea.reduced(10));

    // Remaining area for knobs
    auto knobArea = bounds.reduced(10);

    const int totalKnobs = static_cast<int>(parameterSliders.size());
    if (totalKnobs == 0)
        return;

    // Determine rows and columns
    const int rows = (totalKnobs <= 3) ? 1 : 2;
    const int knobsInTopRow = (rows == 1) ? totalKnobs : (totalKnobs + 1) / 2; // ceil
    const int knobsInBottomRow = (rows == 1) ? 0 : (totalKnobs - knobsInTopRow);

    constexpr int hSpacing = 10;
    constexpr int vSpacing = 18;
    constexpr int labelH = 14;

    // Compute max knob sizes that fit both width and height constraints
    const float knobSizeTopW = (knobArea.getWidth() - juce::jmax(0, knobsInTopRow - 1) * hSpacing) / static_cast<float> (juce::jmax (1, knobsInTopRow));
    const float knobSizeBotW = (rows == 2 && knobsInBottomRow > 0)
                                ? (knobArea.getWidth() - juce::jmax(0, knobsInBottomRow - 1) * hSpacing) / static_cast<float> (knobsInBottomRow)
                                : 10000.0f; // no bottom row constraint
    const int rowHeights = rows * (labelH + vSpacing); // label + spacing per row (spacing after row except last, handled below)
    const float knobSizeH = (knobArea.getHeight() - rowHeights + vSpacing) / static_cast<float> (rows); // add back spacing after last row

    const int knobSize = static_cast<int> (juce::jlimit (24.0f,
        120.0f,
        std::floor (juce::jmin (knobSizeTopW, knobSizeBotW, knobSizeH))));

    // Layout function for a given row
    auto layoutRow = [&] (int rowIndex, int knobsInRow, int yStart)
    {
        if (knobsInRow <= 0) return;
        const int totalRowWidth = knobsInRow * knobSize + (knobsInRow - 1) * hSpacing;
        int startX = knobArea.getX() + (knobArea.getWidth() - totalRowWidth) / 2;
        for (int i = 0; i < knobsInRow; ++i)
        {
            const int x = startX + i * (knobSize + hSpacing);
            const int y = yStart;
            const int idx = (rowIndex == 0 ? i : knobsInTopRow + i);
            if (idx >= totalKnobs) break;
            juce::Rectangle<int> knobBounds(x, y, knobSize, knobSize);
            parameterSliders[idx]->setBounds(knobBounds);

            juce::Rectangle<int> labelBounds(x, y + knobSize + 2, knobSize, labelH);
            parameterLabels[idx]->setBounds(labelBounds);
        }
    };

    // Compute Y positions for rows
    const int contentHeight = rows * knobSize + (rows - 1) * vSpacing + rows * labelH;
    int y0 = knobArea.getY() + (knobArea.getHeight() - contentHeight) / 2;
    int y1 = y0 + knobSize + labelH + vSpacing;

    layoutRow(0, knobsInTopRow, y0);
    if (rows == 2)
        layoutRow(1, knobsInBottomRow, y1);
}

void EffectPedalComponent::setEnabled(bool shouldBeEnabled)
{
    enableButton.setToggleState(shouldBeEnabled, juce::sendNotification);
}