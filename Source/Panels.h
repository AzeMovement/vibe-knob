#pragma once

#include "PluginProcessor.h"
#include "MoodLibrary.h"
#include "Style.h"

// Covers a feature the free edition doesn't have.
class ProLock : public juce::Component
{
public:
    explicit ProLock (juce::String feature) : what (std::move (feature)) {}
    void paint (juce::Graphics&) override;

private:
    juce::String what;
};

// "Under the hood": the real settings the knob is turning. In Pro you can take over and teach it a new mood.
class HoodPanel : public juce::Component
{
public:
    HoodPanel (VibeKnobProcessor&, MoodLibrary&);

    void refresh (juce::Colour moodColour);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::String& savedName, bool addedToKnob)> onSaved;

private:
    void askForName();
    void saveAs (const juce::String& name);

    struct Row
    {
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::Rectangle<int> nameArea, valueArea;
    };

    VibeKnobProcessor& processor;
    MoodLibrary& library;
    std::array<Row, Settings::count> rows;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> groups;
    juce::TextButton teach { "TEACH A MOOD" }, save { "SAVE MOOD" };
    juce::String status;
    std::unique_ptr<juce::AlertWindow> nameWindow;
};

// Pro: choose which moods sit around the knob, and share moods as files.
class MoodsPanel : public juce::Component, private juce::ChangeListener
{
public:
    MoodsPanel (VibeKnobProcessor&, MoodLibrary&);
    ~MoodsPanel() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void slotChanged();
    void rebuildMenus();

    VibeKnobProcessor& processor;
    MoodLibrary& library;
    std::vector<Mood> choices;
    std::array<juce::ComboBox, edition::maxRingMoods> slots;
    juce::TextButton importButton { "IMPORT" }, exportButton { "EXPORT MY MOODS" }, folderButton { "OPEN FOLDER" };
    juce::String status;
    int shownVersion = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Pro: let the knob move by itself.
class MotionPanel : public juce::Component
{
public:
    explicit MotionPanel (VibeKnobProcessor&);

    void refresh (juce::Colour moodColour);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VibeKnobProcessor& processor;
    std::array<juce::TextButton, 3> modes { juce::TextButton ("OFF"), juce::TextButton ("DRIFT"), juce::TextButton ("FOLLOW") };
    juce::Slider depth { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox },
                 speed { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment depthAttachment, speedAttachment;
    juce::ParameterAttachment modeAttachment;
    int mode = 0;
};
