#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "PresetAI.h"
#include <map>

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

    // AI-made / saved sounds (the "AI-lyd" slot). Message thread only.
    void applyCustomSound (hk::SoundDef def, const juce::String& name);
    bool saveCustomPreset (const juce::String& name, juce::String& message);
    bool loadCustomPreset (const juce::File& file);
    static juce::File presetFolder();
    static juce::Array<juce::File> listCustomPresets();
    juce::String getCustomName() const { return customName; }
    bool hasCustomSound() const { return customDef.load() != nullptr; }
    const hk::SoundDef& defFor (int index) const;

    // AI chat (free, offline). The conversation lives here so it survives closing the window.
    struct ChatMessage
    {
        bool fromUser = false;
        juce::String text;
        bool hasSound = false;
        hk::SoundDef def;
        juce::String name, request;
        int kind = hk::kCatHousePiano;
        juce::StringArray chips;
        bool saved = false;
        bool edited = false;   // a change of the sound before (not a brand-new sound)
        bool scratch = false;  // designed from scratch
    };
    const std::vector<ChatMessage>& getChat() const { return chat; }
    bool isAiThinking() const { return aiBusy; }
    int getActiveChatSound() const;                 // message whose sound is playing now, or -1
    void sendChat (const juce::String& text);
    void useChatSound (int index, bool preview);
    bool saveChatSound (int index, juce::String& message);
    void chatVariant (int index);
    void clearChat();
    juce::String getApiKey() const { return apiKey; }
    void setApiKey (const juce::String& k) { apiKey = k.trim(); PresetAI::saveApiKey (apiKey); chatChanged.sendChangeMessage(); }
    bool usesOnlineAi() const { return apiKey.isNotEmpty(); }
    void previewSound (int kind, bool mono) { previewRequest.store (kind | (mono ? 256 : 0)); }
    juce::ChangeBroadcaster chatChanged;

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

    void finishChat (PresetAI::Result r);
    juce::String uniqueName (const juce::String& base) const;
    std::vector<ChatMessage> chat;
    bool aiBusy = false;
    int activeChat = -1;
    const hk::SoundDef* activeDef = nullptr;
    int customKind = hk::kCatHousePiano;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    struct AiWorker : public juce::Thread   // own thread with a big stack (the AI renders audio to level-match)
    {
        explicit AiWorker (std::function<void()> f) : juce::Thread ("HouseKeys AI", 8 * 1024 * 1024), fn (std::move (f)) {}
        void run() override { fn(); }
        std::function<void()> fn;
    };
    std::unique_ptr<AiWorker> aiWorker;
    juce::String apiKey = PresetAI::loadApiKey();
    std::atomic<int> previewRequest { -1 };
    int pvPos = -1, pvKind = 0;   // audio thread
    bool pvMono = false;

    hk::Engine engine;
    juce::AudioBuffer<float> scratch;
    std::atomic<int> appliedSound { hk::kDefaultSound };   // sound whose defaults are currently loaded

    std::vector<std::unique_ptr<hk::SoundDef>> customDefs;   // kept alive: voices may still use old ones
    std::vector<std::unique_ptr<std::string>> customNames;
    std::atomic<const hk::SoundDef*> customDef { nullptr };
    juce::String customName;

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
