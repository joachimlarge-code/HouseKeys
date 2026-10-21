// HouseKeys - small DSP building blocks (no JUCE dependency).
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

struct Rng
{
    uint32_t s = 1;
    float next() { s = s * 1664525u + 1013904223u; return ((float) (s >> 9) * (1.0f / 8388608.0f)) * 2.0f - 1.0f; } // -1..1
};

//------------------------------------------------------------------------------
struct Smoother
{
    float value = 0.0f, coef = 0.999f;
    void  init (float v, double sr, float ms) { value = v; coef = std::exp (-1.0f / (float) (ms * 0.001 * sr)); }
    float next (float target) { value = target + (value - target) * coef; return value; }
};

//------------------------------------------------------------------------------
// Topology-preserving state variable filter (Zavalishin). One instance = one channel.
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0;
    void reset() { ic1 = ic2 = 0; }
    void set (float hz, float q, float sr)
    {
        g  = std::tan (kPi * std::min (hz, 0.49f * sr) / sr);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    // returns low-pass, writes band/high if wanted
    float process (float x, float* band = nullptr, float* high = nullptr)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        if (band) *band = v1;
        if (high) *high = x - k * v1 - v2;
        return v2;
    }
    float hp (float x) { float h; process (x, nullptr, &h); return h; }
    float bp (float x) { float b; process (x, &b, nullptr); return b; }
};

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Lush 8-line feedback delay network reverb with input high-pass (keeps the low end clean)
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
        pre.init ((int) (0.08f * sr));
        inHp.reset(); inHp.set (180.0f, 0.707f, sr);
        lfo = 0.0; lastSize = -1.0f;
    }

    void process (float inL, float inR, float size, float& outL, float& outR)
    {
        if (std::abs (size - lastSize) > 1.0e-4f)   // recompute line gains only when Size changes
        {
            lastSize = size;
            scale = 0.55f + 1.05f * size;
            const float rt60 = 0.7f + 6.0f * size * size;
            for (int i = 0; i < 8; ++i) gain[i] = std::pow (10.0f, -3.0f * (base[i] * scale / sr) / rt60);
            damp = 0.42f - 0.15f * size;
        }

        pre.push (inHp.hp ((inL + inR) * 0.5f));
        float x = pre.read ((0.012f + 0.02f * size) * sr);
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
            v[i] = lp[i] * gain[i];
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
    Svf inHp;
    float base[8] {}, lp[8] {}, out[8] {}, gain[8] {};
    float lastSize = -1.0f, scale = 1.0f, damp = 0.35f;
    std::vector<float> ap[4]; int apLen[4] {}, apW[4] {};
    double lfo = 0;
};

//------------------------------------------------------------------------------
// "Air": musical top-end enhancer, inspired by how Fresh Air-style processors work.
//  - an exciter creates NEW soft harmonics from the 2.5-6 kHz band. The band is normalised by its
//    own envelope before shaping, so quiet and loud notes get the same amount of sparkle.
//  - a gentle high shelf lifts whatever top the source already has (> ~9 kHz)
//  - a slight dip around 3 kHz keeps it open instead of harsh
//  - the added air is held below the program level by a fast envelope limiter
class AirProcessor
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            src[c].reset();   src[c].set (2500.0f, 0.6f, sr);
            out1[c].reset();  out1[c].set (5200.0f, 0.6f, sr);
            out2[c].reset();  out2[c].set (5200.0f, 0.6f, sr);
            shelf[c].reset(); shelf[c].set (9000.0f, 0.55f, sr);
            dip[c].reset();   dip[c].set (3200.0f, 0.8f, sr);
            bandEnv[c] = 0.0f;
        }
        envAir = envProg = 0.0f;
        att  = std::exp (-1.0f / (0.0008f * sr));
        rel  = std::exp (-1.0f / (0.06f * sr));
        bAtt = std::exp (-1.0f / (0.002f * sr));
        bRel = std::exp (-1.0f / (0.04f * sr));
    }

    void process (float& l, float& r, float amount)
    {
        if (amount <= 0.0005f) return;
        float add[2];
        const float in[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            const float band = src[c].hp (in[c]);
            const float a = std::abs (band);
            bandEnv[c] = a > bandEnv[c] ? a + (bandEnv[c] - a) * bAtt : a + (bandEnv[c] - a) * bRel;
            const float e = bandEnv[c] + 1.0e-5f;
            const float x = band / e;                                   // ~unit level
            const float h = (std::tanh (2.5f * x) + 0.35f * x * std::abs (x)) * e;
            const float ex = out2[c].hp (out1[c].hp (h));               // only the new top content
            const float top = shelf[c].hp (in[c]);
            const float pres = dip[c].bp (in[c]);
            add[c] = ex * 3.2f * amount + top * (dbToGain (8.0f * amount) - 1.0f) - pres * 0.18f * amount;
        }
        const float a = std::max (std::abs (add[0]), std::abs (add[1]));
        const float pgm = std::max (std::abs (l), std::abs (r));
        envAir  = a   > envAir  ? a   + (envAir  - a)   * att : a   + (envAir  - a)   * rel;
        envProg = pgm > envProg ? pgm + (envProg - pgm) * att : pgm + (envProg - pgm) * rel;
        const float limit = 0.55f * envProg + 1.0e-6f;
        const float g = envAir > limit ? limit / envAir : 1.0f;
        l += add[0] * g;
        r += add[1] * g;
    }

private:
    float sr = 44100.0f, envAir = 0, envProg = 0, att = 0.9f, rel = 0.999f, bAtt = 0.9f, bRel = 0.999f;
    float bandEnv[2] {};
    Svf src[2], out1[2], out2[2], shelf[2], dip[2];
};

//------------------------------------------------------------------------------
// Warm saturation: asymmetric tanh with unity small-signal gain and a soft top.
struct Saturator
{
    float lpL = 0, lpR = 0;
    void process (float& l, float& r, float drive, float sr)
    {
        if (drive <= 0.001f) return;
        const float k = 1.0f + 9.0f * drive, b = 0.25f * drive;
        const float tb = std::tanh (b), norm = 1.0f / (k * (1.0f - tb * tb));
        float yl = (std::tanh (k * l + b) - tb) * norm;
        float yr = (std::tanh (k * r + b) - tb) * norm;
        const float c = onePoleCoef (16000.0f - 7000.0f * drive, sr);
        lpL += c * (yl - lpL); lpR += c * (yr - lpR);
        const float makeup = 1.0f / (1.0f + 0.35f * drive);
        l = lpL * makeup; r = lpR * makeup;
    }
};
} // namespace hk
