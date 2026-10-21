#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class HouseKeysProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    HouseKeysProcessor();
    ~HouseKeysProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HouseKeys"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    // Programs = the factory sounds
    int getNumPrograms() override { return hk::kNumSounds; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void selectSound (int index);
    float soundDefault (const juce::String& id) const;   // the current sound's default for a control
    void resetToSoundDefault (const juce::String& id);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };   // output peak for the UI meter

    // Drag & drop: load any audio file as the "Ditt sample" sound. Message thread only.
    bool loadUserSample (const juce::File& file, juce::String& message);
    juce::String getUserSampleName() const { return userName; }
    int getUserSampleRoot() const { return userRoot; }
    bool isUserSampleChord() const { return userChord; }
    juce::String getUserSampleInfo() const { return userInfo; }

private:
    hk::Params readParams() const;
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void applySoundPreset (int index);
    void setParam (const juce::String& id, float value);
    void handleMidi (const juce::MidiMessage& m);
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    hk::Engine engine;
    juce::AudioBuffer<float> scratch;
    std::atomic<int> appliedSound { hk::kDefaultSound };   // sound whose defaults are currently loaded

    void installUserSample (juce::AudioBuffer<float> prepared, double sr, int root, float cents, bool chord, const juce::String& name);
    std::vector<std::unique_ptr<hk::SampleBank>> userBanks;  // kept alive: voices may still read old ones
    std::atomic<const hk::SampleBank*> userBank { nullptr };
    juce::String userName;
    int userRoot = 60;
    float userCents = 0.0f;
    bool userChord = false;
    juce::String userInfo;
    juce::MemoryBlock userFlac;
    double userRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysProcessor)
};
