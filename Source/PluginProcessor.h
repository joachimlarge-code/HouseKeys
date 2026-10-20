#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class HouseKeysProcessor : public juce::AudioProcessor
{
public:
    HouseKeysProcessor();
    ~HouseKeysProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;

private:
    hk::Params readParams() const;
    void handleMidi (const juce::MidiMessage& m);
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    hk::Engine engine;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysProcessor)
};
