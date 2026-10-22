// HouseKeys AI - a free, offline sound designer.
//
// New sound:  pick the kind of sound from the words -> choose two "parent" factory sounds that fit
//             (tags decide) -> cross-breed every layer and setting -> mutate -> maybe add a spice layer
//             -> shape with the words (fet, mørk, luftig, pump ...) -> name it -> match loudness.
// Follow-up:  "fetere", "mer luft", "uten reverb", "ny variant", "kall den X" change the current sound.
#include "PresetAI.h"
#include "SoundCodec.h"
#include "Engine.h"

namespace PresetAI
{
namespace
{
juce::String u (const char* s) { return juce::String::fromUTF8 (s); }

// lower case, Norwegian letters folded so "mørk" == "mork", punctuation -> spaces
juce::String normalise (juce::String t)
{
    t = t.toLowerCase();
    t = t.replace (u ("\xc3\xa6"), "ae").replace (u ("\xc3\xb8"), "o").replace (u ("\xc3\xa5"), "a")
         .replace (u ("\xc3\xa9"), "e").replace ("-", "").replace ("'", "");
    juce::String out;
    for (auto c : t) out << (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String (" "));
    return out;
}

juce::StringArray words (const juce::String& text)
{
    juce::StringArray w;
    w.addTokens (normalise (text), " ", "");
    w.removeEmptyStrings();
    return w;
}

// word matches keyword: exact, or Norwegian/English inflection ("pianoet", "pads", "akkordene")
bool matches (const juce::String& word, const juce::String& key)
{
    if (word == key) return true;
    if (key.length() >= 4 && word.startsWith (key) && word.length() <= key.length() + 4) return true;
    return false;
}

bool contains (const juce::StringArray& w, std::initializer_list<const char*> keys, int* at = nullptr)
{
    for (int i = 0; i < w.size(); ++i)
        for (auto* k : keys)
            if (matches (w[i], k)) { if (at) *at = i; return true; }
    return false;
}

bool isMoreLess (const juce::String& word)
{
    for (auto* k : { "mer", "mindre", "more", "less", "litt", "lite", "mye" }) if (word == k) return true;
    return false;
}

const std::map<int, std::vector<const char*>>& categoryNouns()
{
    static const std::map<int, std::vector<const char*>> m = {
        { hk::kCatRealPiano,  { "piano", "flygel", "grand", "klaver" } },
        { hk::kCatHousePiano, { "piano", "flygel", "klaver" } },
        { hk::kCatDeepKeys,   { "keys", "orgel", "organ", "stab", "stabs" } },
        { hk::kCatRhodes,     { "rhodes", "epiano", "elpiano", "wurli", "electric" } },
        { hk::kCatSupersaw,   { "supersaw", "chord", "chords", "akkord", "akkorder", "stab", "stabs", "trance" } },
        { hk::kCatSynths,     { "synth", "synther", "poly", "brass", "juno", "strings", "strykere" } },
        { hk::kCatLeads,      { "lead", "leads", "melodi", "melody", "sagtann" } },
        { hk::kCatPads,       { "pad", "pads", "flate", "ambient", "atmosfaere", "bakgrunn" } },
        { hk::kCatBass,       { "bass", "basslinje", "bassline", "808" } },
        { hk::kCatPlucks,     { "pluck", "plucks", "plukk", "arp", "arpeggio" } },
    };
    return m;
}

const std::map<int, std::vector<const char*>>& soundTags()
{
    static const std::map<int, std::vector<const char*>> m = {
        { hk::kConcertGrand,     { "konsert", "concert", "realistisk", "ekte", "real", "naturlig", "natural", "klassisk", "classical" } },
        { hk::kIntimateFelt,     { "felt", "myk", "soft", "intim", "intimate", "rolig", "calm", "stille", "nart" } },
        { hk::kBalladGrand,      { "ballade", "ballad", "emosjonell", "emotional", "trist", "sad" } },
        { hk::kOctaveGrand,      { "oktav", "octave", "ekte", "real" } },
        { hk::kDeepNightPiano,   { "natt", "night", "mork", "dark", "dyp", "deep", "drommende", "dreamy" } },
        { hk::kFreshHouse,       { "house", "fresh", "frisk", "luftig", "airy", "polert", "moderne", "modern" } },
        { hk::kBigHousePiano,    { "fet", "feit", "fat", "big", "stor", "kraftig", "energisk", "oktav", "house" } },
        { hk::kBrightPiano,      { "lys", "bright", "skarp", "crisp", "glitrende", "sparkling" } },
        { hk::kSupersawPiano,    { "supersaw", "lag", "layer", "episk", "epic" } },
        { hk::kSaturatedPiano,   { "mettet", "saturated", "distortion", "drive", "crunchy", "skitten", "dirty" } },
        { hk::kLofiHousePiano,   { "lofi", "vintage", "tape", "stovete", "dusty", "retro", "chill" } },
        { hk::kPianoHouse90s,    { "90s", "90", "nittitall", "m1", "oldschool", "klassisk", "classic", "rave" } },
        { hk::kSunsetPiano,      { "sunset", "solnedgang", "melodisk", "melodic", "sommer", "summer" } },
        { hk::kPumpingChords,    { "pump", "pumpende", "pumping", "sidechain" } },
        { hk::kDeepHouseKeys,    { "deep", "dyp", "varm", "warm", "myk" } },
        { hk::kUkGarageKeys,     { "uk", "garage", "orgel", "organ", "lys" } },
        { hk::kDeepWideStab,     { "bred", "wide", "dyp", "deep" } },
        { hk::kShortOrganStab,   { "kort", "short", "orgel", "organ", "tett" } },
        { hk::kWarmRhodes,       { "varm", "warm", "soul", "jazz", "neo" } },
        { hk::kLofiRhodes,       { "lofi", "tape", "chill", "stovete", "dusty", "vintage" } },
        { hk::kSupersawChords,   { "chords", "akkorder" } },
        { hk::kEuphoricSupersaw, { "euforisk", "euphoric", "stor", "episk", "epic", "festival", "drop", "big" } },
        { hk::kSupersawStab,     { "kort", "short", "punchy", "stab" } },
        { hk::kMelodicChords,    { "melodisk", "melodic" } },
        { hk::kRaveStab,         { "rave", "hard", "oldschool", "90s" } },
        { hk::kPolySynth,        { "poly", "analog", "80s", "80" } },
        { hk::kAnalogBrass,      { "brass", "horn", "messing", "analog" } },
        { hk::kJunoStrings,      { "juno", "strings", "strykere", "80s", "vintage" } },
        { hk::kHollowSynth,      { "hul", "hollow", "square", "firkant" } },
        { hk::kFatSawLead,       { "fet", "feit", "fat", "kraftig", "tung", "heavy", "saw" } },
        { hk::kSawLead,          { "saw", "skarp", "sharp" } },
        { hk::kOctaveSawLead,    { "oktav", "octave", "saw" } },
        { hk::kWideSawLead,      { "bred", "wide", "saw", "stor" } },
        { hk::kScreamLead,       { "scream", "skrik", "aggressiv", "aggressive", "distort", "hard", "ra", "raw" } },
        { hk::kHouseLead,        { "house", "klassisk", "classic", "hul" } },
        { hk::kSupersawLead,     { "supersaw", "episk", "epic", "trance", "bred" } },
        { hk::kWarmPad,          { "varm", "warm", "myk", "soft" } },
        { hk::kAirPad,           { "luftig", "airy", "lys", "bright", "ambient" } },
        { hk::kDarkStrings,      { "mork", "dark", "strings", "strykere", "emosjonell", "emotional" } },
        { hk::kDeepSub,          { "sub", "dyp", "deep", "ren", "clean", "808" } },
        { hk::kHouseBass,        { "house", "rund", "round" } },
        { hk::kReeseBass,        { "reese", "tung", "heavy", "dnb", "mork", "dark", "growl" } },
        { hk::kOrganBass,        { "orgel", "organ", "m1", "90s", "klassisk" } },
        { hk::kSawBass,          { "saw", "sagtann" } },
        { hk::kAcidBass,         { "acid", "303", "squelch", "resonans" } },
        { hk::kPluckBass,        { "pluck", "punchy", "kort", "short" } },
        { hk::kHousePluck,       { "house" } },
        { hk::kBrightPluck,      { "lys", "bright", "crisp", "skarp" } },
        { hk::kSupersawPluck,    { "supersaw", "trance" } },
        { hk::kMalletPluck,      { "mallet", "marimba", "klokke", "bell", "vibrafon", "kalimba" } },
        { hk::kDeepPluck,        { "dyp", "deep", "mork", "dark" } },
        { hk::kSoftPluck,        { "myk", "soft", "rolig", "calm" } },
    };
    return m;
}

float intensityBefore (const juce::StringArray& w, int at)
{
    for (int i = juce::jmax (0, at - 2); i < at; ++i)
    {
        if (contains (juce::StringArray (w[i]), { "mindre", "less", "lavere" })) return -0.6f;
        if (contains (juce::StringArray (w[i]), { "litt", "slightly", "bit", "lite", "subtle", "subtil" })) return 0.5f;
        if (contains (juce::StringArray (w[i]), { "mye", "veldig", "very", "masse", "ekstra", "extra", "super", "mega", "mer", "more", "much" })) return 1.6f;
    }
    return 1.0f;
}

bool negatedBefore (const juce::StringArray& w, int at)
{
    for (int i = juce::jmax (0, at - 2); i < at; ++i)
        if (contains (juce::StringArray (w[i]), { "uten", "ingen", "ikke", "no", "without", "not", "fjern", "skru" })) return true;
    return false;
}

const char* kindName (int c)   // Norwegian, for sentences
{
    static const char* n[hk::kNumCategories] = { "piano", "house-piano", "deep house keys", "rhodes", "supersaw",
                                                  "synth", "lead", "pad", "bass", "pluck", "sample", "lyd" };
    return n[juce::jlimit (0, hk::kNumCategories - 1, c)];
}

//==============================================================================
struct Builder
{
    const hk::Params base;
    std::map<juce::String, float>& m;
    float get (const char* id)
    {
        auto it = m.find (id);
        if (it != m.end()) return it->second;
        const juce::String s (id);
        if (s == "tone") return base.tone;           if (s == "air") return base.air;
        if (s == "attack") return base.attackMs;     if (s == "decay") return base.decay;
        if (s == "release") return base.releaseS;    if (s == "width") return base.width;
        if (s == "reverbMix") return base.reverbMix; if (s == "reverbSize") return base.reverbSize;
        if (s == "chorusMix") return base.chorusMix; if (s == "delayMix") return base.delayMix;
        if (s == "delayFb") return base.delayFeedback;
        if (s == "satDrive") return base.satDrive;   if (s == "cutoff") return base.cutoffHz;
        if (s == "pumpDepth") return base.pumpDepth; if (s == "volume") return base.volumeDb;
        if (s == "vibDepth") return base.vibDepth;
        return 0.0f;
    }
    void set (const char* id, float v) { clampParam (id, v); m[id] = v; }
    void add (const char* id, float d) { set (id, get (id) + d); }
    void scale (const char* id, float k) { set (id, get (id) * k); }
};

void writeParams (const std::map<juce::String, float>& m, hk::Params& p)
{
    for (auto& [id, v] : m)
    {
        if (id == "tone") p.tone = v;              else if (id == "air") p.air = v;
        else if (id == "attack") p.attackMs = v;   else if (id == "decay") p.decay = v;
        else if (id == "release") p.releaseS = v;  else if (id == "width") p.width = v;
        else if (id == "volume") p.volumeDb = v;   else if (id == "chord") p.chord = (int) v;
        else if (id == "filterOn") p.filterOn = v > 0.5f; else if (id == "cutoff") p.cutoffHz = v; else if (id == "reso") p.resonance = v;
        else if (id == "satOn") p.satOn = v > 0.5f;       else if (id == "satDrive") p.satDrive = v;
        else if (id == "chorusOn") p.chorusOn = v > 0.5f; else if (id == "chorusMix") p.chorusMix = v;
        else if (id == "delayOn") p.delayOn = v > 0.5f;   else if (id == "delayTime") p.delayTime = (int) v;
        else if (id == "delayFb") p.delayFeedback = v;    else if (id == "delayMix") p.delayMix = v;
        else if (id == "reverbOn") p.reverbOn = v > 0.5f; else if (id == "reverbSize") p.reverbSize = v; else if (id == "reverbMix") p.reverbMix = v;
        else if (id == "pumpOn") p.pumpOn = v > 0.5f;     else if (id == "pumpRate") p.pumpRate = (int) v; else if (id == "pumpDepth") p.pumpDepth = v;
        else if (id == "vibOn") p.vibOn = v > 0.5f;       else if (id == "vibRate") p.vibRate = v; else if (id == "vibDepth") p.vibDepth = v;
    }
}

void clampParams (hk::Params& p)
{
    auto c = [] (float& v, float lo, float hi) { v = juce::jlimit (lo, hi, std::isfinite (v) ? v : lo); };
    c (p.tone, 0, 1); c (p.air, 0, 1); c (p.attackMs, 0, 2000); c (p.decay, 0, 1); c (p.releaseS, 0.01f, 6); c (p.width, 0, 1);
    c (p.volumeDb, -24, 6); c (p.cutoffHz, 80, 18000); c (p.resonance, 0, 1); c (p.satDrive, 0, 1); c (p.chorusMix, 0, 1);
    c (p.delayFeedback, 0, 0.9f); c (p.delayMix, 0, 1); c (p.reverbMix, 0, 1); c (p.reverbSize, 0, 1); c (p.pumpDepth, 0, 1);
    c (p.vibRate, 0.5f, 12); c (p.vibDepth, 0, 1);
    p.delayTime = juce::jlimit (0, 5, p.delayTime); p.pumpRate = juce::jlimit (0, 2, p.pumpRate); p.chord = juce::jlimit (0, 6, p.chord);
}

//==============================================================================
// Controls (the knobs and effects). Returns notes in Norwegian.
void applyControls (const juce::StringArray& w, Builder& b, juce::StringArray& notes)
{
    int at = 0;
    auto has = [&] (std::initializer_list<const char*> k) { return contains (w, k, &at); };
    auto k = [&] { return intensityBefore (w, at); };

    if (has ({ "lys", "lysere", "bright", "brighter", "skarp", "crisp", "glitrende", "sparkling", "klar", "clear" }))
    { const float i = k(); b.add ("tone", 0.15f * i); b.add ("air", 0.2f * i); notes.add (i < 0 ? u ("Mindre lys") : u ("Lysere tone og mer Air")); }
    if (has ({ "mork", "morkere", "dark", "darker", "dempet", "muffled", "dull" }))
    { const float i = k(); b.add ("tone", -0.2f * i); b.add ("air", -0.25f * i); notes.add (i < 0 ? u ("Mindre m\xc3\xb8rk") : u ("M\xc3\xb8rkere tone")); }
    if (has ({ "varm", "varmere", "warm", "warmer" }))
    { const float i = k(); b.add ("tone", -0.08f * i); notes.add (u ("Varmere")); }
    if (has ({ "luftig", "luft", "airy", "air", "fresh", "frisk", "apen", "open" }))
    { const float i = k(); b.add ("air", 0.3f * i); notes.add (i < 0 ? u ("Mindre Air") : u ("Mer Air (luft og detaljer i toppen)")); }
    if (has ({ "bred", "bredere", "wide", "wider", "stor", "big", "huge", "enorm" }))
    { const float i = k(); b.add ("width", 0.18f * i); b.add ("reverbSize", 0.12f * i); notes.add (i < 0 ? u ("Smalere stereo") : u ("Bredere stereo")); }
    if (has ({ "smal", "smalere", "narrow", "tett" }))
    { b.add ("width", -0.25f); notes.add (u ("Smalere stereo")); }

    if (has ({ "fet", "fetere", "feit", "feitere", "fat", "fatter", "kraftig", "tung", "tyngre", "heavy", "punch", "punchy", "hard", "aggressiv", "aggressive" }))
    {
        const float i = k();
        if (i < 0) { b.scale ("satDrive", 0.5f); notes.add (u ("Mindre drive")); }
        else { b.set ("satOn", 1); b.set ("satDrive", juce::jmin (0.6f, 0.25f * i + 0.1f)); notes.add (u ("Saturation p\xc3\xa5 for mer kraft")); }
    }
    if (has ({ "distortion", "distort", "skitten", "dirty", "crunchy", "drive", "mettet", "saturated", "saturation" }))
    {
        const float i = k();
        if (negatedBefore (w, at)) { b.set ("satOn", 0); notes.add (u ("Saturation av")); }
        else if (i < 0) { b.scale ("satDrive", 0.5f); notes.add (u ("Mindre drive")); }
        else { b.set ("satOn", 1); b.set ("satDrive", juce::jmin (0.85f, 0.4f * i)); notes.add (u ("Mer saturation / drive")); }
    }

    if (has ({ "reverb", "romklang", "hall", "rom", "space", "ambient", "drommende", "dreamy", "romslig" }))
    {
        const float i = k();
        if (negatedBefore (w, at)) { b.set ("reverbOn", 0); notes.add (u ("Reverb av")); }
        else if (i < 0) { b.scale ("reverbMix", 0.55f); notes.add (u ("Mindre reverb")); }
        else { b.set ("reverbOn", 1); b.set ("reverbMix", juce::jmax (b.get ("reverbMix"), 0.22f + 0.14f * i)); b.add ("reverbSize", 0.15f * i);
               notes.add (u ("Mer reverb")); }
    }
    if (has ({ "tort", "torr", "torrere", "dry", "drier" })) { b.set ("reverbOn", 0); b.set ("delayOn", 0); notes.add (u ("T\xc3\xb8rr lyd (reverb og delay av)")); }
    if (has ({ "delay", "ekko", "echo", "pingpong" }))
    {
        const float i = k();
        if (negatedBefore (w, at)) { b.set ("delayOn", 0); notes.add (u ("Delay av")); }
        else if (i < 0) { b.scale ("delayMix", 0.55f); notes.add (u ("Mindre delay")); }
        else { b.set ("delayOn", 1); b.set ("delayMix", 0.14f + 0.08f * i); b.set ("delayFb", 0.3f + 0.08f * i); notes.add (u ("Delay p\xc3\xa5")); }
    }
    if (has ({ "chorus", "wobble", "shimmer" }))
    {
        const float i = k();
        if (negatedBefore (w, at)) { b.set ("chorusOn", 0); notes.add (u ("Chorus av")); }
        else if (i < 0) { b.scale ("chorusMix", 0.55f); notes.add (u ("Mindre chorus")); }
        else { b.set ("chorusOn", 1); b.set ("chorusMix", 0.25f + 0.12f * i); notes.add (u ("Chorus p\xc3\xa5")); }
    }
    if (has ({ "pump", "pumpende", "pumping", "sidechain", "ducking", "bouncy", "sprettende" }))
    {
        const float i = k();
        if (negatedBefore (w, at)) { b.set ("pumpOn", 0); notes.add (u ("Pump av")); }
        else if (i < 0) { b.scale ("pumpDepth", 0.6f); notes.add (u ("Mindre pump")); }
        else { b.set ("pumpOn", 1); b.set ("pumpDepth", juce::jmin (0.9f, 0.4f + 0.2f * i)); notes.add (u ("Pump i takt med kicken")); }
    }
    if (has ({ "vibrato", "vibrerende", "wobbly" }))
    {
        const float i = k();
        if (negatedBefore (w, at) || i < 0) { b.set ("vibOn", 0); notes.add (u ("Vibrato av")); }
        else { b.set ("vibOn", 1); b.set ("vibDepth", 0.2f * i + 0.05f); notes.add (u ("Vibrato p\xc3\xa5")); }
    }
    if (has ({ "filter", "filtrert", "filtered", "lowpass", "lavpass", "filtrer" }))
    {
        if (negatedBefore (w, at)) { b.set ("filterOn", 0); notes.add (u ("Filter av")); }
        else { b.set ("filterOn", 1); b.set ("cutoff", 1200.0f); b.set ("reso", 0.25f); notes.add (u ("Filter p\xc3\xa5 (1,2 kHz)")); }
    }

    if (has ({ "kort", "kortere", "short", "shorter", "staccato", "stab", "stabbete", "tight" }))
    { b.scale ("decay", 0.55f); b.set ("release", juce::jmin (b.get ("release"), 0.12f)); notes.add (u ("Kortere toner")); }
    if (has ({ "lang", "lange", "lengre", "long", "longer", "sustain", "svevende", "evig", "langt" }))
    { b.set ("decay", juce::jmax (0.75f, b.get ("decay"))); b.add ("release", 0.5f); notes.add (u ("Lengre toner")); }
    if (has ({ "swell", "fade", "langsom", "slow", "sakte", "saktere" }))
    { b.set ("attack", 350.0f); notes.add (u ("Langsom attack")); }
    if (has ({ "rask", "raskere", "fast", "snappy", "direkte" }))
    { b.set ("attack", 0.0f); notes.add (u ("Rask attack")); }
    if (has ({ "myk", "mykere", "soft", "softer", "rolig", "calm", "gentle" }))
    { b.add ("tone", -0.08f); b.add ("air", -0.1f); notes.add (u ("Mykere")); }
    if (has ({ "hoyere", "hoyt", "louder", "loud", "sterkere" }))
    { b.add ("volume", 3.0f); notes.add (u ("H\xc3\xb8yere volum")); }
    if (has ({ "lavere", "svakere", "quieter", "stillere" }) && ! contains (w, { "mindre", "less" }))
    { b.add ("volume", -3.0f); notes.add (u ("Lavere volum")); }

    // chords only when asked for explicitly
    struct ChordWord { const char* w; int c; };
    for (auto cw : { ChordWord { "min7", hk::kMin7 }, ChordWord { "m7", hk::kMin7 }, ChordWord { "min9", hk::kMin9 },
                     ChordWord { "m9", hk::kMin9 }, ChordWord { "min11", hk::kMin11 }, ChordWord { "maj7", hk::kMaj7 },
                     ChordWord { "maj9", hk::kMaj9 }, ChordWord { "sus", hk::kSus }, ChordWord { "sus4", hk::kSus } })
        if (w.contains (cw.w)) { b.set ("chord", (float) cw.c); notes.add (u ("Chord: ") + juce::String (cw.w).toUpperCase()); break; }
}

//==============================================================================
// Changes the sound DESIGN itself (oscillators, layers, filter, envelopes), not just the controls.
void designFromWords (const juce::StringArray& w, hk::SoundDef& d, juce::StringArray& notes, juce::StringArray& adjectives)
{
    int at = 0;
    auto has = [&] (std::initializer_list<const char*> k) { return contains (w, k, &at); };
    const bool isVa = d.va.level > 0.0f, isPiano = d.sampler.level > 0.0f;
    auto& va = d.va;

    if (has ({ "fet", "fetere", "feit", "feitere", "fat", "fatter", "tykk", "kraftig", "tung", "tyngre", "heavy", "massiv", "massive" }))
    {
        if (intensityBefore (w, at) < 0)
        {
            if (isVa) { va.unison = std::max (1, va.unison - 2); va.detune *= 0.7f; }
            d.sampler.octave *= 0.5f; d.drive *= 0.6f;
            notes.add (u ("Slankere (f\xc3\xa6rre stemmer og mindre drive)"));
        }
        else
        {
            if (isVa) { va.unison = std::min (7, va.unison + 2); va.detune += 8.0f; va.sub = std::max (va.sub, 0.3f); }
            if (isPiano) d.sampler.octave = std::max (d.sampler.octave, 0.35f);
            d.drive = std::min (1.0f, d.drive + 0.2f); d.glue = std::min (1.0f, d.glue + 0.15f);
            notes.add (u ("Fetere (flere stemmer, mer sub/oktav og drive)")); adjectives.add ("Fat");
        }
    }
    if (has ({ "bred", "bredere", "wide", "wider", "enorm", "huge" }) && intensityBefore (w, at) > 0)
    {
        if (isVa) { va.spread = 1.0f; va.detune += 8.0f; va.unison = std::max (va.unison, 5); }
        if (d.keys.level > 0.0f) d.keys.spread = std::min (1.0f, d.keys.spread + 0.3f);
        notes.add (u ("Bredere oscillatorer")); adjectives.add ("Wide");
    }
    if (has ({ "supersaw", "detune", "detuned", "trance" }))
    {
        if (isVa) { va.wave1 = hk::kSaw; va.unison = 7; va.detune = std::max (va.detune, 30.0f); va.spread = std::max (va.spread, 0.9f); }
        else { va = hk::sounds()[(size_t) hk::kSupersawChords].va; va.level = 0.3f; va.decayMs = 1400.0f; va.sustain = 0.35f; d.gainDb -= 2.0f; }
        notes.add (isVa ? u ("Supersaw (7 detunede sagtenner)") : u ("La til et supersaw-lag under"));
        adjectives.add ("Supersaw");
    }
    if (has ({ "orgel", "organ" }) && d.organ.level <= 0.0f && ! negatedBefore (w, at))
    {
        d.organ.level = 0.35f; d.organ.bars = { 0.0f, 1.0f, 0.6f, 0.0f, 0.3f }; d.organ.decay = 1.5f; d.organ.click = 0.1f;
        notes.add (u ("La til et orgel-lag")); adjectives.add ("Organ"); d.gainDb -= 2.0f;
    }
    if (has ({ "bjelle", "klokke", "bell", "bells", "glass", "glitter" }) && d.fm.level <= 0.0f && ! negatedBefore (w, at))
    {
        d.fm.level = 0.3f; d.fm.ratioA = 3.5f; d.fm.indexA = 1.2f; d.fm.susA = 0.25f; d.fm.decA = 1.2f;
        d.fm.octB = 2; d.fm.ratioB = 1.41f; d.fm.indexB = 0.8f; d.fm.decB = 0.7f; d.fm.mixB = 0.4f; d.fm.decay = 3.0f;
        notes.add (u ("La til et bjelle-lag (FM)")); adjectives.add ("Crystal"); d.gainDb -= 2.0f;
    }
    if (has ({ "oktav", "octave" }))
    {
        if (isPiano) d.sampler.octave = std::max (d.sampler.octave, 0.45f);
        if (isVa) { va.semi2 = 12.0f; va.mix2 = std::max (va.mix2, 0.6f); }
        notes.add (u ("Oktav-stablet"));
    }
    if (has ({ "sub", "808", "bunn", "bass" }) && ((at > 0 && isMoreLess (w[at - 1])) || w[at] == "sub" || w[at] == "808"))
    {
        const float i = intensityBefore (w, at);
        if (i < 0) { d.lowDb -= 3.0f; va.sub *= 0.5f; notes.add (u ("Mindre bunn")); }
        else { d.lowDb += 2.5f; if (isVa) va.sub = std::min (1.0f, std::max (va.sub, 0.3f) + 0.2f); notes.add (u ("Mer bunn og sub")); }
    }
    if (has ({ "acid", "303", "squelch", "resonans", "resonant" }) && isVa)
    { va.reso = 0.75f; va.envAmt = std::max (va.envAmt, 3.0f); va.fD = 200.0f; va.fS = 0.1f; notes.add (u ("Acid-filter med resonans")); adjectives.add ("Acid"); }
    if (has ({ "kort", "kortere", "short", "pluck", "plucky", "stab", "staccato" }) && isVa)
    { va.decayMs = std::min (va.decayMs > 0 ? va.decayMs : 1000.0f, 350.0f); va.sustain = 0.0f; va.fD = std::min (va.fD, 180.0f); va.fS = 0.0f;
      va.envAmt = std::max (va.envAmt, 3.0f); notes.add (u ("Kort pluck-konvolutt")); }
    if (has ({ "lys", "lysere", "bright", "skarp", "crisp", "aapen", "open" }))
    { const float i = intensityBefore (w, at); if (isVa) va.cutoff = juce::jlimit (60.0f, 16000.0f, va.cutoff * std::pow (1.8f, i)); d.highDb += 2.0f * i; adjectives.add ("Bright"); }
    if (has ({ "mork", "morkere", "dark", "dempet", "muffled" }))
    { const float i = intensityBefore (w, at); if (isVa) va.cutoff = juce::jlimit (60.0f, 16000.0f, va.cutoff * std::pow (0.5f, i)); d.highDb -= 3.0f * i; adjectives.add ("Midnight"); }
    if (has ({ "lofi", "vintage", "tape", "stovete", "dusty", "retro" }))
    { d.tape = std::max (d.tape, 0.5f); d.highDb -= 2.0f; notes.add (u ("Tape og vintage-farge")); adjectives.add ("Dusty"); }
    if (has ({ "analog", "drift", "ustabil" }) && isVa) { va.drift = 6.0f; va.motion = std::max (va.motion, 0.15f); adjectives.add ("Analog"); }
    if (has ({ "sinus", "sine", "ren", "clean", "pure" }) && isVa) { va.wave1 = hk::kSine; va.mix2 = std::min (va.mix2, 0.2f); }
    if (has ({ "square", "firkant", "hul", "hollow" }) && isVa) { va.wave1 = hk::kSquare; adjectives.add ("Hollow"); }
    if (has ({ "mono", "legato", "glide", "portamento" })) { d.mono = true; d.glideMs = std::max (d.glideMs, 60.0f); notes.add (u ("Mono med glide")); }
    if (has ({ "poly", "akkorder" }) && d.mono && ! has ({ "bass", "lead" })) { d.mono = false; d.glideMs = 0; notes.add (u ("Polyfon (spiller akkorder)")); }
    if (has ({ "bevegelse", "movement", "levende", "evolving" }) && isVa) { va.motion = 0.35f; notes.add (u ("Langsom filterbevegelse")); }
    if (has ({ "luftig", "luft", "airy", "fresh", "frisk" })) adjectives.add ("Airy");
    if (has ({ "drommende", "dreamy", "ambient" })) adjectives.add ("Dream");
    if (has ({ "pump", "pumpende", "pumping", "sidechain" })) adjectives.add ("Pumping");
    if (has ({ "varm", "warm" })) adjectives.add ("Golden");
    if (has ({ "myk", "soft", "rolig" })) adjectives.add ("Velvet");
    if (has ({ "dyp", "deep" })) adjectives.add ("Deep");
    if (has ({ "euforisk", "euphoric", "episk", "epic", "festival" })) adjectives.add ("Euphoric");
    if (has ({ "sommer", "summer", "sunset", "solnedgang", "strand", "beach" })) adjectives.add ("Sunset");
    if (has ({ "natt", "night" })) adjectives.add ("Night");
    if (has ({ "90s", "90", "oldschool", "rave" })) adjectives.add ("90s");
}

//==============================================================================
// Cross-breeding: every field is blended between parent A and B (t = share of B).
struct Mix
{
    float t;
    juce::Random& rnd;
    float operator() (float a, float b) const { return a + (b - a) * t; }
    int   operator() (int a, int b)     const { return rnd.nextFloat() < t ? b : a; }
    bool  operator() (bool a, bool b)   const { return rnd.nextFloat() < t ? b : a; }
};

#define HK_BX(f) o.f = m (a.f, b.f);
hk::SamplerDef blend (const hk::SamplerDef& a, const hk::SamplerDef& b, Mix& m) { auto o = a; HK_FIELDS_SAMPLER (HK_BX) return o; }
hk::OrganDef   blend (const hk::OrganDef& a, const hk::OrganDef& b, Mix& m)
{ auto o = a; HK_FIELDS_ORGAN (HK_BX) for (size_t i = 0; i < 5; ++i) o.bars[i] = m (a.bars[i], b.bars[i]); return o; }
hk::FmDef      blend (const hk::FmDef& a, const hk::FmDef& b, Mix& m)     { auto o = a; HK_FIELDS_FM (HK_BX) return o; }
hk::KeysDef    blend (const hk::KeysDef& a, const hk::KeysDef& b, Mix& m) { auto o = a; HK_FIELDS_KEYS (HK_BX) return o; }
hk::VaDef      blend (const hk::VaDef& a, const hk::VaDef& b, Mix& m)
{
    auto o = a; HK_FIELDS_VA (HK_BX)
    auto geo = [&] (float x, float y) { return (x > 0 && y > 0) ? std::exp (std::log (x) + (std::log (y) - std::log (x)) * m.t) : m (x, y); };
    o.cutoff = geo (a.cutoff, b.cutoff); o.fD = geo (a.fD, b.fD); o.fR = geo (a.fR, b.fR); o.decayMs = geo (a.decayMs, b.decayMs);
    return o;
}
hk::StrDef     blend (const hk::StrDef& a, const hk::StrDef& b, Mix& m)   { auto o = a; HK_FIELDS_STR (HK_BX) return o; }
hk::Params     blend (const hk::Params& a, const hk::Params& b, Mix& m)   { auto o = a; HK_FIELDS_PARAMS (HK_BX) return o; }
#undef HK_BX

template <class L> L crossLayer (const L& a, const L& b, Mix& m, juce::Random& rnd)
{
    if (a.level > 0 && b.level > 0) return blend (a, b, m);
    if (a.level > 0) { L o = a; o.level *= 1.0f - 0.45f * m.t; return o; }
    if (b.level > 0 && rnd.nextFloat() < m.t + 0.2f) { L o = b; o.level *= 0.35f + 0.5f * m.t; return o; }
    L o = a; o.level = 0.0f; return o;
}

hk::SoundDef crossBreed (const hk::SoundDef& A, const hk::SoundDef& B, float t, juce::Random& rnd)
{
    Mix m { t, rnd };
    hk::SoundDef d = A;
    d.sampler = crossLayer (A.sampler, B.sampler, m, rnd);
    d.organ   = crossLayer (A.organ, B.organ, m, rnd);
    d.fm      = crossLayer (A.fm, B.fm, m, rnd);
    d.keys    = crossLayer (A.keys, B.keys, m, rnd);
    d.va      = crossLayer (A.va, B.va, m, rnd);
    d.str     = crossLayer (A.str, B.str, m, rnd);
    if (d.sampler.level > 0)
    {
        d.fallback = A.sampler.level > 0 ? A.fallback : B.fallback;
        d.fallback.level = d.sampler.level;
    }
    else d.fallback.level = 0;
    d.sampler.user = false;
    d.mono = A.mono;
    d.glideMs = A.mono ? m (A.glideMs, B.glideMs) : 0.0f;
    d.lowCutHz = m (A.lowCutHz, B.lowCutHz); d.lowDb = m (A.lowDb, B.lowDb); d.highDb = m (A.highDb, B.highDb);
    d.drive = m (A.drive, B.drive); d.glue = m (A.glue, B.glue); d.tape = m (A.tape, B.tape); d.gainDb = m (A.gainDb, B.gainDb);
    d.preset = blend (A.preset, B.preset, m);
    d.preset.chord = hk::kChordOff;
    d.preset.smpMode = 0;
    return d;
}

//==============================================================================
float gauss (juce::Random& r) { return (r.nextFloat() + r.nextFloat() + r.nextFloat() - 1.5f) * 1.4f; }

void mutate (hk::SoundDef& d, int kind, float m, juce::Random& r)
{
    auto& va = d.va;
    const bool chordy = kind != hk::kCatBass && kind != hk::kCatLeads;
    if (va.level > 0)
    {
        va.detune *= std::exp (gauss (r) * m * 1.5f);
        va.cutoff *= std::exp2 (gauss (r) * m * 2.0f);
        if (kind == hk::kCatBass) va.cutoff = std::min (va.cutoff, 3500.0f);
        va.envAmt += gauss (r) * m * 2.0f;
        va.fD *= std::exp (gauss (r) * m * 1.5f);
        if (va.decayMs > 0) va.decayMs *= std::exp (gauss (r) * m);
        va.spread = juce::jlimit (0.0f, 1.0f, va.spread + gauss (r) * m);
        va.sub = juce::jlimit (0.0f, 1.0f, va.sub + gauss (r) * m * 0.5f);
        va.motion = juce::jlimit (0.0f, 0.6f, va.motion + std::abs (gauss (r)) * m * 0.3f);
        va.drift = juce::jlimit (0.0f, 10.0f, va.drift + gauss (r) * m * 4.0f);
        va.reso = juce::jlimit (0.0f, va.reso > 0.6f ? 0.9f : 0.7f, va.reso + gauss (r) * m * 0.4f);
        if (r.nextFloat() < m) va.unison = juce::jlimit (1, 7, va.unison + (r.nextFloat() < 0.65f ? 2 : -1));
        if (r.nextFloat() < m * 0.7f)
        {
            // a second oscillator at a musical interval
            static const float chordSafe[] = { 0.0f, 12.0f, -12.0f };
            static const float any[] = { 0.0f, 12.0f, -12.0f, 7.0f, 19.0f };
            va.semi2 = chordy ? chordSafe[r.nextInt (3)] : any[r.nextInt (5)];
            va.fine2 = gauss (r) * 8.0f;
            va.mix2 = juce::jlimit (0.2f, 0.8f, va.mix2 + 0.3f);
            if (r.nextFloat() < 0.4f) va.wave2 = r.nextInt (4);
        }
    }
    if (d.fm.level > 0)
    {
        static const float ratiosA[] = { 1.0f, 1.0f, 2.0f, 3.0f, 0.5f };
        static const float ratiosB[] = { 7.0f, 9.0f, 14.0f, 17.0f };
        if (r.nextFloat() < m * 0.8f) d.fm.ratioA = ratiosA[r.nextInt (5)];
        if (r.nextFloat() < m * 0.6f) d.fm.ratioB = ratiosB[r.nextInt (4)];
        d.fm.indexA *= std::exp (gauss (r) * m);
        d.fm.indexB *= std::exp (gauss (r) * m);
        d.fm.decay *= std::exp (gauss (r) * m);
    }
    if (d.keys.level > 0)
    {
        d.keys.tilt = juce::jlimit (0.0f, 1.0f, d.keys.tilt + gauss (r) * m * 0.3f);
        d.keys.bright = juce::jlimit (0.0f, 1.0f, d.keys.bright + gauss (r) * m * 0.3f);
        d.keys.tau *= std::exp (gauss (r) * m);
        d.keys.detune *= std::exp (gauss (r) * m);
        d.keys.spread = juce::jlimit (0.0f, 1.0f, d.keys.spread + gauss (r) * m * 0.5f);
    }
    if (d.sampler.level > 0)
    {
        d.sampler.octave = juce::jlimit (0.0f, 0.6f, d.sampler.octave + gauss (r) * m * 0.4f);
        d.sampler.dynamics += gauss (r) * m * 5.0f;
    }
    if (d.organ.level > 0)
    {
        for (auto& b : d.organ.bars) b = juce::jlimit (0.0f, 1.0f, b + gauss (r) * m * 0.4f);
        d.organ.click = juce::jlimit (0.0f, 1.0f, d.organ.click + gauss (r) * m * 0.3f);
    }
    d.lowDb += gauss (r) * m * 4.0f;
    d.highDb += gauss (r) * m * 4.0f;
    d.drive = juce::jlimit (0.0f, 0.7f, d.drive + gauss (r) * m * 0.4f);
    d.glue = juce::jlimit (0.0f, 0.9f, d.glue + gauss (r) * m * 0.3f);
    if (r.nextFloat() < m) d.tape = juce::jlimit (0.0f, 0.7f, d.tape + std::abs (gauss (r)) * 0.25f);

    auto& p = d.preset;
    p.air += gauss (r) * m * 0.5f;
    p.tone += gauss (r) * m * 0.3f;
    p.width += gauss (r) * m * 0.4f;
    p.reverbMix += gauss (r) * m * 0.2f;
    p.reverbSize += gauss (r) * m * 0.4f;
    p.chorusMix += gauss (r) * m * 0.2f;
    if (r.nextFloat() < m * 0.5f) p.chorusOn = ! p.chorusOn;
    if (r.nextFloat() < m * 0.4f && kind != hk::kCatBass) { p.delayOn = true; p.delayTime = 1 + r.nextInt (3); p.delayMix = 0.12f + 0.1f * r.nextFloat(); }
    clampParams (p);
}

// one extra ingredient that the parents did not have
void spice (hk::SoundDef& d, int kind, juce::Random& r, juce::StringArray& notes)
{
    const bool pianoLike = kind == hk::kCatRealPiano || kind == hk::kCatHousePiano || kind == hk::kCatDeepKeys || kind == hk::kCatRhodes;
    std::vector<int> options;
    if (d.fm.level <= 0 && kind != hk::kCatBass) options.push_back (0);                  // bell sparkle
    if (d.va.level <= 0 && pianoLike) options.push_back (1);                             // soft pad underneath
    if (d.organ.level <= 0 && (pianoLike || kind == hk::kCatSynths || kind == hk::kCatBass)) options.push_back (2);
    if (d.sampler.level > 0) options.push_back (3);                                      // octave piano
    if (d.va.level > 0) options.push_back (4);                                           // sub
    if (d.sampler.level <= 0 && (kind == hk::kCatSupersaw || kind == hk::kCatPlucks)) options.push_back (5);  // piano under the saws
    if (d.va.level > 0 && kind != hk::kCatBass) options.push_back (6);                   // slow filter movement
    if (options.empty()) return;

    switch (options[(size_t) r.nextInt ((int) options.size())])
    {
        case 0:
            d.fm.level = 0.12f + 0.12f * r.nextFloat(); d.fm.ratioA = 3.5f; d.fm.indexA = 1.0f; d.fm.susA = 0.2f; d.fm.decA = 1.0f;
            d.fm.octB = 2; d.fm.ratioB = 1.41f; d.fm.indexB = 0.7f; d.fm.decB = 0.6f; d.fm.mixB = 0.4f; d.fm.decay = 2.5f;
            notes.add (u ("Ekstra: et svakt bjelle-lag p\xc3\xa5 toppen")); break;
        case 1:
            d.va = hk::sounds()[(size_t) hk::kWarmPad].va;
            d.va.level = 0.16f + 0.1f * r.nextFloat(); d.va.decayMs = 2500.0f; d.va.sustain = 0.4f;
            notes.add (u ("Ekstra: en myk pad under")); break;
        case 2:
            d.organ.level = 0.15f + 0.12f * r.nextFloat(); d.organ.bars = { 0.0f, 1.0f, 0.5f, 0.0f, 0.25f }; d.organ.decay = 1.2f; d.organ.click = 0.15f;
            notes.add (u ("Ekstra: litt orgel i kroppen")); break;
        case 3:
            d.sampler.octave = std::max (d.sampler.octave, 0.3f + 0.2f * r.nextFloat());
            notes.add (u ("Ekstra: piano en oktav opp")); break;
        case 4:
            d.va.sub = std::max (d.va.sub, 0.35f + 0.25f * r.nextFloat());
            notes.add (u ("Ekstra: sub-oscillator for mer bunn")); break;
        case 5:
            d.sampler = hk::sounds()[(size_t) hk::kFreshHouse].sampler; d.sampler.level = 0.3f + 0.15f * r.nextFloat(); d.sampler.octave = 0.0f;
            d.fallback = hk::sounds()[(size_t) hk::kFreshHouse].fallback; d.fallback.level = d.sampler.level;
            notes.add (u ("Ekstra: ekte piano under saw-ene")); break;
        default:
            d.va.motion = std::max (d.va.motion, 0.3f);
            notes.add (u ("Ekstra: langsom filterbevegelse")); break;
    }
}

//==============================================================================
juce::String makeNameOnce (int kind, const juce::StringArray& adjectives, juce::Random& r);
juce::String makeName (int kind, const juce::StringArray& adjectives, juce::Random& r)
{
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        auto n = makeNameOnce (kind, attempt < 4 ? adjectives : juce::StringArray(), r);
        bool taken = false;
        for (auto& d : hk::sounds()) taken |= n.equalsIgnoreCase (u (d.name));
        if (! taken) return n;
    }
    return "Neon " + makeNameOnce (kind, {}, r);
}

juce::String makeNameOnce (int kind, const juce::StringArray& adjectives, juce::Random& r)
{
    static const char* general[] = { "Midnight", "Velvet", "Neon", "Golden", "Sunset", "Hazy", "Ibiza", "Warehouse", "Afterhours",
                                     "Electric", "Liquid", "Cosmic", "Silk", "Summer", "Moonlit", "Ocean", "Late Night", "Purple",
                                     "Disco", "Sunrise", "Rooftop", "Paradise", "Soul", "Chicago", "Club", "Dawn", "Tropical", "Lunar" };
    static const std::map<int, std::vector<const char*>> nouns = {
        { hk::kCatRealPiano,  { "Grand", "Piano", "Keys" } },
        { hk::kCatHousePiano, { "Piano", "House Piano", "Keys" } },
        { hk::kCatDeepKeys,   { "Keys", "Stab", "Organ Keys" } },
        { hk::kCatRhodes,     { "Rhodes", "EP", "Tines" } },
        { hk::kCatSupersaw,   { "Chords", "Saws", "Supersaw", "Stab" } },
        { hk::kCatSynths,     { "Poly", "Synth", "Chords" } },
        { hk::kCatLeads,      { "Lead", "Saw Lead", "Topline" } },
        { hk::kCatPads,       { "Pad", "Haze", "Atmosphere", "Clouds" } },
        { hk::kCatBass,       { "Bass", "Sub", "Groove Bass" } },
        { hk::kCatPlucks,     { "Pluck", "Plucks", "Arp" } } };
    juce::String first;
    if (adjectives.size() > 0) first = adjectives[r.nextInt (adjectives.size())];
    else first = general[r.nextInt ((int) std::size (general))];
    juce::String second;
    if (r.nextFloat() < 0.35f)
    {
        juce::String g = general[r.nextInt ((int) std::size (general))];
        if (g != first) second = g;
    }
    const auto it = nouns.find (kind);
    const juce::String noun = it != nouns.end() ? it->second[(size_t) r.nextInt ((int) it->second.size())] : "Sound";
    return (first + " " + (second.isNotEmpty() ? second + " " : juce::String()) + noun).trim();
}

// "Fetere: flere stemmer", "La til et bjelle-lag (FM)" -> joined inside a sentence, first letter lower-case
juce::String joinNotes (const juce::StringArray& notes)
{
    juce::StringArray out;
    for (auto n : notes) out.add (n.substring (0, 1).toLowerCase() + n.substring (1));
    return out.joinIntoString (", ");
}

juce::String pick (juce::Random& r, std::initializer_list<const char*> options)
{
    const int i = r.nextInt ((int) options.size());
    return u (*(options.begin() + i));
}

//==============================================================================
int chooseWeighted (const std::vector<std::pair<int, float>>& c, juce::Random& r, int exclude = -1)
{
    float total = 0;
    for (auto& [i, w] : c) if (i != exclude) total += w;
    if (total <= 0) return -1;
    float x = r.nextFloat() * total;
    for (auto& [i, w] : c)
    {
        if (i == exclude) continue;
        if ((x -= w) <= 0) return i;
    }
    for (auto it = c.rbegin(); it != c.rend(); ++it) if (it->first != exclude) return it->first;
    return -1;
}

float tagScore (int sound, const juce::StringArray& w)
{
    float s = 0;
    if (auto it = soundTags().find (sound); it != soundTags().end())
        for (auto* k : it->second) if (contains (w, { k })) s += 1.0f;
    for (auto& nw : words (u (hk::sounds()[(size_t) sound].name))) if (nw.length() >= 3 && contains (w, { nw.toRawUTF8() })) s += 0.5f;
    return s;
}

bool usable (int i) { return i != hk::kDroppedSample && i != hk::kAiSound && ! hk::sounds()[(size_t) i].sampler.user; }

// the categories mentioned ("stab" can be keys or supersaw); "mer bass" is an edit, not a noun
std::vector<int> mentionedKinds (const juce::StringArray& w)
{
    std::vector<int> kinds;
    for (auto& [cat, keys] : categoryNouns())
        for (int i = 0; i < w.size(); ++i)
        {
            bool hit = false;
            for (auto* k : keys) if (matches (w[i], k)) hit = true;
            if (hit && ! (i > 0 && isMoreLess (w[i - 1]))) { kinds.push_back (cat); break; }
        }
    // "piano" alone = house piano, unless it should sound real
    const bool real = contains (w, { "ekte", "real", "realistisk", "konsert", "concert", "klassisk", "classical", "akustisk", "acoustic", "flygel", "grand" });
    if (std::find (kinds.begin(), kinds.end(), hk::kCatRealPiano) != kinds.end() && std::find (kinds.begin(), kinds.end(), hk::kCatHousePiano) != kinds.end())
        kinds.erase (std::find (kinds.begin(), kinds.end(), real ? hk::kCatHousePiano : hk::kCatRealPiano));
    return kinds;
}

const std::map<int, std::vector<int>>& friends()
{
    static const std::map<int, std::vector<int>> f = {
        { hk::kCatRealPiano,  { hk::kCatHousePiano } },
        { hk::kCatHousePiano, { hk::kCatRealPiano, hk::kCatDeepKeys, hk::kCatRhodes } },
        { hk::kCatDeepKeys,   { hk::kCatRhodes, hk::kCatHousePiano } },
        { hk::kCatRhodes,     { hk::kCatDeepKeys, hk::kCatHousePiano } },
        { hk::kCatSupersaw,   { hk::kCatSynths, hk::kCatPlucks, hk::kCatLeads } },
        { hk::kCatSynths,     { hk::kCatSupersaw, hk::kCatPads } },
        { hk::kCatLeads,      { hk::kCatSupersaw } },
        { hk::kCatPads,       { hk::kCatSynths } },
        { hk::kCatBass,       {} },
        { hk::kCatPlucks,     { hk::kCatSupersaw, hk::kCatSynths } } };
    return f;
}

Result generate (const juce::StringArray& w, const juce::String& request, juce::uint32 seed, int forcedParent)
{
    juce::Random r ((juce::int64) seed);
    Result res;
    res.request = request;

    //---- 1. what kind of sound
    int kind = -1;
    if (forcedParent >= 0) kind = hk::sounds()[(size_t) forcedParent].category;
    else
    {
        auto kinds = mentionedKinds (w);
        if (kinds.empty())
        {
            int best = -1; float bestScore = 1.5f;
            for (int i = 0; i < hk::kNumSounds; ++i)
                if (usable (i)) if (float s = tagScore (i, w); s > bestScore) { bestScore = s; best = i; }
            if (best >= 0) kind = hk::sounds()[(size_t) best].category;
            else
            {
                static const int gen[] = { hk::kCatHousePiano, hk::kCatHousePiano, hk::kCatDeepKeys, hk::kCatRhodes, hk::kCatSupersaw,
                                           hk::kCatSynths, hk::kCatLeads, hk::kCatPads, hk::kCatBass, hk::kCatPlucks };
                kind = gen[r.nextInt ((int) std::size (gen))];
            }
        }
        else kind = kinds[(size_t) r.nextInt ((int) kinds.size())];
    }
    res.kind = kind;

    //---- 2. two parents: tags decide who is likely, a friend category sometimes joins in
    std::vector<std::pair<int, float>> pool, friendPool;
    for (int i = 0; i < hk::kNumSounds; ++i)
    {
        if (! usable (i)) continue;
        const int c = hk::sounds()[(size_t) i].category;
        const float wgt = 0.5f + 2.0f * tagScore (i, w);
        if (c == kind) pool.push_back ({ i, wgt });
        else if (auto f = friends().find (kind); f != friends().end() && std::find (f->second.begin(), f->second.end(), c) != f->second.end())
        {
            // mono sounds (leads, bass) only borrow from pure synth sounds
            const auto& sd = hk::sounds()[(size_t) i];
            const bool pureSynth = sd.sampler.level <= 0 && sd.organ.level <= 0 && sd.fm.level <= 0 && sd.keys.level <= 0;
            if (kind != hk::kCatLeads || pureSynth) friendPool.push_back ({ i, wgt });
        }
    }
    int A = forcedParent >= 0 ? forcedParent : chooseWeighted (pool, r);
    if (A < 0) A = hk::kDefaultSound;
    int B = -1;
    if (! friendPool.empty() && (r.nextFloat() < 0.3f || pool.size() < 2)) B = chooseWeighted (friendPool, r);
    if (B < 0) B = chooseWeighted (pool, r, A);
    if (B < 0) B = A;
    res.parentA = A; res.parentB = B; res.levelSound = A;

    const bool experimental = contains (w, { "eksperimentell", "experimental", "rar", "weird", "crazy", "gal", "unik", "unique", "annerledes", "different", "sprett", "vill", "wild" });
    const bool simple = contains (w, { "enkel", "simple", "klassisk", "classic", "ren", "clean", "naturlig", "natural" });
    const float mutation = experimental ? 0.4f : (simple ? 0.08f : 0.18f);

    //---- 3. breed, mutate, spice
    const auto& pa = hk::sounds()[(size_t) A];
    const auto& pb = hk::sounds()[(size_t) B];
    juce::StringArray notes;
    hk::SoundDef d = crossBreed (pa, pb, A == B ? 0.0f : 0.2f + 0.3f * r.nextFloat(), r);
    mutate (d, kind, mutation, r);
    if (r.nextFloat() < (experimental ? 0.7f : (simple ? 0.0f : 0.35f))) spice (d, kind, r, notes);

    //---- 4. shape with the words
    juce::StringArray adjectives;
    designFromWords (w, d, notes, adjectives);
    std::map<juce::String, float> m;
    Builder b { d.preset, m };
    applyControls (w, b, notes);
    writeParams (m, d.preset);
    clampParams (d.preset);
    codec::sanitise (d);
    d.name = ""; d.tag = ""; d.category = kind;
    res.def = d;

    adjectives.removeDuplicates (true);
    res.name = makeName (kind, adjectives, r);
    res.chips = describe (d);
    res.hasSound = true;

    //---- 5. what the AI says
    juce::String s;
    s << pick (r, { "Her er **%N** \xe2\x80\x93 en helt ny %K.", "Ferdig! **%N** er en ny %K designet fra bunnen.",
                    "Jeg har bygget **%N**, en ny %K.", "Sjekk **%N** \xe2\x80\x93 en fersk %K." });
    if (A != B) s << " " << u ("Den er en krysning av ") << u (pa.name) << " og " << u (pb.name);
    else s << " " << u ("Den bygger p\xc3\xa5 ") << u (pa.name);
    s << (notes.isEmpty() ? juce::String (".") : ", " + joinNotes (notes) + ".");
    s << "\n" << pick (r, { "Si ifra om den skal bli fetere, m\xc3\xb8rkere eller luftigere.",
                            "Vil du ha en annen versjon? Trykk \xc2\xabNy variant\xc2\xbb.",
                            "Liker du den? Trykk \xc2\xabLagre\xc2\xbb, s\xc3\xa5 ligger den under Mine lyder.",
                            "Skriv f.eks. \xc2\xabmer pump\xc2\xbb eller \xc2\xabkortere\xc2\xbb for \xc3\xa5 justere den." });
    res.reply = s.replace ("%N", res.name).replace ("%K", u (kindName (kind)));
    return res;
}

//==============================================================================
// FROM SCRATCH: no factory sound involved. Picks the architecture (sources, oscillators, filter type,
// modulation, envelopes, effects) inside musical rules for the role, then lets the words steer.
bool wantsScratch (const juce::StringArray& w, const juce::String& text)
{
    const auto norm = " " + normalise (text) + " ";
    return contains (w, { "bunnen", "scratch", "unik", "unique", "eksperimentell", "experimental", "rar", "weird", "alien",
                          "futuristisk", "futuristic", "ukjent", "sci", "romvesen", "robot", "glitch", "original" })
        || norm.contains (" fra bunn ") || norm.contains (" helt ny ") || norm.contains (" aldri hort ") || norm.contains (" ny synth ") || norm.contains (" egen synth ");
}

enum Role { kRoleChord, kRoleStab, kRolePluck, kRoleLead, kRolePad, kRoleBass, kRoleKeys };

Result generateScratch (const juce::StringArray& w, const juce::String& request, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed);
    auto uni = [&] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto logUni = [&] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto chance = [&] (float p) { return r.nextFloat() < p; };
    const bool wild = contains (w, { "rar", "weird", "alien", "eksperimentell", "experimental", "glitch", "romvesen", "robot", "sci", "futuristisk", "futuristic", "vill", "wild" });

