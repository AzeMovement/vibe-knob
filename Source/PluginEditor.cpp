#include "PluginEditor.h"

namespace
{
    const juce::String proOnly = juce::String ("Available in ") + edition::proName();

    void paintTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& chip, juce::Colour chipColour)
    {
        g.setColour (style::text);
        g.setFont (style::font (18.0f, true));
        const auto title = juce::String ("VIBE KNOB");
        const auto width = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title));
        g.drawText (title, area.removeFromLeft (width), juce::Justification::centredLeft);

        area.removeFromLeft (8);
        g.setFont (style::font (10.0f, true));
        const auto chipWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), chip)) + 14;
        const auto chipArea = area.removeFromLeft (chipWidth).withSizeKeepingCentre (chipWidth, 18).toFloat();
        g.setColour (chipColour);
        g.drawRoundedRectangle (chipArea.reduced (0.5f), 9.0f, 1.0f);
        g.drawText (chip, chipArea, juce::Justification::centred);
    }
}

VibeKnobEditor::VibeKnobEditor (VibeKnobProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      knob (p), pad (p, *library),
      modeAttachment (*p.params.getParameter ("mode"), [this] (float v) { showControl (edition::isPro && v > 0.5f); }),
      amountAttachment (p.params, "amount", amount),
      outputAttachment (p.params, "output", output),
      autoGainAttachment (p.params, "autogain", autoGainButton),
      hood (p, *library), moods (p, *library), motion (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (knob);
    addChildComponent (pad);

    knobButton.setTooltip ("One knob that moves through the moods in order");
    knobButton.onClick = [this] { modeAttachment.setValueAsCompleteGesture (0.0f); };
    padButton.setTooltip (edition::isPro ? "A square with a mood in each corner: blend all four at once" : proOnly);
    padButton.setEnabled (edition::isPro);
    padButton.onClick = [this] { modeAttachment.setValueAsCompleteGesture (1.0f); };
    for (auto* b : { &knobButton, &padButton })
        addAndMakeVisible (*b);

    amount.setTooltip ("How strongly the mood is applied. At 0% the sound passes through untouched.");
    output.setTooltip ("Output volume");
    autoGainButton.setTooltip ("Keeps the output as loud as the input, so you judge the mood and not the volume");
    autoGainButton.setClickingTogglesState (true);
    for (auto* c : std::initializer_list<juce::Component*> { &amount, &output, &autoGainButton })
        addAndMakeVisible (*c);

    for (size_t i = 0; i < tabs.size(); ++i)
    {
        tabs[i].onClick = [this, i] { showTab ((int) i); };
        addAndMakeVisible (tabs[i]);
    }

    addChildComponent (hood);
    addChildComponent (moods);
    addChildComponent (motion);
    addChildComponent (moodsLock);
    addChildComponent (motionLock);

    modeAttachment.sendInitialUpdate();
    showTab (0);
    setSize (900, 520);
    timerCallback();
    startTimerHz (30);
}

VibeKnobEditor::~VibeKnobEditor()
{
    setLookAndFeel (nullptr);
}

void VibeKnobEditor::showTab (int index)
{
    tab = index;
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setToggleState ((int) i == tab, juce::dontSendNotification);

    hood.setVisible (tab == 0);
    moods.setVisible (tab == 1);
    motion.setVisible (tab == 2);
    moodsLock.setVisible (tab == 1 && ! edition::isPro);
    motionLock.setVisible (tab == 2 && ! edition::isPro);
    moods.setEnabled (edition::isPro);
    motion.setEnabled (edition::isPro);
}

void VibeKnobEditor::showControl (bool usePad)
{
    knob.setVisible (! usePad);
    pad.setVisible (usePad);
    knobButton.setToggleState (! usePad, juce::dontSendNotification);
    padButton.setToggleState (usePad, juce::dontSendNotification);
}

void VibeKnobEditor::timerCallback()
{
    colour = processor.isTeaching() ? style::text : processor.getColourNow();

    for (auto* b : { &knobButton, &padButton, &autoGainButton })
        b->setColour (juce::TextButton::buttonOnColourId, colour);
    for (auto& t : tabs)
        t.setColour (juce::TextButton::buttonOnColourId, colour);
    amount.setColour (juce::Slider::trackColourId, colour);
    output.setColour (juce::Slider::trackColourId, colour);

    hood.refresh (colour);
    moods.refresh();
    motion.refresh (colour);
    knob.repaint();
    pad.repaint();
    motion.repaint();
    repaint (controlsArea);
}

void VibeKnobEditor::paint (juce::Graphics& g)
{
    g.fillAll (style::background);
    paintTitle (g, getLocalBounds().reduced (20, 0).removeFromTop (52), edition::isPro ? "PRO" : "FREE", colour);

    g.setColour (style::panel);
    g.fillRoundedRectangle (leftPanel.toFloat(), 12.0f);
    g.fillRoundedRectangle (rightPanel.toFloat(), 12.0f);

    // Readouts under the small knobs.
    g.setFont (style::font (12.0f));
    for (auto* s : { &amount, &output })
    {
        const auto below = s->getBounds().withY (s->getBottom() - 2).withHeight (30);
        g.setColour (style::text);
        g.drawText (s == &amount ? "Amount" : "Output", below.withHeight (15), juce::Justification::centred);
        g.setColour (style::quietText);
        const auto v = s->getValue();
        g.drawText (s == &amount ? juce::String (juce::roundToInt (v)) + "%" : (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB",
                    below.withTrimmedTop (15), juce::Justification::centred);
    }

    const auto gainDb = processor.getAutoGainDb();
    g.setColour (style::quietText);
    g.drawFittedText (autoGainButton.getToggleState() ? "Matching input: " + juce::String (gainDb > 0.05f ? "+" : "") + juce::String (gainDb, 1) + " dB"
                                                      : juce::String ("Off: louder moods will sound louder"),
                      autoGainButton.getBounds().translated (0, 34).withHeight (18).expanded (30, 0), juce::Justification::centred, 1);
}

void VibeKnobEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    auto header = area.removeFromTop (52);
    area.removeFromBottom (16);

    leftPanel = area.removeFromLeft (480);
    area.removeFromLeft (14);
    rightPanel = area;

    auto left = leftPanel.reduced (14, 10);
    auto toggles = header.removeFromLeft (leftPanel.getWidth()).removeFromRight (150).withSizeKeepingCentre (150, 28);
    knobButton.setBounds (toggles.removeFromLeft (72));
    toggles.removeFromLeft (6);
    padButton.setBounds (toggles);

    controlsArea = left.removeFromBottom (104);
    knob.setBounds (left);
    pad.setBounds (left);

    auto controls = controlsArea.withTrimmedBottom (30);
    const auto third = controls.getWidth() / 3;
    amount.setBounds (controls.removeFromLeft (third).withSizeKeepingCentre (64, 64).translated (0, -6));
    output.setBounds (controls.removeFromLeft (third).withSizeKeepingCentre (64, 64).translated (0, -6));
    autoGainButton.setBounds (controls.withSizeKeepingCentre (130, 28).translated (0, -12));

    auto right = rightPanel.reduced (16, 12);
    auto tabRow = right.removeFromTop (30);
    const auto widths = { 150, 90, 90 };
    size_t i = 0;
    for (auto w : widths)
    {
        tabs[i++].setBounds (tabRow.removeFromLeft (w));
        tabRow.removeFromLeft (8);
    }

    right.removeFromTop (8);
    for (auto* c : std::initializer_list<juce::Component*> { &hood, &moods, &motion, &moodsLock, &motionLock })
        c->setBounds (right);
}
