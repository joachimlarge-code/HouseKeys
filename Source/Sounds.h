// HouseKeys - sound definitions and factory presets (no JUCE dependency).
//
// A sound is a mix of sources, each with its own level:
//   sampler  - multisampled acoustic grand (Salamander Grand Piano, 4 velocity layers)
//   acoustic - additive modelled piano (used as a fallback while samples load)
//   fm       - two FM operator pairs: electric pianos, bells, bite
//   keys     - additive organ-keys with two detuned stereo copies (UK house stabs)
//   organ    - drawbar sines with key click
//   va       - virtual analog: anti-aliased saw/square/tri/sine with unison, sub, noise,
//              4-pole filter with its own envelope (pads, bass, plucks, synth chords)
// followed by a per-sound colour chain (low cut, shelves, drive, glue compressor, tape).
// Every sound also carries the values all user controls reset to when it is selected.
#pragma once

#include "Dsp.h"
#include <string>

namespace hk
{
//==============================================================================
enum Chord { kChordOff = 0, kMin7, kMin9, kMin11, kMaj7, kMaj9, kSus, kNumChords };

inline const std::vector<int>& chordIntervals (int c)
{
    static const std::vector<int> table[kNumChords] = {
        { 0 }, { 0, 3, 7, 10 }, { 0, 3, 7, 10, 14 }, { 0, 3, 7, 10, 14, 17 },
        { 0, 4, 7, 11 }, { 0, 4, 7, 11, 14 }, { 0, 5, 7, 10 } };
    return table[std::clamp (c, 0, kNumChords - 1)];
}

inline double delayBeats (int idx)
{
    static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return b[std::clamp (idx, 0, 5)];
}

//==============================================================================
// Everything the user can change. Each sound has its own set of defaults.
struct Params
{
    int   sound    = 0;
    float tone     = 0.5f;    // 0 dark .. 1 bright (0.5 = as designed)
    float air      = 0.3f;    // 0 .. 1
    float attackMs = 1.0f;    // 0 .. 2000
    float decay    = 0.5f;    // 0 short .. 1 long (0.5 = as designed)
    float releaseS = 0.3f;    // 0.01 .. 6
    float width    = 0.5f;    // 0 mono .. 1 extra wide (0.5 = as designed)
    float volumeDb = -3.0f;
    int   chord    = kChordOff;

