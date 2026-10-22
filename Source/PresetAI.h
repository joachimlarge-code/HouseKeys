#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include "Sounds.h"

namespace hk { struct SampleBank; }

// HouseKeys AI: a free, offline sound designer you chat with.
//   respond()   reads one chat message (Norwegian or English) and either designs a brand-new sound
//               (two factory sounds are cross-bred, mutated and spiced with extra layers, then shaped
//               by the words), or edits the current sound ("fetere", "mer luft", "ny variant",
//               "kall den Sommer Keys").
//   autoLevel() renders the new sound offline and matches its loudness to the factory sounds.
namespace PresetAI
{
    // the sound the conversation is about right now
    struct Context
    {
        hk::SoundDef def;
        juce::String name;
        int kind = -1;              // category it was made as
        juce::String request;       // the description it was made from
        bool isFactory = false;     // a factory sound (editing it makes a new sound)
    };

    struct Result
    {
        bool hasSound = false;
        bool renamed = false;       // only the name changed
        bool scratch = false;       // designed from scratch (no factory sound)
        juce::String reply;         // what the AI says (supports **bold**)
        juce::StringArray chips;    // what is in the sound (layers, effects)
        hk::SoundDef def;
        juce::String name;
        int kind = hk::kCatHousePiano;
        int parentA = -1, parentB = -1;
        int levelSound = -1;        // factory sound to match loudness against (-1 = keep context loudness)
        juce::String request;
    };

    Result respond (const juce::String& text, const Context* current, juce::uint32 seed);

    // match loudness against the reference (parentA, or the previous version when editing)
    void autoLevel (Result& r, const hk::SampleBank* bank, const Context* previous);

    juce::StringArray describe (const hk::SoundDef& d);   // layers and effects, for the sound card

    // chips under the input
    juce::StringArray examples();
    juce::StringArray followUps();

    bool clampParam (const juce::String& id, float& value);

    // a short phrase that shows off a sound (used for the "Spill" button and for level matching)
    struct PreviewNote { float t0, t1; int note; float vel; };
    const std::vector<PreviewNote>& previewPattern (int kind, bool mono);
    float previewLength (int kind, bool mono);
}
