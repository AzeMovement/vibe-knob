// Plays made-up sounds through each mood and checks the effects do what the mood promises.
#include <juce_core/juce_core.h>
#include "VibeEngine.h"

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 512;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
        if (! ok)
            ++failures;
    }

    using Source = std::function<std::pair<float, float> (juce::int64)>;

    struct Run { juce::AudioBuffer<float> in, out; int latency; };

    Run play (const Settings& settings, const Source& source, double seconds, bool autoGain = false)
    {
        VibeEngine engine;
        engine.prepare (rate, block, 2);
        engine.setAutoGain (autoGain);

        const auto total = (int) (seconds * rate);
        Run run { juce::AudioBuffer<float> (2, total), juce::AudioBuffer<float> (2, total), engine.getLatency() };
        for (int i = 0; i < total; ++i)
        {
            const auto s = source (i);
            run.in.setSample (0, i, s.first);
            run.in.setSample (1, i, s.second);
        }

        juce::AudioBuffer<float> chunk (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            const auto n = juce::jmin (block, total - pos);
            for (int ch = 0; ch < 2; ++ch)
                chunk.copyFrom (ch, 0, run.in, ch, pos, n);
            engine.process (chunk, n, settings);
            for (int ch = 0; ch < 2; ++ch)
                run.out.copyFrom (ch, pos, chunk, ch, 0, n);
        }
        return run;
    }

    Source noise (float level = 0.2f, float correlation = 1.0f)
    {
        auto random = std::make_shared<juce::Random> (7);
        return [=] (juce::int64)
        {
            const auto common = random->nextFloat() * 2.0f - 1.0f;
            const auto own = random->nextFloat() * 2.0f - 1.0f;
            const auto own2 = random->nextFloat() * 2.0f - 1.0f;
            return std::make_pair (level * (common * correlation + own * (1.0f - correlation)),
                                   level * (common * correlation + own2 * (1.0f - correlation)));
        };
    }

    Source sines (std::initializer_list<double> freqs, float level)
    {
        std::vector<double> f (freqs);
        return [=] (juce::int64 n)
        {
            auto x = 0.0;
            for (auto hz : f)
                x += std::sin (juce::MathConstants<double>::twoPi * hz * (double) n / rate + hz);
            return std::make_pair ((float) x * level, (float) x * level);
        };
    }

    // Level in dB of one channel from `from` seconds on, optionally through a band-pass filter.
    float levelDb (const juce::AudioBuffer<float>& b, int ch, double from, double to = 1.0e9, double bandHz = 0.0)
    {
        juce::IIRFilter band;
        if (bandHz > 0.0)
            band.setCoefficients (juce::IIRCoefficients::makeBandPass (rate, bandHz, 4.0));

        auto sum = 0.0;
        int count = 0;
        const auto first = (int) (from * rate), last = (int) juce::jmin ((double) b.getNumSamples(), to * rate);
        for (int i = 0; i < last; ++i)
        {
            auto x = b.getSample (ch, i);
            if (bandHz > 0.0)
                x = band.processSingleSampleRaw (x);
            if (i >= first)
            {
                sum += x * x;
                ++count;
            }
        }
        return juce::Decibels::gainToDecibels ((float) std::sqrt (sum / juce::jmax (1, count)), -200.0f);
    }

    // How much a band changed from input to output.
    float bandChange (const Run& r, double hz) { return levelDb (r.out, 0, 1.0, 1.0e9, hz) - levelDb (r.in, 0, 1.0, 1.0e9, hz); }

    float sideToMidDb (const juce::AudioBuffer<float>& b, double from)
    {
        auto mid = 0.0, side = 0.0;
        for (int i = (int) (from * rate); i < b.getNumSamples(); ++i)
        {
            const auto l = b.getSample (0, i), r = b.getSample (1, i);
            mid += (l + r) * (l + r);
            side += (l - r) * (l - r);
        }
        return (float) (10.0 * std::log10 ((side + 1e-20) / (mid + 1e-20)));
    }

    const Settings& mood (const char* name) { return findBuiltIn (name)->settings; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI* noGui = nullptr;
    juce::ignoreUnused (noGui);

    std::cout << "Amount at zero\n";
    {
        const auto r = play (withAmount (mood ("Aggressive"), 0.0f), sines ({ 110.0, 440.0, 1234.0, 5000.0 }, 0.1f), 1.0);
        auto error = 0.0, signal = 0.0;
        for (int i = (int) (0.2 * rate); i < r.out.getNumSamples(); ++i)
        {
            const auto d = r.out.getSample (0, i) - r.in.getSample (0, i - r.latency);
            error += d * d;
            signal += r.in.getSample (0, i) * r.in.getSample (0, i);
        }
        const auto db = 10.0 * std::log10 (error / signal + 1e-20);
        check (db < -60.0, "the sound passes through unchanged (difference " + juce::String (db, 1) + " dB)");
    }

    std::cout << "Moods\n";
    {
        const auto r = play (mood ("Dark"), noise(), 2.0);
        const auto tilt = bandChange (r, 8000.0) - bandChange (r, 100.0);
        check (tilt < -8.0f, "Dark: top end comes down against the bass (" + juce::String (tilt, 1) + " dB)");
    }
    {
        const auto r = play (mood ("Bright"), noise(), 2.0);
        const auto tilt = bandChange (r, 8000.0) - bandChange (r, 100.0);
        check (tilt > 5.0f, "Bright: top end comes up (" + juce::String (tilt, 1) + " dB)");
    }
    {
        const auto r = play (mood ("Warm"), noise(), 2.0);
        const auto tilt = bandChange (r, 10000.0) - bandChange (r, 300.0);
        check (tilt < -3.0f, "Warm: low mids lead over the top (" + juce::String (tilt, 1) + " dB)");
    }
    {
        const auto r = play (mood ("Telephone"), noise (0.2f, 0.5f), 2.0);
        const auto cut = bandChange (r, 100.0) - bandChange (r, 1500.0);
        check (cut < -12.0f, "Telephone: bass is gone (" + juce::String (cut, 1) + " dB)");
        check (sideToMidDb (r.out, 1.0) < -40.0f, "Telephone: mono");
    }
    {
        const auto r = play (mood ("Wide"), noise (0.2f, 0.7f), 2.0);
        const auto change = sideToMidDb (r.out, 1.0) - sideToMidDb (r.in, 1.0);
        check (change > 4.0f, "Wide: sides come up against the middle (" + juce::String (change, 1) + " dB)");
    }
    {
        const auto r = play (mood ("Aggressive"), sines ({ 200.0 }, 0.3f), 2.0);
        const auto third = levelDb (r.out, 0, 1.0, 1.0e9, 600.0) - levelDb (r.out, 0, 1.0, 1.0e9, 200.0);
        const auto before = levelDb (r.in, 0, 1.0, 1.0e9, 600.0) - levelDb (r.in, 0, 1.0, 1.0e9, 200.0);
        check (third > -30.0f && third > before + 8.0f, "Aggressive: adds harmonics (" + juce::String (third, 1) + " dB at 3x)");
    }
    {
        auto burst = noise();
        const auto r = play (mood ("Dreamy"), [burst] (juce::int64 n) { return n < (juce::int64) rate ? burst (n) : std::make_pair (0.0f, 0.0f); }, 2.0);
        const auto tail = levelDb (r.out, 0, 1.1, 1.4) - levelDb (r.out, 0, 0.5, 1.0);
        check (tail > -30.0f, "Dreamy: leaves a reverb tail (" + juce::String (tail, 1) + " dB after the sound stops)");
    }
    {
        const auto r = play (mood ("Lo-fi"), sines ({ 1000.0 }, 0.3f), 2.0);
        const auto alias = levelDb (r.out, 0, 1.0, 1.0e9, 2200.0) - levelDb (r.out, 0, 1.0, 1.0e9, 1000.0);
        check (alias > -40.0f, "Lo-fi: crunchy (" + juce::String (alias, 1) + " dB of grit next to the note)");
    }

    std::cout << "Auto gain\n";
    {
        juce::Random random (11);
        auto music = sines ({ 55.0, 220.0, 330.0, 880.0, 2500.0 }, 0.05f);
        auto hiss = noise (0.05f, 0.6f);
        const Source mix = [=] (juce::int64 n) { auto a = music (n); auto b = hiss (n); return std::make_pair (a.first + b.first, a.second + b.second); };

        auto worst = 0.0f;
        juce::String worstName;
        for (auto& m : builtInMoods())
        {
            const auto r = play (m.settings, mix, 4.0, true);
            const auto diff = levelDb (r.out, 0, 3.0) - levelDb (r.in, 0, 3.0);
            if (std::abs (diff) > std::abs (worst))
            {
                worst = diff;
                worstName = m.name;
            }
        }
        check (std::abs (worst) < 1.5f, "every mood is as loud as the input (furthest off: " + worstName + " " + juce::String (worst, 1) + " dB)");
    }

    std::cout << "Safety\n";
    {
        bool finite = true;
        auto loudestInSilence = 0.0f;
        for (auto& m : builtInMoods())
        {
            const auto quiet = play (m.settings, [] (juce::int64) { return std::make_pair (0.0f, 0.0f); }, 0.5, true);
            const auto loud = play (m.settings, noise (4.0f), 0.5, true);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < quiet.out.getNumSamples(); ++i)
                {
                    loudestInSilence = juce::jmax (loudestInSilence, std::abs (quiet.out.getSample (ch, i)));
                    finite = finite && std::isfinite (loud.out.getSample (ch, i)) && std::abs (loud.out.getSample (ch, i)) < 100.0f;
                }
        }
        check (loudestInSilence < 1.0e-6f, "silence in, silence out (" + juce::String (loudestInSilence) + ")");
        check (finite, "very loud input stays finite");
    }

    std::cout << "Morphing\n";
    {
        const std::vector<Mood> moods { *findBuiltIn ("Dark"), *findBuiltIn ("Warm"), *findBuiltIn ("Aggressive") };
        auto exact = true, between = true;
        for (int i = 0; i < 3; ++i)
        {
            const auto s = ring::settingsAt (moods, ring::positionOf (i, 3));
            for (int k = 0; k < Settings::count; ++k)
                exact = exact && std::abs (s[k] - moods[(size_t) i].settings[k]) < 1.0e-3f * juce::jmax (1.0f, std::abs (s[k]));
        }
        const auto half = ring::settingsAt (moods, 0.25f);
        for (int k = 0; k < Settings::count; ++k)
        {
            const auto lo = juce::jmin (moods[0].settings[k], moods[1].settings[k]) - 1.0e-3f;
            const auto hi = juce::jmax (moods[0].settings[k], moods[1].settings[k]) + 1.0e-3f;
            between = between && half[k] >= lo && half[k] <= hi;
        }
        check (exact, "the knob lands exactly on each mood");
        check (between, "between two moods, every setting is between theirs");
    }

    std::cout << "Mood files\n";
    {
        Mood original = *findBuiltIn ("Vintage");
        original.name = "My Tape";
        Mood loaded;
        const auto ok = Mood::fromVar (juce::JSON::parse (juce::JSON::toString (original.toVar())), loaded);
        auto same = ok && loaded.name == original.name && loaded.colour == original.colour;
        for (int k = 0; k < Settings::count; ++k)
            same = same && std::abs (loaded.settings[k] - original.settings[k]) < 1.0e-4f;
        check (same, "a saved mood loads back the same");
        check (! Mood::fromVar (juce::JSON::parse ("{\"name\": \"x\"}"), loaded), "a broken file is refused");
    }

    std::cout << (failures == 0 ? "All passed\n" : juce::String (failures) + " failed\n");
    return failures == 0 ? 0 : 1;
}
