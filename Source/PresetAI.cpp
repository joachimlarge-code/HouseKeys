#include "PresetAI.h"
#include <juce_events/juce_events.h>
#include "Sounds.h"

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

const std::map<int, std::vector<const char*>>& categoryNouns()
{
    static const std::map<int, std::vector<const char*>> m = {
        { hk::kCatRealPiano,  { "piano", "flygel", "grand", "klaver" } },
        { hk::kCatHousePiano, { "piano", "flygel", "klaver" } },
        { hk::kCatDeepKeys,   { "keys", "orgel", "organ", "stab" } },
        { hk::kCatRhodes,     { "rhodes", "epiano", "elpiano", "wurli", "electric" } },
        { hk::kCatSupersaw,   { "supersaw", "chord", "chords", "akkord", "akkorder", "stab", "trance" } },
        { hk::kCatSynths,     { "synth", "synther", "poly", "brass", "juno", "strings", "strykere" } },
        { hk::kCatLeads,      { "lead", "leads", "melodi", "melody", "sagtann" } },
        { hk::kCatPads,       { "pad", "pads", "flate", "ambient", "atmosfaere", "bakgrunn" } },
        { hk::kCatBass,       { "bass", "basslinje", "bassline", "sub", "808" } },
        { hk::kCatPlucks,     { "pluck", "plucks", "plukk" } },
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
        { hk::kSupersawChords,   { "chords" } },
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
        if (contains (juce::StringArray (w[i]), { "litt", "slightly", "bit", "lite", "subtle", "subtil" })) return 0.5f;
        if (contains (juce::StringArray (w[i]), { "mye", "veldig", "very", "masse", "ekstra", "extra", "super", "mega", "mer", "more", "much" })) return 1.6f;
    }
    return 1.0f;
}

bool negatedBefore (const juce::StringArray& w, int at)
{
    for (int i = juce::jmax (0, at - 2); i < at; ++i)
        if (contains (juce::StringArray (w[i]), { "uten", "ingen", "ikke", "no", "without", "not", "fjern" })) return true;
    return false;
}

struct Builder
{
    const hk::Params base;
    Result& r;
    float get (const char* id)
    {
        auto it = r.params.find (id);
        if (it != r.params.end()) return it->second;
        const juce::String s (id);
        if (s == "tone") return base.tone;           if (s == "air") return base.air;
        if (s == "attack") return base.attackMs;     if (s == "decay") return base.decay;
        if (s == "release") return base.releaseS;    if (s == "width") return base.width;
        if (s == "reverbMix") return base.reverbMix; if (s == "reverbSize") return base.reverbSize;
        if (s == "chorusMix") return base.chorusMix; if (s == "delayMix") return base.delayMix;
        if (s == "satDrive") return base.satDrive;   if (s == "cutoff") return base.cutoffHz;
        if (s == "pumpDepth") return base.pumpDepth; if (s == "volume") return base.volumeDb;
        return 0.0f;
    }
    void set (const char* id, float v) { clampParam (id, v); r.params[id] = v; }
    void add (const char* id, float d) { set (id, get (id) + d); }
};
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

