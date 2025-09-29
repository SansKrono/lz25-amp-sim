#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

class AmpPanel : public juce::Component
{
public:
    AmpPanel(juce::AudioProcessorValueTreeState& apvts);
    ~AmpPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setSliderProperties(juce::Slider* sliderToSet);

    // Background amp faceplate image and computed areas
    juce::Image _ampFaceImage;
    juce::Rectangle<int> _ampImageArea; // scaled image area within component
    juce::Rectangle<int> _knobArea;     // lower half area where knobs are placed

    // Input stage sliders
    juce::Slider _sliderInput;
    juce::Label _labelInput;
    
    juce::Slider _sliderPreGain;
    juce::Label _labelPreGain;
    
    juce::Slider _sliderResonance;
    juce::Label _labelResonance;

    // EQ and output sliders
    juce::Slider _sliderBass;
    juce::Label _labelBass;

    juce::Slider _sliderMid;
    juce::Label _labelMid;

    juce::Slider _sliderTreble;
    juce::Label _labelTreble;

    juce::Slider _sliderPresence;
    juce::Label _labelPresence;

    juce::Slider _sliderPostGain;
    juce::Label _labelPostGain;

    // Waveshaper parameter sliders
    juce::Slider _sliderDrive;
    juce::Label _labelDrive;

    juce::Slider _sliderAsymmetry;
    juce::Label _labelAsymmetry;

    juce::Slider _sliderHarmonicCharacter;
    juce::Label _labelHarmonicCharacter;

    juce::Slider _sliderSaturationShape;
    juce::Label _labelSaturationShape;

    juce::Slider _sliderTubeSag;
    juce::Label _labelTubeSag;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentInput;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentPreGain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentBass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentMid;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentTreble;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentPresence;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentPostGain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentResonance;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentDrive;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentAsymmetry;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentHarmonicCharacter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentSaturationShape;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _sliderAttachmentTubeSag;


    // Look and feel
    MXRLookAndFeel _sliderLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpPanel)
};