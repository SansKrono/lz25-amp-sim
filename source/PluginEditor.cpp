
#include "PluginEditor.h"
#include "BinaryData.h"
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

void LZ25AudioProcessorEditor::initialise_ir_toggle()
{
    addAndMakeVisible (_irEnableToggle);
    _irEnableToggle.setButtonText ("Cab IR");
    _irEnableToggle.setTooltip ("Enable/disable the built-in cabinet impulse response. Turn OFF to use an external cab sim later in your chain.");
    _irEnableToggle.setToggleState (true, juce::dontSendNotification);
    _irEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "IR_ENABLE", _irEnableToggle);
    _irEnableToggle.onClick = [this]() {
        const bool enabled = _irEnableToggle.getToggleState();
        _nextButton.setEnabled (enabled);
        _prevButton.setEnabled (enabled);
        if (enabled)
        {
            // Enable the components visually and set the current file name as the selected IR.
            _irName.setText (audioProcessor.savedFile.getFileName(), juce::dontSendNotification);
            _irName.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            _nextButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            _prevButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            // Grey out the IR navigation components and state that the IR is deactivated.
            _irName.setText ("IR Disabled", juce::dontSendNotification);
            _irName.setColour (juce::TextButton::textColourOffId, juce::Colours::grey);
            _nextButton.setColour (juce::TextButton::textColourOffId, juce::Colours::grey);
            _prevButton.setColour (juce::TextButton::textColourOffId, juce::Colours::grey);
        }
    };
}
void LZ25AudioProcessorEditor::initialise_melatonin_inspector (juce::Rectangle<int> bounds)
{
    auto bottom = bounds.removeFromBottom (40);

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    inspectButton.setBounds (bottom.removeFromRight (160).reduced (10));
}
void LZ25AudioProcessorEditor::initialise_meter_and_timer()
{
    addAndMakeVisible (_meterInput);
    addAndMakeVisible (_meterOutput);
    startTimerHz (24);
}
void LZ25AudioProcessorEditor::initialise_load_button()
{
    addAndMakeVisible (_loadButton);
    _loadButton.setButtonText ("Load IR/Folder");
    _loadButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::black); // Blue outline
    _loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _loadButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    _loadButton.onClick = [this]() {
        fileLoader();
    };
}
void LZ25AudioProcessorEditor::initialise_ir_navigation_buttons()
{
    addAndMakeVisible (_prevButton);
    _prevButton.setButtonText (""); // Remove text
    _prevButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    _prevButton.onClick = [this]() {
        if (audioProcessor.prevIR())
            _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
    };

    addAndMakeVisible (_nextButton);
    _nextButton.setButtonText (""); // Remove text
    _nextButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    _nextButton.onClick = [this]() {
        if (audioProcessor.nextIR())
            _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
    };
}
void LZ25AudioProcessorEditor::initialise_ir_name_display()
{
    _irName.setText (audioProcessor.savedFile.getFileName(), juce::dontSendNotification);
    _irName.setJustificationType (juce::Justification::centred); // Centred
    _irName.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _irName.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    addAndMakeVisible (_irName);
}
void LZ25AudioProcessorEditor::initialise_tabbed_components()
{
    // Create tabbed interface panels
    _pitchDynPanel = std::make_unique<PitchDynamicsPanel> (audioProcessor.apvts);
    _preFXPanel = std::make_unique<PreFXPanel> (audioProcessor.apvts);
    _ampPanel = std::make_unique<AmpPanel> (audioProcessor.apvts);
    _postFXPanel = std::make_unique<PostFXPanel> (audioProcessor.apvts);

    // Add panels to the tabbed component (new order)
    _tabbedComponent.addTab ("PITCH/DYNAMICS", juce::Colours::transparentBlack, _pitchDynPanel.get(), false);
    _tabbedComponent.addTab ("PRE-AMP", juce::Colours::transparentBlack, _preFXPanel.get(), false);
    _tabbedComponent.addTab ("AMP", juce::Colours::transparentBlack, _ampPanel.get(), false);
    _tabbedComponent.addTab ("POST FX", juce::Colours::transparentBlack, _postFXPanel.get(), false);

    // Style the tabbed component
    _tabbedComponent.setTabBarDepth (36);
    _tabbedComponent.setCurrentTabIndex (0); // Start with PRE FX tab

    // Apply custom LookAndFeel to tabs for even spread and rounded headers
    auto& tabBar = _tabbedComponent.getTabbedButtonBar();
    tabBar.setLookAndFeel (&_tabsLookAndFeel);
    tabBar.setMinimumTabScaleFactor (1.0); // don't shrink; our LAF sizes evenly

    // Remove internal borders/outline around the tab content area
    _tabbedComponent.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    _tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabBar.setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);

    addAndMakeVisible (_tabbedComponent);

    // Wire double-click on tabs to toggle panel enable
    _tabbedComponent.setOnTabDoubleClick ([this] (int idx) { handleTabDoubleClick (idx); });

    // Ensure initial tab visuals reflect bypass state
    updateTabVisuals();
}
void LZ25AudioProcessorEditor::initialise_preset_buttons()
{
    addAndMakeVisible (_savePresetButton);
    addAndMakeVisible (_loadPresetButton);

    _savePresetButton.onClick = [this]() {
        auto dir = LZ25AudioProcessor::getDefaultPresetDirectory();
        juce::File initial = dir.getNonexistentChildFile ("Preset", ".lz25preset");
        _fileChooser = std::make_unique<juce::FileChooser> ("Save Preset", initial, "*.lz25preset");
        _fileChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc) {
                juce::File chosen = fc.getResult();
                if (!chosen.exists() && chosen.getFileName().isNotEmpty())
                {
                    // proceed, user typed a new name
                }
                else if (!chosen.existsAsFile())
                {
                    return; // user cancelled
                }

                if (chosen.getFileExtension().isEmpty())
                    chosen = chosen.withFileExtension (".lz25preset");

                if (!audioProcessor.savePreset (chosen))
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Save Preset", "Failed to save preset.");
            });
    };

    _loadPresetButton.onClick = [this]() {
        auto dir = LZ25AudioProcessor::getDefaultPresetDirectory();
        _fileChooser = std::make_unique<juce::FileChooser> ("Load Preset", dir, "*.lz25preset;*.xml");
        _fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc) {
                auto chosen = fc.getResult();
                if (!chosen.existsAsFile())
                    return; // user cancelled or invalid

                if (!audioProcessor.loadPreset (chosen))
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Load Preset", "Failed to load preset.");
                else
                    _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
            });
    };
}

