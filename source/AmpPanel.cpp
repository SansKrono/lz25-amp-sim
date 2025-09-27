#include "AmpPanel.h"

AmpPanel::AmpPanel(juce::AudioProcessorValueTreeState& apvts)
{
    // Disable knob surround on amp panel knobs
    _sliderLookAndFeel.setKnobSurroundEnabled(false);

    // Input slider setup
    addAndMakeVisible(_sliderInput);
    setSliderProperties(&_sliderInput);
    _sliderInput.setLookAndFeel(&_sliderLookAndFeel);
    _sliderInput.setTooltip("Input level into the amp. Turn up to push the whole circuit harder (hotter signal, more saturation/noise). Turn down to keep things clean and prevent clipping.");
    addAndMakeVisible(_labelInput);
    _labelInput.setText("INPUT", juce::dontSendNotification);
    _labelInput.setJustificationType(juce::Justification::centred);
    _labelInput.setColour(juce::Label::textColourId, juce::Colours::white);

    // Pre Gain slider setup
    addAndMakeVisible(_sliderPreGain);
    setSliderProperties(&_sliderPreGain);
    _sliderPreGain.setLookAndFeel(&_sliderLookAndFeel);
    _sliderPreGain.setTooltip("Preamp gain before distortion. Turn up for more drive and sustain; turn down for a cleaner tone and more headroom.");
    addAndMakeVisible(_labelPreGain);
    _labelPreGain.setText("PRE GAIN", juce::dontSendNotification);
    _labelPreGain.setJustificationType(juce::Justification::centred);
    _labelPreGain.setColour(juce::Label::textColourId, juce::Colours::white);

        // Bass slider setup
    addAndMakeVisible(_sliderBass);
    setSliderProperties(&_sliderBass);
    _sliderBass.setLookAndFeel(&_sliderLookAndFeel);
    _sliderBass.setTooltip("Low frequencies (thump). Turn up for more weight and sub lows; turn down to tighten the bottom and reduce boom.");
    addAndMakeVisible(_labelBass);
    _labelBass.setText("BASS", juce::dontSendNotification);
    _labelBass.setJustificationType(juce::Justification::centred);
    _labelBass.setColour(juce::Label::textColourId, juce::Colours::white);

    // Mid slider setup
    addAndMakeVisible(_sliderMid);
    setSliderProperties(&_sliderMid);
    _sliderMid.setLookAndFeel(&_sliderLookAndFeel);
    _sliderMid.setTooltip("Midrange body and note definition. Turn up for more presence and cut; turn down for a modern scooped tone.");
    addAndMakeVisible(_labelMid);
    _labelMid.setText("MID", juce::dontSendNotification);
    _labelMid.setJustificationType(juce::Justification::centred);
    _labelMid.setColour(juce::Label::textColourId, juce::Colours::white);

    // Treble slider setup
    addAndMakeVisible(_sliderTreble);
    setSliderProperties(&_sliderTreble);
    _sliderTreble.setLookAndFeel(&_sliderLookAndFeel);
    _sliderTreble.setTooltip("High frequencies/brightness. Turn up for more bite and sizzle; turn down to smooth the top end.");
    addAndMakeVisible(_labelTreble);
    _labelTreble.setText("TREBLE", juce::dontSendNotification);
    _labelTreble.setJustificationType(juce::Justification::centred);
    _labelTreble.setColour(juce::Label::textColourId, juce::Colours::white);

    // Presence slider setup
    addAndMakeVisible(_sliderPresence);
    setSliderProperties(&_sliderPresence);
    _sliderPresence.setLookAndFeel(&_sliderLookAndFeel);
    _sliderPresence.setTooltip("High-shelf in the power-amp region (air/attack). Turn up for more sparkle and pick clarity; turn down to tame fizz and harshness.");
    addAndMakeVisible(_labelPresence);
    _labelPresence.setText("PRESENCE", juce::dontSendNotification);
    _labelPresence.setJustificationType(juce::Justification::centred);
    _labelPresence.setColour(juce::Label::textColourId, juce::Colours::white);

    // Post Gain slider setup
    addAndMakeVisible(_sliderPostGain);
    setSliderProperties(&_sliderPostGain);
    _sliderPostGain.setLookAndFeel(&_sliderLookAndFeel);
    _sliderPostGain.setTooltip("Output volume after the amp/EQ. Turn up to make the plugin louder (does not add extra distortion); turn down to match levels.");
    addAndMakeVisible(_labelPostGain);
    _labelPostGain.setText("POST GAIN", juce::dontSendNotification);
    _labelPostGain.setJustificationType(juce::Justification::centred);
    _labelPostGain.setColour(juce::Label::textColourId, juce::Colours::white);

    // Resonance slider setup
    addAndMakeVisible(_sliderResonance);
    setSliderProperties(&_sliderResonance);
    _sliderResonance.setLookAndFeel(&_sliderLookAndFeel);
    _sliderResonance.setTooltip("Low-frequency resonance around the cab. Turn up for bigger low-end bloom and punch; turn down for a tighter, more controlled bottom.");
    addAndMakeVisible(_labelResonance);
    _labelResonance.setText("RESONANCE", juce::dontSendNotification);
    _labelResonance.setJustificationType(juce::Justification::centred);
    _labelResonance.setColour(juce::Label::textColourId, juce::Colours::white);

    // Drive slider setup
    addAndMakeVisible(_sliderDrive);
    setSliderProperties(&_sliderDrive);
    _sliderDrive.setLookAndFeel(&_sliderLookAndFeel);
    _sliderDrive.setTooltip("Amount of saturation in the waveshaper. Turn up for heavier distortion and sustain; turn down for a cleaner tone.");
    addAndMakeVisible(_labelDrive);
    _labelDrive.setText("DRIVE", juce::dontSendNotification);
    _labelDrive.setJustificationType(juce::Justification::centred);
    _labelDrive.setColour(juce::Label::textColourId, juce::Colours::white);

    // Asymmetry slider setup
    addAndMakeVisible(_sliderAsymmetry);
    setSliderProperties(&_sliderAsymmetry);
    _sliderAsymmetry.setLookAndFeel(&_sliderLookAndFeel);
    _sliderAsymmetry.setTooltip("Balances positive vs. negative clipping. Turn up for more asymmetry (adds even-order harmonics and a warmer compressed feel); turn down for symmetric clipping (more odd-order bite).");
    addAndMakeVisible(_labelAsymmetry);
    _labelAsymmetry.setText("ASYMMETRY", juce::dontSendNotification);
    _labelAsymmetry.setJustificationType(juce::Justification::centred);
    _labelAsymmetry.setColour(juce::Label::textColourId, juce::Colours::white);

    // Harmonic Character slider setup
    addAndMakeVisible(_sliderHarmonicCharacter);
    setSliderProperties(&_sliderHarmonicCharacter);
    _sliderHarmonicCharacter.setLookAndFeel(&_sliderLookAndFeel);
    _sliderHarmonicCharacter.setTooltip("Voices which harmonics are emphasized. Turn up toward odd-harmonics for aggressive bite and edge; turn down toward even-harmonics for smoother warmth.");
    addAndMakeVisible(_labelHarmonicCharacter);
    _labelHarmonicCharacter.setText("HARMONIC", juce::dontSendNotification);
    _labelHarmonicCharacter.setJustificationType(juce::Justification::centred);
    _labelHarmonicCharacter.setColour(juce::Label::textColourId, juce::Colours::white);

    // Saturation Shape slider setup
    addAndMakeVisible(_sliderSaturationShape);
    setSliderProperties(&_sliderSaturationShape);
    _sliderSaturationShape.setLookAndFeel(&_sliderLookAndFeel);
    _sliderSaturationShape.setTooltip("Shape of the clipping knee. Turn up for a harder clip (sharper, more aggressive); turn down for softer saturation (rounder, smoother).");
    addAndMakeVisible(_labelSaturationShape);
    _labelSaturationShape.setText("SATURATION", juce::dontSendNotification);
    _labelSaturationShape.setJustificationType(juce::Justification::centred);
    _labelSaturationShape.setColour(juce::Label::textColourId, juce::Colours::white);

    // Tube Sag slider setup
    addAndMakeVisible(_sliderTubeSag);
    setSliderProperties(&_sliderTubeSag);
    _sliderTubeSag.setLookAndFeel(&_sliderLookAndFeel);
    _sliderTubeSag.setTooltip("Simulated power-supply sag. Turn up for more sag and a chewy, compressed attack (looser low end); turn down for a tighter, faster response.");
    addAndMakeVisible(_labelTubeSag);
    _labelTubeSag.setText("TUBE SAG", juce::dontSendNotification);
    _labelTubeSag.setJustificationType(juce::Justification::centred);
    _labelTubeSag.setColour(juce::Label::textColourId, juce::Colours::white);

    // Create parameter attachments
    _sliderAttachmentInput = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "INPUT", _sliderInput);
    _sliderAttachmentPreGain = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PREGAIN", _sliderPreGain);
    _sliderAttachmentResonance = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RESONANCE", _sliderResonance);
    _sliderAttachmentDrive = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DRIVE", _sliderDrive);
    _sliderAttachmentAsymmetry = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ASYMMETRY", _sliderAsymmetry);
    _sliderAttachmentHarmonicCharacter = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "HARMONIC_CHARACTER", _sliderHarmonicCharacter);
    _sliderAttachmentSaturationShape = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "SATURATION_SHAPE", _sliderSaturationShape);
    _sliderAttachmentTubeSag = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TUBE_SAG", _sliderTubeSag);
    _sliderAttachmentBass = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "BASS", _sliderBass);
    _sliderAttachmentMid = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "MID", _sliderMid);
    _sliderAttachmentTreble = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TREBLE", _sliderTreble);
    _sliderAttachmentPresence = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PRESENCE", _sliderPresence);
    _sliderAttachmentPostGain = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "POSTGAIN", _sliderPostGain);
}

