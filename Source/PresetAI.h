#pragma once

#include <juce_core/juce_core.h>
#include <map>

// "Lag lyden du vil ha": turns a description ("fet deep house piano med mye luft og pump")
// into a sound + control settings.
//   - interpretLocal(): built-in, offline, Norwegian + English, instant
//   - askClaude():      optional, uses the user's own Anthropic API key for free-form requests;
//                       falls back to the local interpreter on any error
namespace PresetAI
{
    struct Result
    {
        bool ok = false;
        int sound = -1;
        std::map<juce::String, float> params;   // parameter id -> real value
        juce::StringArray notes;                // what was done, in Norwegian
        juce::String error;
        bool fromClaude = false;
    };

    Result interpretLocal (const juce::String& text, int currentSound);

    // Runs on a background thread; 'done' is called on the message thread.
    void askClaude (const juce::String& text, const juce::String& apiKey, int currentSound,
                    std::function<void (Result)> done);

    // clamps a value to the valid range of a parameter (false if the id is unknown)
    bool clampParam (const juce::String& id, float& value);

    // API key is stored per user (not in projects) in ~/Library/Application Support/HouseKeys/settings.xml
    juce::String loadApiKey();
    void saveApiKey (const juce::String& key);

    juce::StringArray examples();
}
