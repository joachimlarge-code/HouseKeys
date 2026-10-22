#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SampleLoader.h"
#include "SoundCodec.h"

namespace
{
using Range = juce::NormalisableRange<float>;
using Fmt   = std::function<juce::String (float, int)>;

Fmt pct()  { return [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }; }
Fmt db()   { return [] (float v, int) { return juce::String (v, 1) + " dB"; }; }
Fmt hz()   { return [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                             : juce::String (juce::roundToInt (v)) + " Hz"; }; }
Fmt ms()   { return [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s"
                                                             : juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }; }
Fmt secs() { return [] (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                                                        : juce::String (v, 2) + " s"; }; }

std::unique_ptr<juce::AudioParameterFloat> fparam (const char* id, const char* name, Range r, float def, Fmt f)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)));
}

std::unique_ptr<juce::AudioParameterBool> bparam (const char* id, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> cparam (const char* id, const char* name, juce::StringArray items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}

Range skewed (float lo, float hi, float centre) { Range r (lo, hi); r.setSkewForCentre (centre); return r; }

// the value every parameter takes for a given sound
float presetValue (const hk::Params& q, const juce::String& id)
{
    if (id == "sound") return (float) q.sound;
    if (id == "tone") return q.tone;            if (id == "air") return q.air;
    if (id == "attack") return q.attackMs;      if (id == "decay") return q.decay;
    if (id == "release") return q.releaseS;     if (id == "width") return q.width;
    if (id == "volume") return q.volumeDb;      if (id == "chord") return (float) q.chord;
    if (id == "filterOn") return q.filterOn;    if (id == "cutoff") return q.cutoffHz;   if (id == "reso") return q.resonance;
    if (id == "satOn") return q.satOn;          if (id == "satDrive") return q.satDrive;
    if (id == "chorusOn") return q.chorusOn;    if (id == "chorusMix") return q.chorusMix;
    if (id == "delayOn") return q.delayOn;      if (id == "delayTime") return (float) q.delayTime;
    if (id == "delayFb") return q.delayFeedback; if (id == "delayMix") return q.delayMix;
    if (id == "reverbOn") return q.reverbOn;    if (id == "reverbSize") return q.reverbSize; if (id == "reverbMix") return q.reverbMix;
    if (id == "pumpOn") return q.pumpOn;        if (id == "pumpRate") return (float) q.pumpRate; if (id == "pumpDepth") return q.pumpDepth;
    if (id == "vibOn") return q.vibOn;          if (id == "vibRate") return q.vibRate;   if (id == "vibDepth") return q.vibDepth;
    if (id == "smpMode") return (float) q.smpMode;
    return 0.0f;
}

const char* const kResetIds[] = { "tone", "air", "attack", "decay", "release", "width", "volume", "chord",
                                  "filterOn", "cutoff", "reso", "satOn", "satDrive", "chorusOn", "chorusMix",
                                  "delayOn", "delayTime", "delayFb", "delayMix", "reverbOn", "reverbSize", "reverbMix",
                                  "pumpOn", "pumpRate", "pumpDepth", "vibOn", "vibRate", "vibDepth", "smpMode" };
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HouseKeysProcessor::createLayout()
{
    const hk::Params d = hk::sounds()[(size_t) hk::kDefaultSound].preset;
    juce::StringArray names;
    for (auto& s : hk::sounds()) names.add (juce::String::fromUTF8 (s.name));

    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (cparam ("sound", "Sound", names, d.sound));
    l.add (fparam ("tone",    "Tone",    Range (0.0f, 1.0f), d.tone, pct()));
    l.add (fparam ("air",     "Air",     Range (0.0f, 1.0f), d.air, pct()));
    l.add (fparam ("attack",  "Attack",  skewed (0.0f, 2000.0f, 120.0f), d.attackMs, ms()));
    l.add (fparam ("decay",   "Decay",   Range (0.0f, 1.0f), d.decay, pct()));
    l.add (fparam ("release", "Release", skewed (0.01f, 6.0f, 0.6f), d.releaseS, secs()));
    l.add (fparam ("width",   "Width",   Range (0.0f, 1.0f), d.width, pct()));
    l.add (fparam ("volume",  "Volume",  Range (-24.0f, 6.0f), d.volumeDb, db()));
    l.add (cparam ("chord",   "Chord",   { "Off", "Min7", "Min9", "Min11", "Maj7", "Maj9", "7sus4" }, d.chord));

    l.add (bparam ("filterOn", "Filter On", d.filterOn));
    l.add (fparam ("cutoff", "Filter Cutoff", skewed (80.0f, 18000.0f, 1500.0f), d.cutoffHz, hz()));
    l.add (fparam ("reso",   "Filter Reso",   Range (0.0f, 1.0f), d.resonance, pct()));

    l.add (bparam ("satOn", "Saturation On", d.satOn));
    l.add (fparam ("satDrive", "Saturation Drive", Range (0.0f, 1.0f), d.satDrive, pct()));

    l.add (bparam ("chorusOn", "Chorus On", d.chorusOn));
    l.add (fparam ("chorusMix", "Chorus Mix", Range (0.0f, 1.0f), d.chorusMix, pct()));

    l.add (bparam ("delayOn", "Delay On", d.delayOn));
    l.add (cparam ("delayTime", "Delay Time", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, d.delayTime));
    l.add (fparam ("delayFb",  "Delay Feedback", Range (0.0f, 0.9f), d.delayFeedback, pct()));
    l.add (fparam ("delayMix", "Delay Mix",      Range (0.0f, 1.0f), d.delayMix, pct()));

    l.add (bparam ("reverbOn", "Reverb On", d.reverbOn));
    l.add (fparam ("reverbSize", "Reverb Size", Range (0.0f, 1.0f), d.reverbSize, pct()));
    l.add (fparam ("reverbMix",  "Reverb Mix",  Range (0.0f, 1.0f), d.reverbMix, pct()));

    l.add (bparam ("pumpOn", "Pump On", d.pumpOn));
    l.add (cparam ("pumpRate", "Pump Rate", { "1/4", "1/8", "1/16" }, d.pumpRate));
    l.add (fparam ("pumpDepth", "Pump Depth", Range (0.0f, 1.0f), d.pumpDepth, pct()));

    l.add (bparam ("vibOn", "Vibrato On", d.vibOn));
    l.add (fparam ("vibRate",  "Vibrato Rate",  skewed (0.5f, 12.0f, 5.0f), d.vibRate,
                   [] (float v, int) { return juce::String (v, 1) + " Hz"; }));
    l.add (fparam ("vibDepth", "Vibrato Depth", Range (0.0f, 1.0f), d.vibDepth, pct()));
    l.add (cparam ("smpMode", "Sample Mode", { "Auto", "Poly", "Chord stab" }, d.smpMode));
    return l;
}

HouseKeysProcessor::HouseKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HouseKeys", createLayout())
{
    apvts.addParameterListener ("sound", this);
    SampleLibrary::get();   // start loading the piano samples in the background
    for (int k = 0; k < hk::kNumCategories; ++k) { PresetAI::previewPattern (k, false); PresetAI::previewPattern (k, true); }  // build the static tables here, not on the audio thread
}

HouseKeysProcessor::~HouseKeysProcessor()
{
    alive->store (false);   // an AI answer still on its way is dropped
    apvts.removeParameterListener ("sound", this);
    cancelPendingUpdate();
}

// The sound parameter can change from the UI, a preset or automation (possibly on the audio
// thread). Resetting all other controls happens on the message thread.
void HouseKeysProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

void HouseKeysProcessor::handleAsyncUpdate()
{
    const int cur = (int) raw ("sound");
    if (cur != appliedSound.load()) applySoundPreset (cur);
}

void HouseKeysProcessor::setParam (const juce::String& id, float value)
{
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        const float norm = prm->convertTo0to1 (value);
        if (std::abs (prm->getValue() - norm) < 1.0e-6f) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (norm);
        prm->endChangeGesture();
    }
}