    //---- role
    int role;
    const auto kinds = mentionedKinds (w);
    auto has = [&] (int c) { return std::find (kinds.begin(), kinds.end(), c) != kinds.end(); };
    if (has (hk::kCatBass)) role = kRoleBass;
    else if (has (hk::kCatLeads)) role = kRoleLead;
    else if (has (hk::kCatPlucks)) role = kRolePluck;
    else if (has (hk::kCatPads)) role = kRolePad;
    else if (contains (w, { "stab", "stabs", "kort", "short" })) role = kRoleStab;
    else if (has (hk::kCatRealPiano) || has (hk::kCatHousePiano) || has (hk::kCatDeepKeys) || has (hk::kCatRhodes)) role = kRoleKeys;
    else if (has (hk::kCatSupersaw) || has (hk::kCatSynths)) role = kRoleChord;
    else { static const int roles[] = { kRoleChord, kRoleChord, kRoleStab, kRolePluck, kRolePluck, kRoleLead, kRolePad, kRoleBass, kRoleKeys };
           role = roles[r.nextInt ((int) std::size (roles))]; }

    static const int kindOf[] = { hk::kCatSynths, hk::kCatSupersaw, hk::kCatPlucks, hk::kCatLeads, hk::kCatPads, hk::kCatBass, hk::kCatDeepKeys };
    static const int refOf[]  = { hk::kPolySynth, hk::kSupersawStab, hk::kHousePluck, hk::kSawLead, hk::kWarmPad, hk::kHouseBass, hk::kDeepHouseKeys };
    const int kind = kindOf[role];
    const bool mono = role == kRoleBass || (role == kRoleLead && chance (0.85f));
    const bool chordy = ! mono;

