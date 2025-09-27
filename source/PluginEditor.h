
#pragma once
#include "AmpPanel.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "PostFXPanel.h"
#include "PreFXPanel.h"
#include "VerticalGradientMeter.h"

//==============================================================================

class LZ25AudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    LZ25AudioProcessorEditor (LZ25AudioProcessor&);
    ~LZ25AudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void fileLoader();

private:
    juce::LookAndFeel_V4 _lookAndFeel;
    MXRLookAndFeel _sliderLookAndFeel;
    TabsLookAndFeel _tabsLookAndFeel;

    Gui::VerticalGradientMeter _meterOutput;

    juce::TextButton _loadButton;
    juce::TextButton _prevButton;
    juce::TextButton _nextButton;
    juce::Label _irName;
    juce::Image _backgroundImage;

    // Tooltip manager for delayed hints
    juce::TooltipWindow _tooltipWindow; // configured in ctor to 2s delay
    juce::ToggleButton _instantTooltipToggle; // top bar toggle to switch between instant and delayed tooltips

    // IR enable/disable toggle and attachment
    juce::ToggleButton _irEnableToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> _irEnableAttachment;

    std::unique_ptr<juce::FileChooser> _fileChooser;

    // Tabbed interface
    juce::TabbedComponent _tabbedComponent;
    std::unique_ptr<PreFXPanel> _preFXPanel;
    std::unique_ptr<AmpPanel> _ampPanel;
    std::unique_ptr<PostFXPanel> _postFXPanel;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    LZ25AudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LZ25AudioProcessorEditor)
};