float HouseKeysProcessor::soundDefault (const juce::String& id) const
{
    return presetValue (defFor ((int) raw ("sound")).preset, id);
}

void HouseKeysProcessor::resetToSoundDefault (const juce::String& id) { setParam (id, soundDefault (id)); }

void HouseKeysProcessor::applySoundPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) hk::kNumSounds)) return;
    appliedSound = index;
    const hk::Params q = defFor (index).preset;
    for (auto* id : kResetIds) setParam (id, presetValue (q, id));
}

const hk::SoundDef& HouseKeysProcessor::defFor (int index) const
{
    index = juce::jlimit (0, hk::kNumSounds - 1, index);
    if (index == hk::kAiSound)
        if (auto* d = customDef.load()) return *d;
    return hk::sounds()[(size_t) index];
}

void HouseKeysProcessor::applyCustomSound (hk::SoundDef def, const juce::String& name)
{
    codec::sanitise (def);
    customNames.push_back (std::make_unique<std::string> (name.toStdString()));
    def.name = customNames.back()->c_str();
    def.tag = "";
    def.category = hk::kCatMine;
    def.preset.sound = hk::kAiSound;
    def.preset.smpMode = 0;
    customDefs.push_back (std::make_unique<hk::SoundDef> (def));
    customDef.store (customDefs.back().get());
    customName = name;
    appliedSound = -1;            // force the controls to the new sound's values
    selectSound (hk::kAiSound);
}