    hk::SoundDef d;
    d.name = ""; d.tag = ""; d.category = kind;
    juce::StringArray parts;   // what was built, in Norwegian

    //---- oscillators (the core of almost every design)
    auto& va = d.va;
    const bool useVa = ! (role == kRoleKeys && chance (0.3f)) && ! (role == kRolePluck && chance (0.25f));
    if (useVa)
    {
        static const char* waveNo[] = { "sagtann", "firkant", "trekant", "sinus" };
        auto pickWave = [&] { const float x = r.nextFloat(); return x < 0.38f ? hk::kSaw : x < 0.62f ? hk::kSquare : x < 0.82f ? hk::kTri : hk::kSine; };
        va.level = 1.0f;
        va.wave1 = pickWave();
        va.wave2 = pickWave();
        static const float safe[] = { 0.0f, 12.0f, -12.0f, 24.0f };
        static const float any[]  = { 0.0f, 12.0f, -12.0f, 7.0f, 19.0f, 5.0f, 24.0f, -5.0f };
        va.semi2 = (chordy && ! wild) ? safe[r.nextInt (4)] : any[r.nextInt (8)];
        va.fine2 = uni (-14.0f, 14.0f);
        va.mix2 = chance (0.8f) ? uni (0.25f, 1.0f) : 0.0f;
        const int maxUni = role == kRoleBass ? 3 : 7;
        va.unison = role == kRolePad || role == kRoleChord ? juce::jlimit (1, 7, 3 + r.nextInt (5)) : 1 + r.nextInt (maxUni);
        va.detune = va.unison > 1 ? logUni (6.0f, role == kRoleBass ? 18.0f : 45.0f) : 0.0f;
        va.spread = va.unison > 1 ? uni (0.4f, 1.0f) : 0.0f;
        va.sub = role == kRoleBass ? uni (0.35f, 0.85f) : (chance (0.25f) ? uni (0.1f, 0.35f) : 0.0f);
        va.noise = chance (wild ? 0.4f : 0.18f) ? uni (0.02f, 0.1f) : 0.0f;
        va.velSens = uni (0.3f, 0.6f);
        va.keytrack = uni (0.3f, 0.8f);
        va.velAmt = uni (0.2f, 0.8f);
        va.drift = uni (0.0f, 6.0f);
        va.motion = chance (0.4f) ? uni (0.05f, 0.35f) : 0.0f;
        va.poles = chance (0.6f) ? 4 : 2;

        juce::String osc = juce::String (va.unison > 1 ? juce::String (va.unison) + " " : juce::String()) + u (waveNo[va.wave1]);
        if (va.mix2 > 0)
        {
            const float st = va.semi2;
            juce::String iv = st == 0 ? u ("") : st == 12 ? u (" en oktav opp") : st == -12 ? u (" en oktav ned") : st == 24 ? u (" to oktaver opp")
                            : st == 7 || st == 19 ? u (" en kvint opp") : st == 5 ? u (" en kvart opp") : u (" en kvart ned");
            osc << " + " << u (waveNo[va.wave2]) << iv;
        }
        parts.add (osc);

        // filter
        switch (role)
        {
            case kRoleBass:  va.cutoff = logUni (150.0f, 1200.0f); va.envAmt = uni (0.5f, 3.0f); va.fD = logUni (80.0f, 600.0f); va.fS = uni (0.0f, 0.4f); break;
            case kRolePluck: va.cutoff = logUni (250.0f, 2500.0f); va.envAmt = uni (2.5f, 5.0f); va.fD = logUni (80.0f, 400.0f); va.fS = uni (0.0f, 0.15f); break;
            case kRoleStab:  va.cutoff = logUni (500.0f, 4000.0f); va.envAmt = uni (1.0f, 3.5f); va.fD = logUni (100.0f, 500.0f); va.fS = uni (0.1f, 0.4f); break;
            case kRoleLead:  va.cutoff = logUni (900.0f, 8000.0f); va.envAmt = uni (0.0f, 2.5f); va.fD = logUni (150.0f, 900.0f); va.fS = uni (0.3f, 0.8f); break;
            case kRolePad:   va.cutoff = logUni (400.0f, 4000.0f); va.envAmt = uni (-0.5f, 1.5f); va.fA = logUni (5.0f, 1500.0f); va.fD = logUni (400.0f, 3000.0f); va.fS = uni (0.3f, 0.9f); break;
            default:         va.cutoff = logUni (600.0f, 5000.0f); va.envAmt = uni (0.0f, 2.5f); va.fD = logUni (200.0f, 1200.0f); va.fS = uni (0.3f, 0.8f); break;
        }
        va.reso = chance (0.12f) ? uni (0.65f, 0.85f) : uni (0.0f, 0.45f);
        va.fR = logUni (100.0f, 800.0f);
        if (role != kRoleBass && chance (wild ? 0.3f : 0.12f)) { va.fType = 1; va.cutoff *= 1.6f; parts.add (u ("bandpass-filter")); }
        else if (role != kRoleBass && chance (0.05f)) { va.fType = 2; va.cutoff = logUni (150.0f, 600.0f); parts.add (u ("highpass-filter")); }

        // amp envelope
        switch (role)
        {
            case kRolePluck: va.decayMs = logUni (140.0f, 600.0f); va.sustain = 0.0f; break;
            case kRoleStab:  va.decayMs = logUni (250.0f, 900.0f); va.sustain = uni (0.0f, 0.25f); break;
            case kRoleBass:  if (chance (0.5f)) { va.decayMs = logUni (250.0f, 900.0f); va.sustain = uni (0.2f, 0.7f); } break;
            case kRoleKeys:  va.decayMs = logUni (500.0f, 2500.0f); va.sustain = uni (0.1f, 0.5f); break;
            case kRoleChord: va.decayMs = logUni (600.0f, 2500.0f); va.sustain = uni (0.4f, 1.0f); break;
            default: break;
        }

        // character blocks
        if (chance (wild ? 0.55f : 0.25f)) { va.fold = uni (0.15f, wild ? 0.9f : 0.6f); parts.add ("wavefolder"); }
        if (va.mix2 > 0 && chance (wild ? 0.45f : 0.15f)) { va.ring = uni (0.3f, 1.0f); parts.add (u ("ring-modulasjon")); }
        if (chance (wild ? 0.4f : 0.16f)) { va.formant = uni (0.4f, 0.9f); va.vowel = r.nextFloat(); parts.add ("vokalfilter"); }
        if (chance (wild ? 0.6f : 0.35f))
        {
            static const char* divNo[] = { "", "1/2", "1/4", "1/8", "1/16", "1/8T" };
            if (chance (0.7f)) va.lfoDiv = 1 + r.nextInt (5); else va.lfoRate = logUni (0.2f, 7.0f);
            juce::StringArray tg;
            const float x = r.nextFloat();
            if (x < 0.45f || role == kRoleBass) { va.lfoCut = uni (0.6f, 2.5f); tg.add ("filter"); }
            else if (x < 0.65f && chordy) { va.lfoAmp = uni (0.25f, 0.6f); tg.add ("volum"); }
            else if (x < 0.8f && va.formant > 0) { va.lfoVowel = uni (0.4f, 1.0f); tg.add ("vokal"); }
            else { va.lfoPitch = uni (6.0f, 25.0f); tg.add ("tonehøyde"); }
            if (va.formant > 0 && va.lfoVowel <= 0 && chance (0.5f)) { va.lfoVowel = uni (0.3f, 0.9f); tg.add ("vokal"); }
            parts.add (u ("LFO ") + (va.lfoDiv > 0 ? u ("i takt (") + divNo[va.lfoDiv] + ")" : u ("fri")) + u (" p\xc3\xa5 ") + u (tg.joinIntoString (" og ").toRawUTF8()));
        }
        if ((role == kRoleBass && chance (0.35f)) || (role == kRolePluck && chance (0.25f)) || (role == kRoleLead && chance (0.2f)) || (wild && chance (0.3f)))
        {
            va.pEnvSemi = (chance (0.8f) ? 1.0f : -1.0f) * (chance (0.5f) ? 12.0f : uni (5.0f, 24.0f));
            va.pEnvMs = logUni (10.0f, wild ? 400.0f : 90.0f);
            parts.add (va.pEnvSemi > 0 ? u ("pitch-zap p\xc3\xa5 starten") : u ("pitch-l\xc3\xb8" "ft p\xc3\xa5 starten"));
        }
    }

