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