juce::File HouseKeysProcessor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Application Support").getChildFile ("HouseKeys").getChildFile ("Presets");
}

juce::Array<juce::File> HouseKeysProcessor::listCustomPresets()
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.hkpreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

bool HouseKeysProcessor::saveCustomPreset (const juce::String& rawName, juce::String& message)
{
    const auto name = rawName.trim().isNotEmpty() ? rawName.trim() : juce::String ("Min lyd");
    hk::SoundDef d = defFor (getCurrentProgram());
    d.preset = readParams();               // save the controls exactly as they are now
    auto json = codec::toVar (d);
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("version", 1);
    root->setProperty ("kind", customKind);
    root->setProperty ("sound", json);
    auto folder = presetFolder();
    folder.createDirectory();
    auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".hkpreset");
    if (! file.replaceWithText (juce::JSON::toString (juce::var (root.get()))))
    {
        message = "Kunne ikke lagre i " + folder.getFullPathName();
        return false;
    }
    message = juce::String::fromUTF8 ("Lagret \u00ab") + name + juce::String::fromUTF8 ("\u00bb under Mine lyder");
    applyCustomSound (d, name);
    return true;
}

bool HouseKeysProcessor::loadCustomPreset (const juce::File& file)
{
    const auto root = juce::JSON::parse (file);
    if (! root.isObject()) return false;
    hk::SoundDef d = hk::sounds()[(size_t) hk::kFreshHouse];
    codec::fromVar (root.getProperty ("sound", {}), d);
    customKind = (int) root.getProperty ("kind", hk::kCatHousePiano);
    applyCustomSound (d, root.getProperty ("name", file.getFileNameWithoutExtension()).toString());
    return true;
}

//==============================================================================
int HouseKeysProcessor::getActiveChatSound() const
{
    if (activeChat < 0 || activeChat >= (int) chat.size()) return -1;
    return ((int) raw ("sound") == hk::kAiSound && customDef.load() == activeDef) ? activeChat : -1;
}

juce::String HouseKeysProcessor::uniqueName (const juce::String& base) const
{
    auto taken = [this] (const juce::String& n)
    {
        for (auto& m : chat) if (m.hasSound && m.name.equalsIgnoreCase (n)) return true;
        return presetFolder().getChildFile (juce::File::createLegalFileName (n) + ".hkpreset").existsAsFile();
    };
    if (! taken (base)) return base;
    for (int i = 2; i < 100; ++i)
        if (! taken (base + " " + juce::String (i))) return base + " " + juce::String (i);
    return base;
}