    //---- other sources
    const bool keysWords = contains (w, { "piano", "flygel", "klaver" });
    if (keysWords || (role == kRoleKeys && chance (0.35f)) || chance (0.04f))
    {
        d.sampler = hk::sounds()[(size_t) hk::kFreshHouse].sampler;
        d.sampler.level = useVa ? uni (0.4f, 0.8f) : 1.0f;
        d.sampler.octave = chance (0.4f) ? uni (0.2f, 0.5f) : 0.0f;
        d.fallback = hk::sounds()[(size_t) hk::kFreshHouse].fallback; d.fallback.level = d.sampler.level;
        parts.add (u ("ekte piano"));
    }
    if (! useVa || chance (role == kRoleKeys ? 0.5f : (role == kRoleBass ? 0.1f : 0.28f)))
    {
        static const float ra[] = { 0.5f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f };
        static const float rb[] = { 1.0f, 2.0f, 3.5f, 5.0f, 7.0f, 9.0f, 14.0f };
        auto& fm = d.fm;
        fm.level = useVa ? uni (0.2f, 0.55f) : uni (0.7f, 1.0f);
        fm.ratioA = ra[r.nextInt (6)]; fm.indexA = logUni (0.4f, wild ? 6.0f : 3.0f); fm.susA = uni (0.05f, 0.5f); fm.decA = logUni (0.1f, 2.0f);
        fm.octB = chance (0.5f) ? 1.0f : 2.0f; fm.ratioB = rb[r.nextInt (7)]; fm.indexB = logUni (0.3f, 3.0f); fm.decB = logUni (0.05f, 1.2f);
        fm.mixB = uni (0.1f, 0.6f); fm.feedback = chance (0.3f) ? uni (0.1f, 0.6f) : 0.0f;
        fm.decay = role == kRolePluck || role == kRoleStab ? logUni (0.3f, 1.5f) : logUni (1.0f, 5.0f);
        fm.sustain = role == kRolePad || role == kRoleLead ? uni (0.3f, 0.8f) : 0.0f;
        fm.bark = chance (0.3f) ? uni (0.2f, 0.7f) : 0.0f;
        parts.add (u ("FM-lag (") + juce::String (fm.ratioA, 1).trimCharactersAtEnd (".0") + ":" + juce::String (fm.ratioB, 1).trimCharactersAtEnd (".0") + ")");
    }
    if ((! useVa && d.fm.level <= 0) || chance (role == kRolePluck ? 0.45f : (role == kRoleBass ? 0.08f : 0.2f)))
    {
        auto& st = d.str;
        st.level = useVa ? uni (0.3f, 0.7f) : uni (0.8f, 1.0f);
        st.damp = uni (0.2f, 0.95f); st.pick = uni (0.3f, 1.0f);
        st.decay = role == kRolePad || role == kRoleChord ? logUni (1.0f, 5.0f) : logUni (0.3f, 2.5f);
        parts.add (u ("plukket streng"));
    }
    if (chance (role == kRoleKeys ? 0.3f : 0.07f))
    {
        d.organ.level = uni (0.2f, 0.5f);
        for (auto& b : d.organ.bars) b = chance (0.55f) ? uni (0.2f, 1.0f) : 0.0f;
        d.organ.bars[1] = std::max (d.organ.bars[1], 0.6f);
        d.organ.decay = role == kRolePad ? 0.0f : uni (0.5f, 3.0f); d.organ.click = uni (0.0f, 0.4f);
        parts.add (u ("orgel"));
    }
    if (d.va.level <= 0 && d.fm.level <= 0 && d.str.level <= 0 && d.sampler.level <= 0 && d.organ.level <= 0)
    { d.str.level = 1.0f; parts.add (u ("plukket streng")); }

