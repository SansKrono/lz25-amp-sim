#include "AmpPanel.h"
#include "BinaryData.h"

AmpPanel::AmpPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // Disable knob surround on amp panel knobs
    _sliderLookAndFeel.setKnobSurroundEnabled (false);

    // Load amp face image from embedded BinaryData (fallback-safe)
    if (BinaryData::amphead_png != nullptr && BinaryData::amphead_pngSize > 0)
        _ampFaceImage = juce::ImageFileFormat::loadFrom (BinaryData::amphead_png, (size_t) BinaryData::amphead_pngSize);

    // Input slider setup
    addAndMakeVisible (_sliderInput);
    setSliderProperties (&_sliderInput);
    _sliderInput.setLookAndFeel (&_sliderLookAndFeel);
    _sliderInput.setTooltip ("Input level into the amp. Turn up to push the whole circuit harder (hotter signal, more saturation/noise). Turn down to keep things clean and prevent clipping.");
    addAndMakeVisible (_labelInput);
    _labelInput.setText ("INPUT", juce::dontSendNotification);
    _labelInput.setJustificationType (juce::Justification::centred);
    _labelInput.setColour (juce::Label::textColourId, juce::Colours::white);

    // Pre Gain slider setup
    addAndMakeVisible (_sliderPreGain);
    setSliderProperties (&_sliderPreGain);
    _sliderPreGain.setLookAndFeel (&_sliderLookAndFeel);
    _sliderPreGain.setTooltip ("Preamp gain before distortion. Turn up for more drive and sustain; turn down for a cleaner tone and more headroom.");
    addAndMakeVisible (_labelPreGain);
    _labelPreGain.setText ("PRE GAIN", juce::dontSendNotification);
    _labelPreGain.setJustificationType (juce::Justification::centred);
    _labelPreGain.setColour (juce::Label::textColourId, juce::Colours::white);

    // Bass slider setup
    addAndMakeVisible (_sliderBass);
    setSliderProperties (&_sliderBass);
    _sliderBass.setLookAndFeel (&_sliderLookAndFeel);
    _sliderBass.setTooltip ("Low frequencies (thump). Turn up for more weight and sub lows; turn down to tighten the bottom and reduce boom.");
    addAndMakeVisible (_labelBass);
    _labelBass.setText ("BASS", juce::dontSendNotification);
    _labelBass.setJustificationType (juce::Justification::centred);
    _labelBass.setColour (juce::Label::textColourId, juce::Colours::white);

    // Mid slider setup
    addAndMakeVisible (_sliderMid);
    setSliderProperties (&_sliderMid);
    _sliderMid.setLookAndFeel (&_sliderLookAndFeel);
    _sliderMid.setTooltip ("Midrange body and note definition. Turn up for more presence and cut; turn down for a modern scooped tone.");
    addAndMakeVisible (_labelMid);
    _labelMid.setText ("MID", juce::dontSendNotification);
    _labelMid.setJustificationType (juce::Justification::centred);
    _labelMid.setColour (juce::Label::textColourId, juce::Colours::white);

    // Treble slider setup
    addAndMakeVisible (_sliderTreble);
    setSliderProperties (&_sliderTreble);
    _sliderTreble.setLookAndFeel (&_sliderLookAndFeel);
    _sliderTreble.setTooltip ("High frequencies/brightness. Turn up for more bite and sizzle; turn down to smooth the top end.");
    addAndMakeVisible (_labelTreble);
    _labelTreble.setText ("TREBLE", juce::dontSendNotification);
    _labelTreble.setJustificationType (juce::Justification::centred);
    _labelTreble.setColour (juce::Label::textColourId, juce::Colours::white);

    // Presence slider setup
    addAndMakeVisible (_sliderPresence);
    setSliderProperties (&_sliderPresence);
    _sliderPresence.setLookAndFeel (&_sliderLookAndFeel);
    _sliderPresence.setTooltip ("High-shelf in the power-amp region (air/attack). Turn up for more sparkle and pick clarity; turn down to tame fizz and harshness.");
    addAndMakeVisible (_labelPresence);
    _labelPresence.setText ("PRESENCE", juce::dontSendNotification);
    _labelPresence.setJustificationType (juce::Justification::centred);
    _labelPresence.setColour (juce::Label::textColourId, juce::Colours::white);

    // Post Gain slider setup
    addAndMakeVisible (_sliderPostGain);
    setSliderProperties (&_sliderPostGain);
    _sliderPostGain.setLookAndFeel (&_sliderLookAndFeel);
    _sliderPostGain.setTooltip ("Output volume after the amp/EQ. Turn up to make the plugin louder (does not add extra distortion); turn down to match levels.");
    addAndMakeVisible (_labelPostGain);
    _labelPostGain.setText ("POST GAIN", juce::dontSendNotification);
    _labelPostGain.setJustificationType (juce::Justification::centred);
    _labelPostGain.setColour (juce::Label::textColourId, juce::Colours::white);

    // Resonance slider setup
    addAndMakeVisible (_sliderResonance);
    setSliderProperties (&_sliderResonance);
    _sliderResonance.setLookAndFeel (&_sliderLookAndFeel);
    _sliderResonance.setTooltip ("Low-frequency resonance around the cab. Turn up for bigger low-end bloom and punch; turn down for a tighter, more controlled bottom.");
    addAndMakeVisible (_labelResonance);
    _labelResonance.setText ("RESONANCE", juce::dontSendNotification);
    _labelResonance.setJustificationType (juce::Justification::centred);
    _labelResonance.setColour (juce::Label::textColourId, juce::Colours::white);

    // Drive slider setup
    addAndMakeVisible (_sliderDrive);
    setSliderProperties (&_sliderDrive);
    _sliderDrive.setLookAndFeel (&_sliderLookAndFeel);
    _sliderDrive.setTooltip ("Amount of saturation in the waveshaper. Turn up for heavier distortion and sustain; turn down for a cleaner tone.");
    addAndMakeVisible (_labelDrive);
    _labelDrive.setText ("DRIVE", juce::dontSendNotification);
    _labelDrive.setJustificationType (juce::Justification::centred);
    _labelDrive.setColour (juce::Label::textColourId, juce::Colours::white);

    // Asymmetry slider setup
    addAndMakeVisible (_sliderAsymmetry);
    setSliderProperties (&_sliderAsymmetry);
    _sliderAsymmetry.setLookAndFeel (&_sliderLookAndFeel);
    _sliderAsymmetry.setTooltip ("Balances positive vs. negative clipping. Turn up for more asymmetry (adds even-order harmonics and a warmer compressed feel); turn down for symmetric clipping (more odd-order bite).");
    addAndMakeVisible (_labelAsymmetry);
    _labelAsymmetry.setText ("ASYMMETRY", juce::dontSendNotification);
    _labelAsymmetry.setJustificationType (juce::Justification::centred);
    _labelAsymmetry.setColour (juce::Label::textColourId, juce::Colours::white);

    // Harmonic Character slider setup
    addAndMakeVisible (_sliderHarmonicCharacter);
    setSliderProperties (&_sliderHarmonicCharacter);
    _sliderHarmonicCharacter.setLookAndFeel (&_sliderLookAndFeel);
    _sliderHarmonicCharacter.setTooltip ("Voices which harmonics are emphasized. Turn up toward odd-harmonics for aggressive bite and edge; turn down toward even-harmonics for smoother warmth.");
    addAndMakeVisible (_labelHarmonicCharacter);
    _labelHarmonicCharacter.setText ("HARMONIC", juce::dontSendNotification);
    _labelHarmonicCharacter.setJustificationType (juce::Justification::centred);
    _labelHarmonicCharacter.setColour (juce::Label::textColourId, juce::Colours::white);

    // Saturation Shape slider setup
    addAndMakeVisible (_sliderSaturationShape);
    setSliderProperties (&_sliderSaturationShape);
    _sliderSaturationShape.setLookAndFeel (&_sliderLookAndFeel);
    _sliderSaturationShape.setTooltip ("Shape of the clipping knee. Turn up for a harder clip (sharper, more aggressive); turn down for softer saturation (rounder, smoother).");
    addAndMakeVisible (_labelSaturationShape);
    _labelSaturationShape.setText ("SATURATION", juce::dontSendNotification);
    _labelSaturationShape.setJustificationType (juce::Justification::centred);
    _labelSaturationShape.setColour (juce::Label::textColourId, juce::Colours::white);

    // Tube Sag slider setup
    addAndMakeVisible (_sliderTubeSag);
    setSliderProperties (&_sliderTubeSag);
    _sliderTubeSag.setLookAndFeel (&_sliderLookAndFeel);
    _sliderTubeSag.setTooltip ("Simulated power-supply sag. Turn up for more sag and a chewy, compressed attack (looser low end); turn down for a tighter, faster response.");
    addAndMakeVisible (_labelTubeSag);
    _labelTubeSag.setText ("TUBE SAG", juce::dontSendNotification);
    _labelTubeSag.setJustificationType (juce::Justification::centred);
    _labelTubeSag.setColour (juce::Label::textColourId, juce::Colours::white);

    // New Tube Amp components setup
    // GAIN2 slider setup
    addAndMakeVisible (_sliderGain2);
    setSliderProperties (&_sliderGain2);
    _sliderGain2.setLookAndFeel (&_sliderLookAndFeel);
    _sliderGain2.setTooltip ("Second gain stage for tube amp saturation. Turn up for more drive and sustain.");
    addAndMakeVisible (_labelGain2);
    _labelGain2.setText ("GAIN 2", juce::dontSendNotification);
    _labelGain2.setJustificationType (juce::Justification::centred);
    _labelGain2.setColour (juce::Label::textColourId, juce::Colours::white);

    // GAIN3 slider setup
    addAndMakeVisible (_sliderGain3);
    setSliderProperties (&_sliderGain3);
    _sliderGain3.setLookAndFeel (&_sliderLookAndFeel);
    _sliderGain3.setTooltip ("Third gain stage for tube amp saturation. Turn up for maximum drive and distortion.");
    addAndMakeVisible (_labelGain3);
    _labelGain3.setText ("GAIN 3", juce::dontSendNotification);
    _labelGain3.setJustificationType (juce::Justification::centred);
    _labelGain3.setColour (juce::Label::textColourId, juce::Colours::white);

    // BRIGHTNESS toggle setup
    addAndMakeVisible (_toggleBrightness);
    _toggleBrightness.setButtonText ("");
    _toggleBrightness.setTooltip ("Brightness cap switch. Turn on for brighter, more cutting tone.");
    addAndMakeVisible (_labelBrightness);
    _labelBrightness.setText ("BRIGHT", juce::dontSendNotification);
    _labelBrightness.setJustificationType (juce::Justification::centred);
    _labelBrightness.setColour (juce::Label::textColourId, juce::Colours::white);

    // TUBE_MODEL combo box setup
    addAndMakeVisible (_comboTubeModel);
    _comboTubeModel.addItem ("Soft (Tanh)", 1);
    _comboTubeModel.addItem ("Medium (Arctan)", 2);
    _comboTubeModel.addItem ("Hard (Cubic)", 3);
    _comboTubeModel.addItem ("Asymmetric (Push-Pull)", 4);
    _comboTubeModel.setTooltip ("Select tube saturation model for different distortion characteristics.");
    addAndMakeVisible (_labelTubeModel);
    _labelTubeModel.setText ("TUBE MODEL", juce::dontSendNotification);
    _labelTubeModel.setJustificationType (juce::Justification::centred);
    _labelTubeModel.setColour (juce::Label::textColourId, juce::Colours::white);

    // Create parameter attachments
    _sliderAttachmentInput = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "INPUT", _sliderInput);
    _sliderAttachmentPreGain = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "GAIN1", _sliderPreGain);
    _sliderAttachmentResonance = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "RESONANCE", _sliderResonance);
    _sliderAttachmentDrive = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "DRIVE", _sliderDrive);
    _sliderAttachmentAsymmetry = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "ASYMMETRY", _sliderAsymmetry);
    _sliderAttachmentHarmonicCharacter = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "HARMONIC_CHARACTER", _sliderHarmonicCharacter);
    _sliderAttachmentSaturationShape = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "SATURATION_SHAPE", _sliderSaturationShape);
    _sliderAttachmentTubeSag = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "TUBE_SAG", _sliderTubeSag);
    _sliderAttachmentBass = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "BASS", _sliderBass);
    _sliderAttachmentMid = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "MID", _sliderMid);
    _sliderAttachmentTreble = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "TREBLE", _sliderTreble);
    _sliderAttachmentPresence = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "PRESENCE", _sliderPresence);
    _sliderAttachmentPostGain = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "POSTGAIN", _sliderPostGain);
        
    // New parameter attachments
    _sliderAttachmentGain2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "GAIN2", _sliderGain2);
    _sliderAttachmentGain3 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "GAIN3", _sliderGain3);
    _toggleAttachmentBrightness = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "BRIGHTNESS", _toggleBrightness);
    _comboAttachmentTubeModel = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "TUBE_MODEL", _comboTubeModel);
}

