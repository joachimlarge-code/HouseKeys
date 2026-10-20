// HouseKeys - FM house piano engine.
// Plain C++17, no JUCE dependency, so the same code runs in the plugin and in
// the offline demo renderer (tools/render_demo.cpp).
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

//==============================================================================
enum Model { kClassic = 0, kDeep, kOrgan, kStab, kNumModels };
enum Chord { kChordOff = 0, kMin7, kMin9, kMin11, kMaj7, kMaj9, kSus, kNumChords };

inline const std::vector<int>& chordIntervals (int c)
{
    static const std::vector<int> table[kNumChords] = {
        { 0 },                     // Off
        { 0, 3, 7, 10 },           // Min7
        { 0, 3, 7, 10, 14 },       // Min9
        { 0, 3, 7, 10, 14, 17 },   // Min11
        { 0, 4, 7, 11 },           // Maj7
        { 0, 4, 7, 11, 14 },       // Maj9
        { 0, 5, 7, 10 },           // 7sus4
    };
    return table[std::clamp (c, 0, kNumChords - 1)];
}

// Each model is two FM operator pairs (A = body, B = layer) plus a hammer click.
struct ModelSpec
{
    float ratioA, indexA, susIndexA, idxDecayA; // modulator ratio, peak index, sustained fraction, decay (s)
    float octB, ratioB, indexB, idxDecayB, susIndexB; // layer carrier multiple, ratio, index, decay, sustained fraction
    float feedback;                             // modulator self-feedback (pair A)
    float click;                                // hammer / key click amount
    float sustain;                              // amp sustain level (0 = piano-like)
    float gain;                                 // level trim
};

inline const ModelSpec& modelSpec (int m)
{
    static const ModelSpec specs[kNumModels] = {
        //  rA    idxA   susA   decA    octB  rB     idxB   decB   susB   fb     click  sus    gain
        { 1.0f, 2.6f, 0.30f, 0.55f,  2.0f, 1.0f,  0.90f, 0.90f, 0.15f, 0.10f, 0.35f, 0.00f, 0.55f }, // Classic house piano
        { 1.0f, 1.3f, 0.25f, 1.20f,  1.0f, 14.0f, 1.60f, 0.06f, 0.00f, 0.00f, 0.10f, 0.00f, 0.70f }, // Deep keys (EP / tine)
        { 1.0f, 0.5f, 1.00f, 1.00f,  2.0f, 1.0f,  0.35f, 1.00f, 1.00f, 0.05f, 0.25f, 0.85f, 0.42f }, // Organ
        { 1.0f, 3.2f, 0.20f, 0.25f,  1.0f, 3.0f,  1.80f, 0.30f, 0.10f, 0.20f, 0.45f, 0.00f, 0.52f }, // Stab
    };
    return specs[std::clamp (m, 0, kNumModels - 1)];
}

//==============================================================================
struct Params
{
    int   model      = kClassic;
    float brightness = 0.7f;     // 0..1
    float layer      = 0.45f;    // 0..1
    float detune     = 6.0f;     // cents 0..30
    float tape       = 0.0f;     // 0..1
    float attackMs   = 1.0f;     // 0..300
    float decayS     = 2.5f;     // 0.1..10
    float releaseS   = 0.35f;    // 0.02..5
    float velSens    = 0.5f;     // 0..1
    int   chord      = kChordOff;
    float cutoffHz   = 16000.0f; // 80..20000
    float resonance  = 0.1f;     // 0..1
    float chorus     = 0.25f;    // 0..1
    float reverb     = 0.2f;     // 0..1
    float roomSize   = 0.55f;    // 0..1
    float comp       = 0.4f;     // 0..1
    float pump       = 0.0f;     // 0..1
    int   pumpRate   = 0;        // 0 = 1/4, 1 = 1/8, 2 = 1/16
    float volumeDb   = -6.0f;    // -24..+6
};

//==============================================================================
struct Smoother
{
    float value = 0.0f, coef = 0.999f;
    void  reset (float v, double sr, float ms) { value = v; coef = std::exp (-1.0f / (float) (ms * 0.001 * sr)); }
    float next (float target) { value = target + (value - target) * coef; return value; }
};

