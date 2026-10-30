#include "Panels.h"

namespace
{
    const juce::String proOnly = juce::String ("Available in ") + edition::proName();

    void paintHeading (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
    {
        g.setColour (style::quietText);
        g.setFont (style::font (11.0f, true));
        g.drawText (text, area, juce::Justification::bottomLeft);
    }

    // A colour for a new mood, from what it does: dark and heavy is violet, bright is yellow, gritty leans red.
    juce::Colour colourFor (const Settings& s)
    {
        const auto tilt = juce::jlimit (-1.0f, 1.0f, (s[Settings::high] - s[Settings::low]) / 10.0f
                                                     - (s[Settings::highCut] < 8000.0f ? 0.4f : 0.0f));
        auto hue = juce::jmap (tilt, -1.0f, 1.0f, 0.72f, 0.15f);
        hue = hue + (0.0f - hue) * s[Settings::grit] * s[Settings::drive];
        const auto saturation = 0.55f + 0.35f * juce::jmin (1.0f, s[Settings::drive] + s[Settings::reverb]);
        return juce::Colour::fromHSV (hue, saturation, 1.0f, 1.0f);
    }
}

void ProLock::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds();
    g.setColour (style::background.withAlpha (0.94f));
    g.fillRoundedRectangle (area.toFloat(), 10.0f);
    g.setColour (style::text);
    g.setFont (style::font (14.0f, true));
    g.drawText ("Unlock " + what + " with Vibe Knob Pro", area.withHeight (22).withCentre (area.getCentre().translated (0, -16)), juce::Justification::centred);
    g.setColour (style::quietText);
    g.setFont (style::font (12.0f));
    g.drawFittedText ("Pro adds 8 more moods, teaching your own moods, the mood pad, motion, and sharing moods as files",
                      area.reduced (40, 0).withHeight (36).withCentre (area.getCentre().translated (0, 16)), juce::Justification::centredTop, 2, 1.0f);
}

//==============================================================================
HoodPanel::HoodPanel (VibeKnobProcessor& p, MoodLibrary& l) : processor (p), library (l)
{
    for (int i = 0; i < Settings::count; ++i)
    {
        const auto& info = settingInfo()[(size_t) i];
        auto& s = rows[(size_t) i].slider;
        s.setRange (info.min, info.max);
        if (info.logScale)
            s.setSkewFactorFromMidPoint (std::sqrt (info.min * info.max));
        s.getProperties().set ("rest", info.rest);
        s.setTooltip (info.help);
        s.setEnabled (false);
        s.onValueChange = [this, i] { processor.setTeachSetting (i, (float) rows[(size_t) i].slider.getValue()); };
        addAndMakeVisible (s);
    }

    teach.setClickingTogglesState (false);
    teach.setTooltip (edition::isPro ? "Pause the knob and set the effects by hand. When it sounds right, save it as a new mood."
                                     : proOnly);
    teach.setEnabled (edition::isPro);
    teach.onClick = [this]
    {
        if (processor.isTeaching())
            processor.stopTeaching();
        else
            processor.startTeaching();
        status = {};
        repaint();
    };
    addAndMakeVisible (teach);

    save.setTooltip (edition::isPro ? "Save these settings as a mood you can put on the knob" : proOnly);
    save.onClick = [this] { askForName(); };
    addAndMakeVisible (save);
}

void HoodPanel::refresh (juce::Colour moodColour)
{
    const auto teaching = processor.isTeaching();
    const auto settings = teaching ? processor.getTeachSettings() : processor.getPlaying();

    for (int i = 0; i < Settings::count; ++i)
    {
        auto& s = rows[(size_t) i].slider;
        s.setEnabled (teaching);
        s.setColour (juce::Slider::trackColourId, teaching ? style::text : moodColour);
        if (! s.isMouseButtonDown())
            s.setValue (settings[i], juce::dontSendNotification);
    }

    teach.setToggleState (teaching, juce::dontSendNotification);
    teach.setButtonText (teaching ? "STOP TEACHING" : "TEACH A MOOD");
    teach.setColour (juce::TextButton::buttonOnColourId, style::text);
    save.setEnabled (edition::isPro && teaching);
    repaint();
}

