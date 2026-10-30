#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <vector>

// Everything one mood sets on the effects under the knob.
struct Settings
{
    enum Index { low, mid, midFreq, high, lowCut, highCut, drive, grit, crush, width, reverb, space, count };

    std::array<float, count> v {};

    float& operator[] (int i)       { return v[(size_t) i]; }
    float  operator[] (int i) const { return v[(size_t) i]; }

    // Every effect at rest: the sound passes through unchanged.
    static Settings neutral();
};

struct SettingInfo
{
    const char* id;
    const char* name;
    float min, max, rest;
    bool logScale;   // frequencies blend by octaves, not by Hz
    const char* help;
};

const std::array<SettingInfo, Settings::count>& settingInfo();
juce::String formatSetting (int index, float value);

// Blends two settings; t = 0 is a, t = 1 is b.
Settings blend (const Settings& a, const Settings& b, float t);

// How strongly a mood is applied; 0 is no effect at all.
inline Settings withAmount (const Settings& s, float amount) { return blend (Settings::neutral(), s, amount); }

struct Mood
{
    juce::String name;
    juce::Colour colour;
    Settings settings;
    bool builtIn = false;
    bool proOnly = false;

    juce::var toVar() const;
    static bool fromVar (const juce::var&, Mood&);
};

// The moods that come with the plugin. The first four are in the free edition.
const std::vector<Mood>& builtInMoods();
const Mood* findBuiltIn (const juce::String& name);

namespace ring
{
    // Where mood i of n sits along the knob (0 to 1).
    inline float positionOf (int i, int n) { return n < 2 ? 0.0f : (float) i / (float) (n - 1); }

    // The two moods either side of a knob position and how far between them it is.
    struct Between { int from, to; float t; };
    Between locate (float position, int n);

    Settings settingsAt (const Settings* const* moods, int n, float position);
    Settings settingsAt (const std::vector<Mood>&, float position);
    juce::Colour colourAt (const std::vector<Mood>&, float position);
}

namespace pad
{
    // Corners: 0 top left, 1 top right, 2 bottom left, 3 bottom right. y = 0 is the bottom.
    Settings settingsAt (const std::array<const Settings*, 4>&, float x, float y);
    juce::Colour colourAt (const std::array<juce::Colour, 4>&, float x, float y);
}
