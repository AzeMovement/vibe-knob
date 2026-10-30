// Renders the editor after playing it some sound, for checking the layout.
// Usage: VibeScreenshot <out.png> [knob|pad|teach|moods|motion]
#include <juce_audio_utils/juce_audio_utils.h>
#define private public
#include "PluginProcessor.h"
#include "PluginEditor.h"
#undef private

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double rate = 48000.0;
    constexpr int block = 512;
    const juce::String what = argc > 2 ? argv[2] : "knob";

    VibeKnobProcessor plugin;
    plugin.setPlayConfigDetails (2, 2, rate, block);
    plugin.prepareToPlay (rate, block);

    auto set = [&] (const char* id, float value)
    {
        auto* p = plugin.params.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set ("vibe", edition::isPro ? 0.52f : 0.42f);
    if (what == "pad")
    {
        set ("mode", 1.0f);
        set ("padx", 0.3f);
        set ("pady", 0.62f);
    }
    if (what == "motion")
    {
        set ("motion", 1.0f);
        set ("depth", 40.0f);
    }

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, block);
    juce::Random random (3);
    for (int b = 0; b < (int) (2.0 * rate / block); ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const auto t = (double) (b * block + i) / rate;
            const auto x = (float) (0.2 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t)) + (random.nextFloat() - 0.5f) * 0.1f;
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x * 0.9f);
        }
        plugin.processBlock (buffer, midi);
    }

    if (what == "teach")
    {
        plugin.startTeaching();
        plugin.setTeachSetting (Settings::high, 4.5f);
        plugin.setTeachSetting (Settings::reverb, 0.3f);
    }

    VibeKnobEditor editor (plugin);
    if (what == "moods")
        editor.showTab (1);
    if (what == "motion")
        editor.showTab (2);
    editor.timerCallback();

    auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
    const juce::File out (argc > 1 ? argv[1] : "shot.png");
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    return 0;
}