void LZ25AudioProcessorEditor::initialise_instant_tooltip_toggle()
{
    addAndMakeVisible (_instantTooltipToggle);
    _instantTooltipToggle.setButtonText ("Instant Tooltips");
    _instantTooltipToggle.setTooltip ("When enabled, knob tooltips show immediately on hover. Disable to restore 2s delay.");
    _instantTooltipToggle.onClick = [this]() {
        const bool enabled = _instantTooltipToggle.getToggleState();
        _tooltipWindow.setMillisecondsBeforeTipAppears (enabled ? 0 : 2000);
    };
}
//==============================================================================
LZ25AudioProcessorEditor::LZ25AudioProcessorEditor (LZ25AudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      _meterOutput ([&]() { return audioProcessor.getRMSOutputValue (0); }),
      _meterInput ([&]() { return audioProcessor.getRMSInputValue (0); }),
      _tooltipWindow (this, 2000),
      _tabbedComponent (juce::TabbedButtonBar::TabsAtTop),
      audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (1100, 600); // Increased height to accommodate tabs
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom (20);
    bottom = bottom.removeFromRight (40);
    inspectButton.setBounds (bottom);

    // Load arrow images
    _leftArrowImage = juce::ImageCache::getFromMemory (BinaryData::Left_Arrowpng1x_png, BinaryData::Left_Arrowpng1x_pngSize);
    _rightArrowImage = juce::ImageCache::getFromMemory (BinaryData::Right_Arrowpng1x_png, BinaryData::Right_Arrowpng1x_pngSize);
    _backgroundImage = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    initialise_meter_and_timer();
    initialise_melatonin_inspector (area);
    initialise_load_button();
    initialise_ir_navigation_buttons();
    initialise_ir_name_display(); // Currently Selected IR display box.
    initialise_ir_toggle(); // IR enable toggle (top bar).
    initialise_instant_tooltip_toggle(); // Instant tooltip toggle (top bar).
    initialise_process_mode_selector(); // Stereo/Mono selector (top bar).
    initialise_preset_buttons(); // Preset Save/Load buttons (top bar).
    initialise_tabbed_components();

    _backgroundImage = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);
}