    //---- play style and colour
    d.mono = mono;
    d.glideMs = mono ? (role == kRoleLead ? uni (25.0f, 120.0f) : (chance (0.4f) ? uni (20.0f, 70.0f) : 0.0f)) : 0.0f;
    d.lowCutHz = role == kRoleBass ? 20.0f : logUni (60.0f, 220.0f);
    d.lowDb = uni (-2.0f, 2.5f); d.highDb = uni (-3.0f, 2.5f);
    d.drive = chance (0.4f) ? uni (0.1f, 0.5f) : 0.0f;
    d.glue = uni (0.2f, 0.6f);
    d.tape = chance (0.2f) ? uni (0.2f, 0.6f) : 0.0f;
    d.gainDb = 0.0f;

    auto& p = d.preset;
    p = hk::Params {};
    p.air = uni (0.2f, 0.65f);
    p.attackMs = role == kRolePad ? logUni (60.0f, 700.0f) : uni (0.0f, 6.0f);
    p.releaseS = role == kRolePad ? uni (1.0f, 3.0f) : role == kRolePluck ? uni (0.2f, 0.5f) : role == kRoleBass ? uni (0.08f, 0.2f)
               : role == kRoleLead ? uni (0.15f, 0.4f) : uni (0.3f, 0.8f);
    p.width = role == kRoleBass ? uni (0.3f, 0.5f) : uni (0.5f, 0.9f);
    p.reverbOn = chance (role == kRoleBass ? 0.1f : role == kRolePad || role == kRolePluck ? 0.95f : 0.75f);
    p.reverbMix = uni (0.12f, 0.35f); p.reverbSize = uni (0.3f, 0.85f);
    p.delayOn = role != kRoleBass && chance (role == kRolePluck || role == kRoleLead ? 0.45f : 0.2f);
    p.delayTime = 1 + r.nextInt (3); p.delayMix = uni (0.1f, 0.22f); p.delayFeedback = uni (0.25f, 0.45f);
    p.chorusOn = chance (0.25f); p.chorusMix = uni (0.15f, 0.4f);
    p.pumpOn = (role == kRoleChord || role == kRolePad || role == kRoleStab) && chance (0.35f); p.pumpDepth = uni (0.35f, 0.7f);
    p.vibOn = role == kRoleLead && chance (0.2f); p.vibDepth = uni (0.15f, 0.35f);
    p.chord = hk::kChordOff;
    clampParams (p);