    bool  filterOn = false; float cutoffHz = 3000.0f; float resonance = 0.2f;
    bool  satOn    = false; float satDrive = 0.35f;
    bool  chorusOn = false; float chorusMix = 0.3f;
    bool  delayOn  = false; int delayTime = 2; float delayFeedback = 0.35f; float delayMix = 0.22f;
    bool  reverbOn = true;  float reverbMix = 0.22f; float reverbSize = 0.55f;
    bool  pumpOn   = false; float pumpDepth = 0.6f; int pumpRate = 0;
    bool  vibOn    = false; float vibRate = 5.5f; float vibDepth = 0.3f;   // vibrato (Hz, 0..1 = 0..60 cents)
};

//==============================================================================
struct SamplerDef
{
    float level    = 0.0f;
    float dynamics = 18.0f;   // loudness range from softest to hardest touch (dB)
    float velCurve = 1.0f;    // >1 = needs harder playing to reach the bright layers
    float relNoise = 0.5f;    // key-release (damper) noise level
    float octave   = 0.0f;    // level of an extra piano layer one octave up (the classic fat house piano)
    bool  user     = false;   // play the sample the user dropped onto the plugin instead of the piano
};

struct AcousticDef
{
    float level    = 0.0f;
    float hardness = 0.6f, strike = 0.12f, decay = 5.0f, inharm = 1.0f, detune = 1.2f, hammer = 0.4f;
};

struct OrganDef
{
    float level = 0.0f;
    std::array<float, 5> bars { 0, 1, 0, 0, 0 }; // 16', 8', 4', 2 2/3', 2'
    float decay = 0.0f;      // 0 = sustains, otherwise decay time in seconds
    float click = 0.3f;
};

struct FmDef
{
    float level = 0.0f;
    float ratioA = 1, indexA = 1, susA = 0.2f, decA = 0.5f;          // body pair
    float octB = 1, ratioB = 14, indexB = 1, decB = 0.06f, susB = 0;  // tine / bite pair
    float mixB = 0.5f;
    float feedback = 0;
    float decay = 3.0f, sustain = 0.0f;
    float bark = 0.0f;
};

struct KeysDef
{
    float level = 0.0f;
    float tilt = 0.5f, kc = 20.0f, odd = 1.0f, h2 = 1.0f, h3 = 1.0f;
    float tau = 0.4f, bright = 0.3f, sus = 0.3f, tauS = 2.0f;
    float detune = 10.0f, spread = 0.4f, unused1 = 0.0f, unused2 = 0.0f, brightS = 0.0f;
};

enum Wave { kSaw = 0, kSquare, kTri, kSine };

struct VaDef
{
    float level = 0.0f;
    int   wave1 = kSaw, wave2 = kSaw;
    float semi2 = 0.0f, fine2 = 0.0f, mix2 = 0.0f;   // osc 2 offset (semitones, cents) and level
    int   unison = 1;                               // 1..7 voices per oscillator
    float detune = 0.0f;                            // total unison spread in cents
    float spread = 0.0f;                            // unison stereo spread 0..1
    float sub = 0.0f, noise = 0.0f;                 // sine one octave down, white noise
    float cutoff = 4000.0f, reso = 0.1f;            // filter
    float envAmt = 0.0f, keytrack = 0.5f, velAmt = 0.5f;   // octaves
    float fA = 1.0f, fD = 300.0f, fS = 0.5f, fR = 300.0f;  // filter envelope (ms, level)
    float decayMs = 0.0f, sustain = 1.0f;           // amp: 0 decay = no decay stage
    int   poles = 4;
    float drift = 2.0f;                             // analog pitch drift (cents)
    float motion = 0.0f;                            // slow random filter movement (octaves)
    float velSens = 0.45f;
};

enum Category
{
    kCatRealPiano = 0, kCatHousePiano, kCatDeepKeys, kCatRhodes, kCatSupersaw, kCatSynths,
    kCatLeads, kCatPads, kCatBass, kCatPlucks, kCatUser, kNumCategories
};

inline const char* categoryName (int c)
{
    static const char* n[kNumCategories] = { "Real Piano", "House Piano", "Deep House Keys", "Rhodes",
                                              "Supersaw & Chords", "Synths", "Leads", "Pads", "Bass", "Plucks", "Ditt sample" };
    return n[std::clamp (c, 0, kNumCategories - 1)];
}

struct SoundDef
{
    const char* name = "";
    const char* tag  = "";
    int category = 0;
    SamplerDef  sampler;
    AcousticDef fallback;     // modelled piano used until the samples are loaded
    OrganDef    organ;
    FmDef       fm;
    KeysDef     keys;
    VaDef       va;
    bool  mono = false;       // one note at a time (bass, leads)
    float glideMs = 0.0f;     // portamento between legato notes (mono sounds)
    float lowCutHz = 20.0f, lowDb = 0, highDb = 0, drive = 0, glue = 0.2f, tape = 0, gainDb = 0;
    Params preset;
};

enum SoundId
{
    // Real Piano
    kConcertGrand = 0, kIntimateFelt, kBalladGrand, kOctaveGrand, kDeepNightPiano,
    // House Piano
    kFreshHouse, kBigHousePiano, kBrightPiano, kSupersawPiano, kSaturatedPiano, kLofiHousePiano,
    kPianoHouse90s, kSunsetPiano, kPumpingChords,
    // Deep House Keys
    kDeepHouseKeys, kUkGarageKeys, kDeepWideStab, kShortOrganStab,
    // Rhodes
    kWarmRhodes, kLofiRhodes,
    // Supersaw & Chords
    kSupersawChords, kEuphoricSupersaw, kSupersawStab, kMelodicChords, kRaveStab,
    // Synths
    kPolySynth, kAnalogBrass, kJunoStrings, kHollowSynth,
    // Leads
    kFatSawLead, kSawLead, kOctaveSawLead, kWideSawLead, kScreamLead, kHouseLead, kSupersawLead,
    // Pads
    kWarmPad, kAirPad, kDarkStrings,
    // Bass
    kDeepSub, kHouseBass, kReeseBass, kOrganBass, kSawBass, kAcidBass, kPluckBass,
    // Plucks
    kHousePluck, kBrightPluck, kSupersawPluck, kMalletPluck, kDeepPluck, kSoftPluck,
    // Your sample (drag & drop)
    kDroppedSample,
    kNumSounds
};

static constexpr int kDefaultSound = kFreshHouse;

inline const std::array<SoundDef, kNumSounds>& sounds()
{
    static const std::array<SoundDef, kNumSounds> defs = [] {
        std::array<SoundDef, kNumSounds> d {};
        auto def = [&d] (int id, int cat, const char* name, const char* tag) -> SoundDef&
        {
            auto& s = d[(size_t) id];
            s = SoundDef {};
            s.name = name; s.tag = tag; s.category = cat;
            s.preset.sound = id;
            return s;
        };
        // the modelled piano that plays while samples are loading
        const AcousticDef fb { 1.0f, 0.62f, 0.12f, 6.0f, 1.0f, 1.0f, 0.3f };

        //================ REAL PIANO ================
        { auto& s = def (kConcertGrand, kCatRealPiano, "Concert Grand", "Naturlig flygel");
          s.sampler = { 1.0f, 22.0f, 1.0f, 0.45f }; s.fallback = fb;
          s.lowCutHz = 28.0f; s.glue = 0.1f; s.gainDb = 3.5f;
          auto& q = s.preset; q.air = 0.15f; q.attackMs = 0.0f; q.releaseS = 0.35f; q.width = 0.42f;
          q.reverbMix = 0.2f; q.reverbSize = 0.6f; }

        { auto& s = def (kIntimateFelt, kCatRealPiano, "Intimate Felt", "Mykt og n\xc3\xa6rt");
          s.sampler = { 1.0f, 24.0f, 1.25f, 0.9f }; s.fallback = fb; s.fallback.hardness = 0.2f;
          s.lowCutHz = 30.0f; s.lowDb = 1.5f; s.highDb = -7.0f; s.glue = 0.2f; s.tape = 0.12f; s.gainDb = 8.2f;
          auto& q = s.preset; q.tone = 0.4f; q.air = 0.0f; q.attackMs = 0.0f; q.releaseS = 0.3f; q.width = 0.40f;
          q.reverbMix = 0.28f; q.reverbSize = 0.5f; }

        { auto& s = def (kBalladGrand, kCatRealPiano, "Ballad Grand", "Stort og varmt");
          s.sampler = { 1.0f, 20.0f, 1.1f, 0.4f }; s.fallback = fb;
          s.lowCutHz = 30.0f; s.lowDb = 1.0f; s.highDb = -1.5f; s.glue = 0.25f; s.gainDb = 3.8f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 0.0f; q.releaseS = 0.6f; q.width = 0.48f;
          q.reverbMix = 0.34f; q.reverbSize = 0.82f; }

        //================ HOUSE PIANO ================
        { auto& s = def (kFreshHouse, kCatHousePiano, "Fresh House Piano", "Lys, luftig og polert");
          s.sampler = { 1.0f, 12.0f, 0.85f, 0.3f }; s.fallback = fb; s.fallback.hardness = 0.85f;
          s.lowCutHz = 95.0f; s.lowDb = -1.5f; s.highDb = 2.5f; s.drive = 0.12f; s.glue = 0.6f; s.gainDb = 4.1f;
          auto& q = s.preset; q.air = 0.6f; q.attackMs = 0.0f; q.releaseS = 0.25f; q.width = 0.42f;
          q.chorusOn = true; q.chorusMix = 0.16f; q.reverbMix = 0.2f; q.reverbSize = 0.5f; }

        { auto& s = def (kPianoHouse90s, kCatHousePiano, "Piano House 90s", "Klassisk M1-aktig stab");
          s.sampler = { 0.85f, 10.0f, 0.8f, 0.2f }; s.fallback = fb; s.fallback.hardness = 0.9f;
          s.organ.level = 0.32f; s.organ.bars = { 0.0f, 1.0f, 0.6f, 0.0f, 0.3f }; s.organ.decay = 1.6f; s.organ.click = 0.0f;
          s.fm.level = 0.22f; s.fm.ratioA = 1; s.fm.indexA = 2.4f; s.fm.susA = 0.1f; s.fm.decA = 0.22f; s.fm.mixB = 0.0f; s.fm.decay = 1.0f;
          s.lowCutHz = 120.0f; s.lowDb = -1.0f; s.highDb = 3.0f; s.drive = 0.28f; s.glue = 0.65f; s.gainDb = 3.5f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 0.0f; q.releaseS = 0.18f; q.width = 0.38f;
          q.chorusOn = true; q.chorusMix = 0.25f; q.reverbMix = 0.18f; q.reverbSize = 0.45f; }

        { auto& s = def (kSunsetPiano, kCatHousePiano, "Sunset Piano", "Melodisk med delay");
          s.sampler = { 1.0f, 14.0f, 0.9f, 0.35f }; s.fallback = fb;
          s.lowCutHz = 75.0f; s.highDb = 1.5f; s.glue = 0.45f; s.gainDb = 3.7f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 0.0f; q.releaseS = 0.4f; q.width = 0.45f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.17f;
          q.reverbMix = 0.3f; q.reverbSize = 0.72f; }

        { auto& s = def (kPumpingChords, kCatHousePiano, "Pumping Chords", "Pumper i takt med kicken");
          s.sampler = { 1.0f, 10.0f, 0.85f, 0.2f }; s.fallback = fb; s.fallback.hardness = 0.8f;
          s.lowCutHz = 130.0f; s.lowDb = -1.0f; s.highDb = 2.0f; s.drive = 0.2f; s.glue = 0.65f; s.gainDb = 6.4f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 0.0f; q.releaseS = 0.25f; q.width = 0.40f;
          q.chorusOn = true; q.chorusMix = 0.2f; q.reverbMix = 0.22f; q.pumpOn = true; q.pumpDepth = 0.6f; }

        //================ DEEP HOUSE KEYS ================
        { auto& s = def (kDeepHouseKeys, kCatDeepKeys, "Deep House Keys", "Varm, myk og dyp");
          s.fm.level = 1.0f; s.fm.ratioA = 1; s.fm.indexA = 1.1f; s.fm.susA = 0.3f; s.fm.decA = 1.4f;
          s.fm.octB = 1; s.fm.ratioB = 14; s.fm.indexB = 1.2f; s.fm.decB = 0.05f; s.fm.mixB = 0.35f; s.fm.decay = 5.0f; s.fm.bark = 0.25f;
          s.organ.level = 0.18f; s.organ.bars = { 0.0f, 1.0f, 0.3f, 0.0f, 0.0f }; s.organ.decay = 3.0f; s.organ.click = 0.0f;
          s.lowCutHz = 60.0f; s.lowDb = 1.5f; s.highDb = -2.0f; s.drive = 0.15f; s.glue = 0.4f; s.gainDb = -0.6f;
          auto& q = s.preset; q.tone = 0.45f; q.air = 0.25f; q.attackMs = 1.0f; q.releaseS = 0.4f; q.width = 0.6f;
          q.chorusOn = true; q.chorusMix = 0.4f; q.reverbMix = 0.26f; q.reverbSize = 0.62f; }

        { auto& s = def (kUkGarageKeys, kCatDeepKeys, "UK Garage Keys", "Lys orgel-stab");
          s.keys = KeysDef { 1.0f, 0.698f, 26.0f, 3.00f, 2.10f, 2.75f, 0.046f, 0.127f, 0.604f, 0.466f, 11.4f, 0.36f, 0, 0, 0.034f };
          s.drive = 0.10f; s.glue = 0.0f; s.highDb = -2.0f; s.gainDb = 3.3f;
          auto& q = s.preset; q.air = 0.2f; q.attackMs = 7.5f; q.releaseS = 0.03f; q.width = 0.5f;
          q.reverbMix = 0.12f; q.reverbSize = 0.4f; }

        { auto& s = def (kDeepWideStab, kCatDeepKeys, "Deep Wide Stab", "Dyp og bred");
          s.keys = KeysDef { 1.0f, 0.510f, 40.0f, 1.82f, 0.59f, 0.30f, 0.162f, 0.0f, 0.68f, 1.839f, 22.9f, 0.68f, 0, 0, 0.681f };
          s.drive = 0.17f; s.glue = 0.0f; s.lowDb = 1.0f; s.highDb = -2.0f; s.gainDb = 4.2f;
          auto& q = s.preset; q.air = 0.2f; q.attackMs = 0.8f; q.releaseS = 0.04f; q.width = 0.5f;
          q.reverbMix = 0.12f; q.reverbSize = 0.45f; }

        { auto& s = def (kShortOrganStab, kCatDeepKeys, "Short Organ Stab", "Kort og tett");
          s.keys = KeysDef { 1.0f, 0.232f, 12.0f, 2.20f, 1.67f, 0.98f, 0.073f, 0.937f, 0.95f, 0.478f, 21.9f, 0.20f, 0, 0, 0.195f };
          s.drive = 0.55f; s.glue = 0.0f; s.highDb = -3.0f; s.gainDb = 5.1f;
          auto& q = s.preset; q.air = 0.2f; q.attackMs = 1.0f; q.releaseS = 0.08f; q.width = 0.5f;
          q.reverbMix = 0.15f; q.reverbSize = 0.4f; }

        //================ RHODES ================
        { auto& s = def (kWarmRhodes, kCatRhodes, "Warm Rhodes", "Klassisk el-piano");
          s.fm.level = 1.0f; s.fm.ratioA = 1; s.fm.indexA = 1.3f; s.fm.susA = 0.25f; s.fm.decA = 1.2f;
          s.fm.octB = 1; s.fm.ratioB = 14; s.fm.indexB = 1.6f; s.fm.decB = 0.06f; s.fm.mixB = 0.45f; s.fm.decay = 4.5f; s.fm.bark = 0.4f;
          s.lowCutHz = 45.0f; s.lowDb = 2.0f; s.highDb = -1.0f; s.drive = 0.12f; s.glue = 0.35f; s.gainDb = -0.3f;
          auto& q = s.preset; q.air = 0.2f; q.attackMs = 1.0f; q.releaseS = 0.35f; q.width = 0.6f;
          q.chorusOn = true; q.chorusMix = 0.45f; q.reverbMix = 0.26f; q.reverbSize = 0.6f; }

        { auto& s = def (kLofiRhodes, kCatRhodes, "Lo-fi Rhodes", "St\xc3\xb8vete tape-Rhodes");
          s.fm.level = 1.0f; s.fm.ratioA = 1; s.fm.indexA = 1.2f; s.fm.susA = 0.25f; s.fm.decA = 1.0f;
          s.fm.octB = 1; s.fm.ratioB = 14; s.fm.indexB = 1.0f; s.fm.decB = 0.06f; s.fm.mixB = 0.35f; s.fm.decay = 3.5f; s.fm.bark = 0.3f;
          s.lowCutHz = 60.0f; s.lowDb = 2.0f; s.highDb = -5.0f; s.drive = 0.25f; s.glue = 0.45f; s.tape = 0.8f; s.gainDb = 0.7f;
          auto& q = s.preset; q.air = 0.0f; q.attackMs = 1.0f; q.releaseS = 0.3f; q.width = 0.55f;
          q.chorusOn = true; q.chorusMix = 0.3f; q.reverbMix = 0.28f; q.reverbSize = 0.55f; }

        //================ WIDE PADS ================
        { auto& s = def (kWarmPad, kCatPads, "Warm Pad", "Varm og bred bakgrunn");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.mix2 = 0.35f; v.fine2 = 7.0f;
          v.unison = 5; v.detune = 22.0f; v.spread = 0.85f; v.sub = 0.15f;
          v.cutoff = 1300.0f; v.reso = 0.12f; v.envAmt = 0.6f; v.keytrack = 0.4f; v.velAmt = 0.3f;
          v.fA = 900.0f; v.fD = 1500.0f; v.fS = 0.6f; v.fR = 1500.0f; v.motion = 0.3f; v.poles = 2; v.velSens = 0.25f;
          s.lowCutHz = 90.0f; s.glue = 0.25f; s.tape = 0.1f; s.gainDb = 0.7f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 450.0f; q.releaseS = 1.8f; q.width = 0.75f;
          q.chorusOn = true; q.chorusMix = 0.35f; q.reverbMix = 0.42f; q.reverbSize = 0.85f; }

        { auto& s = def (kAirPad, kCatPads, "Air Pad", "Lys og luftig");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.3f;
          v.unison = 7; v.detune = 30.0f; v.spread = 1.0f; v.noise = 0.04f;
          v.cutoff = 3200.0f; v.reso = 0.1f; v.envAmt = 0.4f; v.keytrack = 0.5f; v.velAmt = 0.2f;
          v.fA = 1200.0f; v.fD = 2000.0f; v.fS = 0.7f; v.fR = 2000.0f; v.motion = 0.35f; v.poles = 2; v.velSens = 0.2f;
          s.lowCutHz = 260.0f; s.highDb = 1.0f; s.glue = 0.25f; s.gainDb = -2.1f;
          auto& q = s.preset; q.air = 0.6f; q.attackMs = 700.0f; q.releaseS = 2.5f; q.width = 0.85f;
          q.chorusOn = true; q.chorusMix = 0.25f; q.delayOn = true; q.delayTime = 3; q.delayFeedback = 0.35f; q.delayMix = 0.15f;
          q.reverbMix = 0.5f; q.reverbSize = 0.92f; }

        { auto& s = def (kDarkStrings, kCatPads, "Dark Strings", "Dyp og emosjonell");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = -12.0f; v.mix2 = 0.45f;
          v.unison = 5; v.detune = 18.0f; v.spread = 0.8f;
          v.cutoff = 850.0f; v.reso = 0.1f; v.envAmt = 0.8f; v.keytrack = 0.45f; v.velAmt = 0.4f;
          v.fA = 1200.0f; v.fD = 2500.0f; v.fS = 0.5f; v.fR = 2000.0f; v.motion = 0.3f; v.poles = 4; v.velSens = 0.3f;
          s.lowCutHz = 70.0f; s.lowDb = 1.0f; s.glue = 0.3f; s.tape = 0.15f; s.gainDb = 1.9f;
          auto& q = s.preset; q.tone = 0.45f; q.air = 0.15f; q.attackMs = 650.0f; q.releaseS = 2.2f; q.width = 0.75f;
          q.chorusOn = true; q.chorusMix = 0.3f; q.reverbMix = 0.45f; q.reverbSize = 0.85f; }

        //================ HOUSE BASS ================
        { auto& s = def (kDeepSub, kCatBass, "Deep Sub", "Ren og dyp subbass");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSine; v.wave2 = kTri; v.mix2 = 0.18f;
          v.cutoff = 900.0f; v.reso = 0.0f; v.keytrack = 0.0f; v.velAmt = 0.0f; v.poles = 2; v.drift = 0.0f; v.velSens = 0.2f;
          s.mono = true; s.lowCutHz = 25.0f; s.drive = 0.12f; s.glue = 0.3f; s.gainDb = 2.5f;
          auto& q = s.preset; q.air = 0.0f; q.attackMs = 3.0f; q.releaseS = 0.06f; q.width = 0.0f; q.reverbOn = false; }

        { auto& s = def (kHouseBass, kCatBass, "House Bass", "Rund og tydelig");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSquare; v.wave2 = kSaw; v.mix2 = 0.5f; v.fine2 = -6.0f; v.sub = 0.55f;
          v.cutoff = 380.0f; v.reso = 0.22f; v.envAmt = 2.2f; v.keytrack = 0.35f; v.velAmt = 0.7f;
          v.fA = 1.0f; v.fD = 220.0f; v.fS = 0.2f; v.fR = 120.0f; v.decayMs = 500.0f; v.sustain = 0.6f; v.poles = 4; v.drift = 1.0f;
          s.mono = true; s.lowCutHz = 28.0f; s.drive = 0.35f; s.glue = 0.4f; s.gainDb = 3.7f;
          auto& q = s.preset; q.air = 0.1f; q.attackMs = 2.0f; q.releaseS = 0.08f; q.width = 0.0f; q.reverbOn = false; }

        { auto& s = def (kPluckBass, kCatBass, "Pluck Bass", "Kort og punchy");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.mix2 = 0.25f; v.semi2 = -12.0f; v.sub = 0.5f;
          v.cutoff = 260.0f; v.reso = 0.18f; v.envAmt = 3.2f; v.keytrack = 0.3f; v.velAmt = 0.8f;
          v.fA = 1.0f; v.fD = 140.0f; v.fS = 0.0f; v.fR = 100.0f; v.decayMs = 380.0f; v.sustain = 0.25f; v.poles = 4; v.drift = 1.0f;
          s.mono = true; s.lowCutHz = 28.0f; s.drive = 0.3f; s.glue = 0.4f; s.gainDb = 6.0f;
          auto& q = s.preset; q.air = 0.15f; q.attackMs = 1.5f; q.releaseS = 0.08f; q.width = 0.0f; q.reverbOn = false; }

        //================ PLUCKS ================
        { auto& s = def (kHousePluck, kCatPlucks, "House Pluck", "Crisp og rytmisk");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.semi2 = 12.0f; v.mix2 = 0.25f;
          v.unison = 3; v.detune = 12.0f; v.spread = 0.55f;
          v.cutoff = 420.0f; v.reso = 0.22f; v.envAmt = 4.2f; v.keytrack = 0.5f; v.velAmt = 0.8f;
          v.fA = 3.0f; v.fD = 170.0f; v.fS = 0.0f; v.fR = 150.0f; v.decayMs = 450.0f; v.sustain = 0.0f; v.poles = 4;
          s.lowCutHz = 140.0f; s.highDb = 1.0f; s.glue = 0.55f; s.gainDb = 3.4f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 1.0f; q.releaseS = 0.18f; q.width = 0.65f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.2f; q.reverbMix = 0.22f; q.reverbSize = 0.55f; }

        { auto& s = def (kSoftPluck, kCatPlucks, "Soft Pluck", "Myk og varm");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kTri; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.2f;
          v.unison = 3; v.detune = 10.0f; v.spread = 0.5f;
          v.cutoff = 700.0f; v.reso = 0.1f; v.envAmt = 2.8f; v.keytrack = 0.5f; v.velAmt = 0.6f;
          v.fA = 3.0f; v.fD = 260.0f; v.fS = 0.0f; v.fR = 200.0f; v.decayMs = 750.0f; v.sustain = 0.0f; v.motion = 0.08f; v.poles = 2;
          s.lowCutHz = 110.0f; s.glue = 0.35f; s.gainDb = 1.4f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 1.0f; q.releaseS = 0.3f; q.width = 0.6f;
          q.chorusOn = true; q.chorusMix = 0.3f; q.reverbMix = 0.3f; q.reverbSize = 0.65f; }

        //================ MELODIC CHORDS ================
        { auto& s = def (kMelodicChords, kCatSupersaw, "Melodic Chords", "Fyldige synth-akkorder");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.22f;
          v.unison = 5; v.detune = 16.0f; v.spread = 0.7f; v.sub = 0.1f;
          v.cutoff = 800.0f; v.reso = 0.15f; v.envAmt = 2.4f; v.keytrack = 0.45f; v.velAmt = 0.6f;
          v.fA = 2.0f; v.fD = 380.0f; v.fS = 0.25f; v.fR = 300.0f; v.decayMs = 700.0f; v.sustain = 0.4f; v.motion = 0.12f; v.poles = 4;
          s.lowCutHz = 120.0f; s.glue = 0.45f; s.drive = 0.15f; s.gainDb = 0.0f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 2.0f; q.releaseS = 0.35f; q.width = 0.7f;
          q.chorusOn = true; q.chorusMix = 0.2f; q.reverbMix = 0.28f; q.reverbSize = 0.65f; }

        { auto& s = def (kSupersawChords, kCatSupersaw, "Supersaw Chords", "Stort og breit");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.fine2 = 9.0f; v.mix2 = 0.5f;
          v.unison = 7; v.detune = 32.0f; v.spread = 0.95f;
          v.cutoff = 2200.0f; v.reso = 0.1f; v.envAmt = 1.4f; v.keytrack = 0.4f; v.velAmt = 0.4f;
          v.fA = 2.0f; v.fD = 600.0f; v.fS = 0.55f; v.fR = 400.0f; v.decayMs = 900.0f; v.sustain = 0.75f; v.motion = 0.15f; v.poles = 2;
          s.lowCutHz = 160.0f; s.highDb = 1.0f; s.glue = 0.5f; s.gainDb = 3.7f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 4.0f; q.releaseS = 0.4f; q.width = 0.8f;
          q.reverbMix = 0.3f; q.reverbSize = 0.7f; q.pumpOn = true; q.pumpDepth = 0.5f; }

        { auto& s = def (kRaveStab, kCatSupersaw, "Rave Stab", "Kort og hard stab");
          s.sampler = { 0.45f, 8.0f, 0.8f, 0.0f }; s.fallback = fb; s.fallback.level = 0.45f;
          s.organ.level = 0.35f; s.organ.bars = { 0.6f, 1.0f, 0.4f, 0.0f, 0.0f }; s.organ.decay = 0.7f; s.organ.click = 0.2f;
          s.fm.level = 0.75f; s.fm.ratioA = 1; s.fm.indexA = 3.4f; s.fm.susA = 0.15f; s.fm.decA = 0.22f;
          s.fm.octB = 1; s.fm.ratioB = 2; s.fm.indexB = 2.5f; s.fm.decB = 0.15f; s.fm.susB = 0.1f; s.fm.mixB = 0.5f;
          s.fm.feedback = 0.25f; s.fm.decay = 0.55f;
          s.lowCutHz = 110.0f; s.lowDb = 2.0f; s.highDb = 2.5f; s.drive = 0.5f; s.glue = 0.7f; s.gainDb = 2.8f;
          auto& q = s.preset; q.air = 0.3f; q.attackMs = 0.0f; q.releaseS = 0.12f; q.width = 0.40f;
          q.chorusOn = true; q.chorusMix = 0.2f; q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.2f;
          q.reverbMix = 0.2f; }

        //================ NEW HOUSE PIANOS ================
        { auto& s = def (kBigHousePiano, kCatHousePiano, "Big House Piano", "Fet, oktav-stablet");
          s.sampler = { 1.0f, 10.0f, 0.8f, 0.2f, 0.45f }; s.fallback = fb; s.fallback.hardness = 0.9f;
          s.lowCutHz = 110.0f; s.lowDb = -1.0f; s.highDb = 2.5f; s.drive = 0.25f; s.glue = 0.65f; s.gainDb = 3.3f;
          auto& q = s.preset; q.air = 0.55f; q.attackMs = 0.0f; q.releaseS = 0.25f; q.width = 0.5f;
          q.chorusOn = true; q.chorusMix = 0.2f; q.reverbMix = 0.2f; q.reverbSize = 0.5f; }

        { auto& s = def (kSupersawPiano, kCatHousePiano, "Supersaw Piano", "Piano + supersaw-lag");
          s.sampler = { 1.0f, 10.0f, 0.85f, 0.2f, 0.0f }; s.fallback = fb;
          auto& v = s.va; v.level = 0.32f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.25f;
          v.unison = 7; v.detune = 26.0f; v.spread = 0.9f;
          v.cutoff = 2200.0f; v.reso = 0.1f; v.envAmt = 1.2f; v.keytrack = 0.4f; v.velAmt = 0.4f;
          v.fA = 2.0f; v.fD = 500.0f; v.fS = 0.4f; v.fR = 300.0f; v.decayMs = 1400.0f; v.sustain = 0.35f; v.poles = 2;
          s.lowCutHz = 120.0f; s.highDb = 1.5f; s.drive = 0.15f; s.glue = 0.55f; s.gainDb = 1.5f;
          auto& q = s.preset; q.air = 0.5f; q.attackMs = 0.0f; q.releaseS = 0.3f; q.width = 0.55f;
          q.reverbMix = 0.24f; q.reverbSize = 0.6f; }

        { auto& s = def (kSaturatedPiano, kCatHousePiano, "Saturated Piano", "Varm, kraftig og mettet");
          s.sampler = { 1.0f, 9.0f, 0.85f, 0.25f, 0.0f }; s.fallback = fb;
          s.lowCutHz = 90.0f; s.lowDb = 1.5f; s.highDb = 1.0f; s.drive = 0.3f; s.glue = 0.6f; s.tape = 0.15f; s.gainDb = 7.0f;
          auto& q = s.preset; q.air = 0.4f; q.attackMs = 0.0f; q.releaseS = 0.22f; q.width = 0.45f;
          q.satOn = true; q.satDrive = 0.22f; q.reverbMix = 0.18f; q.reverbSize = 0.5f; }

        //================ NEW SUPERSAWS ================
        { auto& s = def (kEuphoricSupersaw, kCatSupersaw, "Euphoric Supersaw", "Stor, bred og euforisk");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.35f;
          v.unison = 7; v.detune = 36.0f; v.spread = 1.0f;
          v.cutoff = 3000.0f; v.reso = 0.1f; v.envAmt = 0.8f; v.keytrack = 0.4f; v.velAmt = 0.3f;
          v.fA = 5.0f; v.fD = 800.0f; v.fS = 0.7f; v.fR = 600.0f; v.motion = 0.15f; v.poles = 2;
          s.lowCutHz = 180.0f; s.highDb = 1.0f; s.glue = 0.5f; s.gainDb = 2.8f;
          auto& q = s.preset; q.air = 0.55f; q.attackMs = 15.0f; q.releaseS = 0.8f; q.width = 0.85f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.15f;
          q.reverbMix = 0.38f; q.reverbSize = 0.8f; q.pumpOn = true; q.pumpDepth = 0.55f; }

        { auto& s = def (kSupersawStab, kCatSupersaw, "Supersaw Stab", "Kort og punchy supersaw");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.3f;
          v.unison = 7; v.detune = 30.0f; v.spread = 0.9f;
          v.cutoff = 900.0f; v.reso = 0.15f; v.envAmt = 3.2f; v.keytrack = 0.4f; v.velAmt = 0.6f;
          v.fA = 2.0f; v.fD = 220.0f; v.fS = 0.05f; v.fR = 150.0f; v.decayMs = 380.0f; v.sustain = 0.0f; v.poles = 4;
          s.lowCutHz = 150.0f; s.drive = 0.2f; s.glue = 0.55f; s.gainDb = 1.3f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 1.0f; q.releaseS = 0.15f; q.width = 0.75f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.18f; q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        //================ LEADS ================
        { auto& s = def (kSawLead, kCatLeads, "Saw Lead", "Fet og skarp");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.fine2 = 10.0f; v.mix2 = 0.8f;
          v.unison = 3; v.detune = 14.0f; v.spread = 0.45f; v.sub = 0.2f;
          v.cutoff = 2400.0f; v.reso = 0.2f; v.envAmt = 1.2f; v.keytrack = 0.5f; v.velAmt = 0.5f;
          v.fA = 2.0f; v.fD = 300.0f; v.fS = 0.6f; v.fR = 200.0f; v.poles = 4; v.drift = 3.0f;
          s.mono = true; s.glideMs = 60.0f; s.lowCutHz = 120.0f; s.drive = 0.3f; s.glue = 0.4f; s.gainDb = -1.4f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 3.0f; q.releaseS = 0.2f; q.width = 0.5f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.35f; q.delayMix = 0.22f; q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        { auto& s = def (kHouseLead, kCatLeads, "House Lead", "Klassisk og hul");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSquare; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.3f;
          v.unison = 2; v.detune = 8.0f; v.spread = 0.3f;
          v.cutoff = 1800.0f; v.reso = 0.25f; v.envAmt = 1.5f; v.keytrack = 0.5f; v.velAmt = 0.6f;
          v.fA = 2.0f; v.fD = 250.0f; v.fS = 0.5f; v.fR = 200.0f; v.poles = 4; v.drift = 2.0f;
          s.mono = true; s.glideMs = 40.0f; s.lowCutHz = 140.0f; s.drive = 0.2f; s.glue = 0.4f; s.gainDb = -1.9f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 2.0f; q.releaseS = 0.15f; q.width = 0.5f;
          q.chorusOn = true; q.chorusMix = 0.2f; q.delayOn = true; q.delayTime = 1; q.delayFeedback = 0.3f; q.delayMix = 0.18f;
          q.reverbMix = 0.22f; q.reverbSize = 0.55f; }

        { auto& s = def (kSupersawLead, kCatLeads, "Supersaw Lead", "Bred og episk");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.35f;
          v.unison = 7; v.detune = 26.0f; v.spread = 0.8f;
          v.cutoff = 3200.0f; v.reso = 0.1f; v.envAmt = 0.8f; v.keytrack = 0.4f; v.velAmt = 0.4f;
          v.fA = 3.0f; v.fD = 400.0f; v.fS = 0.7f; v.fR = 300.0f; v.poles = 2;
          s.mono = true; s.glideMs = 50.0f; s.lowCutHz = 150.0f; s.glue = 0.45f; s.gainDb = 0.5f;
          auto& q = s.preset; q.air = 0.5f; q.attackMs = 4.0f; q.releaseS = 0.3f; q.width = 0.7f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.35f; q.delayMix = 0.2f; q.reverbMix = 0.3f; q.reverbSize = 0.7f; }

        //================ MORE PIANOS ================
        { auto& s = def (kOctaveGrand, kCatRealPiano, "Octave Grand", "");
          s.sampler = { 1.0f, 18.0f, 1.0f, 0.35f, 0.35f }; s.fallback = fb;
          s.lowCutHz = 35.0f; s.glue = 0.2f; s.gainDb = 3.6f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 0.0f; q.releaseS = 0.4f; q.width = 0.48f;
          q.reverbMix = 0.28f; q.reverbSize = 0.7f; }

        { auto& s = def (kDeepNightPiano, kCatRealPiano, "Deep Night Piano", "");
          s.sampler = { 1.0f, 20.0f, 1.2f, 0.6f, 0.0f }; s.fallback = fb; s.fallback.hardness = 0.35f;
          s.lowCutHz = 30.0f; s.lowDb = 2.0f; s.highDb = -5.0f; s.glue = 0.25f; s.tape = 0.1f; s.gainDb = 6.5f;
          auto& q = s.preset; q.tone = 0.42f; q.air = 0.1f; q.attackMs = 0.0f; q.releaseS = 0.5f; q.width = 0.5f;
          q.delayOn = true; q.delayTime = 4; q.delayFeedback = 0.25f; q.delayMix = 0.12f;
          q.reverbMix = 0.4f; q.reverbSize = 0.85f; }

        { auto& s = def (kBrightPiano, kCatHousePiano, "Bright Piano", "");
          s.sampler = { 1.0f, 12.0f, 0.8f, 0.25f, 0.0f }; s.fallback = fb; s.fallback.hardness = 0.95f;
          s.lowCutHz = 100.0f; s.lowDb = -1.0f; s.highDb = 4.0f; s.drive = 0.1f; s.glue = 0.55f; s.gainDb = 3.0f;
          auto& q = s.preset; q.air = 0.75f; q.attackMs = 0.0f; q.releaseS = 0.25f; q.width = 0.5f;
          q.chorusOn = true; q.chorusMix = 0.12f; q.reverbMix = 0.2f; q.reverbSize = 0.5f; }

        { auto& s = def (kLofiHousePiano, kCatHousePiano, "Lo-fi House Piano", "");
          s.sampler = { 1.0f, 10.0f, 0.9f, 0.3f, 0.0f }; s.fallback = fb;
          s.lowCutHz = 140.0f; s.highDb = -3.0f; s.drive = 0.3f; s.glue = 0.6f; s.tape = 0.6f; s.gainDb = 5.9f;
          auto& q = s.preset; q.air = 0.2f; q.attackMs = 0.0f; q.releaseS = 0.25f; q.width = 0.45f;
          q.satOn = true; q.satDrive = 0.3f; q.chorusOn = true; q.chorusMix = 0.2f; q.reverbMix = 0.2f; q.reverbSize = 0.5f; }

        //================ SYNTHS ================
        { auto& s = def (kPolySynth, kCatSynths, "Poly Synth", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.fine2 = -7.0f; v.mix2 = 0.6f;
          v.unison = 3; v.detune = 14.0f; v.spread = 0.6f;
          v.cutoff = 1500.0f; v.reso = 0.15f; v.envAmt = 1.5f; v.keytrack = 0.5f; v.velAmt = 0.5f;
          v.fA = 3.0f; v.fD = 400.0f; v.fS = 0.5f; v.fR = 300.0f; v.motion = 0.08f; v.poles = 4;
          s.lowCutHz = 110.0f; s.glue = 0.4f; s.gainDb = 1.4f;
          auto& q = s.preset; q.air = 0.3f; q.attackMs = 3.0f; q.releaseS = 0.4f; q.width = 0.65f;
          q.chorusOn = true; q.chorusMix = 0.35f; q.reverbMix = 0.3f; q.reverbSize = 0.65f; }

        { auto& s = def (kAnalogBrass, kCatSynths, "Analog Brass", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.fine2 = 6.0f; v.mix2 = 0.7f;
          v.unison = 3; v.detune = 10.0f; v.spread = 0.5f;
          v.cutoff = 500.0f; v.reso = 0.12f; v.envAmt = 2.6f; v.keytrack = 0.5f; v.velAmt = 0.8f;
          v.fA = 120.0f; v.fD = 600.0f; v.fS = 0.45f; v.fR = 300.0f; v.poles = 4;
          s.lowCutHz = 100.0f; s.drive = 0.2f; s.glue = 0.45f; s.gainDb = 0.0f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 25.0f; q.releaseS = 0.3f; q.width = 0.6f;
          q.chorusOn = true; q.chorusMix = 0.25f; q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        { auto& s = def (kJunoStrings, kCatSynths, "Juno Strings", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.semi2 = -12.0f; v.mix2 = 0.3f;
          v.unison = 5; v.detune = 18.0f; v.spread = 0.8f;
          v.cutoff = 2000.0f; v.reso = 0.1f; v.envAmt = 0.5f; v.keytrack = 0.5f; v.velAmt = 0.2f;
          v.fA = 300.0f; v.fD = 1000.0f; v.fS = 0.7f; v.fR = 1000.0f; v.motion = 0.15f; v.poles = 2;
          s.lowCutHz = 120.0f; s.glue = 0.3f; s.gainDb = 0.4f;
          auto& q = s.preset; q.air = 0.3f; q.attackMs = 250.0f; q.releaseS = 1.2f; q.width = 0.75f;
          q.chorusOn = true; q.chorusMix = 0.6f; q.reverbMix = 0.35f; q.reverbSize = 0.75f; }

        { auto& s = def (kHollowSynth, kCatSynths, "Hollow Synth", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSquare; v.wave2 = kSquare; v.semi2 = 12.0f; v.mix2 = 0.5f;
          v.unison = 3; v.detune = 10.0f; v.spread = 0.55f;
          v.cutoff = 1400.0f; v.reso = 0.3f; v.envAmt = 1.0f; v.keytrack = 0.5f; v.velAmt = 0.5f;
          v.fA = 3.0f; v.fD = 350.0f; v.fS = 0.55f; v.fR = 250.0f; v.poles = 4;
          s.lowCutHz = 130.0f; s.glue = 0.4f; s.gainDb = 0.7f;
          auto& q = s.preset; q.air = 0.3f; q.attackMs = 2.0f; q.releaseS = 0.3f; q.width = 0.6f;
          q.chorusOn = true; q.chorusMix = 0.3f; q.delayOn = true; q.delayTime = 4; q.delayFeedback = 0.3f; q.delayMix = 0.16f;
          q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        //================ FAT SAW LEADS ================
        { auto& s = def (kFatSawLead, kCatLeads, "Fat Saw Lead", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.fine2 = 14.0f; v.mix2 = 1.0f;
          v.unison = 5; v.detune = 22.0f; v.spread = 0.6f; v.sub = 0.35f;
          v.cutoff = 3000.0f; v.reso = 0.15f; v.envAmt = 0.8f; v.keytrack = 0.4f; v.velAmt = 0.4f;
          v.fA = 2.0f; v.fD = 300.0f; v.fS = 0.7f; v.fR = 200.0f; v.poles = 4; v.drift = 3.0f;
          s.mono = true; s.glideMs = 50.0f; s.lowCutHz = 110.0f; s.drive = 0.5f; s.glue = 0.5f; s.gainDb = 2.2f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 3.0f; q.releaseS = 0.2f; q.width = 0.6f;
          q.satOn = true; q.satDrive = 0.35f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.35f; q.delayMix = 0.2f; q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        { auto& s = def (kOctaveSawLead, kCatLeads, "Octave Saw Lead", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.75f;
          v.unison = 3; v.detune = 16.0f; v.spread = 0.5f; v.sub = 0.3f;
          v.cutoff = 3500.0f; v.reso = 0.12f; v.envAmt = 0.6f; v.keytrack = 0.4f; v.velAmt = 0.4f;
          v.fA = 2.0f; v.fD = 300.0f; v.fS = 0.75f; v.fR = 200.0f; v.poles = 4; v.drift = 2.0f;
          s.mono = true; s.glideMs = 45.0f; s.lowCutHz = 120.0f; s.drive = 0.35f; s.glue = 0.45f; s.gainDb = -1.0f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 3.0f; q.releaseS = 0.2f; q.width = 0.55f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.35f; q.delayMix = 0.2f; q.reverbMix = 0.25f; q.reverbSize = 0.6f; }

        { auto& s = def (kWideSawLead, kCatLeads, "Wide Saw Lead", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = -12.0f; v.mix2 = 0.4f;
          v.unison = 7; v.detune = 40.0f; v.spread = 1.0f;
          v.cutoff = 2600.0f; v.reso = 0.1f; v.envAmt = 0.7f; v.keytrack = 0.4f; v.velAmt = 0.3f;
          v.fA = 3.0f; v.fD = 400.0f; v.fS = 0.7f; v.fR = 300.0f; v.poles = 2;
          s.mono = true; s.glideMs = 60.0f; s.lowCutHz = 140.0f; s.drive = 0.25f; s.glue = 0.45f; s.gainDb = -1.6f;
          auto& q = s.preset; q.air = 0.5f; q.attackMs = 4.0f; q.releaseS = 0.3f; q.width = 0.75f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.35f; q.delayMix = 0.2f; q.reverbMix = 0.3f; q.reverbSize = 0.7f; }

        { auto& s = def (kScreamLead, kCatLeads, "Scream Lead", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.fine2 = 7.0f; v.mix2 = 0.6f;
          v.unison = 3; v.detune = 18.0f; v.spread = 0.5f;
          v.cutoff = 1600.0f; v.reso = 0.55f; v.envAmt = 1.8f; v.keytrack = 0.5f; v.velAmt = 0.6f;
          v.fA = 2.0f; v.fD = 350.0f; v.fS = 0.55f; v.fR = 200.0f; v.poles = 4; v.drift = 3.0f;
          s.mono = true; s.glideMs = 70.0f; s.lowCutHz = 150.0f; s.drive = 0.6f; s.glue = 0.5f; s.gainDb = 3.6f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 3.0f; q.releaseS = 0.2f; q.width = 0.55f;
          q.satOn = true; q.satDrive = 0.5f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.18f; q.reverbMix = 0.22f; q.reverbSize = 0.6f; }

        //================ MORE BASS ================
        { auto& s = def (kReeseBass, kCatBass, "Reese Bass", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.fine2 = 18.0f; v.mix2 = 1.0f; v.sub = 0.5f;
          v.unison = 2; v.detune = 10.0f; v.spread = 0.0f;
          v.cutoff = 700.0f; v.reso = 0.1f; v.envAmt = 0.4f; v.keytrack = 0.3f; v.velAmt = 0.3f;
          v.fA = 2.0f; v.fD = 300.0f; v.fS = 0.8f; v.fR = 150.0f; v.motion = 0.25f; v.poles = 4; v.drift = 1.0f;
          s.mono = true; s.glideMs = 20.0f; s.lowCutHz = 28.0f; s.drive = 0.35f; s.glue = 0.45f; s.gainDb = 6.8f;
          auto& q = s.preset; q.air = 0.1f; q.attackMs = 3.0f; q.releaseS = 0.1f; q.width = 0.0f; q.reverbOn = false;
          q.satOn = true; q.satDrive = 0.3f; }

        { auto& s = def (kOrganBass, kCatBass, "Organ Bass", "");
          s.organ.level = 0.8f; s.organ.bars = { 0.0f, 1.0f, 0.5f, 0.3f, 0.0f }; s.organ.decay = 0.9f; s.organ.click = 0.15f;
          auto& v = s.va; v.level = 0.5f; v.wave1 = kSine; v.cutoff = 600.0f; v.reso = 0.0f; v.keytrack = 0.0f; v.velAmt = 0.0f;
          v.poles = 2; v.drift = 0.0f; v.decayMs = 700.0f; v.sustain = 0.5f;
          s.mono = true; s.lowCutHz = 28.0f; s.drive = 0.2f; s.glue = 0.4f; s.gainDb = 4.7f;
          auto& q = s.preset; q.air = 0.1f; q.attackMs = 2.0f; q.releaseS = 0.08f; q.width = 0.0f; q.reverbOn = false; }

        { auto& s = def (kSawBass, kCatBass, "Saw Bass", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.sub = 0.6f;
          v.cutoff = 500.0f; v.reso = 0.2f; v.envAmt = 1.5f; v.keytrack = 0.3f; v.velAmt = 0.6f;
          v.fA = 1.0f; v.fD = 250.0f; v.fS = 0.3f; v.fR = 120.0f; v.decayMs = 600.0f; v.sustain = 0.6f; v.poles = 4; v.drift = 1.0f;
          s.mono = true; s.lowCutHz = 28.0f; s.drive = 0.3f; s.glue = 0.4f; s.gainDb = 6.2f;
          auto& q = s.preset; q.air = 0.1f; q.attackMs = 2.0f; q.releaseS = 0.08f; q.width = 0.0f; q.reverbOn = false; }

        { auto& s = def (kAcidBass, kCatBass, "Acid Bass", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.sub = 0.25f;
          v.cutoff = 300.0f; v.reso = 0.8f; v.envAmt = 3.5f; v.keytrack = 0.3f; v.velAmt = 1.0f;
          v.fA = 1.0f; v.fD = 180.0f; v.fS = 0.1f; v.fR = 100.0f; v.decayMs = 400.0f; v.sustain = 0.5f; v.poles = 4; v.drift = 1.0f;
          s.mono = true; s.glideMs = 80.0f; s.lowCutHz = 30.0f; s.drive = 0.5f; s.glue = 0.45f; s.gainDb = 3.3f;
          auto& q = s.preset; q.air = 0.15f; q.attackMs = 1.5f; q.releaseS = 0.08f; q.width = 0.0f; q.reverbOn = false;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.25f; q.delayMix = 0.12f; }

        //================ MORE PLUCKS ================
        { auto& s = def (kBrightPluck, kCatPlucks, "Bright Pluck", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSquare; v.semi2 = 12.0f; v.mix2 = 0.35f;
          v.unison = 3; v.detune = 12.0f; v.spread = 0.55f;
          v.cutoff = 900.0f; v.reso = 0.25f; v.envAmt = 4.5f; v.keytrack = 0.5f; v.velAmt = 0.8f;
          v.fA = 3.0f; v.fD = 120.0f; v.fS = 0.0f; v.fR = 100.0f; v.decayMs = 300.0f; v.sustain = 0.0f; v.poles = 4;
          s.lowCutHz = 150.0f; s.highDb = 1.5f; s.glue = 0.55f; s.gainDb = 1.0f;
          auto& q = s.preset; q.air = 0.55f; q.attackMs = 1.0f; q.releaseS = 0.15f; q.width = 0.65f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.2f; q.reverbMix = 0.22f; q.reverbSize = 0.55f; }

        { auto& s = def (kSupersawPluck, kCatPlucks, "Supersaw Pluck", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.wave2 = kSaw; v.semi2 = 12.0f; v.mix2 = 0.3f;
          v.unison = 7; v.detune = 28.0f; v.spread = 0.9f;
          v.cutoff = 600.0f; v.reso = 0.15f; v.envAmt = 4.2f; v.keytrack = 0.5f; v.velAmt = 0.7f;
          v.fA = 3.0f; v.fD = 200.0f; v.fS = 0.0f; v.fR = 150.0f; v.decayMs = 500.0f; v.sustain = 0.0f; v.poles = 4;
          s.lowCutHz = 160.0f; s.glue = 0.55f; s.gainDb = 0.7f;
          auto& q = s.preset; q.air = 0.45f; q.attackMs = 1.0f; q.releaseS = 0.2f; q.width = 0.75f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.18f; q.reverbMix = 0.3f; q.reverbSize = 0.65f; }

        { auto& s = def (kMalletPluck, kCatPlucks, "Mallet Pluck", "");
          s.fm.level = 1.0f; s.fm.ratioA = 4.0f; s.fm.indexA = 1.2f; s.fm.susA = 0.0f; s.fm.decA = 0.04f;
          s.fm.mixB = 0.0f; s.fm.decay = 0.45f;
          s.lowCutHz = 140.0f; s.glue = 0.4f; s.gainDb = 2.4f;
          auto& q = s.preset; q.air = 0.35f; q.attackMs = 0.5f; q.releaseS = 0.2f; q.width = 0.6f;
          q.delayOn = true; q.delayTime = 2; q.delayFeedback = 0.3f; q.delayMix = 0.18f; q.reverbMix = 0.3f; q.reverbSize = 0.6f; }

        { auto& s = def (kDeepPluck, kCatPlucks, "Deep Pluck", "");
          auto& v = s.va; v.level = 1.0f; v.wave1 = kSaw; v.sub = 0.5f;
          v.cutoff = 250.0f; v.reso = 0.2f; v.envAmt = 3.5f; v.keytrack = 0.5f; v.velAmt = 0.7f;
          v.fA = 3.0f; v.fD = 280.0f; v.fS = 0.0f; v.fR = 200.0f; v.decayMs = 600.0f; v.sustain = 0.0f; v.poles = 4;
          s.lowCutHz = 60.0f; s.glue = 0.5f; s.gainDb = 4.1f;
          auto& q = s.preset; q.air = 0.25f; q.attackMs = 1.0f; q.releaseS = 0.25f; q.width = 0.6f;
          q.reverbMix = 0.3f; q.reverbSize = 0.6f; q.chorusOn = true; q.chorusMix = 0.2f; }

        //================ YOUR SAMPLE ================
        { auto& s = def (kDroppedSample, kCatUser, "Ditt sample", "");
          s.sampler = { 1.0f, 10.0f, 1.0f, 0.0f, 0.0f, true };
          s.lowCutHz = 30.0f; s.glue = 0.3f; s.gainDb = 3.5f;
          auto& q = s.preset; q.air = 0.3f; q.attackMs = 0.5f; q.releaseS = 0.3f; q.width = 0.5f;
          q.reverbMix = 0.18f; q.reverbSize = 0.55f; }

        return d;
    }();
    return defs;
}
} // namespace hk