class Freeverb
{
public:
    void prepare (double sr)
    {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4]   = { 556, 441, 341, 225 };
        const double s = sr / 44100.0;
        for (int i = 0; i < 8; ++i)
        {
            combL[i].init ((int) (combT[i] * s));
            combR[i].init ((int) ((combT[i] + 23) * s));
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[i].init ((int) (apT[i] * s));
            apR[i].init ((int) ((apT[i] + 23) * s));
        }
    }

    void process (float inL, float inR, float size, float& outL, float& outR)
    {
        const float fb = 0.70f + 0.28f * size, damp = 0.35f;
        const float in = (inL + inR) * 0.015f;
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < 8; ++i) { l += combL[i].tick (in, fb, damp); r += combR[i].tick (in, fb, damp); }
        for (int i = 0; i < 4; ++i) { l = apL[i].tick (l); r = apR[i].tick (r); }
        outL = l * 3.0f;
        outR = r * 3.0f;
    }

private:
    struct Comb
    {
        std::vector<float> buf; int idx = 0; float store = 0.0f;
        void  init (int n) { buf.assign ((size_t) std::max (1, n), 0.0f); idx = 0; store = 0.0f; }
        float tick (float x, float fb, float damp)
        {
            float y = buf[(size_t) idx];
            store   = y * (1.0f - damp) + store * damp;
            buf[(size_t) idx] = x + store * fb;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };
    struct Allpass
    {
        std::vector<float> buf; int idx = 0;
        void  init (int n) { buf.assign ((size_t) std::max (1, n), 0.0f); idx = 0; }
        float tick (float x)
        {
            float b = buf[(size_t) idx];
            buf[(size_t) idx] = x + b * 0.5f;
            if (++idx >= (int) buf.size()) idx = 0;
            return b - x;
        }
    };
    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];
};

//==============================================================================
class Engine
{
public:
    static constexpr int kMaxVoices = 48;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        block = std::max (32, maxBlock);
        pitchBuf.assign ((size_t) block, 1.0f);
        voices = {};
        reverbFx.prepare (sr);
        chorusLen = (int) (0.05 * sr) + 4;
        chorusL.assign ((size_t) chorusLen, 0.0f);
        chorusR.assign ((size_t) chorusLen, 0.0f);
        chorusW = 0;
        ic1L = ic2L = ic1R = ic2R = 0.0f;
        compEnv = 0.0f; tapeLpL = tapeLpR = 0.0f; hissLp = 0.0f;
        sCutoff.reset (std::log (p.cutoffHz), sr, 25.0f);
        sVol.reset (dbToGain (p.volumeDb), sr, 20.0f);
        sChorus.reset (p.chorus, sr, 20.0f);
        sReverb.reset (p.reverb, sr, 20.0f);
        sPumpGain.reset (1.0f, sr, 1.5f);
        sTape.reset (p.tape, sr, 30.0f);
        freePpq = 0.0;
        dcL = dcR = dcInL = dcInR = 0.0f;
        dcCoef = 1.0f - kTwoPi * 10.0f / (float) sr;
        needsSnap = true;
    }

    void setParams (const Params& np) { p = np; }
    const Params& params() const { return p; }

    //--------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        // retrigger: release anything still held from this key
        noteOffInternal (note, true);

        const auto& iv = chordIntervals (p.chord);
        const float norm = 1.0f / std::sqrt ((float) iv.size());
        for (size_t k = 0; k < iv.size(); ++k)
        {
            const int pitch = note + iv[k];
            if (pitch > 127) continue;
            Voice& v = allocate();
            v = Voice {};
            v.active   = true;
            v.note     = pitch;
            v.trigger  = note;
            v.order    = ++counter;
            v.velocity = clamp01 (velocity);
            v.gain     = norm * (k == 0 ? 1.0f : 0.92f);
            v.rng      = 0x9E3779B9u ^ (uint32_t) (counter * 2654435761u);
        }
    }

    void noteOff (int note) { noteOffInternal (note, false); }

    void setSustainPedal (bool down)
    {
        pedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.active && v.sustained) { v.sustained = false; v.stage = 2; }
    }

    void allNotesOff()
    {
        for (auto& v : voices) if (v.active) { v.stage = 2; v.sustained = false; }
    }

    //--------------------------------------------------------------------------
    // bpm / ppqStart / playing come from the host transport. When the host is
    // stopped the pump runs on an internal clock at the same tempo.
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

    int activeVoices() const
    {
        int c = 0; for (auto& v : voices) c += v.active ? 1 : 0; return c;
    }

