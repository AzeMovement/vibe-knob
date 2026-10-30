#include "MoodLibrary.h"
#include "Edition.h"

MoodLibrary::MoodLibrary()
{
    reload();
}

juce::File MoodLibrary::getFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Vibe Knob").getChildFile ("Moods");
}

juce::File MoodLibrary::fileFor (const juce::String& name) const
{
    return getFolder().getChildFile (juce::File::createLegalFileName (name) + moodExtension);
}

void MoodLibrary::reload()
{
    user.clear();

    for (auto& file : getFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + moodExtension))
    {
        Mood m;
        if (Mood::fromVar (juce::JSON::parse (file), m) && find (m.name) == nullptr)
            user.push_back (m);
    }

    std::sort (user.begin(), user.end(), [] (const Mood& a, const Mood& b) { return a.name.compareNatural (b.name) < 0; });
    sendChangeMessage();
}

std::vector<Mood> MoodLibrary::available() const
{
    std::vector<Mood> all;
    for (auto& m : builtInMoods())
        if (edition::isPro || ! m.proOnly)
            all.push_back (m);
    if (edition::isPro)
        all.insert (all.end(), user.begin(), user.end());
    return all;
}

const Mood* MoodLibrary::find (const juce::String& name) const
{
    if (auto* m = findBuiltIn (name))
        return m;
    for (auto& m : user)
        if (m.name.equalsIgnoreCase (name))
            return &m;
    return nullptr;
}

juce::String MoodLibrary::save (Mood mood)
{
    mood.builtIn = false;
    mood.proOnly = false;
    mood.name = mood.name.trim().substring (0, 24);
    if (mood.name.isEmpty())
        mood.name = "My mood";

    // Built-in names are taken.
    if (findBuiltIn (mood.name) != nullptr)
        mood.name = mood.name + " (mine)";

    getFolder().createDirectory();
    fileFor (mood.name).replaceWithText (juce::JSON::toString (mood.toVar()));
    reload();
    return mood.name;
}

bool MoodLibrary::remove (const juce::String& name)
{
    const auto ok = fileFor (name).deleteFile();
    reload();
    return ok;
}

int MoodLibrary::importFrom (const juce::File& file)
{
    const auto parsed = juce::JSON::parse (file);
    juce::Array<juce::var> entries;

    if (auto* list = parsed["moods"].getArray())
        entries = *list;
    else
        entries.add (parsed);

    getFolder().createDirectory();
    int added = 0;
    for (auto& entry : entries)
    {
        Mood m;
        if (! Mood::fromVar (entry, m))
            continue;
        if (findBuiltIn (m.name) != nullptr)
            m.name = m.name + " (mine)";
        fileFor (m.name).replaceWithText (juce::JSON::toString (m.toVar()));
        ++added;
    }

    reload();
    return added;
}

bool MoodLibrary::exportTo (const juce::File& file) const
{
    juce::Array<juce::var> list;
    for (auto& m : user)
        list.add (m.toVar());

    auto* pack = new juce::DynamicObject();
    pack->setProperty ("format", "Vibe Knob moods");
    pack->setProperty ("moods", list);
    return file.replaceWithText (juce::JSON::toString (juce::var (pack)));
}
