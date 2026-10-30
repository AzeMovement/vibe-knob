#include "VibeEngine.h"

void VibeEngine::Biquad::set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
{
    b0 = (float) (nb0 / a0);
    b1 = (float) (nb1 / a0);
    b2 = (float) (nb2 / a0);
    a1 = (float) (na1 / a0);
    a2 = (float) (na2 / a0);
}

// Filter recipes from Robert Bristow-Johnson's Audio EQ Cookbook.
void VibeEngine::Biquad::lowShelf (double r, double f, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w = juce::MathConstants<double>::twoPi * f / r;
    const auto cs = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0);
    const auto sq = 2.0 * std::sqrt (A) * alpha;
    set (A * ((A + 1) - (A - 1) * cs + sq), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sq),
         (A + 1) + (A - 1) * cs + sq, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sq);
}

void VibeEngine::Biquad::highShelf (double r, double f, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w = juce::MathConstants<double>::twoPi * f / r;
    const auto cs = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0);
    const auto sq = 2.0 * std::sqrt (A) * alpha;
    set (A * ((A + 1) + (A - 1) * cs + sq), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sq),
         (A + 1) - (A - 1) * cs + sq, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sq);
}

void VibeEngine::Biquad::peak (double r, double f, double q, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w = juce::MathConstants<double>::twoPi * f / r;
    const auto cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    set (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
}

void VibeEngine::Biquad::highPass (double r, double f)
{
    const auto w = juce::MathConstants<double>::twoPi * f / r;
    const auto cs = std::cos (w), alpha = std::sin (w) / (2.0 * juce::MathConstants<double>::sqrt2 * 0.5);
    set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
}

void VibeEngine::Biquad::lowPass (double r, double f)
{
    const auto w = juce::MathConstants<double>::twoPi * f / r;
    const auto cs = std::cos (w), alpha = std::sin (w) / (2.0 * juce::MathConstants<double>::sqrt2 * 0.5);
    set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
}

//==============================================================================
void VibeEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    rate = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);
    channels = juce::jlimit (1, 2, numChannels);
    smoothing = 1.0f - std::exp (-(float) chunk / (0.03f * (float) rate));

    chunkSettings.assign ((size_t) (maxBlock / chunk + 2), Settings::neutral());

    // Saturation runs at twice the sample rate so its added harmonics don't fold back as noise.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    latency = juce::roundToInt (oversampling->getLatencyInSamples());

    reverb.setSampleRate (rate);
    wet.setSize (channels, maxBlock);
    reset();
}

void VibeEngine::reset()
{
    for (auto* f : { &lowShelf, &midPeak, &highShelf, &lowCut, &highCut })
        f->clear();
    if (oversampling != nullptr)
        oversampling->reset();
    reverb.reset();
    heldSample = {};
    holdCount = 0;
    inPower = outPower = 0.0f;
    targetGain = appliedGain = 1.0f;
}

float VibeEngine::shape (float x, float grit)
{
    // Smooth, slightly lopsided like a valve (adds even harmonics)...
    constexpr float bias = 0.3f;
    const auto smooth = std::tanh (x + bias) - std::tanh (bias);

    // ...or a harder knee that bites (odd harmonics).
    const auto h = juce::jlimit (-1.0f, 1.0f, x * 1.3f);
    const auto hard = 1.5f * (h - h * h * h / 3.0f);

    return smooth + (hard - smooth) * grit;
}

