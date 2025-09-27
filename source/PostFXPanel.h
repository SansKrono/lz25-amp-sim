#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PostFXPanel : public juce::Component
{
public:
    PostFXPanel(juce::AudioProcessorValueTreeState& apvts);
    ~PostFXPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostFXPanel)
};