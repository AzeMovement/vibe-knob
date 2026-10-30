#pragma once

#include "PluginProcessor.h"
#include "MoodLibrary.h"
#include "Style.h"

// The big knob, with its moods around the edge. Drag up or round to turn it, click a mood's name to jump to it.
class KnobView : public juce::Component
{
public:
    explicit KnobView (VibeKnobProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float angleFor (float position) const;
    juce::Point<float> centre() const;
    float radius() const;
    juce::Rectangle<float> labelArea (int index, int count) const;

    VibeKnobProcessor& processor;
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    float value = 0.0f, dragStart = 0.0f;
    bool dragging = false;
};

// Pro: a square with a mood in each corner. Drag the dot to blend all four.
class PadView : public juce::Component
{
public:
    PadView (VibeKnobProcessor&, MoodLibrary&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    juce::Rectangle<float> cornerLabel (int corner) const;
    void moveTo (juce::Point<float>);

    VibeKnobProcessor& processor;
    MoodLibrary& library;
    juce::ParameterAttachment xAttachment, yAttachment;
    float x = 0.5f, y = 0.5f;
    bool dragging = false;
};