    //---- the words steer the result
    juce::StringArray notes, adjectives;
    designFromWords (w, d, notes, adjectives);
    std::map<juce::String, float> m;
    Builder b { d.preset, m };
    applyControls (w, b, notes);
    writeParams (m, d.preset);
    clampParams (d.preset);
    codec::sanitise (d);
    d.name = ""; d.tag = ""; d.category = kind;

    Result res;
    res.def = d;
    res.kind = kind;
    res.request = request;
    res.levelSound = refOf[role];
    res.hasSound = true;
    res.scratch = true;
    res.chips = describe (d);

    static const char* future[] = { "Quantum", "Prism", "Nova", "Glitch", "Chrome", "Plasma", "Vapor", "Hyper", "Orbit", "Pulse",
                                    "Mirage", "Flux", "Aurora", "Nebula", "Laser", "Crystal", "Static", "Halo", "Zero", "Spectra" };
    juce::StringArray adj = adjectives;
    if (adj.isEmpty() || chance (0.5f)) adj.add (future[r.nextInt ((int) std::size (future))]);
    res.name = makeName (kind, adj, r);

    static const char* roleNo[] = { "akkord-synth", "stab", "pluck", "lead", "pad", "bass", "keys" };
    juce::String sTxt;
    sTxt << u ("Her er **") << res.name << u ("** \xe2\x80\x93 en helt ny ") << u (roleNo[role])
         << u (" bygget fra bunnen, uten noen ferdig lyd som utgangspunkt: ") << parts.joinIntoString (", ") << ".";
    if (! notes.isEmpty()) sTxt << " " << u ("Tilpasset ordene dine: ") << joinNotes (notes) << ".";
    sTxt << "\n" << pick (r, { "Trykk \xc2\xabNy variant\xc2\xbb for en helt annen oppskrift.",
                                "Hver gang blir den forskjellig \xe2\x80\x93 trykk \xc2\xabNy variant\xc2\xbb for \xc3\xa5 lete videre.",
                                "Skriv f.eks. \xc2\xab""fetere\xc2\xbb, \xc2\xabm\xc3\xb8rkere\xc2\xbb eller \xc2\xabmer pump\xc2\xbb for \xc3\xa5 justere." });
    res.reply = sTxt;
    return res;
}

