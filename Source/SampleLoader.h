#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleBank.h"

// Loads the piano multisamples once per process, on a background thread, and shares them
// between all HouseKeys instances.
//
// Source, in order of preference:
//   1. your own samples in  ~/Library/Application Support/HouseKeys/Piano/
//      (same naming as the built-in set: <Note><Octave>v<layer>.wav|flac|ogg|aif, rel<key>.*)
//   2. the built-in Salamander Grand Piano samples compiled into the plugin
namespace SampleLibrary
{
    // nullptr until loading has finished. Starts loading on the first call.
    const hk::SampleBank* get();

    // Blocks until loaded (used by the offline render tool).
    const hk::SampleBank* getBlocking();

    juce::String statusText();
    juce::File userFolder();

    // Decodes a folder or the built-in data into a bank (exposed for tools/tests).
    std::unique_ptr<hk::SampleBank> loadFromFolder (const juce::File& folder);
    std::unique_ptr<hk::SampleBank> loadBuiltIn();
}

// Drag & drop: turns any audio file into a playable instrument.
namespace UserSample
{
    // Reads an audio file (wav, aiff, flac, ogg, mp3 where supported). Returns false + message on error.
    bool decode (const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error);

    // Trims silence, limits the length, fades the end and sets a consistent level.
    void prepare (juce::AudioBuffer<float>& audio, double sampleRate);

    struct Analysis
    {
        int   root = 60;          // MIDI note (lowest note of a chord)
        float cents = 0.0f;       // how sharp the sample is relative to 'root'
        bool  pitched = false;    // false = noise/drum
        bool  chord = false;      // several notes at once
        int   notes = 0;
        juce::String describe() const;
    };
    // Finds the notes in the sample: single pitch (YIN, exact cents) or a chord (spectral peaks).
    Analysis analyse (const juce::AudioBuffer<float>& audio, double sampleRate);

    // Fundamental frequency -> nearest MIDI note (YIN). Returns -1 if the sound has no clear pitch.
    int detectRootNote (const juce::AudioBuffer<float>& audio, double sampleRate);

    // One-zone bank playing the sample chromatically around its root note.
    std::unique_ptr<hk::SampleBank> makeBank (const juce::AudioBuffer<float>& audio, double sampleRate, int rootNote);

    // FLAC round trip so the sample can be stored inside the FL Studio project.
    juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& audio, double sampleRate);
    bool decodeFlac (const juce::MemoryBlock& data, juce::AudioBuffer<float>& out, double& sampleRate);

    juce::String noteName (int midi);
}