//==============================================================================
Result interpretLocal (const juce::String& text, int currentSound)
{
    Result r;
    const auto w = words (text);
    if (w.isEmpty()) { r.error = "Skriv hva slags lyd du vil ha"; return r; }

    //---- 1. pick the base sound
    int best = -1;
    float bestScore = 0.0f;
    for (int i = 0; i < hk::kNumSounds; ++i)
    {
        const auto& d = hk::sounds()[(size_t) i];
        if (d.sampler.user) continue;
        float score = 0.0f;
        if (auto it = categoryNouns().find (d.category); it != categoryNouns().end())
            for (auto* k : it->second) if (contains (w, { k })) { score += 3.0f; break; }
        if (auto it = soundTags().find (i); it != soundTags().end())
            for (auto* k : it->second) if (contains (w, { k })) score += 2.0f;
        for (auto& nw : words (u (d.name))) if (nw.length() >= 3 && contains (w, { nw.toRawUTF8() })) score += 0.5f;
        // the most typical sound of each kind wins when the description is general ("piano", "bass")
        static const std::map<int, float> prior = { { hk::kFreshHouse, 0.6f }, { hk::kConcertGrand, 0.3f },
            { hk::kDeepHouseKeys, 0.3f }, { hk::kWarmRhodes, 0.3f }, { hk::kSupersawChords, 0.3f }, { hk::kPolySynth, 0.3f },
            { hk::kSawLead, 0.3f }, { hk::kWarmPad, 0.3f }, { hk::kHouseBass, 0.3f }, { hk::kHousePluck, 0.3f } };
        if (auto it = prior.find (i); it != prior.end() && score > 0.0f) score += it->second;
        score -= 0.001f * (float) i;   // stable tie-break
        if (score > bestScore) { bestScore = score; best = i; }
    }
    if (best < 0 || bestScore < 1.4f)
    {
        best = currentSound >= 0 ? currentSound : hk::kDefaultSound;
        r.notes.add (u ("Beholdt ") + u (hk::sounds()[(size_t) best].name) + u (" som utgangspunkt"));
    }
    else
        r.notes.add (u ("Valgte ") + u (hk::sounds()[(size_t) best].name));
    r.sound = best;

    //---- 2. adjust the controls
    Builder b { hk::sounds()[(size_t) best].preset, r };
    int at = 0;
    auto has = [&] (std::initializer_list<const char*> k) { return contains (w, k, &at); };

    if (has ({ "lys", "lysere", "bright", "brighter", "skarp", "crisp", "glitrende", "sparkling", "klar", "clear" }))
    { const float k = intensityBefore (w, at); b.add ("tone", 0.15f * k); b.add ("air", 0.2f * k); r.notes.add (u ("Lysere tone og mer Air")); }
    if (has ({ "mork", "morkere", "dark", "darker", "dempet", "muffled", "dull" }))
    { const float k = intensityBefore (w, at); b.add ("tone", -0.2f * k); b.add ("air", -0.25f * k); r.notes.add (u ("M\xc3\xb8rkere tone")); }
    if (has ({ "varm", "varmere", "warm", "warmer" }))
    { const float k = intensityBefore (w, at); b.add ("tone", -0.08f * k); r.notes.add (u ("Varmere")); }
    if (has ({ "luftig", "luft", "airy", "air", "fresh", "frisk", "apen", "open" }))
    { const float k = intensityBefore (w, at); b.add ("air", 0.3f * k); r.notes.add (u ("Mer Air (luft og detaljer i toppen)")); }
    if (has ({ "bred", "bredere", "wide", "wider", "stor", "big", "huge", "enorm" }))
    { const float k = intensityBefore (w, at); b.add ("width", 0.18f * k); b.add ("reverbSize", 0.12f * k); r.notes.add (u ("Bredere stereo")); }
    if (has ({ "smal", "mono", "narrow", "tett" }))
    { b.add ("width", -0.25f); r.notes.add (u ("Smalere stereo")); }

    if (has ({ "fet", "feit", "fat", "kraftig", "tung", "heavy", "punch", "punchy", "hard", "aggressiv", "aggressive" }))
    { const float k = intensityBefore (w, at); b.set ("satOn", 1); b.set ("satDrive", juce::jmin (0.6f, 0.25f * k + 0.1f));
      r.notes.add (u ("Saturation p\xc3\xa5 for mer kraft")); }
    if (has ({ "distortion", "distort", "skitten", "dirty", "crunchy", "drive", "mettet", "saturated" }))
    { const float k = intensityBefore (w, at); b.set ("satOn", 1); b.set ("satDrive", juce::jmin (0.85f, 0.4f * k));
      r.notes.add (u ("Mer saturation / drive")); }

    if (has ({ "reverb", "romklang", "hall", "rom", "space", "ambient", "drommende", "dreamy", "romslig" }))
    {
        if (negatedBefore (w, at)) { b.set ("reverbOn", 0); r.notes.add (u ("Reverb av")); }
        else { const float k = intensityBefore (w, at); b.set ("reverbOn", 1); b.set ("reverbMix", 0.22f + 0.14f * k); b.add ("reverbSize", 0.15f * k);
               r.notes.add (u ("Mer reverb")); }
    }
    if (has ({ "tort", "torr", "dry" })) { b.set ("reverbOn", 0); b.set ("delayOn", 0); r.notes.add (u ("T\xc3\xb8rr lyd (reverb og delay av)")); }
    if (has ({ "delay", "ekko", "echo", "pingpong" }))
    {
        if (negatedBefore (w, at)) { b.set ("delayOn", 0); r.notes.add (u ("Delay av")); }
        else { const float k = intensityBefore (w, at); b.set ("delayOn", 1); b.set ("delayMix", 0.14f + 0.08f * k); b.set ("delayFb", 0.3f + 0.08f * k);
               r.notes.add (u ("Delay p\xc3\xa5")); }
    }
    if (has ({ "chorus", "wobble", "shimmer" }))
    {
        if (negatedBefore (w, at)) { b.set ("chorusOn", 0); r.notes.add (u ("Chorus av")); }
        else { const float k = intensityBefore (w, at); b.set ("chorusOn", 1); b.set ("chorusMix", 0.25f + 0.12f * k); r.notes.add (u ("Chorus p\xc3\xa5")); }
    }
    if (has ({ "pump", "pumpende", "pumping", "sidechain", "ducking" }))
    {
        if (negatedBefore (w, at)) { b.set ("pumpOn", 0); r.notes.add (u ("Pump av")); }
        else { const float k = intensityBefore (w, at); b.set ("pumpOn", 1); b.set ("pumpDepth", juce::jmin (0.9f, 0.4f + 0.2f * k));
               r.notes.add (u ("Pump i takt med kicken")); }
    }
    if (has ({ "vibrato", "vibrerende", "wobbly" }))
    { const float k = intensityBefore (w, at); b.set ("vibOn", 1); b.set ("vibDepth", 0.2f * k + 0.05f); r.notes.add (u ("Vibrato p\xc3\xa5")); }
    if (has ({ "filter", "filtrert", "filtered", "lowpass", "lavpass" }))
    { b.set ("filterOn", 1); b.set ("cutoff", 1200.0f); b.set ("reso", 0.25f); r.notes.add (u ("Filter p\xc3\xa5 (1,2 kHz)")); }

    if (has ({ "kort", "short", "staccato", "stab", "stabbete", "tight" }))
    { b.set ("decay", 0.25f); b.set ("release", juce::jmin (b.get ("release"), 0.12f)); r.notes.add (u ("Kortere toner")); }
    if (has ({ "lang", "lange", "long", "sustain", "svevende", "evig", "langt" }))
    { b.set ("decay", 0.75f); b.add ("release", 0.5f); r.notes.add (u ("Lengre toner")); }
    if (has ({ "swell", "fade", "langsom", "slow", "myk start", "sakte" }))
    { b.set ("attack", 350.0f); r.notes.add (u ("Langsom attack")); }
    if (has ({ "myk", "mykere", "soft", "softer", "rolig", "calm", "gentle" }))
    { b.add ("tone", -0.08f); b.add ("air", -0.1f); r.notes.add (u ("Mykere")); }

    // chords only when asked for explicitly
    struct ChordWord { const char* w; int c; };
    for (auto cw : { ChordWord { "min7", hk::kMin7 }, ChordWord { "m7", hk::kMin7 }, ChordWord { "min9", hk::kMin9 },
                     ChordWord { "m9", hk::kMin9 }, ChordWord { "min11", hk::kMin11 }, ChordWord { "maj7", hk::kMaj7 },
                     ChordWord { "maj9", hk::kMaj9 }, ChordWord { "sus", hk::kSus }, ChordWord { "sus4", hk::kSus } })
        if (w.contains (cw.w)) { b.set ("chord", (float) cw.c); r.notes.add (u ("Chord: ") + juce::String (cw.w).toUpperCase()); break; }

    r.ok = true;
    return r;
}

