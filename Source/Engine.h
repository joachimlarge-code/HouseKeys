// HouseKeys 2 - sound engine.
// Plain C++17 with no JUCE dependency, so the plugin and the offline demo
// renderer (tools/render_demo.cpp) run exactly the same code.
//
// Every piano is a mix of three sound sources:
//   - Acoustic: additive "modelled" piano (inharmonic partials, two-stage decay,
//               detuned strings, hammer noise, damper on release)
//   - Organ:    drawbar sines with key click (optionally decaying)
//   - FM:       two FM operator pairs for electric pianos, stabs and bells
// followed by a per-piano colour chain (EQ, drive, glue compressor, tape) and
// the user effects (filter, chorus, delay, reverb, pump), each with on/off.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace hk
{
static constexpr float kPi    = 3.14159265358979f;
static constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clamp01  (float x)  { return std::min (1.0f, std::max (0.0f, x)); }
inline float onePoleCoef (float hz, float sr) { return 1.0f - std::exp (-kTwoPi * hz / sr); }

//==============================================================================
// Chords
enum Chord { kChordOff = 0, kMin7, kMin9, kMin11, kMaj7, kMaj9, kSus, kNumChords };

inline const std::vector<int>& chordIntervals (int c)
{
    static const std::vector<int> table[kNumChords] = {
        { 0 }, { 0, 3, 7, 10 }, { 0, 3, 7, 10, 14 }, { 0, 3, 7, 10, 14, 17 },
        { 0, 4, 7, 11 }, { 0, 4, 7, 11, 14 }, { 0, 5, 7, 10 } };
    return table[std::clamp (c, 0, kNumChords - 1)];
}

//==============================================================================
// Piano definitions
struct AcousticDef
{
    float level    = 0.0f;
    float hardness = 0.6f;   // hammer hardness: 0 felt .. 1 very bright
    float strike   = 0.12f;  // strike position along the string (shapes the spectrum)
    float decay    = 5.0f;   // seconds at middle C (key tracked)
    float inharm   = 1.0f;   // inharmonicity scale
    float detune   = 1.2f;   // string detune in cents (honky-tonk when high)
    float damper   = 0.15f;  // release time when the key is lifted (s)
    float hammer   = 0.4f;   // hammer noise amount
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
    float ratioA = 1, indexA = 1, susA = 0.2f, decA = 0.5f;    // body pair
    float octB = 1, ratioB = 14, indexB = 1, decB = 0.06f, susB = 0; // tine / bite pair
    float mixB = 0.5f;
    float feedback = 0;
    float decay = 3.0f, sustain = 0.0f;  // amp envelope
    float bark = 0.0f;                   // velocity driven asymmetric drive (EP growl)
};

struct PianoDef
{
    const char* name;
    const char* tag;          // short description for the UI
    bool        energetic;
    AcousticDef ac;
    OrganDef    organ;
    FmDef       fm;
    float attackMs = 0.5f;
    float release  = 0.25f;   // organ / FM release time (s)
    float lowDb = 0, highDb = 0, drive = 0, glue = 0.3f, tape = 0, gainDb = 0;
};

enum PianoId
{
    kClassicHouse = 0, kRaveStab, kGarageBounce, kBigRoomGrand, kDiscoUpright, kPartyOrgan,
    kGrandPiano, kSoftFelt, kDeepRhodes, kWurliDream, kLofiKeys, kGlassBells,
    kNumPianos
};

inline const std::array<PianoDef, kNumPianos>& pianos()
{
    static const std::array<PianoDef, kNumPianos> defs = [] {
        std::array<PianoDef, kNumPianos> d {};
        auto def = [&d] (int id, const char* name, const char* tag, bool energetic) -> PianoDef&
        {
            d[(size_t) id] = PianoDef {};
            d[(size_t) id].name = name; d[(size_t) id].tag = tag; d[(size_t) id].energetic = energetic;
            return d[(size_t) id];
        };

        // ---------------- ENERGETIC ----------------
        { auto& p = def (kClassicHouse, "Classic House", "90s house-piano", true);
          p.gainDb = 5.0f;
          p.ac = { 0.85f, 0.88f, 0.11f, 2.2f, 1.6f, 2.5f, 0.12f, 0.35f };
          p.organ.level = 0.40f; p.organ.bars = { 0.0f, 1.0f, 0.55f, 0.0f, 0.25f }; p.organ.decay = 2.2f; p.organ.click = 0.0f;
          p.fm.level = 0.22f; p.fm.ratioA = 1; p.fm.indexA = 2.4f; p.fm.susA = 0.1f; p.fm.decA = 0.25f;
          p.fm.mixB = 0.0f; p.fm.decay = 1.2f;
          p.lowDb = 1.0f; p.highDb = 3.0f; p.drive = 0.30f; p.glue = 0.65f; }

        { auto& p = def (kRaveStab, "Rave Stab", "Kort og hard", true);
          p.gainDb = 6.0f;
          p.ac = { 0.45f, 1.0f, 0.10f, 1.0f, 1.5f, 4.0f, 0.08f, 0.5f };
          p.organ.level = 0.35f; p.organ.bars = { 0.6f, 1.0f, 0.4f, 0.0f, 0.0f }; p.organ.decay = 0.7f; p.organ.click = 0.2f;
          p.fm.level = 0.75f; p.fm.ratioA = 1; p.fm.indexA = 3.4f; p.fm.susA = 0.15f; p.fm.decA = 0.22f;
          p.fm.octB = 1; p.fm.ratioB = 2; p.fm.indexB = 2.5f; p.fm.decB = 0.15f; p.fm.susB = 0.1f; p.fm.mixB = 0.5f;
          p.fm.feedback = 0.25f; p.fm.decay = 0.55f;
          p.release = 0.12f; p.lowDb = 2.0f; p.highDb = 2.5f; p.drive = 0.55f; p.glue = 0.75f; }

        { auto& p = def (kGarageBounce, "Garage Bounce", "Punchy med bite", true);
          p.gainDb = 5.0f;
          p.ac = { 0.40f, 0.8f, 0.12f, 2.0f, 1.2f, 3.0f, 0.10f, 0.3f };
          p.fm.level = 0.85f; p.fm.ratioA = 1; p.fm.indexA = 2.0f; p.fm.susA = 0.15f; p.fm.decA = 0.35f;
          p.fm.octB = 1; p.fm.ratioB = 3; p.fm.indexB = 1.2f; p.fm.decB = 0.12f; p.fm.mixB = 0.35f;
          p.fm.decay = 1.6f; p.fm.bark = 0.6f;
          p.release = 0.15f; p.lowDb = 1.5f; p.highDb = 1.5f; p.drive = 0.35f; p.glue = 0.6f; }

        { auto& p = def (kBigRoomGrand, "Big Room Grand", "Stort og lyst", true);
          p.gainDb = 4.0f;
          p.ac = { 1.0f, 0.97f, 0.12f, 4.0f, 1.0f, 2.0f, 0.18f, 0.45f };
          p.organ.level = 0.12f; p.organ.bars = { 0.0f, 1.0f, 0.5f, 0.0f, 0.0f }; p.organ.decay = 3.0f; p.organ.click = 0.0f;
          p.lowDb = 2.5f; p.highDb = 4.0f; p.drive = 0.20f; p.glue = 0.65f; }

        { auto& p = def (kDiscoUpright, "Disco Upright", "Honky-tonk groove", true);
          p.gainDb = 3.3f;
          p.ac = { 1.0f, 0.78f, 0.10f, 2.6f, 1.4f, 9.0f, 0.12f, 0.5f };
          p.lowDb = -1.0f; p.highDb = 2.0f; p.drive = 0.15f; p.glue = 0.5f; }

        { auto& p = def (kPartyOrgan, "Party Organ", "Garage-orgel", true);
          p.gainDb = 1.5f;
          p.organ.level = 1.0f; p.organ.bars = { 0.5f, 1.0f, 0.6f, 0.4f, 0.3f }; p.organ.decay = 0.0f; p.organ.click = 0.45f;
          p.attackMs = 2.0f; p.release = 0.08f; p.lowDb = 1.0f; p.highDb = 1.0f; p.drive = 0.35f; p.glue = 0.45f; }

        // ---------------- CALM ----------------
        { auto& p = def (kGrandPiano, "Grand Piano", "Naturlig flygel", false);
          p.gainDb = -0.5f;
          p.ac = { 1.0f, 0.60f, 0.12f, 7.0f, 1.0f, 1.0f, 0.25f, 0.30f };
          p.lowDb = 1.0f; p.highDb = 0.0f; p.glue = 0.2f; }

        { auto& p = def (kSoftFelt, "Soft Felt", "Mykt og intimt", false);
          p.gainDb = 3.0f;
          p.ac = { 1.0f, 0.12f, 0.16f, 4.0f, 0.9f, 1.5f, 0.35f, 0.7f };
          p.lowDb = 2.0f; p.highDb = -4.0f; p.glue = 0.3f; p.tape = 0.12f; }

        { auto& p = def (kDeepRhodes, "Deep Rhodes", "Varm el-piano", false);
          p.gainDb = -0.5f;
          p.fm.level = 1.0f; p.fm.ratioA = 1; p.fm.indexA = 1.3f; p.fm.susA = 0.25f; p.fm.decA = 1.2f;
          p.fm.octB = 1; p.fm.ratioB = 14; p.fm.indexB = 1.6f; p.fm.decB = 0.06f; p.fm.mixB = 0.45f;
          p.fm.decay = 4.5f; p.fm.bark = 0.35f;
          p.attackMs = 1.0f; p.release = 0.35f; p.lowDb = 2.0f; p.highDb = -1.0f; p.drive = 0.10f; p.glue = 0.35f; }

        { auto& p = def (kWurliDream, "Wurli Dream", "Drømmende reed-piano", false);
          p.gainDb = 0.5f;
          p.fm.level = 1.0f; p.fm.ratioA = 1; p.fm.indexA = 1.7f; p.fm.susA = 0.3f; p.fm.decA = 0.6f;
          p.fm.octB = 1; p.fm.ratioB = 3; p.fm.indexB = 0.7f; p.fm.decB = 0.2f; p.fm.mixB = 0.3f;
          p.fm.decay = 3.0f; p.fm.bark = 0.5f;
          p.release = 0.3f; p.lowDb = 1.0f; p.highDb = -2.0f; p.drive = 0.2f; p.glue = 0.35f; p.tape = 0.35f; }

        { auto& p = def (kLofiKeys, "Lo-fi Keys", "Støvete tape-piano", false);
          p.gainDb = 1.5f;
          p.ac = { 1.0f, 0.45f, 0.13f, 3.5f, 1.2f, 4.0f, 0.25f, 0.5f };
          p.lowDb = 2.0f; p.highDb = -5.0f; p.drive = 0.25f; p.glue = 0.45f; p.tape = 0.8f; }

        { auto& p = def (kGlassBells, "Glass Bells", "Klokker og glitter", false);
          p.gainDb = -0.5f;
          p.ac = { 0.35f, 0.55f, 0.13f, 5.0f, 1.0f, 1.0f, 0.3f, 0.1f };
          p.fm.level = 0.8f; p.fm.ratioA = 3.5f; p.fm.indexA = 1.4f; p.fm.susA = 0.3f; p.fm.decA = 1.5f;
          p.fm.octB = 2; p.fm.ratioB = 1.41f; p.fm.indexB = 1.0f; p.fm.decB = 0.8f; p.fm.mixB = 0.4f;
          p.fm.decay = 5.0f;
          p.release = 0.6f; p.highDb = 1.0f; p.glue = 0.25f; }
        return d;
    }();
    return defs;
}

//==============================================================================
// User parameters
struct Params
{
    int   piano  = kClassicHouse;
    float tone   = 0.5f;   // 0 dark .. 1 bright (0.5 = as designed)
    float decay  = 0.5f;   // 0 short .. 1 long  (0.5 = as designed)
    float width  = 0.6f;   // 0 mono .. 1 extra wide
    float volumeDb = -3.0f;
    int   chord  = kChordOff;

    bool  filterOn = false; float cutoffHz = 3000.0f; float resonance = 0.2f;
    bool  chorusOn = true;  float chorusMix = 0.3f;
    bool  delayOn  = false; int delayTime = 2; float delayFeedback = 0.35f; float delayMix = 0.25f;
    bool  reverbOn = true;  float reverbMix = 0.25f; float reverbSize = 0.5f;
    bool  pumpOn   = false; float pumpDepth = 0.6f; int pumpRate = 0;
};

inline double delayBeats (int idx)
{
    static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return b[std::clamp (idx, 0, 5)];
}

//==============================================================================
struct Smoother
{
    float value = 0.0f, coef = 0.999f;
    void  init (float v, double sr, float ms) { value = v; coef = std::exp (-1.0f / (float) (ms * 0.001 * sr)); }
    float next (float target) { value = target + (value - target) * coef; return value; }
};

// Fractional-read delay line
struct DelayLine
{
    std::vector<float> buf; int w = 0, len = 1;
    void  init (int n) { len = std::max (4, n); buf.assign ((size_t) len, 0.0f); w = 0; }
    void  push (float x) { buf[(size_t) w] = x; if (++w >= len) w = 0; }
    float read (float d) const  // d samples behind the most recent push
    {
        float pos = (float) w - 1.0f - d;
        while (pos < 0.0f) pos += (float) len;
        int i0 = (int) pos; const float fr = pos - (float) i0;
        if (i0 >= len) i0 -= len;
        const int i1 = (i0 + 1 == len) ? 0 : i0 + 1;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
};

//==============================================================================
// Lush 8-line feedback delay network reverb
class FdnReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        static const float baseMs[8] = { 31.7f, 37.3f, 41.9f, 47.3f, 53.1f, 59.3f, 67.1f, 73.7f };
        static const float apMs[4]   = { 4.77f, 3.59f, 12.73f, 9.30f };
        for (int i = 0; i < 8; ++i) { base[i] = baseMs[i] * 0.001f * sr; lines[i].init ((int) (base[i] * 1.7f) + 64); lp[i] = 0; }
        for (int i = 0; i < 4; ++i) { apLen[i] = (int) (apMs[i] * 0.001f * sr); ap[i].assign ((size_t) apLen[i], 0.0f); apW[i] = 0; }
        pre.init ((int) (0.05f * sr));
        lfo = 0.0;
    }

    void process (float inL, float inR, float size, float& outL, float& outR)
    {
        const float scale = 0.55f + 1.05f * size;
        const float rt60  = 0.7f + 6.0f * size * size;
        const float damp  = 0.42f - 0.15f * size;   // high-frequency damping

        // input: predelay + diffusion
        pre.push ((inL + inR) * 0.5f);
        float x = pre.read (0.012f * sr);
        for (int i = 0; i < 4; ++i)
        {
            float& slot = ap[i][(size_t) apW[i]];
            const float y = slot - 0.6f * x;
            slot = x + 0.6f * y;
            if (++apW[i] >= apLen[i]) apW[i] = 0;
            x = y;
        }

        lfo += 0.31 / sr; lfo -= std::floor (lfo);
        const float m = std::sin (kTwoPi * (float) lfo) * 0.0006f * sr;

        float v[8];
        for (int i = 0; i < 8; ++i)
        {
            const float d = base[i] * scale + (i & 1 ? m : -m) * (i < 4 ? 1.0f : 0.5f);
            const float o = lines[i].read (d);
            lp[i] += (1.0f - damp) * (o - lp[i]);
            const float g = std::pow (10.0f, -3.0f * (d / sr) / rt60);
            v[i] = lp[i] * g;
            out[i] = o;
        }
        hadamard8 (v);
        for (int i = 0; i < 8; ++i) lines[i].push (v[i] + x * ((i & 1) ? -0.5f : 0.5f));

        outL = (out[0] - out[2] + out[4] - out[6]) * 0.45f;
        outR = (out[1] - out[3] + out[5] - out[7]) * 0.45f;
    }

private:
    static void hadamard8 (float* v)
    {
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b; v[j + h] = a - b;
                }
        for (int i = 0; i < 8; ++i) v[i] *= 0.35355339f;
    }

    float sr = 44100.0f;
    DelayLine lines[8], pre;
    float base[8] {}, lp[8] {}, out[8] {};
    std::vector<float> ap[4]; int apLen[4] {}, apW[4] {};
    double lfo = 0;
};

