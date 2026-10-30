#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Mood.h"

// The effects under the knob, in order: tone EQ, saturation, low and high cut, crush, stereo width, reverb.
// Settings glide to new values over a few milliseconds, so turning the knob never clicks.
class VibeEngine
{
public:
    static constexpr int chunk = 32;   // filters are retuned this often while settings move

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // numSamples must not be more than the prepared block size.
    void process (juce::AudioBuffer<float>&, int numSamples, const Settings& target);

    int getLatency() const { return latency; }
    const Settings& getCurrent() const { return current; }

    // Keeps the output as loud as the input, so a mood never wins just by being louder.
    void setAutoGain (bool on) { autoGain = on; }
    float getAutoGainDb() const { return juce::Decibels::gainToDecibels (appliedGain); }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};

        float process (float x, int ch)
        {
            const auto y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }

        void clear() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
        void set (double b0, double b1, double b2, double a0, double a1, double a2);
        void lowShelf (double rate, double freq, double gainDb);
        void highShelf (double rate, double freq, double gainDb);
        void peak (double rate, double freq, double q, double gainDb);
        void highPass (double rate, double freq);
        void lowPass (double rate, double freq);
    };

    static float shape (float x, float grit);

    double rate = 44100.0;
    int maxBlock = 0, channels = 2, latency = 0;
    float smoothing = 1.0f;   // per chunk

    Settings current = Settings::neutral();
    std::vector<Settings> chunkSettings;

    Biquad lowShelf, midPeak, highShelf, lowCut, highCut;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    std::array<float, 2> heldSample {};
    int holdCount = 0;

    juce::Reverb reverb;
    juce::AudioBuffer<float> wet;

    bool autoGain = true;
    float inPower = 0.0f, outPower = 0.0f, targetGain = 1.0f, appliedGain = 1.0f;
};