LZ25AudioProcessorEditor::~LZ25AudioProcessorEditor() = default;

void LZ25AudioProcessorEditor::timerCallback()
{
    if (audioProcessor.input)
    {
        _meterInput.setLevel (audioProcessor.getRMSInputValue (0));
        _meterInput.repaint();

        _meterOutput.setLevel (audioProcessor.getRMSOutputValue (0));
        _meterOutput.repaint();
    }

    // Keep tab visuals in sync with parameter automation
    updateTabVisuals();
}

void LZ25AudioProcessorEditor::fileLoader()
{
    _fileChooser = std::make_unique<juce::FileChooser> ("Choose IR file or folder", audioProcessor.root, "*");

    constexpr auto fileChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories;

    _fileChooser->launchAsync (fileChooserFlags, [this] (const juce::FileChooser& chooser) {
        juce::File result (chooser.getResult());

        if (result.isDirectory())
        {
            audioProcessor.setIRFolder (result);
            _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
            return;
        }

        if (result.existsAsFile())
        {
            auto ext = result.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac")
            {
                audioProcessor.setIRFolder (result.getParentDirectory());
                audioProcessor.loadIRFile (result);
                _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
            }
        }
    });
}

//==============================================================================
void LZ25AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Modern flat background with light colour palette
    auto bounds = getLocalBounds().toFloat();

    // Primary light background
    g.setColour (juce::Colours::black); // PRIMARY_BG
    g.fillAll();

    // Create modern flat "amp head" design
    auto ampBounds = bounds.reduced (10);

    // Main amp panel with rounded corners
    g.setColour (juce::Colours::darkgrey); // SECONDARY_BG
    g.fillRoundedRectangle (ampBounds, 12.0f);

    // Subtle border for definition
    g.setColour (juce::Colour::fromRGB (180, 190, 200)); // KNOB_OUTLINE
    g.drawRoundedRectangle (ampBounds, 12.0f, 2.0f);

    // // Modern brand text area at top
    // auto brandArea = ampBounds.removeFromTop (60).reduced (20);
    // g.setColour (juce::Colours::lightsalmon); // ACCENT_COLOR
    // g.setFont (juce::Font ("Arial", 32.0f, juce::Font::bold));
    // g.drawText ("LZ25", brandArea, juce::Justification::centred);

    // Subtle sci-fi accent lines
    // auto accentY = brandArea.getBottom() + 10.0f;
    // g.setColour (juce::Colours::lightsalmon.withAlpha (0.3f));
    // g.fillRect (ampBounds.getX() + 20.0f, accentY, ampBounds.getWidth() - 40.0f, 2.0f);

    // Optional: Add subtle gradient for depth while keeping it flat
    auto gradientArea = ampBounds.reduced (2);
    juce::ColourGradient subtleGradient (
        juce::Colour::fromRGB (240, 245, 250).withAlpha (0.3f),
        gradientArea.getTopLeft(),
        juce::Colour::fromRGB (200, 210, 220).withAlpha (0.1f),
        gradientArea.getBottomRight(),
        false);
    g.setGradientFill (subtleGradient);
    g.fillRoundedRectangle (gradientArea, 10.0f);

    // Draw arrow images on buttons
    if (_leftArrowImage.isValid())
    {
        auto prevBounds = _prevButton.getBounds().toFloat();
        g.drawImage (_leftArrowImage, prevBounds, juce::RectanglePlacement::centred);
    }
    
    if (_rightArrowImage.isValid())
    {
        auto nextBounds = _nextButton.getBounds().toFloat();
        g.drawImage (_rightArrowImage, nextBounds, juce::RectanglePlacement::centred);
    }
}