//==============================================================================
static juce::String buildSystemPrompt()
{
    juce::String s;
    s << "You design sounds for HouseKeys, a house-music keys/synth plugin. The user describes a sound "
         "(often in Norwegian). Pick the best base sound and set the controls.\n\nBase sounds (exact names):\n";
    for (int c = 0; c < hk::kNumCategories; ++c)
    {
        juce::StringArray names;
        for (auto& d : hk::sounds()) if (d.category == c && ! d.sampler.user) names.add (u (d.name));
        if (names.size() > 0) s << "- " << hk::categoryName (c) << ": " << names.joinIntoString (", ") << "\n";
    }
    s << "\nControls (only include the ones you want to change from the base sound's own settings):\n"
         "tone 0..1 (0.5 neutral), air 0..1 (top-end sparkle), attack 0..2000 ms, decay 0..1 (0.5 natural), "
         "release 0.01..6 s, width 0..1 (0.5 normal), volume -24..6 dB,\n"
         "filterOn 0/1, cutoff 80..18000 Hz, reso 0..1, satOn 0/1, satDrive 0..1, chorusOn 0/1, chorusMix 0..1,\n"
         "delayOn 0/1, delayTime 0..5 (1/16,1/8,3/16,1/4,3/8,1/2), delayFb 0..0.9, delayMix 0..1,\n"
         "reverbOn 0/1, reverbSize 0..1, reverbMix 0..1, pumpOn 0/1, pumpRate 0..2 (1/4,1/8,1/16), pumpDepth 0..1,\n"
         "vibOn 0/1, vibRate 0.5..12 Hz, vibDepth 0..1, chord 0..6 (0 off - keep 0 unless the user asks for a chord).\n\n"
         "Answer with ONLY a JSON object, no other text:\n"
         "{\"sound\": \"<exact base sound name>\", \"params\": {\"air\": 0.7, ...}, "
         "\"notes\": [\"short Norwegian sentence about what you did\", ...]}";
    return s;
}