// "kall den Sommer Keys" -> "Sommer Keys"
juce::String renameTarget (const juce::String& text)
{
    const auto lower = text.toLowerCase();
    for (auto* key : { "kall den ", "kall lyden ", "navnet ", "navn: ", "navn ", "name it ", "call it ", "rename to ", "gi den navnet " })
    {
        const int i = lower.indexOf (u (key));
        if (i >= 0 && (i == 0 || lower.startsWith ("gi ") || lower.startsWith ("kan du")))
        {
            auto n = text.substring (i + (int) std::strlen (key)).trim();
            n = n.removeCharacters (u ("\"\xc2\xab\xc2\xbb")).trim();
            while (n.endsWithChar ('.') || n.endsWithChar ('!')) n = n.dropLastCharacters (1);
            return n.substring (0, 40).trim();
        }
    }
    return {};
}
} // namespace

//==============================================================================
bool clampParam (const juce::String& id, float& v)
{
    static const std::map<juce::String, std::pair<float, float>> ranges = {
        { "tone", { 0, 1 } }, { "air", { 0, 1 } }, { "attack", { 0, 2000 } }, { "decay", { 0, 1 } },
        { "release", { 0.01f, 6 } }, { "width", { 0, 1 } }, { "volume", { -24, 6 } }, { "chord", { 0, 6 } },
        { "filterOn", { 0, 1 } }, { "cutoff", { 80, 18000 } }, { "reso", { 0, 1 } },
        { "satOn", { 0, 1 } }, { "satDrive", { 0, 1 } }, { "chorusOn", { 0, 1 } }, { "chorusMix", { 0, 1 } },
        { "delayOn", { 0, 1 } }, { "delayTime", { 0, 5 } }, { "delayFb", { 0, 0.9f } }, { "delayMix", { 0, 1 } },
        { "reverbOn", { 0, 1 } }, { "reverbSize", { 0, 1 } }, { "reverbMix", { 0, 1 } },
        { "pumpOn", { 0, 1 } }, { "pumpRate", { 0, 2 } }, { "pumpDepth", { 0, 1 } },
        { "vibOn", { 0, 1 } }, { "vibRate", { 0.5f, 12 } }, { "vibDepth", { 0, 1 } } };
    auto it = ranges.find (id);
    if (it == ranges.end()) return false;
    v = juce::jlimit (it->second.first, it->second.second, v);
    return true;
}

juce::StringArray describe (const hk::SoundDef& d)
{
    juce::StringArray c;
    if (d.sampler.level > 0) c.add (u ("Ekte piano"));
    if (d.sampler.level > 0 && d.sampler.octave > 0.15f) c.add (u ("Oktav-lag"));
    if (d.keys.level > 0) c.add (u ("House-keys"));
    if (d.organ.level > 0) c.add (u ("Orgel"));
    if (d.fm.level > 0) c.add (d.fm.ratioA <= 1.5f && d.fm.ratioB > 5.0f ? u ("E-piano (FM)") : u ("Bjelle (FM)"));
    if (d.va.level > 0)
    {
        static const char* waves[] = { "Saw", "Square", "Triangle", "Sinus" };
        if (d.va.wave1 == hk::kSaw && d.va.unison >= 5) c.add (u ("Supersaw \xc3\x97") + juce::String (d.va.unison));
        else c.add (u (waves[juce::jlimit (0, 3, d.va.wave1)]) + (d.va.unison > 1 ? u (" \xc3\x97") + juce::String (d.va.unison) : juce::String()));
        if (d.va.sub > 0.2f) c.add ("Sub");
        if (d.va.reso > 0.6f) c.add (u ("Acid-filter"));
        if (d.va.motion > 0.2f) c.add (u ("Bevegelse"));
        if (d.va.noise > 0.05f) c.add (u ("St\xc3\xb8y"));
        if (d.va.fold > 0.05f) c.add ("Wavefolder");
        if (d.va.ring > 0.05f) c.add ("Ring-mod");
        if (d.va.fType == 1) c.add ("Bandpass");
        if (d.va.fType == 2) c.add ("Highpass");
        if (d.va.formant > 0.05f) c.add ("Vokalfilter");
        if (d.va.lfoDiv > 0 || d.va.lfoRate > 0)
        {
            static const char* divs[] = { "", "1/2", "1/4", "1/8", "1/16", "1/8T" };
            if (d.va.lfoCut > 0 || d.va.lfoAmp > 0 || d.va.lfoPitch > 0 || d.va.lfoVowel > 0)
                c.add (d.va.lfoDiv > 0 ? u ("LFO ") + divs[juce::jlimit (1, 5, d.va.lfoDiv)] : u ("LFO"));
        }
        if (d.va.pEnvSemi != 0) c.add (d.va.pEnvSemi > 0 ? "Pitch-zap" : u ("Pitch-l\xc3\xb8" "ft"));
    }
    if (d.str.level > 0) c.add ("Streng");
    if (d.mono) c.add (d.glideMs > 0 ? u ("Mono + glide") : u ("Mono"));
    if (d.tape > 0.3f) c.add ("Tape");
    if (d.drive > 0.35f || (d.preset.satOn && d.preset.satDrive > 0.2f)) c.add ("Drive");
    const auto& p = d.preset;
    if (p.filterOn) c.add ("Filter");
    if (p.chorusOn) c.add ("Chorus");
    if (p.vibOn) c.add ("Vibrato");
    if (p.delayOn) c.add ("Delay");
    if (p.reverbOn) c.add ("Reverb");
    if (p.pumpOn) c.add ("Pump");
    if (p.air > 0.6f) c.add ("Air");
    return c;
}