void LZ25AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto bottom = bounds.removeFromBottom (40);
    inspectButton.setBounds (bottom.removeFromRight (160).reduced (10));

    // Top section for IR controls
    auto topSection = bounds.removeFromTop (70);
    _loadButton.setBounds (25, 20, 120, 35);
    _irName.setBounds (155, 20, 180, 35);
    _prevButton.setBounds (340, 20, 35, 35);
    _nextButton.setBounds (380, 20, 35, 35);

    // Instant tooltip toggle anchored to the right with a slight margin
    {
        constexpr int toggleW = 70;
        constexpr int toggleH = 35;
        const int toggleY = topSection.getY() + 20;
        constexpr int margin = 16; // slight margin from the right edge

        // Position the tooltip toggle.
        const int tooltipToggleX = topSection.getRight() - margin - toggleW;
        _instantTooltipToggle.setBounds (tooltipToggleX, toggleY, toggleW, toggleH);

        // Position the IR enable toggle.
        const int irToggleX = tooltipToggleX - margin - toggleW;
        _irEnableToggle.setBounds (irToggleX, toggleY, toggleW, toggleH);

        // Position the Process Mode selector to the left of IR toggle
        constexpr int processW = 140;
        constexpr int processH = toggleH;
        const int processX = irToggleX - margin - processW;
        _processModeBox.setBounds (processX, toggleY, processW, processH);

        // Preset buttons to the left of process mode
        constexpr int presetW = 120;
        constexpr int presetH = toggleH;
        const int loadX = processX - margin - presetW;
        const int saveX = loadX - margin - presetW;
        _loadPresetButton.setBounds (loadX, toggleY, presetW, presetH);
        _savePresetButton.setBounds (saveX, toggleY, presetW, presetH);
    }

    // Left side for input meter
    const auto leftSection = bounds.removeFromLeft (40);
    int meterWidth = 40;
    int meterHeight = 370 + 45;
    int meterY = leftSection.getY() + 55;
    int meterLeftX = leftSection.getX() + 15;
    _meterInput.setBounds (meterLeftX, meterY, meterWidth, meterHeight);

    // Right side for output meter
    auto rightSection = bounds.removeFromRight (40);
    int meterRightX = rightSection.getX() - 15;
    _meterOutput.setBounds (meterRightX, meterY, meterWidth, meterHeight);

    // Remaining space for the tabbed component
    auto tabArea = bounds.reduced (20);
    _tabbedComponent.setBounds (tabArea);
}

void LZ25AudioProcessorEditor::handleTabDoubleClick (int tabIndex)
{
    const char* paramId = nullptr;
    switch (tabIndex)
    {
        case 0:
            paramId = "PITCH_DYN_PANEL_ENABLE";
            break;
        case 1:
            paramId = "PRE_PANEL_ENABLE";
            break;
        case 2:
            paramId = "AMP_PANEL_ENABLE";
            break;
        case 3:
            paramId = "POST_PANEL_ENABLE";
            break;
        default:
            return;
    }

    auto* param = audioProcessor.apvts.getParameter (paramId);
    if (param == nullptr)
        return;

    const bool enabled = *audioProcessor.apvts.getRawParameterValue (paramId) > 0.5f;
    param->beginChangeGesture();
    param->setValueNotifyingHost (enabled ? 0.0f : 1.0f);
    param->endChangeGesture();

    updateTabVisuals();
}

void LZ25AudioProcessorEditor::updateTabVisuals()
{
    struct Entry
    {
        int index;
        const char* id;
    } entries[] = {
        { 0, "PITCH_DYN_PANEL_ENABLE" },
        { 1, "PRE_PANEL_ENABLE" },
        { 2, "AMP_PANEL_ENABLE" },
        { 3, "POST_PANEL_ENABLE" },
    };

    for (const auto& e : entries)
    {
        const bool enabled = *audioProcessor.apvts.getRawParameterValue (e.id) > 0.5f;
        auto colour = enabled ? juce::Colours::transparentBlack
                              : juce::Colours::darkgrey.withAlpha (0.6f);
        _tabbedComponent.setTabBackgroundColour (e.index, colour);
    }
}

void LZ25AudioProcessorEditor::initialise_process_mode_selector()
{
    addAndMakeVisible (_processModeBox);
    _processModeBox.addItem ("Stereo", 1);
    _processModeBox.addItem ("Mono", 2);
    _processModeBox.setTooltip ("Select processing mode: process left/right independently (Stereo) or duplicate one side to both (Mono)");
    _processModeBox.setJustificationType (juce::Justification::centred);

    _processModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "PROCESS_MODE", _processModeBox);
}