void askClaude (const juce::String& text, const juce::String& apiKey, int currentSound, std::function<void (Result)> done)
{
    juce::Thread::launch ([text, apiKey, currentSound, done]
    {
        Result r;
        auto fail = [&] (const juce::String& why)
        {
            r = interpretLocal (text, currentSound);
            r.notes.insert (0, u ("Claude svarte ikke (") + why + u (") \xe2\x80\x93 brukte innebygd AI"));
        };

        juce::DynamicObject::Ptr msg = new juce::DynamicObject();
        msg->setProperty ("role", "user");
        msg->setProperty ("content", text);
        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty ("model", "claude-sonnet-5-5");
        body->setProperty ("max_tokens", 800);
        body->setProperty ("system", buildSystemPrompt());
        body->setProperty ("messages", juce::Array<juce::var> { juce::var (msg.get()) });

        const juce::URL url = juce::URL ("https://api.anthropic.com/v1/messages")
                                  .withPOSTData (juce::JSON::toString (juce::var (body.get()), true));
        int status = 0;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                 .withExtraHeaders ("x-api-key: " + apiKey.trim() + "\r\n"
                                                                    "anthropic-version: 2023-06-01\r\n"
                                                                    "content-type: application/json")
                                                 .withConnectionTimeoutMs (30000)
                                                 .withStatusCode (&status));
        if (stream == nullptr) fail (u ("ingen nettverkstilkobling"));
        else
        {
            const auto reply = juce::JSON::parse (stream->readEntireStreamAsString());
            if (status != 200)
                fail (status == 401 ? u ("feil API-n\xc3\xb8kkel") : "HTTP " + juce::String (status));
            else
            {
                juce::String answer;
                if (auto* content = reply.getProperty ("content", {}).getArray())
                    for (auto& block : *content)
                        if (block.getProperty ("type", {}).toString() == "text") answer << block.getProperty ("text", {}).toString();
                const int a = answer.indexOfChar ('{'), z = answer.lastIndexOfChar ('}');
                const auto j = a >= 0 && z > a ? juce::JSON::parse (answer.substring (a, z + 1)) : juce::var();
                const auto name = j.getProperty ("sound", {}).toString();
                for (int i = 0; i < hk::kNumSounds; ++i)
                    if (name.equalsIgnoreCase (u (hk::sounds()[(size_t) i].name)) && ! hk::sounds()[(size_t) i].sampler.user) r.sound = i;
                if (r.sound < 0) fail (u ("uventet svar"));
                else
                {
                    if (auto* obj = j.getProperty ("params", {}).getDynamicObject())
                        for (auto& p : obj->getProperties())
                        {
                            float v = (float) p.value;
                            if (clampParam (p.name.toString(), v)) r.params[p.name.toString()] = v;
                        }
                    if (auto* n = j.getProperty ("notes", {}).getArray())
                        for (auto& line : *n) r.notes.add (line.toString());
                    r.ok = true;
                    r.fromClaude = true;
                }
            }
        }
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

//==============================================================================
static juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Application Support").getChildFile ("HouseKeys").getChildFile ("settings.xml");
}

juce::String loadApiKey()
{
    if (auto xml = juce::XmlDocument::parse (settingsFile())) return xml->getStringAttribute ("anthropicApiKey");
    return {};
}

void saveApiKey (const juce::String& key)
{
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    juce::XmlElement xml ("HouseKeysSettings");
    xml.setAttribute ("anthropicApiKey", key.trim());
    xml.writeTo (f);
    f.setReadOnly (false);
}

juce::StringArray examples()
{
    return { u ("fet house piano med mye luft og pump"),
             u ("m\xc3\xb8rk lo-fi rhodes med delay"),
             u ("bred euforisk supersaw med reverb"),
             u ("tung reese bass"),
             u ("myk drømmende pad"),
             u ("kort lys pluck med ekko") };
}
} // namespace PresetAI
