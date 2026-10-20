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

    // Programs = the 12 pianos
    int getNumPrograms() override { return hk::kNumPianos; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void selectPiano (int index);
    float pianoDefault (const juce::String& id) const;   // the current piano's default for a control
    void resetToPianoDefault (const juce::String& id);

    juce::AudioProcessorValueTreeState apvts;

private:
    hk::Params readParams() const;
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void applyPianoPreset (int index);
    void setParam (const char* id, float value);
    void handleMidi (const juce::MidiMessage& m);
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    hk::Engine engine;
    juce::AudioBuffer<float> scratch;
    std::atomic<int> appliedPiano { 0 };   // piano whose default settings are currently loaded

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysProcessor)
};