AmpPanel::~AmpPanel() = default;

void AmpPanel::setSliderProperties(juce::Slider* sliderToSet)
{
    sliderToSet->setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    sliderToSet->setTextBoxStyle(juce::Slider::NoTextBox, false, 76, 38);
    sliderToSet->setDoubleClickReturnValue(true, 0.0f);
}

void AmpPanel::paint(juce::Graphics& g)
{
    // Transparent background to inherit parent's styling
    g.fillAll(juce::Colours::transparentBlack);
}

void AmpPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    const int sliderWidth = 80;
    const int sliderHeight = 100;
    const int rowSpacing = 12; // spacing between rows (between top labels and bottom sliders)
    const int labelHeight = 16;
    const int labelGap = 2; // gap between slider bottom and its label

    // Row 1: External knobs (7 knobs)
    // input, pre-gain, bass, mid, treble, presence, post-gain
    const int row1KnobCount = 7;
    const int row1Spacing = (bounds.getWidth() - (row1KnobCount * sliderWidth)) / (row1KnobCount + 1);

    // Row 2: Internal Params (6 knobs) at 80% scale
    const int row2KnobCount = 6;
    const float secondRowScale = 0.8f;
    const int row2SliderWidth = juce::roundToInt(sliderWidth * secondRowScale);
    const int row2SliderHeight = juce::roundToInt(sliderHeight * secondRowScale);
    const int row2Spacing = (bounds.getWidth() - (row2KnobCount * row2SliderWidth)) / (row2KnobCount + 1);

    // Vertically center the two rows including labels
    const int contentHeight = sliderHeight + labelGap + labelHeight + rowSpacing
                            + row2SliderHeight + labelGap + labelHeight;
    const int topRowY = bounds.getY() + juce::jmax(0, (bounds.getHeight() - contentHeight) / 2);
    const int topLabelsY = topRowY + sliderHeight + labelGap;
    const int bottomRowY = topLabelsY + labelHeight + rowSpacing;
    const int bottomLabelsY = bottomRowY + row2SliderHeight + labelGap;

    // Lay out Row 1 sliders and labels
    int x = bounds.getX() + row1Spacing;

    _sliderInput.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelInput.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPreGain.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelPreGain.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderBass.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelBass.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderMid.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelMid.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderTreble.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelTreble.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPresence.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelPresence.setBounds(x, topLabelsY, sliderWidth, labelHeight);
    x += sliderWidth + row1Spacing;

    _sliderPostGain.setBounds(x, topRowY, sliderWidth, sliderHeight);
    _labelPostGain.setBounds(x, topLabelsY, sliderWidth, labelHeight);

    // Lay out Row 2 sliders and labels (80% scale)
    x = bounds.getX() + row2Spacing;

    _sliderResonance.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelResonance.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
    x += row2SliderWidth + row2Spacing;

    _sliderDrive.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelDrive.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
    x += row2SliderWidth + row2Spacing;

    _sliderAsymmetry.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelAsymmetry.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
    x += row2SliderWidth + row2Spacing;

    _sliderHarmonicCharacter.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelHarmonicCharacter.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
    x += row2SliderWidth + row2Spacing;

    _sliderSaturationShape.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelSaturationShape.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
    x += row2SliderWidth + row2Spacing;

    _sliderTubeSag.setBounds(x, bottomRowY, row2SliderWidth, row2SliderHeight);
    _labelTubeSag.setBounds(x, bottomLabelsY, row2SliderWidth, labelHeight);
}