AmpPanel::~AmpPanel() = default;

void AmpPanel::setSliderProperties (juce::Slider* sliderToSet)
{
    sliderToSet->setSliderStyle (juce::Slider::SliderStyle::RotaryVerticalDrag);
    sliderToSet->setTextBoxStyle (juce::Slider::NoTextBox, false, 76, 38);
    sliderToSet->setDoubleClickReturnValue (true, 0.0f);
}

void AmpPanel::paint (juce::Graphics& g)
{
    // Transparent to inherit parent
    g.fillAll (juce::Colours::transparentBlack);

    // Draw the amp face image if available
    if (_ampFaceImage.isValid())
    {
        // Draw the amp face image if available
        if (_ampFaceImage.isValid())
        {
            // Always draw scaled to current area without pre-resizing the source
            g.drawImage (_ampFaceImage,
                _ampImageArea.toFloat(),
                juce::RectanglePlacement::stretchToFit);
        }
        _knobArea = _ampImageArea;
        _knobArea.removeFromTop (_knobArea.getHeight() * 6 / 10); // keep lower 40%
        _knobArea = _knobArea.reduced (20, 8);
    }
}

void AmpPanel::resized()
{
    auto bounds = getLocalBounds().reduced (6);

    // Calculate aspect-correct image area centered within bounds
    if (_ampFaceImage.isValid())
    {
        const float imgW = (float) _ampFaceImage.getWidth();
        const float imgH = (float) _ampFaceImage.getHeight();
        const float targetW = (float) bounds.getWidth();
        const float targetH = (float) bounds.getHeight();
        const float imageAspect = imgW / imgH;
        const float targetAspect = targetW / targetH;

        juce::Rectangle<float> area = bounds.toFloat();
        if (targetAspect > imageAspect)
        {
            float w = targetH * imageAspect;
            float x = area.getCentreX() - w * 0.5f;
            area = { x, area.getY(), w, targetH };
        }
        else
        {
            float h = targetW / imageAspect;
            float y = area.getCentreY() - h * 0.5f;
            area = { area.getX(), y, targetW, h };
        }
        _ampImageArea = area.toNearestInt();

        // Lower orange half for knobs (approximate lower 40% of the image)
        _knobArea = _ampImageArea;
        _knobArea.removeFromTop (_knobArea.getHeight() * 6.2f / 10); // reserve lower 40%
        _knobArea = _knobArea.reduced (30, 8); // padding from image edges
        _knobArea.translate (20, 0);
    }
    else
    {
        _ampImageArea = bounds;
        _knobArea = bounds;
    }

    // Layout sliders within knob area (two rows)
    const int sliderWidth = 60;
    const int sliderHeight = 70;
    const int rowSpacing = 5;
    const int labelHeight = 12;
    const int labelGap = 2;

    // Compute row rectangles inside _knobArea
    auto area = _knobArea;
    // Ensure minimum height
    if (area.getHeight() < (sliderHeight + labelHeight + labelGap) * 2 + rowSpacing)
        area.setHeight ((sliderHeight + labelHeight + labelGap) * 2 + rowSpacing);

    // Row 1 top region
    auto row1 = area.removeFromTop (sliderHeight + labelGap + labelHeight);
    area.removeFromTop (rowSpacing);
    auto row2 = area; // remaining for row 2

    // Horizontal spacing for each row - updated for new controls
    const int row1KnobCount = 10; // INPUT, GAIN1, GAIN2, GAIN3, BASS, MID, TREBLE, PRESENCE, BRIGHTNESS, POST GAIN
    const int row1Spacing = juce::jmax (4, (row1.getWidth() - row1KnobCount * sliderWidth) / (row1KnobCount + 1));

    const int row2KnobCount = 1;
    const float secondRowScale = 1.0f;
    const int row2SliderWidth = juce::roundToInt (sliderWidth * secondRowScale);
    const int row2SliderHeight = juce::roundToInt (sliderHeight * secondRowScale);
    const int row2Spacing = juce::jmax (row2KnobCount, (row2.getWidth() - row2KnobCount * row2SliderWidth) / (row2KnobCount + 1));

    const int topRowY = row1.getY();
    const int topLabelsY = topRowY + sliderHeight + labelGap;
    const int bottomRowY = row2.getY();
    const int bottomLabelsY = bottomRowY + row2SliderHeight + labelGap;

    int x = row1.getX() + row1Spacing;

    _sliderInput.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelInput.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPreGain.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelPreGain.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    _labelPreGain.setText ("GAIN 1", juce::dontSendNotification); // Update label to reflect GAIN1
    x += sliderWidth + row1Spacing;

    _sliderGain2.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelGain2.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderGain3.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelGain3.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderBass.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelBass.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderMid.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelMid.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderTreble.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelTreble.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPresence.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelPresence.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _toggleBrightness.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelBrightness.setBounds (x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPostGain.setBounds (x, topRowY, sliderWidth, sliderHeight);
    _labelPostGain.setBounds (x, topLabelsY, sliderWidth, labelHeight);

    // Row 2 - TUBE_MODEL dropdown
    int x2 = row2.getX() + row2Spacing;
    const int comboBoxWidth = 180;
    const int comboBoxHeight = 25;
    
    _comboTubeModel.setBounds (x2, bottomRowY + 30, comboBoxWidth, comboBoxHeight);
    _labelTubeModel.setBounds (x2, bottomLabelsY, comboBoxWidth, labelHeight);
}