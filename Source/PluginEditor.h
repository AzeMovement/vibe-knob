#pragma once

#include "PluginProcessor.h"
#include "KnobView.h"
#include "Panels.h"

class VibeKnobEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VibeKnobEditor (VibeKnobProcessor&);
    ~VibeKnobEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int);
    void showControl (bool usePad);

    VibeKnobProcessor& processor;
    style::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this };
    juce::SharedResourcePointer<MoodLibrary> library;

    KnobView knob;
    PadView pad;
    juce::TextButton knobButton { "KNOB" }, padButton { "PAD" };
    juce::ParameterAttachment modeAttachment;

    juce::Slider amount { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox },
                 output { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::TextButton autoGainButton { "AUTO VOLUME" };
    juce::AudioProcessorValueTreeState::SliderAttachment amountAttachment, outputAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAttachment;

    std::array<juce::TextButton, 3> tabs { juce::TextButton ("UNDER THE HOOD"), juce::TextButton ("MOODS"), juce::TextButton ("MOTION") };
    HoodPanel hood;
    MoodsPanel moods;
    MotionPanel motion;
    ProLock moodsLock { "your own knob layout" }, motionLock { "mood motion" };
    int tab = 0;

    juce::Rectangle<int> leftPanel, rightPanel, controlsArea;
    juce::Colour colour = style::accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeKnobEditor)
};
