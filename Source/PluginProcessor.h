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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    hk::Params readParams() const;
    void handleMidi (const juce::MidiMessage& m);

    hk::Engine engine;
    juce::AudioBuffer<float> scratch;
    int currentProgram = 0;

    struct Raw
    {
        std::atomic<float>* model; std::atomic<float>* bright; std::atomic<float>* layer; std::atomic<float>* detune;
        std::atomic<float>* tape;  std::atomic<float>* attack; std::atomic<float>* decay; std::atomic<float>* release;
        std::atomic<float>* vel;   std::atomic<float>* chord;  std::atomic<float>* cutoff; std::atomic<float>* reso;
        std::atomic<float>* chorus; std::atomic<float>* reverb; std::atomic<float>* size; std::atomic<float>* comp;
        std::atomic<float>* pump;  std::atomic<float>* pumpRate; std::atomic<float>* volume;
    } raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysProcessor)
};