//==============================================================================
class Engine
{
public:
    static constexpr int kMaxVoices   = 40;
    static constexpr int kMaxPartials = 40;
    static constexpr int kBeatPartials = 8;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        fs = (float) sr;
        block = std::max (32, std::min (maxBlock, 1024));
        mono.assign ((size_t) block, 0.0f);
        voices = {};
        reverb.prepare (sr);
        chorusLine.init ((int) (0.05 * sr));
        tapeL.init ((int) (0.03 * sr)); tapeR.init ((int) (0.03 * sr));
        delayL.init ((int) (3.2 * sr)); delayR.init ((int) (3.2 * sr));
        resetState();
        needsSnap = true;
    }

    void setParams (const Params& np) { p = np; }
    const Params& params() const { return p; }

    //--------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        noteOffInternal (note, true);
        const auto& iv = chordIntervals (p.chord);
        const float norm = 1.0f / std::sqrt ((float) iv.size());
        for (size_t k = 0; k < iv.size(); ++k)
        {
            const int pitch = note + iv[k];
            if (pitch < 0 || pitch > 127) continue;
            Voice& v = allocate();
            startVoice (v, pitch, note, clamp01 (velocity), norm * (k == 0 ? 1.0f : 0.9f));
        }
    }

    void noteOff (int note) { noteOffInternal (note, false); }

    void setSustainPedal (bool down)
    {
        pedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && v.sustained) { v.sustained = false; releaseVoice (v); }
    }

    void allNotesOff()
    {
        for (auto& v : voices) if (v.active) { v.sustained = false; releaseVoice (v); }
    }

    // bpm / ppq / playing come from the host. When stopped, tempo effects run
    // on an internal clock at the host tempo.
    void process (float* outL, float* outR, int numSamples, double bpm, double ppqStart, bool playing)
    {
        int done = 0;
        while (done < numSamples)
        {
            const int n = std::min (block, numSamples - done);
            const double ppq = playing ? ppqStart + done * (bpm / 60.0 / sr) : freePpq;
            processChunk (outL + done, outR + done, n, bpm, ppq);
            if (! playing) freePpq += n * (bpm / 60.0 / sr);
            done += n;
        }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.active ? 1 : 0; return c; }