private:
    struct Voice
    {
        bool     active = false, sustained = false;
        int      note = -1, trigger = -1, stage = 0; // stage 0 attack, 1 decay, 2 release
        uint64_t order = 0;
        float    velocity = 0.0f, gain = 1.0f, amp = 0.0f;
        double   phC1 = 0, phM1 = 0, phC2 = 0, phM2 = 0;
        float    fbState = 0.0f, idxEnvA = 1.0f, idxEnvB = 1.0f, clickEnv = 1.0f, clickLp = 0.0f;
        uint32_t rng = 1;
    };

    void noteOffInternal (int note, bool hard)
    {
        for (auto& v : voices)
        {
            if (! v.active || v.trigger != note || v.stage == 2) continue;
            if (pedal && ! hard) v.sustained = true;
            else                 { v.stage = 2; v.sustained = false; }
        }
    }

    Voice& allocate()
    {
        for (auto& v : voices) if (! v.active) return v;
        Voice* best = nullptr;  // steal oldest, preferring released voices
        for (auto& v : voices)
        {
            if (best == nullptr) { best = &v; continue; }
            const bool vr = v.stage == 2, br = best->stage == 2;
            if (vr != br) { if (vr) best = &v; continue; }
            if (v.order < best->order) best = &v;
        }
        return *best;
    }

    static float noteHz (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); }

    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const ModelSpec& m = modelSpec (p.model);
        const float fs   = (float) sr;
        const float kt   = std::clamp (std::pow (2.0f, -(v.note - 60) / 24.0f), 0.25f, 2.5f); // key tracking
        const float idxK = std::clamp (std::pow (2.0f, -(v.note - 72) / 36.0f), 0.35f, 1.3f);

        const float vel     = std::pow (v.velocity, 1.2f);
        const float velAmp  = 1.0f - p.velSens + p.velSens * vel;
        const float velIdx  = 1.0f - 0.6f * p.velSens + 0.6f * p.velSens * vel;
        const float bright  = 0.25f + 1.5f * p.brightness;
        const float peakA   = m.indexA * bright * velIdx * idxK;
        const float peakB   = m.indexB * bright * velIdx * idxK;

        const float decT    = std::max (0.02f, p.decayS * kt);
        const float decCoef = std::exp (-1.0f / (decT * fs * 0.3f));          // ~ -30 dB-ish over decay time
        const float relCoef = std::exp (-1.0f / (std::max (0.005f, p.releaseS) * fs * 0.25f));
        const float atkInc  = 1.0f / (std::max (0.5f, p.attackMs) * 0.001f * fs);
        const float idxCA   = std::exp (-1.0f / (m.idxDecayA * kt * fs));
        const float idxCB   = std::exp (-1.0f / (m.idxDecayB * kt * fs));
        const float clkC    = std::exp (-1.0f / (0.004f * fs));
        const float clickAmt = m.click * (0.3f + 0.7f * p.brightness) * (0.3f + 0.7f * vel);

        const double f    = noteHz (v.note) / sr;
        const double det  = std::pow (2.0, p.detune / 1200.0);
        const double incA = f, incMA = f * m.ratioA;
        const double incB = f * m.octB * det, incMB = incB * m.ratioB;
        const float  w    = std::min (1.0f, p.detune / 30.0f) * 0.6f;   // stereo spread
        const float  layerA = 1.0f - 0.5f * p.layer, layerB = p.layer;
        const float  g    = v.gain * velAmp * m.gain;
        const float  sus  = m.sustain;

        for (int i = 0; i < n; ++i)
        {
            // amp envelope
            if (v.stage == 0)      { v.amp += atkInc; if (v.amp >= 1.0f) { v.amp = 1.0f; v.stage = 1; } }
            else if (v.stage == 1) { v.amp = sus + (v.amp - sus) * decCoef; }
            else                   { v.amp *= relCoef; }

            if ((v.stage == 2 || sus <= 0.0f) && v.stage != 0 && v.amp < 1.0e-4f) { v.active = false; return; }

            const double pm = pitchBuf[(size_t) i];
            const float iA = peakA * (m.susIndexA + (1.0f - m.susIndexA) * v.idxEnvA);
            const float iB = peakB * (m.susIndexB + (1.0f - m.susIndexB) * v.idxEnvB);
            v.idxEnvA *= idxCA;
            v.idxEnvB *= idxCB;

            const float modA = std::sin (kTwoPi * (float) v.phM1 + m.feedback * v.fbState);
            v.fbState = modA;
            const float a = std::sin (kTwoPi * (float) v.phC1 + iA * modA);
            const float b = std::sin (kTwoPi * (float) v.phC2 + iB * std::sin (kTwoPi * (float) v.phM2));

            v.phC1 += incA * pm;  v.phC1 -= std::floor (v.phC1);
            v.phM1 += incMA * pm; v.phM1 -= std::floor (v.phM1);
            v.phC2 += incB * pm;  v.phC2 -= std::floor (v.phC2);
            v.phM2 += incMB * pm; v.phM2 -= std::floor (v.phM2);

            // hammer click: short high-passed noise burst
            v.rng = v.rng * 1664525u + 1013904223u;
            const float noise = ((float) (v.rng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
            v.clickLp += 0.25f * (noise - v.clickLp);
            const float click = (noise - v.clickLp) * v.clickEnv * clickAmt;
            v.clickEnv *= clkC;

            const float sa = a * layerA, sb = b * layerB;
            const float amp = v.amp * g;
            L[i] += (sa * (1.0f + w) + sb * (1.0f - w) + click) * amp;
            R[i] += (sa * (1.0f - w) + sb * (1.0f + w) + click) * amp;
        }
    }

    void processChunk (float* L, float* R, int n, double bpm, double ppq)
    {
        const float fs = (float) sr;

        if (needsSnap)  // first block after prepare: jump straight to the current settings
        {
            needsSnap = false;
            sCutoff.value = std::log (std::clamp (p.cutoffHz, 20.0f, 20000.0f));
            sVol.value = dbToGain (p.volumeDb);
            sChorus.value = p.chorus; sReverb.value = p.reverb; sTape.value = p.tape;
        }

        // global tape wow / flutter
        for (int i = 0; i < n; ++i)
        {
            const float t = sTape.next (p.tape);
            wowPh     += 0.55 / sr; wowPh     -= std::floor (wowPh);
            flutterPh += 6.3  / sr; flutterPh -= std::floor (flutterPh);
            pitchBuf[(size_t) i] = 1.0f + t * (0.0045f * std::sin (kTwoPi * (float) wowPh)
                                             + 0.0009f * std::sin (kTwoPi * (float) flutterPh));
        }

        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        for (auto& v : voices) if (v.active) renderVoice (v, L, R, n);

        // effect settings
        const float q     = 0.707f + p.resonance * p.resonance * 9.0f;
        const float k     = 1.0f / q;
        const float cThr  = -8.0f - 22.0f * p.comp;
        const float cRat  = 1.0f + 5.0f * p.comp;
        const float cMake = dbToGain (p.comp * 9.0f);
        const float cAtt  = std::exp (-1.0f / (0.005f * fs));
        const float cRel  = std::exp (-1.0f / (0.120f * fs));
        const double div  = p.pumpRate == 2 ? 4.0 : (p.pumpRate == 1 ? 2.0 : 1.0);
        const double ppqInc = bpm / 60.0 / sr;
        const float logCut = std::log (std::clamp (p.cutoffHz, 20.0f, 20000.0f));

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            const float tape = sTape.value;

            // tape: hiss, saturation, darker top
            if (tape > 0.001f)
            {
                hissRng = hissRng * 1664525u + 1013904223u;
                const float hn = ((float) (hissRng >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f;
                hissLp += 0.5f * (hn - hissLp);
                const float hiss = hissLp * 0.006f * tape;
                const float drive = 1.0f + 2.5f * tape;
                l = std::tanh (l * drive) / std::tanh (drive) + hiss;
                r = std::tanh (r * drive) / std::tanh (drive) + hiss;
                const float lpC = 1.0f - std::exp (-kTwoPi * (18000.0f - 13000.0f * tape) / fs);
                tapeLpL += lpC * (l - tapeLpL); tapeLpR += lpC * (r - tapeLpR);
                l = tapeLpL; r = tapeLpR;
            }

            // filter (TPT state-variable low-pass)
            const float fc = std::min (std::exp (sCutoff.next (logCut)), 0.45f * fs);
            const float gg = std::tan (kPi * fc / fs);
            const float a1 = 1.0f / (1.0f + gg * (gg + k)), a2 = gg * a1, a3 = gg * a2;
            {
                float v3 = l - ic2L, v1 = a1 * ic1L + a2 * v3, v2 = ic2L + a2 * ic1L + a3 * v3;
                ic1L = 2.0f * v1 - ic1L; ic2L = 2.0f * v2 - ic2L; l = v2;
                v3 = r - ic2R; v1 = a1 * ic1R + a2 * v3; v2 = ic2R + a2 * ic1R + a3 * v3;
                ic1R = 2.0f * v1 - ic1R; ic2R = 2.0f * v2 - ic2R; r = v2;
            }

            // compressor (stereo-linked)
            if (p.comp > 0.001f)
            {
                const float lvl = std::max (std::abs (l), std::abs (r));
                compEnv = lvl > compEnv ? lvl + (compEnv - lvl) * cAtt : lvl + (compEnv - lvl) * cRel;
                const float db = 20.0f * std::log10 (compEnv + 1.0e-9f);
                const float over = db - cThr;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / cRat) : 0.0f;
                const float cg = dbToGain (-gr) * cMake;
                l *= cg; r *= cg;
            }

            // pump (sidechain-style ducking locked to tempo)
            {
                const double ph = (ppq + i * ppqInc) * div;
                const float frac = (float) (ph - std::floor (ph));
                const float c = std::max (0.0f, 1.0f - frac / 0.75f);
                const float duck = c * c * (3.0f - 2.0f * c);
                const float pg = sPumpGain.next (1.0f - p.pump * duck);
                l *= pg; r *= pg;
            }

            // chorus (mono in, two modulated taps -> wide stereo)
            const float chMix = sChorus.next (p.chorus);
            {
                const float mono = 0.5f * (l + r);
                chorusL[(size_t) chorusW] = mono;
                chorusR[(size_t) chorusW] = mono;
                chPh += 0.6 / sr; chPh -= std::floor (chPh);
                const float dA = (0.012f + 0.0035f * std::sin (kTwoPi * (float) chPh)) * fs;
                const float dB = (0.014f + 0.0035f * std::sin (kTwoPi * (float) chPh + 1.7f)) * fs;
                const float wl = readDelay (chorusL, dA), wr = readDelay (chorusR, dB);
                if (++chorusW >= chorusLen) chorusW = 0;
                l = l * (1.0f - 0.35f * chMix) + wl * 0.8f * chMix;
                r = r * (1.0f - 0.35f * chMix) + wr * 0.8f * chMix;
            }

            // reverb
            const float rvMix = sReverb.next (p.reverb);
            {
                float wl = 0.0f, wr = 0.0f;
                reverbFx.process (l, r, p.roomSize, wl, wr);
                l = l * (1.0f - 0.3f * rvMix) + wl * rvMix;
                r = r * (1.0f - 0.3f * rvMix) + wr * rvMix;
            }

            // DC blocker (FM feedback can leave a tiny offset)
            const float yl = l - dcInL + dcCoef * dcL; dcInL = l; dcL = yl;
            const float yr = r - dcInR + dcCoef * dcR; dcInR = r; dcR = yr;

            const float vol = sVol.next (dbToGain (p.volumeDb));
            L[i] = std::tanh (yl * vol);
            R[i] = std::tanh (yr * vol);
        }
    }

    float readDelay (const std::vector<float>& buf, float delaySamples) const
    {
        float pos = (float) chorusW - delaySamples;
        while (pos < 0.0f) pos += (float) chorusLen;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % chorusLen;
        const float fr = pos - (float) i0;
        return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }

    //--------------------------------------------------------------------------
    double sr = 44100.0;
    int block = 512;
    Params p;
    std::array<Voice, kMaxVoices> voices {};
    uint64_t counter = 0;
    bool pedal = false;
    std::vector<float> pitchBuf;

    double wowPh = 0, flutterPh = 0, chPh = 0, freePpq = 0;
    uint32_t hissRng = 22222u;
    float hissLp = 0, tapeLpL = 0, tapeLpR = 0;
    float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;
    float compEnv = 0;
    float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0, dcCoef = 0.999f;
    bool needsSnap = true;
    std::vector<float> chorusL, chorusR;
    int chorusLen = 1, chorusW = 0;
    Freeverb reverbFx;
    Smoother sCutoff, sVol, sChorus, sReverb, sPumpGain, sTape;
};

} // namespace hk
