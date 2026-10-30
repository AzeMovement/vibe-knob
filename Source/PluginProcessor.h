#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Edition.h"
#include "Mood.h"
#include "VibeEngine.h"

// One big knob that moves the sound between moods. Each mood is a recipe for the effects underneath
// (tone EQ, saturation, stereo width, reverb), and turning the knob blends from one recipe to the next.
class VibeKnobProcessor : public juce::AudioProcessor
{
public:
    enum class Motion { off, drift, follow };

    VibeKnobProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState params;

    //==============================================================================
    // The moods around the knob, and on the pad's corners (Pro). Message thread.
    const std::vector<Mood>& getRing() const { return ringMoods; }
    void setRing (std::vector<Mood>);
    const std::array<Mood, 4>& getCorners() const { return corners; }
    void setCorner (int index, const Mood&);
    int getSetupVersion() const { return setupVersion; }

    bool usesPad() const;

    // Teaching (Pro): the knob pauses and the effects follow the settings you set by hand.
    void startTeaching();
    void stopTeaching();
    bool isTeaching() const { return teaching; }
    const Settings& getTeachSettings() const { return teachSettings; }
    void setTeachSetting (int index, float value);

    // What's playing right now, for the screen.
    float getKnobNow() const { return knobNow.load(); }
    juce::Point<float> getPadNow() const { return { padXNow.load(), padYNow.load() }; }
    Settings getPlaying() const;
    float getAutoGainDb() const { return autoGainDb.load(); }
    juce::Colour getColourNow() const;


private:
    void pushSetup();
    Settings targetFor (float knob, float x, float y, float amount) const;

    // Message thread copies.
    std::vector<Mood> ringMoods;
    std::array<Mood, 4> corners;
    bool teaching = false;
    Settings teachSettings = Settings::neutral();
    int setupVersion = 0;

    // The audio thread's copy, handed over under a lock it never waits for.
    struct AudioSetup
    {
        std::array<Settings, edition::maxRingMoods> ring {};
        int ringSize = 0;
        std::array<Settings, 4> corners {};
        bool teaching = false;
        Settings teach;
    };

    juce::SpinLock setupLock;
    AudioSetup pending, live;
    std::atomic<bool> setupChanged { false };

    VibeEngine engine;
    int maxBlock = 512;
    juce::SmoothedValue<float> outputGain;

    // Motion.
    double driftPhase = 0.0;
    float follow = 0.0f;

    std::atomic<float> knobNow { 0.0f }, padXNow { 0.5f }, padYNow { 0.5f }, autoGainDb { 0.0f };
    std::array<std::atomic<float>, Settings::count> playing;

    std::atomic<float>* vibe = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* autoGain = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* padX = nullptr;
    std::atomic<float>* padY = nullptr;
    std::atomic<float>* motion = nullptr;
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* speed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeKnobProcessor)
};
