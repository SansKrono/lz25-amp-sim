
#include "PluginEditor.h"
#include "BinaryData.h"
#include "PluginProcessor.h"

//==============================================================================
LZ25AudioProcessorEditor::LZ25AudioProcessorEditor (LZ25AudioProcessor& p)
    : AudioProcessorEditor (&p),
      _meterOutput ([&]() { return audioProcessor.getRMSOutputValue (0); }),
      _tabbedComponent (juce::TabbedButtonBar::TabsAtTop),
      _tooltipWindow (this, 2000),
      audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (1000, 470); // Increased height to accommodate tabs

    addAndMakeVisible (_meterOutput);
    startTimerHz (24);

    addAndMakeVisible (_loadButton);
    _loadButton.setButtonText ("Load IR/Folder");
    _loadButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::black); // Blue outline
    _loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _loadButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    _loadButton.onClick = [this]() {
        fileLoader();
    };

    addAndMakeVisible (_prevButton);
    _prevButton.setButtonText ("<");
    _prevButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _prevButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    _prevButton.onClick = [this]() {
        if (audioProcessor.prevIR())
            _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
    };

    addAndMakeVisible (_nextButton);
    _nextButton.setButtonText (">");
    _nextButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _nextButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    _nextButton.onClick = [this]() {
        if (audioProcessor.nextIR())
            _irName.setText (audioProcessor.getCurrentIRName(), juce::dontSendNotification);
    };

    _irName.setText (audioProcessor.savedFile.getFileName(), juce::dontSendNotification);
    _irName.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey); // Dark button
    _irName.setColour (juce::TextButton::textColourOffId, juce::Colours::white); // White text
    addAndMakeVisible (_irName);

    // IR enable toggle (top bar)
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

    // Instant tooltip toggle (top bar)
    addAndMakeVisible (_instantTooltipToggle);
    _instantTooltipToggle.setButtonText ("Instant Tooltips");
    _instantTooltipToggle.setTooltip ("When enabled, knob tooltips show immediately on hover. Disable to restore 2s delay.");
    _instantTooltipToggle.onClick = [this]() {
        const bool enabled = _instantTooltipToggle.getToggleState();
        _tooltipWindow.setMillisecondsBeforeTipAppears (enabled ? 0 : 2000);
    };

    // Create tabbed interface panels
    _preFXPanel = std::make_unique<PreFXPanel> (audioProcessor.apvts);
    _ampPanel = std::make_unique<AmpPanel> (audioProcessor.apvts);
    _postFXPanel = std::make_unique<PostFXPanel> (audioProcessor.apvts);

    // Add panels to tabbed component
    _tabbedComponent.addTab ("PRE FX", juce::Colours::transparentBlack, _preFXPanel.get(), false);
    _tabbedComponent.addTab ("AMP", juce::Colours::transparentBlack, _ampPanel.get(), false);
    _tabbedComponent.addTab ("POST FX", juce::Colours::transparentBlack, _postFXPanel.get(), false);

    // Style the tabbed component
    _tabbedComponent.setTabBarDepth (36);
    _tabbedComponent.setCurrentTabIndex (0); // Start with PRE FX tab

    // Apply custom LookAndFeel to tabs for even spread and rounded headers
    auto& tabBar = _tabbedComponent.getTabbedButtonBar();
    tabBar.setLookAndFeel (&_tabsLookAndFeel);
    tabBar.setMinimumTabScaleFactor (1.0); // don't shrink; our LAF sizes evenly

    addAndMakeVisible (_tabbedComponent);

    _backgroundImage = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);
}

LZ25AudioProcessorEditor::~LZ25AudioProcessorEditor() = default;

void LZ25AudioProcessorEditor::timerCallback()
{
    if (audioProcessor.input)
    {
        _meterOutput.setLevel (audioProcessor.getRMSOutputValue (0));
        _meterOutput.repaint();
    }
}

void LZ25AudioProcessorEditor::fileLoader()
{
    _fileChooser = std::make_unique<juce::FileChooser> ("Choose IR file or folder", audioProcessor.root, "*");

    const auto fileChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories;

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
    // Modern flat background with light color palette
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

    // Modern brand text area at top
    auto brandArea = ampBounds.removeFromTop (60).reduced (20);
    g.setColour (juce::Colours::lightsalmon); // ACCENT_COLOR
    g.setFont (juce::Font ("Arial", 32.0f, juce::Font::bold));
    g.drawText ("LZ25", brandArea, juce::Justification::centred);

    // Subtle sci-fi accent lines
    auto accentY = brandArea.getBottom() + 10.0f;
    g.setColour (juce::Colours::lightsalmon.withAlpha (0.3f));
    g.fillRect (ampBounds.getX() + 20.0f, accentY, ampBounds.getWidth() - 40.0f, 2.0f);

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
}

void LZ25AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top section for IR controls
    auto topSection = bounds.removeFromTop (70);
    _loadButton.setBounds (25, 20, 120, 35);
    _irName.setBounds (155, 20, 180, 35);
    _prevButton.setBounds (340, 20, 35, 35);
    _nextButton.setBounds (380, 20, 35, 35);

    // Instant tooltip toggle anchored to the right with a slight margin
    {
        const int toggleW = 160;
        const int toggleH = 35;
        const int toggleY = topSection.getY() + 20;
        const int margin = 16; // slight margin from the right edge

        // Position the tooltip toggle.
        const int tooltipToggleX = topSection.getRight() - margin - toggleW;
        _instantTooltipToggle.setBounds (tooltipToggleX, toggleY, toggleW, toggleH);

        // Position the IR enable toggle.
        const int irToggleX = tooltipToggleX - margin - toggleW;
        _irEnableToggle.setBounds (irToggleX, toggleY, toggleW, toggleH);
    }

    // Right side for output meter
    auto rightSection = bounds.removeFromRight (40);
    int meterWidth = 40;
    int meterHeight = 280 + 45;
    int meterX = rightSection.getX() - 15;
    int meterY = rightSection.getY() + 55;
    _meterOutput.setBounds (meterX, meterY, meterWidth, meterHeight);

    // Remaining space for tabbed component
    auto tabArea = bounds.reduced (20);
    _tabbedComponent.setBounds (tabArea);
}