private:
    //==========================================================================
    struct Partial
    {
        float re, im, c, s;      // main oscillator (rotating phasor)
        float re2, im2, c2, s2;  // detuned second string (low partials only)
        float e1, e2, d1, d2;    // prompt / aftersound envelopes and decay coefficients
        float rel;               // damper coefficient used after release
        bool  beat, on;
    };

    struct Voice
    {
        bool active = false, sustained = false, released = false;
        int note = -1, trigger = -1, piano = 0;
        uint64_t order = 0;
        float vel = 0, gain = 1, panAc = 0;
        float age = 0;

        // acoustic
        int numPartials = 0;
        std::array<Partial, kMaxPartials> part {};
        float hamEnv = 0, hamB1 = 0, hamB2 = 0, hamA = 0, hamR = 0, hamGain = 0;
        uint32_t rng = 1;

        // organ
        std::array<float, 5> orRe {}, orIm {}, orC {}, orS {};
        float orAmp = 0, orDecay = 1, orRel = 1, orClick = 0;

        // fm
        double ph[4] {};
        float fmAmp = 0, fmIdxA = 1, fmIdxB = 1, fbState = 0;
        int fmStage = 0;

        float attack = 1.0f, attackInc = 1.0f;
    };

    //--------------------------------------------------------------------------
    void resetState()
    {
        for (float* f : { &loL, &loR, &hiL, &hiR, &ic1L, &ic2L, &ic1R, &ic2R, &compEnv, &tapeLpL, &tapeLpR,
                          &hissLp, &dlFbL, &dlFbR, &dlHpL, &dlHpR, &dcL, &dcR, &dcInL, &dcInR })
            *f = 0.0f;
        wowPh = flutterPh = chPh = freePpq = 0.0;
    }

    void noteOffInternal (int note, bool hard)
    {
        for (auto& v : voices)
        {
            if (! v.active || v.trigger != note || v.released) continue;
            if (pedal && ! hard) v.sustained = true;
            else                 { v.sustained = false; releaseVoice (v); }
        }
    }

    Voice& allocate()
    {
        for (auto& v : voices) if (! v.active) return v;
        Voice* best = &voices[0];
        for (auto& v : voices)
        {
            if (v.released != best->released) { if (v.released) best = &v; continue; }
            if (v.order < best->order) best = &v;
        }
        return *best;
    }

    static float noteHz (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); }

    float decayScale() const { return std::pow (2.0f, (p.decay - 0.5f) * 4.0f); }       // 0.25x .. 4x
    float toneOffset() const { return (p.tone - 0.5f) * 2.0f; }                          // -1 .. +1

    //--------------------------------------------------------------------------
    void startVoice (Voice& v, int pitch, int trigger, float vel, float gain)
    {
        const PianoDef& d = pianos()[(size_t) std::clamp (p.piano, 0, kNumPianos - 1)];
        v = Voice {};
        v.active = true; v.note = pitch; v.trigger = trigger; v.piano = p.piano;
        v.order = ++counter; v.vel = vel; v.gain = gain;
        v.rng = 0x9E3779B9u ^ (uint32_t) (counter * 2654435761u);
        v.attack = 0.0f;   // short ramp avoids clicks (partials start at random phases)
        v.attackInc = 1.0f / (std::max (0.8f, d.attackMs) * 0.001f * fs);

        const float f  = noteHz (pitch);
        const float ds = decayScale();
        const float kt = std::clamp (std::pow (2.0f, -(pitch - 60) / 18.0f), 0.12f, 3.0f); // key tracked decay
        v.panAc = std::clamp ((pitch - 64) / 40.0f, -1.0f, 1.0f) * 0.35f;

        // ---- acoustic: additive partials
        if (d.ac.level > 0.0f)
        {
            const AcousticDef& a = d.ac;
            const float hard = clamp01 (a.hardness + 0.3f * toneOffset()) * (0.55f + 0.45f * vel);
            const float B    = std::clamp (0.00012f * std::exp ((pitch - 48) * 0.055f) * a.inharm, 0.00002f, 0.02f);
            const float kc   = 2.5f + 34.0f * hard * hard;                 // spectral roll-off
            const float tau  = a.decay * kt * ds;
            const float detR = std::pow (2.0f, a.detune / 1200.0f);
            float norm = 0.0f;
            int n = 0;
            for (int k = 1; k <= kMaxPartials; ++k)
            {
                const float fk = f * (float) k * std::sqrt (1.0f + B * (float) (k * k));
                if (fk > 0.45f * fs) break;
                const float shape = std::abs (std::sin (kPi * (float) k * a.strike)) + 0.08f;
                const float amp   = shape * std::exp (-(float) (k - 1) / kc) / std::pow ((float) k, 0.8f);
                if (amp < 0.0005f) continue;
                Partial& pt = v.part[(size_t) n++];
                const float w = kTwoPi * fk / fs;
                pt.c = std::cos (w); pt.s = std::sin (w);
                const float ph0 = 2.39996f * (float) (k * k);   // spread phases: no buzzy pulse at the attack
                pt.re = std::cos (ph0); pt.im = std::sin (ph0);
                pt.beat = k <= kBeatPartials && a.detune > 0.05f;
                const float w2 = w * (k % 2 ? detR : 1.0f / detR);
                pt.c2 = std::cos (w2); pt.s2 = std::sin (w2);
                pt.re2 = std::cos (ph0 + 1.3f * (float) k); pt.im2 = std::sin (ph0 + 1.3f * (float) k);
                const float tk = tau / (1.0f + 0.045f * std::pow ((float) (k - 1), 1.35f) * (0.6f + hard));
                pt.d1 = std::exp (-1.0f / (std::max (0.02f, tk * 0.22f) * fs));  // prompt sound
                pt.d2 = std::exp (-1.0f / (std::max (0.04f, tk * 1.4f) * fs));   // aftersound
                pt.e1 = amp * 0.65f;
                pt.e2 = amp * 0.35f;
                const float relT = a.damper / (1.0f + 0.15f * (float) (k - 1)) * (pitch > 88 ? 6.0f : 1.0f);
                pt.rel = std::exp (-1.0f / (std::max (0.01f, relT) * fs));
                pt.on = true;
                norm += amp;
            }
            v.numPartials = n;
            const float lvl = a.level * (0.35f + 0.65f * vel) * 1.6f / std::max (1.0f, std::sqrt (norm) * 1.3f);
            for (int i = 0; i < n; ++i) { v.part[(size_t) i].e1 *= lvl; v.part[(size_t) i].e2 *= lvl; }

            // hammer noise: band-passed burst
            const float fc = std::clamp (f * (1.5f + 4.0f * hard), 300.0f, 6000.0f);
            const float bw = 0.7f;
            const float rr = std::exp (-kPi * bw * fc / fs);
            v.hamA = 2.0f * rr * std::cos (kTwoPi * fc / fs);
            v.hamR = rr * rr;
            v.hamEnv = 1.0f;
            v.hamGain = a.hammer * a.level * (0.15f + 0.85f * vel) * (0.25f + 0.75f * hard) * 0.12f;
        }

        // ---- organ
        if (d.organ.level > 0.0f)
        {
            static const float mult[5] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
            for (int i = 0; i < 5; ++i)
            {
                const float w = kTwoPi * std::min (f * mult[i], 0.45f * fs) / fs;
                v.orC[(size_t) i] = std::cos (w); v.orS[(size_t) i] = std::sin (w);
                v.orRe[(size_t) i] = 1.0f; v.orIm[(size_t) i] = 0.0f;
            }
            v.orAmp = d.organ.level * (0.6f + 0.4f * vel);
            v.orDecay = d.organ.decay > 0.0f ? std::exp (-1.0f / (d.organ.decay * kt * ds * 0.35f * fs)) : 1.0f;
            v.orRel = std::exp (-1.0f / (std::max (0.01f, d.release) * 0.3f * fs));
            v.orClick = d.organ.click * d.organ.level * 0.25f;
        }

        // ---- fm
        if (d.fm.level > 0.0f)
        {
            v.fmAmp = d.fm.level;
            v.fmIdxA = 1.0f; v.fmIdxB = 1.0f; v.fmStage = 0;
        }
        (void) ds;
    }

    void releaseVoice (Voice& v)
    {
        if (v.released) return;
        v.released = true;
        for (int i = 0; i < v.numPartials; ++i)
        {
            Partial& pt = v.part[(size_t) i];
            pt.d1 = std::min (pt.d1, pt.rel);
            pt.d2 = std::min (pt.d2, pt.rel);
        }
        v.fmStage = 2;
    }

    //--------------------------------------------------------------------------
    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const PianoDef& d = pianos()[(size_t) v.piano];
        std::fill (mono.begin(), mono.begin() + n, 0.0f);
        float* out = mono.data();
        bool alive = false;

        // ---- acoustic
        if (v.numPartials > 0)
        {
            for (int k = 0; k < v.numPartials; ++k)
            {
                Partial& pt = v.part[(size_t) k];
                if (! pt.on) continue;
                float re = pt.re, im = pt.im, e1 = pt.e1, e2 = pt.e2;
                const float c = pt.c, s = pt.s, d1 = pt.d1, d2 = pt.d2;
                if (pt.beat)
                {
                    float re2 = pt.re2, im2 = pt.im2;
                    const float c2 = pt.c2, s2 = pt.s2;
                    for (int i = 0; i < n; ++i)
                    {
                        out[i] += im * e1 + im2 * e2;
                        const float nr = re * c - im * s;  im = re * s + im * c;  re = nr;
                        const float nr2 = re2 * c2 - im2 * s2; im2 = re2 * s2 + im2 * c2; re2 = nr2;
                        e1 *= d1; e2 *= d2;
                    }
                    const float g2 = 1.5f - 0.5f * (re2 * re2 + im2 * im2);
                    pt.re2 = re2 * g2; pt.im2 = im2 * g2;
                }
                else
                {
                    for (int i = 0; i < n; ++i)
                    {
                        out[i] += im * (e1 + e2);
                        const float nr = re * c - im * s; im = re * s + im * c; re = nr;
                        e1 *= d1; e2 *= d2;
                    }
                }
                const float g = 1.5f - 0.5f * (re * re + im * im);
                pt.re = re * g; pt.im = im * g; pt.e1 = e1; pt.e2 = e2;
                if (e1 + e2 < 1.0e-5f) pt.on = false; else alive = true;
            }

            if (v.hamEnv > 1.0e-4f)
            {
                const float hd = std::exp (-1.0f / (0.006f * fs));
                for (int i = 0; i < n; ++i)
                {
                    v.rng = v.rng * 1664525u + 1013904223u;
                    const float noise = ((float) (v.rng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
                    const float y = noise * (1.0f - v.hamR) + v.hamA * v.hamB1 - v.hamR * v.hamB2;
                    v.hamB2 = v.hamB1; v.hamB1 = y;
                    out[i] += y * v.hamEnv * v.hamGain;
                    v.hamEnv *= hd;
                }
                alive = true;
            }
        }

        // ---- organ
        if (v.orAmp > 1.0e-5f)
        {
            const auto& bars = d.organ.bars;
            const float dec = v.released ? v.orRel : v.orDecay;
            float amp = v.orAmp;
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (size_t b = 0; b < 5; ++b)
                {
                    if (bars[b] <= 0.0f) continue;
                    s += v.orIm[b] * bars[b];
                    const float nr = v.orRe[b] * v.orC[b] - v.orIm[b] * v.orS[b];
                    v.orIm[b] = v.orRe[b] * v.orS[b] + v.orIm[b] * v.orC[b];
                    v.orRe[b] = nr;
                }
                out[i] += s * amp * 0.38f;
                amp *= dec;
            }
            for (size_t b = 0; b < 5; ++b)
            {
                const float g = 1.5f - 0.5f * (v.orRe[b] * v.orRe[b] + v.orIm[b] * v.orIm[b]);
                v.orRe[b] *= g; v.orIm[b] *= g;
            }
            if (v.orClick > 1.0e-4f)
            {
                const float cd = std::exp (-1.0f / (0.003f * fs));
                for (int i = 0; i < n; ++i)
                {
                    v.rng = v.rng * 1664525u + 1013904223u;
                    const float noise = ((float) (v.rng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
                    out[i] += noise * v.orClick;
                    v.orClick *= cd;
                }
            }
            v.orAmp = amp;
            alive = alive || amp > 1.0e-5f;
        }

        // ---- fm
        if (v.fmAmp > 1.0e-5f)
        {
            const FmDef& m = d.fm;
            const float kt   = std::clamp (std::pow (2.0f, -(v.note - 60) / 24.0f), 0.25f, 2.5f);
            const float idxK = std::clamp (std::pow (2.0f, -(v.note - 72) / 36.0f), 0.35f, 1.3f);
            const float bright = std::max (0.15f, 1.0f + 0.6f * toneOffset());
            const float velI = 0.45f + 0.55f * v.vel;
            const float peakA = m.indexA * bright * velI * idxK, peakB = m.indexB * bright * velI * idxK;
            const float ds = decayScale();
            const float decC = std::exp (-1.0f / (std::max (0.02f, m.decay * kt * ds) * fs * 0.3f));
            const float relC = std::exp (-1.0f / (std::max (0.01f, d.release) * fs * 0.25f));
            const float iCA = std::exp (-1.0f / (m.decA * kt * fs)), iCB = std::exp (-1.0f / (m.decB * kt * fs));
            const double f0 = noteHz (v.note) / sr;
            const double inc[4] = { f0, f0 * m.ratioA, f0 * m.octB, f0 * m.octB * m.ratioB };
            const float barkDrive = 1.0f + 4.0f * m.bark * v.vel * v.vel;
            const float lvl = (0.35f + 0.65f * v.vel) * 0.8f;
            float amp = v.fmAmp;
            for (int i = 0; i < n; ++i)
            {
                if (v.fmStage == 0) { v.fmStage = 1; }
                else if (v.fmStage == 1) amp = m.sustain * m.level + (amp - m.sustain * m.level) * decC;
                else amp *= relC;

                const float iA = peakA * (m.susA + (1.0f - m.susA) * v.fmIdxA);
                const float iB = peakB * (m.susB + (1.0f - m.susB) * v.fmIdxB);
                v.fmIdxA *= iCA; v.fmIdxB *= iCB;
                const float modA = std::sin (kTwoPi * (float) v.ph[1] + m.feedback * v.fbState);
                v.fbState = modA;
                const float a = std::sin (kTwoPi * (float) v.ph[0] + iA * modA);
                const float b = std::sin (kTwoPi * (float) v.ph[2] + iB * std::sin (kTwoPi * (float) v.ph[3]));
                for (int j = 0; j < 4; ++j) { v.ph[j] += inc[j]; v.ph[j] -= std::floor (v.ph[j]); }
                float s = a * (1.0f - 0.5f * m.mixB) + b * m.mixB;
                if (m.bark > 0.0f)  // asymmetric drive: EP "bark"
                {
                    const float x = s * barkDrive;
                    s = (x > 0.0f ? std::tanh (x) : std::tanh (x * 0.6f) / 0.6f) / barkDrive * (1.0f + 0.3f * m.bark);
                }
                out[i] += s * amp * lvl;
            }
            v.fmAmp = amp;
            alive = alive || amp > 1.0e-5f;
        }

        // attack ramp + gain + pan
        const float g = v.gain;
        const float pan = v.panAc * p.width;
        const float gl = std::sqrt (0.5f * (1.0f - pan)) * 1.41421f, gr = std::sqrt (0.5f * (1.0f + pan)) * 1.41421f;
        for (int i = 0; i < n; ++i)
        {
            if (v.attack < 1.0f) v.attack = std::min (1.0f, v.attack + v.attackInc);
            const float s = out[i] * g * v.attack;
            L[i] += s * gl;
            R[i] += s * gr;
        }

        if (! alive) v.active = false;
    }

    //--------------------------------------------------------------------------
    void processChunk (float* L, float* R, int n, double bpm, double ppq)
    {
        const PianoDef& d = pianos()[(size_t) std::clamp (p.piano, 0, kNumPianos - 1)];

        if (needsSnap)
        {
            needsSnap = false;
            sVol.init (dbToGain (p.volumeDb + d.gainDb), sr, 20.0f);
            sCutoff.init (std::log (p.cutoffHz), sr, 25.0f);
            for (auto* s : { &sFilterOn, &sChorusOn, &sDelayOn, &sReverbOn, &sPumpOn }) s->init (0.0f, sr, 15.0f);
            sFilterOn.value = p.filterOn; sChorusOn.value = p.chorusOn; sDelayOn.value = p.delayOn;
            sReverbOn.value = p.reverbOn; sPumpOn.value = p.pumpOn;
            sChorusMix.init (p.chorusMix, sr, 20.0f); sDelayMix.init (p.delayMix, sr, 20.0f);
            sReverbMix.init (p.reverbMix, sr, 20.0f); sWidth.init (p.width, sr, 20.0f);
            sTape.init (d.tape, sr, 50.0f); sDrive.init (d.drive, sr, 50.0f); sGlue.init (d.glue, sr, 50.0f);
            sLow.init (dbToGain (d.lowDb) - 1.0f, sr, 50.0f);
            sHigh.init (dbToGain (d.highDb + 6.0f * toneOffset()) - 1.0f, sr, 50.0f);
            sDelayTime.init ((float) (delayBeats (p.delayTime) * 60.0 / bpm * sr), sr, 80.0f);
            sPumpGain.init (1.0f, sr, 1.5f);
        }

        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        for (auto& v : voices) if (v.active) renderVoice (v, L, R, n);

        // per block constants
        const float loC  = onePoleCoef (220.0f, fs), hiC = onePoleCoef (3200.0f, fs);
        const float q    = 0.707f + p.resonance * p.resonance * 9.0f, kq = 1.0f / q;
        const float logCut = std::log (std::clamp (p.cutoffHz, 40.0f, 20000.0f));
        const float cAtt = std::exp (-1.0f / (0.004f * fs)), cRel = std::exp (-1.0f / (0.15f * fs));
        const double pumpDiv = p.pumpRate == 2 ? 4.0 : (p.pumpRate == 1 ? 2.0 : 1.0);
        const double ppqInc = bpm / 60.0 / sr;
        const float delayTarget = (float) std::min (delayBeats (p.delayTime) * 60.0 / bpm * sr, 3.0 * sr);
        const float dlLp = onePoleCoef (4500.0f, fs), dlHp = onePoleCoef (180.0f, fs);
        const float dcCoef = 1.0f - kTwoPi * 12.0f / fs;
        const float fb = std::clamp (p.delayFeedback, 0.0f, 0.95f);

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            // --- piano colour: shelves (tone knob drives the high shelf)
            const float lowG  = sLow.next (dbToGain (d.lowDb) - 1.0f);
            const float highG = sHigh.next (dbToGain (d.highDb + 6.0f * toneOffset()) - 1.0f);
            loL += loC * (l - loL); loR += loC * (r - loR);
            hiL += hiC * (l - hiL); hiR += hiC * (r - hiR);
            l += lowG * loL + highG * (l - hiL);
            r += lowG * loR + highG * (r - hiR);

            // --- drive (fat saturation)
            const float drive = sDrive.next (d.drive);
            if (drive > 0.001f)
            {
                const float k = 1.0f + 3.0f * drive;   // unity gain for quiet signals, rounds off peaks
                l = std::tanh (l * k) / k;
                r = std::tanh (r * k) / k;
            }

            // --- glue compressor
            const float glue = sGlue.next (d.glue);
            if (glue > 0.001f)
            {
                const float thr = -6.0f - 20.0f * glue, ratio = 1.5f + 4.5f * glue;
                const float lvl = std::max (std::abs (l), std::abs (r));
                compEnv = lvl > compEnv ? lvl + (compEnv - lvl) * cAtt : lvl + (compEnv - lvl) * cRel;
                const float over = 20.0f * std::log10 (compEnv + 1.0e-9f) - thr;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                const float cg = dbToGain (-gr + glue * 8.0f);
                l *= cg; r *= cg;
            }

            // --- tape (wow/flutter via short modulated delay, hiss, darker top)
            const float tape = sTape.next (d.tape);
            tapeL.push (l); tapeR.push (r);
            if (tape > 0.001f)
            {
                wowPh += 0.5 / sr; wowPh -= std::floor (wowPh);
                flutterPh += 6.1 / sr; flutterPh -= std::floor (flutterPh);
                const float md = (0.004f + tape * (0.0016f * std::sin (kTwoPi * (float) wowPh)
                                                   + 0.00025f * std::sin (kTwoPi * (float) flutterPh))) * fs;
                const float wl = tapeL.read (md), wr = tapeR.read (md);
                hissRng = hissRng * 1664525u + 1013904223u;
                const float hn = ((float) (hissRng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
                hissLp += 0.45f * (hn - hissLp);
                const float lpC = onePoleCoef (16000.0f - 11000.0f * tape, fs);
                tapeLpL += lpC * (wl - tapeLpL); tapeLpR += lpC * (wr - tapeLpR);
                const float mixT = std::min (1.0f, tape * 4.0f);
                l = l + mixT * (tapeLpL + hissLp * 0.004f * tape - l);
                r = r + mixT * (tapeLpR + hissLp * 0.004f * tape - r);
            }

            // --- FILTER (low-pass, on/off)
            const float fOn = sFilterOn.next (p.filterOn ? 1.0f : 0.0f);
            {
                const float fc = std::min (std::exp (sCutoff.next (logCut)), 0.45f * fs);
                const float gg = std::tan (kPi * fc / fs);
                const float a1 = 1.0f / (1.0f + gg * (gg + kq)), a2 = gg * a1, a3 = gg * a2;
                float v3 = l - ic2L, v1 = a1 * ic1L + a2 * v3, v2 = ic2L + a2 * ic1L + a3 * v3;
                ic1L = 2.0f * v1 - ic1L; ic2L = 2.0f * v2 - ic2L;
                const float fl = v2;
                v3 = r - ic2R; v1 = a1 * ic1R + a2 * v3; v2 = ic2R + a2 * ic1R + a3 * v3;
                ic1R = 2.0f * v1 - ic1R; ic2R = 2.0f * v2 - ic2R;
                l += fOn * (fl - l);
                r += fOn * (v2 - r);
            }

            // --- PUMP (tempo-locked ducking)
            {
                const float on = sPumpOn.next (p.pumpOn ? 1.0f : 0.0f);
                const double ph = (ppq + i * ppqInc) * pumpDiv;
                const float frac = (float) (ph - std::floor (ph));
                const float c = std::max (0.0f, 1.0f - frac / 0.75f);
                const float duck = c * c * (3.0f - 2.0f * c);
                const float pg = sPumpGain.next (1.0f - on * p.pumpDepth * duck);
                l *= pg; r *= pg;
            }

            // --- CHORUS
            {
                const float on = sChorusOn.next (p.chorusOn ? 1.0f : 0.0f) * sChorusMix.next (p.chorusMix);
                chorusLine.push (0.5f * (l + r));
                chPh += 0.55 / sr; chPh -= std::floor (chPh);
                const float a = std::sin (kTwoPi * (float) chPh);
                const float b = std::sin (kTwoPi * (float) chPh + 2.1f);
                const float wl = chorusLine.read ((0.011f + 0.0032f * a) * fs);
                const float wr = chorusLine.read ((0.013f + 0.0032f * b) * fs);
                l = l * (1.0f - 0.3f * on) + wl * 0.75f * on;
                r = r * (1.0f - 0.3f * on) + wr * 0.75f * on;
            }

            // --- DELAY (tempo-synced ping-pong)
            {
                const float on = sDelayOn.next (p.delayOn ? 1.0f : 0.0f);
                const float t = sDelayTime.next (delayTarget);
                const float dl = delayL.read (t), dr = delayR.read (t);
                // filter the feedback path so repeats get darker
                dlFbL += dlLp * (dr - dlFbL); dlHpL += dlHp * (dlFbL - dlHpL);
                dlFbR += dlLp * (dl - dlFbR); dlHpR += dlHp * (dlFbR - dlHpR);
                const float in = 0.5f * (l + r) * on;
                delayL.push (in + fb * (dlFbL - dlHpL));
                delayR.push (fb * (dlFbR - dlHpR));
                const float mix = sDelayMix.next (p.delayMix) * 1.2f;
                l += dl * mix; r += dr * mix;
            }

            // --- REVERB
            {
                const float on = sReverbOn.next (p.reverbOn ? 1.0f : 0.0f);
                float wl = 0.0f, wr = 0.0f;
                reverb.process (l * on, r * on, p.reverbSize, wl, wr);
                const float mix = sReverbMix.next (p.reverbMix);
                l = l * (1.0f - 0.25f * mix * on) + wl * mix * 1.3f;
                r = r * (1.0f - 0.25f * mix * on) + wr * mix * 1.3f;
            }

            // --- width (mid/side)
            {
                const float w = sWidth.next (p.width) * 2.0f;  // 0..2, 1 = unchanged
                const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
                l = m + s; r = m - s;
            }

            // --- DC block, volume, soft clip
            const float yl = l - dcInL + dcCoef * dcL; dcInL = l; dcL = yl;
            const float yr = r - dcInR + dcCoef * dcR; dcInR = r; dcR = yr;
            const float vol = sVol.next (dbToGain (p.volumeDb + d.gainDb));
            L[i] = std::tanh (yl * vol);
            R[i] = std::tanh (yr * vol);
        }
    }

    //--------------------------------------------------------------------------
    double sr = 44100.0;
    float fs = 44100.0f;
    int block = 512;
    Params p;
    std::array<Voice, kMaxVoices> voices {};
    std::vector<float> mono;
    uint64_t counter = 0;
    bool pedal = false, needsSnap = true;

    FdnReverb reverb;
    DelayLine chorusLine, tapeL, tapeR, delayL, delayR;
    float loL = 0, loR = 0, hiL = 0, hiR = 0, ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0, compEnv = 0;
    float tapeLpL = 0, tapeLpR = 0, hissLp = 0, dlFbL = 0, dlFbR = 0, dlHpL = 0, dlHpR = 0;
    float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0;
    uint32_t hissRng = 12345u;
    double wowPh = 0, flutterPh = 0, chPh = 0, freePpq = 0;
    Smoother sVol, sCutoff, sFilterOn, sChorusOn, sDelayOn, sReverbOn, sPumpOn, sChorusMix, sDelayMix,
             sReverbMix, sWidth, sTape, sDrive, sGlue, sLow, sHigh, sDelayTime, sPumpGain;
};

} // namespace hk