void HouseKeysProcessor::sendChat (const juce::String& rawText)
{
    const auto text = rawText.trim();
    if (aiBusy || text.isEmpty()) return;
    ChatMessage um;
    um.fromUser = true;
    um.text = text;
    chat.push_back (um);
    aiBusy = true;
    chatChanged.sendChangeMessage();

    // what the conversation is about: the AI sound that is playing, or the factory sound you are on
    std::shared_ptr<PresetAI::Context> ctx;
    const int cur = getCurrentProgram();
    if (cur == hk::kAiSound && customDef.load() != nullptr)
    {
        ctx = std::make_shared<PresetAI::Context>();
        ctx->def = *customDef.load();
        ctx->def.preset = readParams();          // includes your own knob tweaks
        ctx->name = customName;
        const int a = getActiveChatSound();
        ctx->kind = a >= 0 ? chat[(size_t) a].kind : customKind;
        ctx->request = a >= 0 ? chat[(size_t) a].request : customName;
    }
    else if (cur != hk::kDroppedSample)
    {
        ctx = std::make_shared<PresetAI::Context>();
        ctx->def = hk::sounds()[(size_t) cur];
        ctx->def.preset = readParams();
        ctx->name = juce::String::fromUTF8 (hk::sounds()[(size_t) cur].name);
        ctx->kind = ctx->def.category;
        ctx->request = ctx->name;
        ctx->isFactory = true;
    }

    const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
    auto flag = alive;
    juce::Thread::launch ([this, flag, ctx, text, seed]
    {
        const auto start = juce::Time::getMillisecondCounter();
        auto r = PresetAI::respond (text, ctx.get(), seed);
        if (r.hasSound) PresetAI::autoLevel (r, SampleLibrary::get(), ctx.get());
        const auto took = juce::Time::getMillisecondCounter() - start;
        if (took < 700) juce::Thread::sleep ((int) (700 - took));   // let the "typing" dots show
        juce::MessageManager::callAsync ([this, flag, r] { if (flag->load()) finishChat (r); });
    });
}

void HouseKeysProcessor::finishChat (PresetAI::Result r)
{
    aiBusy = false;
    ChatMessage m;
    m.text = r.reply;
    if (r.renamed)
    {
        const int a = getActiveChatSound();
        if (a >= 0) chat[(size_t) a].name = r.name;
        if (customDef.load() != nullptr && getCurrentProgram() == hk::kAiSound)
        {
            // new name, same sound and the same knob settings
            hk::SoundDef d = *customDef.load();
            customNames.push_back (std::make_unique<std::string> (r.name.toStdString()));
            d.name = customNames.back()->c_str();
            customDefs.push_back (std::make_unique<hk::SoundDef> (d));
            customDef.store (customDefs.back().get());
            customName = r.name;
            if (a >= 0) activeDef = customDef.load();
        }
    }
    else if (r.hasSound)
    {
        const bool isNew = r.levelSound >= 0;
        m.hasSound = true;
        m.def = r.def;
        m.name = isNew ? uniqueName (r.name) : r.name;
        if (m.name != r.name) m.text = m.text.replace ("**" + r.name + "**", "**" + m.name + "**");
        m.kind = r.kind;
        m.chips = r.chips;
        m.request = r.request;
        m.edited = ! isNew;
        m.scratch = r.scratch;
    }
    chat.push_back (m);
    if (m.hasSound) useChatSound ((int) chat.size() - 1, false);
    chatChanged.sendChangeMessage();
}

void HouseKeysProcessor::useChatSound (int index, bool preview)
{
    if (! juce::isPositiveAndBelow (index, (int) chat.size()) || ! chat[(size_t) index].hasSound) return;
    auto& m = chat[(size_t) index];
    if (getActiveChatSound() != index)
    {
        customKind = m.kind;
        applyCustomSound (m.def, m.name);
        activeChat = index;
        activeDef = customDef.load();
    }
    if (preview) previewSound (m.kind, m.def.mono);
    chatChanged.sendChangeMessage();
}

bool HouseKeysProcessor::saveChatSound (int index, juce::String& message)
{
    if (! juce::isPositiveAndBelow (index, (int) chat.size()) || ! chat[(size_t) index].hasSound) return false;
    useChatSound (index, false);
    const bool ok = saveCustomPreset (chat[(size_t) index].name, message);
    activeChat = index;
    activeDef = customDef.load();
    chat[(size_t) index].saved = ok;
    chatChanged.sendChangeMessage();
    return ok;
}

void HouseKeysProcessor::chatVariant (int index)
{
    if (aiBusy) return;
    useChatSound (index, false);
    sendChat (juce::String::fromUTF8 ("Ny variant"));
}

void HouseKeysProcessor::clearChat()
{
    if (aiBusy) return;
    chat.clear();
    activeChat = -1;
    chatChanged.sendChangeMessage();
}

