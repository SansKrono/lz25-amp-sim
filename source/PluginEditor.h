
#pragma once
#include "AmpPanel.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "PostFXPanel.h"
#include "PreFXPanel.h"
#include "PitchDynamicsPanel.h"
#include "VerticalGradientMeter.h"
#include <functional>

//==============================================================================

namespace melatonin
{
    class Inspector;
}
// Custom TabbedComponent with double-click callback to toggle panel enable
class PanelTabbedComponent : public juce::TabbedComponent
{
public:
    explicit PanelTabbedComponent (juce::TabbedButtonBar::Orientation orientation)
        : juce::TabbedComponent (orientation) {}

    void setOnTabDoubleClick (std::function<void (int)> cb) { onTabDoubleClick = std::move (cb); }

protected:
    juce::TabBarButton* createTabButton (const juce::String& tabName, int tabIndex) override
    {
        struct Button : public juce::TabBarButton
        {
            Button (const juce::String& name, juce::TabbedButtonBar& owner, std::function<void (int)>* cbPtr, int idx)
                : juce::TabBarButton (name, owner), callbackPtr (cbPtr), index (idx) {}

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                juce::TabBarButton::mouseDoubleClick (e);
                if (callbackPtr != nullptr && *callbackPtr)
                    (*callbackPtr) (index);
            }

            std::function<void (int)>* callbackPtr;
            int index;
        };

        auto& bar = getTabbedButtonBar();
        return new Button (tabName, bar, &onTabDoubleClick, tabIndex);
    }

private:
    std::function<void (int)> onTabDoubleClick;
};

class LZ25AudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    void initialise_ir_toggle();
    void initialise_melatonin_inspector (juce::Rectangle<int> bounds);
    void initialise_meter_and_timer();
    void initialise_load_button();
    void initialise_ir_navigation_buttons();
    void initialise_ir_name_display();
    void initialise_tabbed_components();
    void initialise_instant_tooltip_toggle();
    void initialise_process_mode_selector();
    void initialise_preset_buttons();
    LZ25AudioProcessorEditor (LZ25AudioProcessor&);
    ~LZ25AudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void fileLoader();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    LZ25AudioProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };


    juce::LookAndFeel_V4 _lookAndFeel;
    MXRLookAndFeel _sliderLookAndFeel;
    TabsLookAndFeel _tabsLookAndFeel;

    Gui::VerticalGradientMeter _meterOutput;
    Gui::VerticalGradientMeter _meterInput;

    juce::TextButton _loadButton;
    juce::TextButton _prevButton;
    juce::TextButton _nextButton;
    juce::Label _irName;
    juce::Image _backgroundImage;

    // Preset save/load buttons
    juce::TextButton _savePresetButton { "Save Preset" };
    juce::TextButton _loadPresetButton { "Load Preset" };

    // Tooltip manager for delayed hints
    juce::TooltipWindow _tooltipWindow; // configured in ctor to 2s delay
    juce::ToggleButton _instantTooltipToggle; // top bar toggle to switch between instant and delayed tooltips

    // IR enable/disable toggle and attachment
    juce::ToggleButton _irEnableToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> _irEnableAttachment;

    // Process mode selector and attachment
    juce::ComboBox _processModeBox; // Stereo/Mono
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> _processModeAttachment;

    std::unique_ptr<juce::FileChooser> _fileChooser;

    // Tabbed interface
    PanelTabbedComponent _tabbedComponent;
    std::unique_ptr<PitchDynamicsPanel> _pitchDynPanel;
    std::unique_ptr<PreFXPanel> _preFXPanel;
    std::unique_ptr<AmpPanel> _ampPanel;
    std::unique_ptr<PostFXPanel> _postFXPanel;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    LZ25AudioProcessor& audioProcessor;

    // Helpers for panel disable visuals and toggling
    void updateTabVisuals();
    void handleTabDoubleClick (int tabIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LZ25AudioProcessorEditor)
};