void HoodPanel::paint (juce::Graphics& g)
{
    for (auto& [name, area] : groups)
        paintHeading (g, name, area);

    const auto teaching = processor.isTeaching();
    const auto settings = teaching ? processor.getTeachSettings() : processor.getPlaying();

    g.setFont (style::font (12.0f));
    for (int i = 0; i < Settings::count; ++i)
    {
        auto& row = rows[(size_t) i];
        g.setColour (style::text.withAlpha (teaching ? 1.0f : 0.8f));
        g.drawText (settingInfo()[(size_t) i].name, row.nameArea, juce::Justification::centredLeft);
        g.setColour (style::quietText);
        g.drawText (formatSetting (i, settings[i]), row.valueArea, juce::Justification::centredRight);
    }

    g.setColour (style::quietText);
    g.setFont (style::font (12.0f));
    g.drawFittedText (status.isNotEmpty() ? status
                                          : juce::String (processor.isTeaching() ? "The knob is paused. Shape the sound, then save it as a mood."
                                                                                 : "These are the real settings the knob is turning right now."),
                      getLocalBounds().removeFromBottom (24), juce::Justification::centredLeft, 1);
}

void HoodPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom (24);   // status line
    auto buttons = area.removeFromBottom (34).withTrimmedTop (4);
    teach.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    save.setBounds (buttons);

    // Two columns: tone and cuts on the left, drive and space on the right.
    struct Group { const char* name; std::initializer_list<int> settings; };
    const Group columns[2][2] = {
        { { "TONE", { Settings::low, Settings::mid, Settings::midFreq, Settings::high } }, { "CUTS", { Settings::lowCut, Settings::highCut } } },
        { { "SATURATION", { Settings::drive, Settings::grit, Settings::crush } }, { "SPACE", { Settings::width, Settings::reverb, Settings::space } } }
    };

    groups.clear();
    auto top = area.withTrimmedBottom (6);
    const auto columnWidth = (top.getWidth() - 20) / 2;
    for (int col = 0; col < 2; ++col)
    {
        auto column = (col == 0 ? top.removeFromLeft (columnWidth) : top.removeFromRight (columnWidth));
        for (auto& group : columns[col])
        {
            groups.push_back ({ group.name, column.removeFromTop (22) });
            for (auto index : group.settings)
            {
                auto line = column.removeFromTop (36);
                auto& row = rows[(size_t) index];
                auto label = line.removeFromTop (16);
                row.nameArea = label.removeFromLeft (label.getWidth() / 2);
                row.valueArea = label;
                row.slider.setBounds (line.withTrimmedRight (0));
            }
            column.removeFromTop (6);
        }
    }
}

void HoodPanel::askForName()
{
    nameWindow = std::make_unique<juce::AlertWindow> ("Save mood", "Give your mood a name.", juce::MessageBoxIconType::NoIcon, this);
    nameWindow->addTextEditor ("name", "My mood");
    nameWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<HoodPanel> safe (this);
    nameWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->saveAs (safe->nameWindow->getTextEditorContents ("name"));
        safe->nameWindow.reset();
    }));
}

void HoodPanel::saveAs (const juce::String& name)
{
    Mood mood;
    mood.name = name;
    mood.settings = processor.getTeachSettings();
    mood.colour = colourFor (mood.settings);
    const auto savedName = library.save (mood);
    mood.name = savedName;

    // Put it on the knob if there's room, and turn the knob to it.
    auto ringMoods = processor.getRing();
    ringMoods.erase (std::remove_if (ringMoods.begin(), ringMoods.end(), [&] (const Mood& m) { return m.name == savedName; }), ringMoods.end());
    const auto added = ringMoods.size() < (size_t) edition::maxRingMoods;
    if (added)
    {
        ringMoods.push_back (mood);
        processor.setRing (ringMoods);
        processor.params.getParameter ("vibe")->setValueNotifyingHost (1.0f);
    }

    processor.stopTeaching();
    status = added ? "Saved \"" + savedName + "\" and added it to the knob."
                   : "Saved \"" + savedName + "\". The knob is full: swap it on in MOODS.";
    if (onSaved)
        onSaved (savedName, added);
    refresh (mood.colour);
}