Result respond (const juce::String& text, const Context* current, juce::uint32 seed)
{
    Result res;
    const auto w = words (text);
    juce::Random r ((juce::int64) seed);

    if (w.isEmpty()) { res.reply = u ("Skriv hva slags lyd du vil ha, s\xc3\xa5 lager jeg den."); return res; }

    // small talk
    const bool greeting = w.size() <= 3 && contains (w, { "hei", "hallo", "hello", "hi", "heisann", "yo", "hey" });
    if (greeting && mentionedKinds (w).empty())
    {
        res.reply = u ("Hei! Beskriv en lyd, f.eks. \xc2\xab""fet deep house piano med pump\xc2\xbb, s\xc3\xa5 designer jeg en helt ny preset til deg.");
        return res;
    }
    if (w.size() <= 3 && contains (w, { "takk", "thanks", "tusen", "nice", "digg", "kult", "bra", "perfekt", "perfect" }))
    {
        res.reply = pick (r, { "Bare hyggelig! Trykk **Lagre** hvis du vil beholde den.", "S\xc3\xa5 bra! Vil du ha en til, skriv bare hva du \xc3\xb8nsker deg.",
                               "Digg! Jeg kan lage flere n\xc3\xa5r du vil." });
        return res;
    }
    if (contains (w, { "hjelp", "help" }) || normalise (text).contains ("hva kan du"))
    {
        res.reply = u ("Jeg lager **helt nye lyder** ut fra det du skriver, gratis og uten internett.\n"
                       "Skriv type lyd (piano, keys, rhodes, supersaw, synth, lead, pad, bass, pluck) og hvordan den skal l\xc3\xa5te "
                       "(fet, m\xc3\xb8rk, lys, luftig, bred, lo-fi, kort, med reverb, delay, pump \xe2\x80\xa6).\n"
                       "Etterp\xc3\xa5 kan du skrive \xc2\xab""fetere\xc2\xbb, \xc2\xabmer luft\xc2\xbb, \xc2\xabuten reverb\xc2\xbb, \xc2\xabny variant\xc2\xbb eller \xc2\xabkall den Sommer Keys\xc2\xbb.");
        return res;
    }

    if (current != nullptr)
    {
        // rename
        if (auto n = renameTarget (text); n.isNotEmpty())
        {
            res.renamed = true; res.hasSound = true;
            res.def = current->def; res.name = n; res.kind = current->kind; res.request = current->request;
            res.chips = describe (res.def);
            res.reply = u ("N\xc3\xa5 heter den **") + n + "**.";
            return res;
        }

        // new variant of the same idea
        const auto norm = " " + normalise (text) + " ";
        const bool variant = contains (w, { "variant", "versjon", "version", "another", "annen", "igjen", "again" })
                             || norm.contains (" en til ") || norm.contains (" one more ");
        bool asksNew = ! mentionedKinds (w).empty() || contains (w, { "overrask", "surprise", "random", "tilfeldig" }) || wantsScratch (w, text);
        for (int i = 0; i < w.size(); ++i)
            if (w[i] == "lag" || w[i] == "make" || w[i] == "create" || w[i] == "ny" || w[i] == "new")
                if (! (i + 1 < w.size() && (w[i + 1] == "den" || w[i + 1] == "det" || w[i + 1] == "it" || w[i + 1] == "lyden"
                                            || w[i + 1] == "denne" || w[i + 1] == "this" || w[i + 1] == "variant" || w[i + 1] == "versjon")))
                    asksNew = true;
        if (variant && ! asksNew)
        {
            int forced = -1;
            if (current->isFactory)
                for (int i = 0; i < hk::kNumSounds; ++i) if (current->name == u (hk::sounds()[(size_t) i].name)) forced = i;
            auto req = current->request.isNotEmpty() ? current->request : current->name;
            if (wantsScratch (words (req), req)) { auto v = generateScratch (words (req), req, seed); v.reply = v.reply.fromFirstOccurrenceOf ("**", true, false); res = v; }
            else
            {
                res = generate (words (req), req, seed, forced);
                res.name = makeName (res.kind, {}, r);   // a variant gets its own name
            }
            res.reply = pick (r, { "Her er en ny variant: **%N**.", "Ny vri: **%N**.", "Pr\xc3\xb8v denne: **%N**." }).replace ("%N", res.name)
                        + " " + res.reply.fromFirstOccurrenceOf ("\n", false, false);
            return res;
        }

        // edit the current sound
        if (! asksNew)
        {
            hk::SoundDef d = current->def;
            juce::StringArray notes, adjectives;
            designFromWords (w, d, notes, adjectives);
            std::map<juce::String, float> m;
            Builder b { d.preset, m };
            applyControls (w, b, notes);
            notes.removeDuplicates (true);
            if (! notes.isEmpty())
            {
                writeParams (m, d.preset);
                clampParams (d.preset);
                codec::sanitise (d);
                d.name = ""; d.tag = "";
                res.hasSound = true;
                res.def = d;
                res.kind = current->kind >= 0 ? current->kind : d.category;
                res.request = current->request.isNotEmpty() ? current->request + " " + text : text;
                res.name = current->isFactory ? makeName (res.kind, adjectives, r) : current->name;
                res.chips = describe (d);
                res.levelSound = -1;   // keep the loudness of the version before
                res.reply = (current->isFactory ? u ("Jeg tok **") + current->name + u ("** og laget en ny lyd av den: **") + res.name + "**. "
                                                : u ("Oppdatert **") + res.name + u ("** \xe2\x80\x93 "))
                            + joinNotes (notes) + ".";
                return res;
            }
            if (w.size() <= 2)
            {
                res.reply = u ("Den skj\xc3\xb8nte jeg ikke helt. Pr\xc3\xb8v f.eks. \xc2\xab""fetere\xc2\xbb, \xc2\xabm\xc3\xb8rkere\xc2\xbb, \xc2\xabmer luft\xc2\xbb, "
                               "\xc2\xabmer reverb\xc2\xbb, \xc2\xabkortere\xc2\xbb, \xc2\xabny variant\xc2\xbb \xe2\x80\x93 eller beskriv en ny lyd.");
                return res;
            }
        }
    }
    if (wantsScratch (w, text) || (contains (w, { "overrask", "surprise", "random", "tilfeldig" }) && r.nextFloat() < 0.6f))
        return generateScratch (w, text.trim(), seed);
    return generate (w, text.trim(), seed, -1);
}

//==============================================================================
const std::vector<PreviewNote>& previewPattern (int kind, bool mono)
{
    static const std::vector<PreviewNote> chords = {
        { 0.00f, 0.55f, 45, 0.85f }, { 0.00f, 0.55f, 57, 0.8f }, { 0.00f, 0.55f, 60, 0.8f }, { 0.00f, 0.55f, 64, 0.8f }, { 0.00f, 0.55f, 67, 0.8f },
        { 0.75f, 1.05f, 57, 0.75f }, { 0.75f, 1.05f, 60, 0.75f }, { 0.75f, 1.05f, 64, 0.75f }, { 0.75f, 1.05f, 67, 0.75f },
        { 1.25f, 2.30f, 41, 0.85f }, { 1.25f, 2.30f, 57, 0.8f }, { 1.25f, 2.30f, 60, 0.8f }, { 1.25f, 2.30f, 64, 0.8f }, { 1.25f, 2.30f, 69, 0.8f } };
    static const std::vector<PreviewNote> bass = {
        { 0.25f, 0.45f, 33, 0.95f }, { 0.75f, 0.95f, 33, 0.85f }, { 1.25f, 1.45f, 33, 0.9f }, { 1.625f, 1.75f, 45, 0.8f }, { 1.75f, 2.10f, 31, 0.9f } };
    static const std::vector<PreviewNote> lead = {
        { 0.00f, 0.42f, 69, 0.85f }, { 0.375f, 0.80f, 72, 0.85f }, { 0.75f, 1.17f, 74, 0.85f }, { 1.125f, 2.20f, 76, 0.9f } };
    static const std::vector<PreviewNote> pad = {
        { 0.0f, 2.6f, 57, 0.75f }, { 0.0f, 2.6f, 60, 0.75f }, { 0.0f, 2.6f, 64, 0.75f }, { 0.0f, 2.6f, 67, 0.75f } };
    static const std::vector<PreviewNote> pluck = [] {
        std::vector<PreviewNote> v;
        const int arp[] = { 57, 60, 64, 67, 72, 67, 64, 60 };
        for (int i = 0; i < 16; ++i) v.push_back ({ i * 0.14f, i * 0.14f + 0.11f, arp[i % 8] + (i >= 8 ? 2 : 0), i % 4 == 0 ? 0.95f : 0.75f });
        return v; } ();
    if (kind == hk::kCatBass) return bass;
    if (kind == hk::kCatLeads || mono) return lead;
    if (kind == hk::kCatPads) return pad;
    if (kind == hk::kCatPlucks) return pluck;
    return chords;
}

float previewLength (int kind, bool mono)
{
    float t = 0;
    for (auto& n : previewPattern (kind, mono)) t = std::max (t, n.t1);
    return t;
}

static double measureDb (const hk::SoundDef& def, int kind, const hk::SampleBank* bank)
{
    constexpr int sr = 44100, blockSize = 256;
    hk::SoundDef d = def;
    hk::Params p = d.preset;
    p.sound = hk::kAiSound; p.volumeDb = -3.0f; p.chord = hk::kChordOff;
    auto eng = std::make_unique<hk::Engine>();
    eng->setCustomSound (&d);
    eng->setSampleBank (bank);
    eng->setParams (p);
    eng->prepare (sr, blockSize);

    const auto& pat = previewPattern (kind, d.mono);
    const int total = (int) ((previewLength (kind, d.mono) + 0.2f) * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    double sum = 0;
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min (blockSize, total - pos);
        for (auto& e : pat)
        {
            const int off = (int) (e.t1 * sr);
            if (off >= pos && off < pos + n) eng->noteOff (e.note);
        }
        for (auto& e : pat)
        {
            const int on = (int) (e.t0 * sr);
            if (on >= pos && on < pos + n) eng->noteOn (e.note, e.vel);
        }
        eng->process (L.data() + pos, R.data() + pos, n, 124.0, pos * 124.0 / 60.0 / sr, true);
    }
    // K-weighting (ITU BS.1770: high shelf +4 dB around 1.7 kHz, high-pass at 38 Hz) so the result is LUFS-like
    struct Biquad
    {
        double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    auto shelf = [] (double fs, double f0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cw = std::cos (w), al = std::sin (w) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) - (A - 1) * cw + sA;
        return Biquad { A * ((A + 1) + (A - 1) * cw + sA) / a0, -2 * A * ((A - 1) + (A + 1) * cw) / a0,
                        A * ((A + 1) + (A - 1) * cw - sA) / a0, 2 * ((A - 1) - (A + 1) * cw) / a0, ((A + 1) - (A - 1) * cw - sA) / a0 };
    };
    auto highpass = [] (double fs, double f0, double q)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f0 / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1 + al;
        return Biquad { (1 + cw) / 2 / a0, -(1 + cw) / a0, (1 + cw) / 2 / a0, -2 * cw / a0, (1 - al) / a0 };
    };
    Biquad sl = shelf (sr, 1681.97, 4.0, 0.7072), sr2 = sl, hl = highpass (sr, 38.135, 0.5003), hr = hl;
    for (int i = 0; i < total; ++i)
    {
        const double l = hl.run (sl.run (L[(size_t) i])), rr = hr.run (sr2.run (R[(size_t) i]));
        sum += l * l + rr * rr;
    }
    return 10.0 * std::log10 (sum / total + 1e-12);
}

void autoLevel (Result& r, const hk::SampleBank* bank, const Context* previous)
{
    if (! r.hasSound || r.renamed) return;
    hk::SoundDef ref;
    if (r.levelSound >= 0) ref = hk::sounds()[(size_t) r.levelSound];
    else if (previous != nullptr) ref = previous->def;
    else return;
    const double refDb = measureDb (ref, r.kind, bank);
    for (int iter = 0; iter < 2; ++iter)
    {
        const double diff = refDb - measureDb (r.def, r.kind, bank);
        if (std::abs (diff) < 0.4) break;
        r.def.gainDb = (float) juce::jlimit (-24.0, 12.0, r.def.gainDb + diff);
    }
}

juce::StringArray examples()
{
    return { u ("fet house piano med mye luft og pump"),
             u ("m\xc3\xb8rk lo-fi rhodes med delay"),
             u ("bred euforisk supersaw"),
             u ("tung acid bass"),
             u ("dr\xc3\xb8mmende pad med bjelle"),
             u ("helt ny synth fra bunnen"),
             u ("overrask meg") };
}

juce::StringArray followUps()
{
    return { u ("Ny variant"), u ("\xe2\x9c\xa6 Fra bunnen"), u ("Fetere"), u ("M\xc3\xb8rkere"), u ("Lysere"), u ("Mer luft"), u ("Mer pump"), u ("Kortere"), u ("Mer reverb") };
}
} // namespace PresetAI
