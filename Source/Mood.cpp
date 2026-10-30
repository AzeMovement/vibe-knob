#include "Mood.h"

const std::array<SettingInfo, Settings::count>& settingInfo()
{
    static const std::array<SettingInfo, Settings::count> info {{
        { "low",     "Low",       -12.0f,    12.0f,    0.0f, false, "Lifts or cuts the bass (below about 150 Hz)" },
        { "mid",     "Mid",       -12.0f,    12.0f,    0.0f, false, "Lifts or cuts a band of mids at Mid freq" },
        { "midFreq", "Mid freq",  150.0f,  6000.0f, 1000.0f, true,  "Where the Mid band sits" },
        { "high",    "High",      -12.0f,    12.0f,    0.0f, false, "Lifts or cuts the top end (above about 5 kHz)" },
        { "lowCut",  "Low cut",    20.0f,   800.0f,   20.0f, true,  "Removes everything below this" },
        { "highCut", "High cut",  500.0f, 20000.0f, 20000.0f, true,  "Removes everything above this" },
        { "drive",   "Drive",       0.0f,     1.0f,    0.0f, false, "How hard the saturation is pushed" },
        { "grit",    "Grit",        0.0f,     1.0f,    0.0f, false, "Saturation character: smooth and warm at 0%, edgy and hard at 100%" },
        { "crush",   "Crush",       0.0f,     1.0f,    0.0f, false, "Lo-fi: fewer bits and a lower sample rate" },
        { "width",   "Width",       0.0f,     2.0f,    1.0f, false, "Stereo width: 0% is mono, 100% unchanged, 200% extra wide" },
        { "reverb",  "Reverb",      0.0f,     1.0f,    0.0f, false, "How much reverb is mixed in" },
        { "space",   "Size",        0.0f,     1.0f,    0.5f, false, "How big the reverb's room is" },
    }};
    return info;
}

Settings Settings::neutral()
{
    Settings s;
    for (int i = 0; i < count; ++i)
        s[i] = settingInfo()[(size_t) i].rest;
    return s;
}

