#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        auto percent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
        auto decibels = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
        auto position = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)); });

        AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vibe", 1 }, "Vibe", NormalisableRange<float> (0.0f, 1.0f), 1.0f / 3.0f, position));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount", NormalisableRange<float> (0.0f, 100.0f), 100.0f, percent));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f, decibels));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto volume", true));

        // Pro.
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Control", StringArray { "Knob", "Pad" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "padx", 1 }, "Pad X", NormalisableRange<float> (0.0f, 1.0f), 0.5f, position));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pady", 1 }, "Pad Y", NormalisableRange<float> (0.0f, 1.0f), 0.5f, position));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "motion", 1 }, "Motion", StringArray { "Off", "Drift", "Follow" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Motion depth", NormalisableRange<float> (0.0f, 100.0f), 30.0f, percent));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Motion speed", NormalisableRange<float> (0.0f, 100.0f), 30.0f, percent));
        return layout;
    }

    std::vector<Mood> moodsNamed (std::initializer_list<const char*> names)
    {
        std::vector<Mood> list;
        for (auto* n : names)
            list.push_back (*findBuiltIn (n));
        return list;
    }

    juce::String moodsToJson (const Mood* moods, size_t count)
    {
        juce::Array<juce::var> list;
        for (size_t i = 0; i < count; ++i)
            list.add (moods[i].toVar());
        return juce::JSON::toString (list, true);
    }

    std::vector<Mood> moodsFromJson (const juce::String& json)
    {
        std::vector<Mood> list;
        if (auto* array = juce::JSON::parse (json).getArray())
            for (auto& v : *array)
            {
                Mood m;
                if (Mood::fromVar (v, m))
                {
                    if (auto* builtIn = findBuiltIn (m.name))
                        m = *builtIn;   // so later tuning of built-in moods reaches old projects
                    list.push_back (m);
                }
            }
        return list;
    }
}

VibeKnobProcessor::VibeKnobProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "VibeKnob", createLayout())
{
    vibe = params.getRawParameterValue ("vibe");
    amount = params.getRawParameterValue ("amount");
    output = params.getRawParameterValue ("output");
    autoGain = params.getRawParameterValue ("autogain");
    mode = params.getRawParameterValue ("mode");
    padX = params.getRawParameterValue ("padx");
    padY = params.getRawParameterValue ("pady");
    motion = params.getRawParameterValue ("motion");
    depth = params.getRawParameterValue ("depth");
    speed = params.getRawParameterValue ("speed");

    for (auto& p : playing)
        p = 0.0f;

    ringMoods = edition::isPro ? moodsNamed ({ "Dark", "Warm", "Vintage", "Punchy", "Aggressive", "Bright", "Airy", "Dreamy" })
                               : moodsNamed ({ "Dark", "Warm", "Aggressive", "Dreamy" });
    const auto c = moodsNamed ({ "Dreamy", "Bright", "Dark", "Aggressive" });
    std::copy (c.begin(), c.end(), corners.begin());

    pushSetup();
}

bool VibeKnobProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && layouts.getMainInputChannelSet() == out;
}

void VibeKnobProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (32, samplesPerBlock);
    engine.prepare (sampleRate, maxBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatency());
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
    follow = 0.0f;
}

//==============================================================================
bool VibeKnobProcessor::usesPad() const
{
    return edition::isPro && mode->load() > 0.5f;
}

Settings VibeKnobProcessor::targetFor (float knob, float x, float y, float amountNow) const
{
    if (live.teaching)
        return live.teach;

    Settings s;
    if (edition::isPro && mode->load() > 0.5f)
    {
        const std::array<const Settings*, 4> c { &live.corners[0], &live.corners[1], &live.corners[2], &live.corners[3] };
        s = pad::settingsAt (c, x, y);
    }
    else
    {
        std::array<const Settings*, edition::maxRingMoods> list {};
        for (int i = 0; i < live.ringSize; ++i)
            list[(size_t) i] = &live.ring[(size_t) i];
        s = ring::settingsAt (list.data(), live.ringSize, knob);
    }

    return withAmount (s, amountNow);
}

void VibeKnobProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    const auto numCh = getTotalNumOutputChannels();
    for (auto ch = getTotalNumInputChannels(); ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples == 0)
        return;

    if (setupChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (setupLock);
        if (lock.isLocked())
        {
            live = pending;
            setupChanged = false;
        }
    }

    // Motion (Pro) nudges the knob, or the pad's puck, away from where you left it.
    auto offset = 0.0f;
    const auto motionMode = edition::isPro ? (Motion) juce::roundToInt (motion->load()) : Motion::off;
    const auto depthNow = depth->load() / 100.0f, speedNow = speed->load() / 100.0f;
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    if (motionMode == Motion::drift)
    {
        // One slow sweep every 50 seconds at 0% speed, every 1.25 seconds at 100%.
        const auto hz = 0.02 * std::pow (40.0, (double) speedNow);
        driftPhase = std::fmod (driftPhase + hz * numSamples / rate, 1.0);
        offset = depthNow * 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * driftPhase);
    }
    else if (motionMode == Motion::follow)
    {
        // Louder playing pushes the knob forward; it falls back slower at low speed.
        auto peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
        const auto level = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak) + 42.0f) / 36.0f);
        const auto seconds = level > follow ? 0.02f : 0.1f + 1.5f * (1.0f - speedNow);
        follow += (level - follow) * (1.0f - std::exp (-(float) numSamples / (seconds * (float) rate)));
        offset = depthNow * 0.6f * follow;
    }
    else
    {
        follow = 0.0f;
    }

    const auto knob = juce::jlimit (0.0f, 1.0f, vibe->load() + offset);
    const auto x = juce::jlimit (0.0f, 1.0f, padX->load() + offset);
    const auto y = juce::jlimit (0.0f, 1.0f, padY->load() + (motionMode == Motion::drift
                                                                  ? depthNow * 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * driftPhase)
                                                                  : offset * 0.5f));
    knobNow = knob;
    padXNow = x;
    padYNow = y;

    const auto target = targetFor (knob, x, y, amount->load() / 100.0f);
    engine.setAutoGain (autoGain->load() > 0.5f);

    for (int pos = 0; pos < numSamples; pos += maxBlock)
    {
        const auto n = juce::jmin (maxBlock, numSamples - pos);
        juce::AudioBuffer<float> part (buffer.getArrayOfWritePointers(), numCh, pos, n);
        engine.process (part, n, target);
    }

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
    outputGain.applyGain (buffer, numSamples);

    for (int i = 0; i < Settings::count; ++i)
        playing[(size_t) i] = engine.getCurrent()[i];
    autoGainDb = engine.getAutoGainDb();
}

//==============================================================================
void VibeKnobProcessor::pushSetup()
{
    AudioSetup s;
    s.ringSize = (int) juce::jmin (ringMoods.size(), (size_t) edition::maxRingMoods);
    for (int i = 0; i < s.ringSize; ++i)
        s.ring[(size_t) i] = ringMoods[(size_t) i].settings;
    for (size_t i = 0; i < 4; ++i)
        s.corners[i] = corners[i].settings;
    s.teaching = teaching;
    s.teach = teachSettings;

    {
        const juce::SpinLock::ScopedLockType lock (setupLock);
        pending = s;
        setupChanged = true;
    }
    ++setupVersion;
}

void VibeKnobProcessor::setRing (std::vector<Mood> moods)
{
    if (! edition::isPro || moods.size() < 2)
        return;
    moods.resize (juce::jmin (moods.size(), (size_t) edition::maxRingMoods));
    ringMoods = std::move (moods);
    pushSetup();
}

void VibeKnobProcessor::setCorner (int index, const Mood& m)
{
    if (! edition::isPro || ! juce::isPositiveAndBelow (index, 4))
        return;
    corners[(size_t) index] = m;
    pushSetup();
}

void VibeKnobProcessor::startTeaching()
{
    if (! edition::isPro || teaching)
        return;
    teachSettings = getPlaying();
    teaching = true;
    pushSetup();
}

void VibeKnobProcessor::stopTeaching()
{
    teaching = false;
    pushSetup();
}

void VibeKnobProcessor::setTeachSetting (int index, float value)
{
    if (! teaching || ! juce::isPositiveAndBelow (index, (int) Settings::count))
        return;
    const auto& info = settingInfo()[(size_t) index];
    teachSettings[index] = juce::jlimit (info.min, info.max, value);
    pushSetup();
}

Settings VibeKnobProcessor::getPlaying() const
{
    Settings s;
    for (int i = 0; i < Settings::count; ++i)
        s[i] = playing[(size_t) i].load();

    // Before any audio has played, show what the knob is set to.
    if (s[Settings::midFreq] <= 0.0f)
    {
        if (teaching)
            return teachSettings;
        s = usesPad() ? pad::settingsAt ({ &corners[0].settings, &corners[1].settings, &corners[2].settings, &corners[3].settings },
                                         padX->load(), padY->load())
                      : ring::settingsAt (ringMoods, vibe->load());
        s = withAmount (s, amount->load() / 100.0f);
    }
    return s;
}

juce::Colour VibeKnobProcessor::getColourNow() const
{
    if (usesPad())
    {
        const auto p = getPadNow();
        return pad::colourAt ({ corners[0].colour, corners[1].colour, corners[2].colour, corners[3].colour }, p.x, p.y);
    }
    return ring::colourAt (ringMoods, getKnobNow());
}

//==============================================================================
void VibeKnobProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    if (edition::isPro)
    {
        state.setProperty ("ring", moodsToJson (ringMoods.data(), ringMoods.size()), nullptr);
        state.setProperty ("corners", moodsToJson (corners.data(), corners.size()), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VibeKnobProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (params.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (edition::isPro)
    {
        auto ring = moodsFromJson (state.getProperty ("ring").toString());
        if (ring.size() >= 2)
        {
            ring.resize (juce::jmin (ring.size(), (size_t) edition::maxRingMoods));
            ringMoods = ring;
        }

        const auto c = moodsFromJson (state.getProperty ("corners").toString());
        if (c.size() == 4)
            std::copy (c.begin(), c.end(), corners.begin());
    }

    state.removeProperty ("ring", nullptr);
    state.removeProperty ("corners", nullptr);
    params.replaceState (state);
    teaching = false;
    pushSetup();
}

juce::AudioProcessorEditor* VibeKnobProcessor::createEditor()
{
    return new VibeKnobEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VibeKnobProcessor();
}