//==============================================================================
MoodsPanel::MoodsPanel (VibeKnobProcessor& p, MoodLibrary& l) : processor (p), library (l)
{
    for (auto& s : slots)
    {
        s.onChange = [this] { slotChanged(); };
        addAndMakeVisible (s);
    }

    importButton.setTooltip ("Add moods from a .vibemoods pack or a .vibemood file");
    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import moods", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       juce::String ("*") + MoodLibrary::packExtension + ";*" + MoodLibrary::moodExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File())
                return;
            const auto added = library.importFrom (fc.getResult());
            status = added > 0 ? "Imported " + juce::String (added) + (added == 1 ? " mood." : " moods.")
                               : juce::String ("No moods found in that file.");
            repaint();
        });
    };
    addAndMakeVisible (importButton);

    exportButton.setTooltip ("Save all the moods you've taught it into one file you can share");
    exportButton.onClick = [this]
    {
        if (library.userMoods().empty())
        {
            status = "Teach it a mood first (UNDER THE HOOD), then you can export it.";
            repaint();
            return;
        }
        chooser = std::make_unique<juce::FileChooser> ("Export moods",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (juce::String ("My moods") + MoodLibrary::packExtension),
                                                       juce::String ("*") + MoodLibrary::packExtension);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File())
                return;
            const auto file = fc.getResult().withFileExtension (MoodLibrary::packExtension);
            status = library.exportTo (file) ? "Exported " + juce::String ((int) library.userMoods().size()) + " moods to " + file.getFileName()
                                             : juce::String ("Couldn't write that file.");
            repaint();
        });
    };
    addAndMakeVisible (exportButton);

    folderButton.setTooltip ("Your moods are files in this folder");
    folderButton.onClick = [this]
    {
        library.getFolder().createDirectory();
        library.getFolder().startAsProcess();
    };
    addAndMakeVisible (folderButton);

    library.addChangeListener (this);
    rebuildMenus();
}

MoodsPanel::~MoodsPanel()
{
    library.removeChangeListener (this);
}

void MoodsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildMenus();
}

void MoodsPanel::rebuildMenus()
{
    choices = library.available();
    const auto builtInCount = std::count_if (choices.begin(), choices.end(), [] (const Mood& m) { return m.builtIn; });

    for (auto& s : slots)
    {
        s.clear (juce::dontSendNotification);
        s.addItem ("(empty)", 1);
        s.addSectionHeading ("Built in");
        for (size_t i = 0; i < choices.size(); ++i)
        {
            if ((long) i == builtInCount)
                s.addSectionHeading ("Yours");
            s.addItem (choices[i].name, (int) i + 2);
        }
    }

    shownVersion = -1;
    refresh();
}

void MoodsPanel::refresh()
{
    if (shownVersion == processor.getSetupVersion())
        return;
    shownVersion = processor.getSetupVersion();

    const auto& ringMoods = processor.getRing();
    for (size_t i = 0; i < slots.size(); ++i)
    {
        auto id = 1;
        if (i < ringMoods.size())
            for (size_t c = 0; c < choices.size(); ++c)
                if (choices[c].name == ringMoods[i].name)
                    id = (int) c + 2;

        // A mood that's on the knob but no longer in the library (deleted, or from another computer).
        if (i < ringMoods.size() && id == 1)
        {
            choices.push_back (ringMoods[i]);
            for (auto& s : slots)
                s.addItem (ringMoods[i].name, (int) choices.size() + 1);
            id = (int) choices.size() + 1;
        }

        slots[i].setSelectedId (id, juce::dontSendNotification);
    }
}

void MoodsPanel::slotChanged()
{
    std::vector<Mood> chosen;
    for (auto& s : slots)
        if (s.getSelectedId() > 1)
            chosen.push_back (choices[(size_t) s.getSelectedId() - 2]);

    if (chosen.size() < 2)
    {
        status = "The knob needs at least two moods.";
        shownVersion = -1;
        refresh();
    }
    else
    {
        status = {};
        processor.setRing (chosen);
    }
    repaint();
}

void MoodsPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    paintHeading (g, "MOODS ON THE KNOB, IN ORDER", area.removeFromTop (22));

    g.setFont (style::font (12.0f));
    for (size_t i = 0; i < slots.size(); ++i)
    {
        g.setColour (style::quietText);
        g.drawText (juce::String ((int) i + 1), slots[i].getBounds().withX (slots[i].getX() - 22).withWidth (18), juce::Justification::centredRight);
        if (slots[i].getSelectedId() > 1)
        {
            g.setColour (choices[(size_t) slots[i].getSelectedId() - 2].colour);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (slots[i].getBounds().toFloat().getCentre().withX ((float) slots[i].getRight() - 30.0f)));
        }
    }

    auto lower = area.withTop (exportButton.getBottom() + 6);
    g.setColour (style::quietText);
    g.drawFittedText (status.isNotEmpty() ? status
                                          : juce::String ("Moods you teach it are saved for every project. Share them with EXPORT, "
                                                          "and add other people's with IMPORT."),
                      lower.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
}

void MoodsPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (28);

    const auto columnWidth = (area.getWidth() - 24) / 2;
    auto grid = area.removeFromTop (4 * 36);
    for (int col = 0; col < 2; ++col)
    {
        auto column = grid.withX (grid.getX() + col * (columnWidth + 24)).withWidth (columnWidth);
        for (int row = 0; row < 4; ++row)
            slots[(size_t) (col * 4 + row)].setBounds (column.removeFromTop (36).withTrimmedBottom (6).withTrimmedLeft (24));
    }

    area.removeFromTop (12);
    auto buttons = area.removeFromTop (30);
    const auto w = (buttons.getWidth() - 16) / 3;
    importButton.setBounds (buttons.removeFromLeft (w));
    buttons.removeFromLeft (8);
    exportButton.setBounds (buttons.removeFromLeft (w + 4));
    buttons.removeFromLeft (8);
    folderButton.setBounds (buttons);
}

//==============================================================================
MotionPanel::MotionPanel (VibeKnobProcessor& p)
    : processor (p),
      depthAttachment (p.params, "depth", depth),
      speedAttachment (p.params, "speed", speed),
      modeAttachment (*p.params.getParameter ("motion"), [this] (float v)
      {
          mode = juce::roundToInt (v);
          for (size_t i = 0; i < modes.size(); ++i)
              modes[i].setToggleState ((int) i == mode, juce::dontSendNotification);
          repaint();
      })
{
    const char* tips[] = { "The knob stays where you put it",
                           "The knob sways slowly back and forth around where you put it",
                           "The knob leans forward when the music gets louder" };
    for (size_t i = 0; i < modes.size(); ++i)
    {
        modes[i].setTooltip (tips[i]);
        modes[i].onClick = [this, i] { modeAttachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modes[i]);
    }

    depth.setTooltip ("How far the knob moves");
    speed.setTooltip ("Drift: how fast it sways. Follow: how quickly it settles back.");
    for (auto* s : { &depth, &speed })
        addAndMakeVisible (*s);

    modeAttachment.sendInitialUpdate();
}

void MotionPanel::refresh (juce::Colour moodColour)
{
    for (auto& b : modes)
        b.setColour (juce::TextButton::buttonOnColourId, moodColour);
    depth.setColour (juce::Slider::trackColourId, moodColour);
    speed.setColour (juce::Slider::trackColourId, moodColour);
}

void MotionPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    paintHeading (g, "MOOD MOTION", area.removeFromTop (22));

    g.setFont (style::font (12.0f));
    for (auto* s : { &depth, &speed })
    {
        const auto below = s->getBounds().withY (s->getBottom()).withHeight (34);
        g.setColour (style::text);
        g.drawText (s == &depth ? "Depth" : "Speed", below.withHeight (16), juce::Justification::centred);
        g.setColour (style::quietText);
        g.drawText (juce::String (juce::roundToInt (s->getValue())) + "%", below.withTrimmedTop (16), juce::Justification::centred);
    }

    const juce::String explain[] = {
        "Turn on Drift or Follow and the knob moves by itself, so the sound keeps shifting between moods.",
        "Drift sways the knob back and forth around where you set it. The ring shows where it is right now.",
        "Follow pushes the knob forward when the music gets louder, so hits land in the next mood along."
    };
    g.setColour (style::quietText);
    g.setFont (style::font (12.0f));
    g.drawFittedText (explain[juce::jlimit (0, 2, mode)], area.withTop (depth.getBottom() + 46).reduced (2, 0), juce::Justification::topLeft, 4, 1.0f);
}

void MotionPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (28);
    auto row = area.removeFromTop (30);
    const auto w = (row.getWidth() - 16) / 3;
    for (auto& b : modes)
    {
        b.setBounds (row.removeFromLeft (w));
        row.removeFromLeft (8);
    }

    area.removeFromTop (20);
    auto knobs = area.removeFromTop (90);
    depth.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).withSizeKeepingCentre (90, 90));
    speed.setBounds (knobs.withSizeKeepingCentre (90, 90));
}