juce::String formatSetting (int index, float value)
{
    switch (index)
    {
        case Settings::low: case Settings::mid: case Settings::high:
            return (value > 0.05f ? "+" : "") + juce::String (value, 1) + " dB";
        case Settings::midFreq: case Settings::lowCut: case Settings::highCut:
            if (index == Settings::lowCut && value <= 20.5f)    return "Off";
            if (index == Settings::highCut && value >= 19900.0f) return "Off";
            return value >= 1000.0f ? juce::String (value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz"
                                    : juce::String (juce::roundToInt (value)) + " Hz";
        default:
            return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }
}

Settings blend (const Settings& a, const Settings& b, float t)
{
    Settings out;
    for (int i = 0; i < Settings::count; ++i)
    {
        if (settingInfo()[(size_t) i].logScale)
            out[i] = std::exp (std::log (a[i]) + (std::log (b[i]) - std::log (a[i])) * t);
        else
            out[i] = a[i] + (b[i] - a[i]) * t;
    }
    return out;
}

//==============================================================================
juce::var Mood::toVar() const
{
    auto* settingsObject = new juce::DynamicObject();
    for (int i = 0; i < Settings::count; ++i)
        settingsObject->setProperty (settingInfo()[(size_t) i].id, settings[i]);

    auto* object = new juce::DynamicObject();
    object->setProperty ("name", name);
    object->setProperty ("colour", colour.toDisplayString (false));
    object->setProperty ("settings", juce::var (settingsObject));
    return juce::var (object);
}

bool Mood::fromVar (const juce::var& v, Mood& mood)
{
    const auto name = v["name"].toString().trim();
    const auto& stored = v["settings"];
    if (name.isEmpty() || ! stored.isObject())
        return false;

    mood = {};
    mood.name = name.substring (0, 24);
    mood.colour = juce::Colour::fromString ("ff" + v["colour"].toString().retainCharacters ("0123456789abcdefABCDEF").substring (0, 6));
    mood.settings = Settings::neutral();

    for (int i = 0; i < Settings::count; ++i)
    {
        const auto& info = settingInfo()[(size_t) i];
        const auto& value = stored[info.id];
        if (! value.isVoid())
            mood.settings[i] = juce::jlimit (info.min, info.max, (float) (double) value);
    }

    return true;
}

//==============================================================================
namespace
{
    Mood make (const char* name, juce::uint32 colour, bool pro,
               std::initializer_list<std::pair<Settings::Index, float>> changes)
    {
        Mood m;
        m.name = name;
        m.colour = juce::Colour (colour);
        m.settings = Settings::neutral();
        m.builtIn = true;
        m.proOnly = pro;
        for (auto& c : changes)
            m.settings[c.first] = c.second;
        return m;
    }
}

const std::vector<Mood>& builtInMoods()
{
    using S = Settings;
    static const std::vector<Mood> moods {
        make ("Dark", 0xff7b6cff, false, { { S::low, 3.0f }, { S::mid, -1.5f }, { S::midFreq, 2500.0f }, { S::high, -7.0f },
                                           { S::highCut, 7000.0f }, { S::drive, 0.15f }, { S::grit, 0.1f },
                                           { S::width, 0.9f }, { S::reverb, 0.12f }, { S::space, 0.5f } }),
        make ("Warm", 0xffffa646, false, { { S::low, 2.5f }, { S::mid, 1.5f }, { S::midFreq, 400.0f }, { S::high, -2.5f },
                                           { S::highCut, 14000.0f }, { S::drive, 0.35f }, { S::grit, 0.0f },
                                           { S::reverb, 0.08f }, { S::space, 0.35f } }),
        make ("Aggressive", 0xffff4b4b, false, { { S::low, 2.0f }, { S::mid, 4.0f }, { S::midFreq, 1800.0f }, { S::high, 2.0f },
                                                 { S::lowCut, 40.0f }, { S::drive, 0.75f }, { S::grit, 0.8f },
                                                 { S::width, 1.1f }, { S::reverb, 0.02f }, { S::space, 0.2f } }),
        make ("Dreamy", 0xff6ee7ff, false, { { S::low, -1.5f }, { S::mid, -2.0f }, { S::midFreq, 600.0f }, { S::high, 3.5f },
                                             { S::lowCut, 60.0f }, { S::highCut, 16000.0f }, { S::drive, 0.1f },
                                             { S::width, 1.5f }, { S::reverb, 0.45f }, { S::space, 0.9f } }),

        make ("Bright", 0xfffff06a, true, { { S::low, -1.0f }, { S::mid, 1.0f }, { S::midFreq, 3500.0f }, { S::high, 6.0f },
                                            { S::drive, 0.1f }, { S::width, 1.15f } }),
        make ("Lo-fi", 0xffc49a6c, true, { { S::mid, 2.0f }, { S::midFreq, 1200.0f }, { S::lowCut, 150.0f }, { S::highCut, 4500.0f },
                                           { S::drive, 0.4f }, { S::grit, 0.3f }, { S::crush, 0.55f },
                                           { S::width, 0.6f }, { S::reverb, 0.1f }, { S::space, 0.3f } }),
        make ("Punchy", 0xffff7a2e, true, { { S::low, 4.0f }, { S::mid, -2.0f }, { S::midFreq, 350.0f }, { S::high, 2.0f },
                                            { S::lowCut, 30.0f }, { S::drive, 0.5f }, { S::grit, 0.45f } }),
        make ("Vintage", 0xffd8b26e, true, { { S::low, 1.5f }, { S::mid, 2.0f }, { S::midFreq, 900.0f }, { S::high, -4.0f },
                                             { S::lowCut, 50.0f }, { S::highCut, 11000.0f }, { S::drive, 0.45f }, { S::grit, 0.15f },
                                             { S::crush, 0.1f }, { S::width, 0.8f }, { S::reverb, 0.12f }, { S::space, 0.4f } }),
        make ("Airy", 0xffa8ffd9, true, { { S::mid, -1.5f }, { S::midFreq, 400.0f }, { S::high, 5.0f }, { S::lowCut, 40.0f },
                                          { S::width, 1.35f }, { S::reverb, 0.25f }, { S::space, 0.7f } }),
        make ("Telephone", 0xff7dff6a, true, { { S::mid, 6.0f }, { S::midFreq, 1500.0f }, { S::lowCut, 400.0f }, { S::highCut, 3400.0f },
                                               { S::drive, 0.55f }, { S::grit, 0.6f }, { S::crush, 0.25f }, { S::width, 0.0f } }),
        make ("Wide", 0xff4d8dff, true, { { S::high, 1.5f }, { S::width, 1.9f }, { S::reverb, 0.15f }, { S::space, 0.6f } }),
        make ("Underwater", 0xff1fa3a3, true, { { S::low, 3.0f }, { S::mid, -3.0f }, { S::midFreq, 2000.0f }, { S::highCut, 900.0f },
                                                { S::drive, 0.05f }, { S::width, 1.3f }, { S::reverb, 0.4f }, { S::space, 0.85f } }),
    };
    return moods;
}

const Mood* findBuiltIn (const juce::String& name)
{
    for (auto& m : builtInMoods())
        if (m.name.equalsIgnoreCase (name))
            return &m;
    return nullptr;
}

//==============================================================================
namespace ring
{
    Between locate (float position, int n)
    {
        if (n < 2)
            return { 0, 0, 0.0f };

        const auto x = juce::jlimit (0.0f, 1.0f, position) * (float) (n - 1);
        const auto from = juce::jmin ((int) x, n - 2);
        const auto t = x - (float) from;
        return { from, from + 1, t };
    }

    // Eases in and out of each mood so the knob settles on it for a moment.
    static float ease (float t) { return t * t * (3.0f - 2.0f * t); }

    Settings settingsAt (const Settings* const* moods, int n, float position)
    {
        if (n <= 0)
            return Settings::neutral();
        const auto b = locate (position, n);
        return blend (*moods[b.from], *moods[b.to], ease (b.t));
    }

    Settings settingsAt (const std::vector<Mood>& moods, float position)
    {
        std::vector<const Settings*> list;
        for (auto& m : moods)
            list.push_back (&m.settings);
        return settingsAt (list.data(), (int) list.size(), position);
    }

    juce::Colour colourAt (const std::vector<Mood>& moods, float position)
    {
        if (moods.empty())
            return juce::Colours::grey;
        const auto b = locate (position, (int) moods.size());
        return moods[(size_t) b.from].colour.interpolatedWith (moods[(size_t) b.to].colour, ease (b.t));
    }
}

namespace pad
{
    Settings settingsAt (const std::array<const Settings*, 4>& c, float x, float y)
    {
        const auto top = blend (*c[0], *c[1], x);
        const auto bottom = blend (*c[2], *c[3], x);
        return blend (bottom, top, y);
    }

    juce::Colour colourAt (const std::array<juce::Colour, 4>& c, float x, float y)
    {
        const auto top = c[0].interpolatedWith (c[1], x);
        const auto bottom = c[2].interpolatedWith (c[3], x);
        return bottom.interpolatedWith (top, y);
    }
}