void VibeEngine::process (juce::AudioBuffer<float>& buffer, int numSamples, const Settings& target)
{
    jassert (numSamples <= maxBlock);
    const auto numCh = juce::jmin (channels, buffer.getNumChannels());
    if (numSamples <= 0 || numCh <= 0)
        return;

    // Loudness going in, for auto gain.
    const auto blockTime = 1.0f - std::exp (-(float) numSamples / (0.4f * (float) rate));
    auto powerIn = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        for (auto x : juce::Span<const float> (buffer.getReadPointer (ch), (size_t) numSamples))
            powerIn += x * x;
    powerIn /= (float) (numSamples * numCh);

    const auto start = current;

    // 1. Tone EQ, retuned every chunk as the settings glide.
    int c = 0;
    for (int pos = 0; pos < numSamples; pos += chunk, ++c)
    {
        for (int i = 0; i < Settings::count; ++i)
            current[i] += (target[i] - current[i]) * smoothing;
        chunkSettings[(size_t) c] = current;

        lowShelf.lowShelf (rate, 150.0, current[Settings::low]);
        midPeak.peak (rate, juce::jmin ((double) current[Settings::midFreq], rate * 0.4), 0.8, current[Settings::mid]);
        highShelf.highShelf (rate, juce::jmin (5000.0, rate * 0.4), current[Settings::high]);

        const auto n = juce::jmin (chunk, numSamples - pos);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, pos);
            for (int i = 0; i < n; ++i)
                d[i] = highShelf.process (midPeak.process (lowShelf.process (d[i], ch), ch), ch);
        }
    }

    const auto& end = current;
    auto ramp = [&] (int index, int i, int total)
    {
        return start[index] + (end[index] - start[index]) * (float) (i + 1) / (float) total;
    };

    // 2. Saturation.
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
        auto up = oversampling->processSamplesUp (block);
        const auto upN = (int) up.getNumSamples();

        if (start[Settings::drive] > 0.0f || end[Settings::drive] > 0.0f)
        {
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (int i = 0; i < upN; ++i)
                {
                    const auto drive = ramp (Settings::drive, i, upN);
                    const auto grit = ramp (Settings::grit, i, upN);
                    const auto gain = juce::Decibels::decibelsToGain (drive * (24.0f + 6.0f * grit));
                    const auto makeUp = juce::Decibels::decibelsToGain (drive * 10.0f);
                    const auto saturated = shape (d[i] * gain, grit) / gain * makeUp;
                    d[i] += (saturated - d[i]) * juce::jmin (1.0f, drive * 8.0f);
                }
            }
        }

        oversampling->processSamplesDown (block);
    }

    // 3. Low and high cut, after the saturation so they also tame what it adds.
    c = 0;
    for (int pos = 0; pos < numSamples; pos += chunk, ++c)
    {
        const auto& s = chunkSettings[(size_t) c];
        lowCut.highPass (rate, s[Settings::lowCut]);
        highCut.lowPass (rate, juce::jmin ((double) s[Settings::highCut], rate * 0.45));

        // At rest the cuts fade out completely, so the sound passes through untouched.
        const auto lowCutMix = juce::jlimit (0.0f, 1.0f, (s[Settings::lowCut] - 20.0f) / 10.0f);
        const auto highCutMix = juce::jlimit (0.0f, 1.0f, (20000.0f - s[Settings::highCut]) / 1000.0f);

        const auto n = juce::jmin (chunk, numSamples - pos);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, pos);
            for (int i = 0; i < n; ++i)
            {
                const auto x = d[i];
                const auto lowCutOut = x + (lowCut.process (x, ch) - x) * lowCutMix;
                d[i] = lowCutOut + (highCut.process (lowCutOut, ch) - lowCutOut) * highCutMix;
            }
        }
    }

    // 4. Crush: fewer bits, and each sample held for a few in a row.
    if (start[Settings::crush] > 0.0f || end[Settings::crush] > 0.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto crush = ramp (Settings::crush, i, numSamples);
            const auto hold = 1 + (int) (crush * crush * 14.0f * (float) (rate / 44100.0));
            const auto levels = std::pow (2.0f, 15.0f - 11.0f * crush);

            if (++holdCount >= hold)
            {
                holdCount = 0;
                for (int ch = 0; ch < numCh; ++ch)
                    heldSample[(size_t) ch] = std::round (buffer.getSample (ch, i) * levels) / levels;
            }

            const auto mix = juce::jmin (1.0f, crush * 10.0f);
            for (int ch = 0; ch < numCh; ++ch)
            {
                const auto x = buffer.getSample (ch, i);
                buffer.setSample (ch, i, x + (heldSample[(size_t) ch] - x) * mix);
            }
        }
    }

    // 5. Stereo width.
    if (numCh == 2)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto width = ramp (Settings::width, i, numSamples);
            const auto mid = 0.5f * (l[i] + r[i]);
            const auto side = 0.5f * (l[i] - r[i]) * width;
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

    // 6. Reverb, mixed in on top of the dry sound.
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.35f + 0.6f * end[Settings::space];
        p.damping = 0.55f - 0.35f * end[Settings::space];
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);

        for (int ch = 0; ch < numCh; ++ch)
            wet.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        if (numCh == 2)
            reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), numSamples);
        else
            reverb.processMono (wet.getWritePointer (0), numSamples);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const auto* w = wet.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto mix = ramp (Settings::reverb, i, numSamples);
                d[i] = d[i] * (1.0f - 0.3f * mix) + w[i] * mix * 0.7f;
            }
        }
    }

    // 7. Auto gain: compare loudness over the last half second or so, and ease towards matching it.
    auto powerOut = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        for (auto x : juce::Span<const float> (buffer.getReadPointer (ch), (size_t) numSamples))
            powerOut += x * x;
    powerOut /= (float) (numSamples * numCh);

    inPower += (powerIn - inPower) * blockTime;
    outPower += (powerOut - outPower) * blockTime;

    if (! autoGain)
        targetGain = 1.0f;
    else if (inPower > 1.0e-7f && outPower > 1.0e-9f)   // hold still through silence
        targetGain = juce::jlimit (0.25f, 4.0f, std::sqrt (inPower / outPower));

    const auto from = appliedGain;
    appliedGain += (targetGain - appliedGain) * (1.0f - std::exp (-(float) numSamples / (0.15f * (float) rate)));
    for (int ch = 0; ch < numCh; ++ch)
        buffer.applyGainRamp (ch, 0, numSamples, from, appliedGain);
}