void HouseKeysProcessor::selectSound (int index)
{
    index = (index % hk::kNumSounds + hk::kNumSounds) % hk::kNumSounds;   // arrows wrap around
    setParam ("sound", (float) index);
    applySoundPreset (index);
}

bool HouseKeysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HouseKeysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.setSize (1, juce::jmax (1, samplesPerBlock));
}

hk::Params HouseKeysProcessor::readParams() const
{
    hk::Params p;
    p.sound = (int) raw ("sound");
    p.tone = raw ("tone"); p.air = raw ("air"); p.attackMs = raw ("attack"); p.decay = raw ("decay");
    p.releaseS = raw ("release"); p.width = raw ("width"); p.volumeDb = raw ("volume");
    p.chord = (int) raw ("chord");
    p.filterOn = raw ("filterOn") > 0.5f; p.cutoffHz = raw ("cutoff"); p.resonance = raw ("reso");
    p.satOn = raw ("satOn") > 0.5f; p.satDrive = raw ("satDrive");
    p.chorusOn = raw ("chorusOn") > 0.5f; p.chorusMix = raw ("chorusMix");
    p.delayOn  = raw ("delayOn") > 0.5f;  p.delayTime = (int) raw ("delayTime");
    p.delayFeedback = raw ("delayFb");    p.delayMix = raw ("delayMix");
    p.reverbOn = raw ("reverbOn") > 0.5f; p.reverbSize = raw ("reverbSize"); p.reverbMix = raw ("reverbMix");
    p.pumpOn   = raw ("pumpOn") > 0.5f;   p.pumpRate = (int) raw ("pumpRate"); p.pumpDepth = raw ("pumpDepth");
    p.vibOn    = raw ("vibOn") > 0.5f;    p.vibRate = raw ("vibRate");         p.vibDepth = raw ("vibDepth");
    p.smpMode  = (int) raw ("smpMode");
    return p;
}

void HouseKeysProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())          engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())   engine.setSustainPedal (true);
    else if (m.isSustainPedalOff())  engine.setSustainPedal (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isController() && m.getControllerNumber() == 1) engine.setModWheel ((float) m.getControllerValue() / 127.0f);
}

void HouseKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    engine.setParams (readParams());
    engine.setSampleBank (SampleLibrary::get());
    engine.setUserBank (userBank.load());
    engine.setCustomSound (customDef.load());

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    bpm = juce::jlimit (40.0, 300.0, bpm);

    if (scratch.getNumSamples() < numSamples) scratch.setSize (1, numSamples, false, false, true);
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : scratch.getWritePointer (0);
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;

    auto render = [&] (int from, int to)
    {
        if (to > from)
            engine.process (L + from, R + from, to - from, bpm, ppq + from * bpm / 60.0 / sr, playing);
    };

    // "Spill" button in the AI chat: a short phrase played straight into the engine
    if (const int req = previewRequest.exchange (-1); req >= 0)
    {
        engine.allNotesOff();
        pvKind = req & 255; pvMono = (req & 256) != 0; pvPos = 0;
    }
    if (pvPos >= 0)
    {
        const auto& pat = PresetAI::previewPattern (pvKind, pvMono);
        for (auto& e : pat) { const int off = (int) (e.t1 * sr); if (off >= pvPos && off < pvPos + numSamples) engine.noteOff (e.note); }
        for (auto& e : pat) { const int on = (int) (e.t0 * sr);   if (on >= pvPos && on < pvPos + numSamples) engine.noteOn (e.note, e.vel); }
        pvPos += numSamples;
        if (pvPos > (int) ((PresetAI::previewLength (pvKind, pvMono) + 0.1f) * sr)) pvPos = -1;
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        render (pos, t);
        handleMidi (meta.getMessage());
        pos = t;
    }
    render (pos, numSamples);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);

    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < numSamples; ++i) { pl = std::max (pl, std::abs (L[i])); pr = std::max (pr, std::abs (R[i])); }
    meterL = std::max (pl, meterL.load()); meterR = std::max (pr, meterR.load());
}

//==============================================================================
int HouseKeysProcessor::getCurrentProgram() { return (int) raw ("sound"); }

