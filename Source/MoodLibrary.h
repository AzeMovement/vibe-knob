#pragma once

#include <juce_events/juce_events.h>
#include "Mood.h"

// Every mood you can put on the knob: the built-in ones, plus the ones you've taught it (Pro).
// Your moods are files in a folder, so they're shared by every project and can be backed up or swapped.
// Message thread only. Shared by all open Vibe Knobs.
class MoodLibrary : public juce::ChangeBroadcaster
{
public:
    MoodLibrary();

    // Built-in moods for this edition, then yours.
    std::vector<Mood> available() const;
    const std::vector<Mood>& userMoods() const { return user; }
    const Mood* find (const juce::String& name) const;

    // Saves (or replaces) one of your moods. Returns the name it was saved under.
    juce::String save (Mood);
    bool remove (const juce::String& name);

    // Packs hold any number of moods; a single mood file works too. Returns how many were added.
    int importFrom (const juce::File&);
    bool exportTo (const juce::File&) const;

    juce::File getFolder() const;
    void reload();

    static constexpr const char* moodExtension = ".vibemood";
    static constexpr const char* packExtension = ".vibemoods";

private:
    juce::File fileFor (const juce::String& name) const;
    std::vector<Mood> user;
};