const juce::String HouseKeysProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) hk::kNumSounds) ? juce::String::fromUTF8 (defFor (index).name)
                                                                  : juce::String();
}

void HouseKeysProcessor::setCurrentProgram (int index) { selectSound (index); }

bool HouseKeysProcessor::loadUserSample (const juce::File& file, juce::String& message)
{
    juce::AudioBuffer<float> audio;
    double sr = 44100.0;
    if (! UserSample::decode (file, audio, sr, message)) return false;
    UserSample::prepare (audio, sr);
    const auto an = UserSample::analyse (audio, sr);
    installUserSample (std::move (audio), sr, an.pitched ? an.root : 60, an.cents, an.chord, file.getFileNameWithoutExtension());
    message = an.describe();
    selectSound (hk::kDroppedSample);
    return true;
}

void HouseKeysProcessor::installUserSample (juce::AudioBuffer<float> prepared, double sr, int root, float cents, bool chord,
                                            const juce::String& name)
{
    userFlac = UserSample::encodeFlac (prepared, sr);
    userRate = sr;
    userRoot = root; userCents = cents; userChord = chord;
    UserSample::Analysis an; an.root = root; an.cents = cents; an.chord = chord; an.pitched = true;
    userInfo = an.describe();
    userName = name;
    auto bank = UserSample::makeBank (prepared, sr, root);
    for (auto& z : bank->zones) z.tuneCents = cents;
    bank->chordSample = chord;
    userBanks.push_back (std::move (bank));
    userBank.store (userBanks.back().get());
}

void HouseKeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (userFlac.getSize() > 0)   // the dropped sample travels with the project
    {
        juce::ValueTree us ("UserSample");
        us.setProperty ("name", userName, nullptr);
        us.setProperty ("root", userRoot, nullptr);
        us.setProperty ("cents", userCents, nullptr);
        us.setProperty ("chord", userChord, nullptr);
        us.setProperty ("data", userFlac.toBase64Encoding(), nullptr);
        state.appendChild (us, nullptr);
    }
    if (auto* d = customDef.load())   // the AI / saved sound travels with the project
    {
        juce::ValueTree cs ("CustomSound");
        cs.setProperty ("name", customName, nullptr);
        cs.setProperty ("json", juce::JSON::toString (codec::toVar (*d), true), nullptr);
        state.appendChild (cs, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void HouseKeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            // Loading a saved project: keep the user's tweaks, don't reset to the sound's defaults.
            auto tree = juce::ValueTree::fromXml (*xml);
            for (auto child : tree)
                if (child.getProperty ("id").toString() == "sound")
                    appliedSound = (int) child.getProperty ("value");
            auto us = tree.getChildWithName ("UserSample");
            if (us.isValid())
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (us.getProperty ("data").toString()))
                {
                    juce::AudioBuffer<float> audio;
                    double sr = 48000.0;
                    if (UserSample::decodeFlac (mb, audio, sr))
                        installUserSample (std::move (audio), sr, (int) us.getProperty ("root", 60), (float) us.getProperty ("cents", 0.0),
                                           (bool) us.getProperty ("chord", false), us.getProperty ("name").toString());
                }
                tree.removeChild (us, nullptr);
            }
            auto cs = tree.getChildWithName ("CustomSound");
            if (cs.isValid())
            {
                hk::SoundDef d = hk::sounds()[(size_t) hk::kFreshHouse];
                codec::fromVar (juce::JSON::parse (cs.getProperty ("json").toString()), d);
                codec::sanitise (d);
                customNames.push_back (std::make_unique<std::string> (cs.getProperty ("name").toString().toStdString()));
                d.name = customNames.back()->c_str(); d.tag = ""; d.category = hk::kCatMine; d.preset.sound = hk::kAiSound;
                customDefs.push_back (std::make_unique<hk::SoundDef> (d));
                customDef.store (customDefs.back().get());
                customName = cs.getProperty ("name").toString();
                tree.removeChild (cs, nullptr);
            }
            apvts.replaceState (tree);
            cancelPendingUpdate();
        }
}

juce::AudioProcessorEditor* HouseKeysProcessor::createEditor() { return new HouseKeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HouseKeysProcessor(); }
