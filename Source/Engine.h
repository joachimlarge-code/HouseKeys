// HouseKeys 3 - sound engine (no JUCE dependency).
// Voices mix the sources described in Sounds.h; the master chain is:
//   colour (low cut, shelves + Tone, drive, glue, tape) -> Filter -> Saturation -> Air -> Pump
//   -> Chorus -> Delay -> Reverb -> Width (bass kept mono) -> volume -> soft limiter
#pragma once

#include "Dsp.h"
#include "SampleBank.h"
#include "Sounds.h"

namespace hk
{
class Engine
{
public:
    static constexpr int kMaxVoices    = 64;
    static constexpr int kMaxPartials  = 40;
    static constexpr int kBeatPartials = 8;
    static constexpr int kMaxUnison    = 7;
    static constexpr int kRelVoices    = 16;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        fs = (float) sr;
        block = std::max (32, std::min (maxBlock, 1024));
        mono.assign ((size_t) block, 0.0f);
        side.assign ((size_t) block, 0.0f);
        voices = {};
        relVoices = {};
        reverb.prepare (sr);
        air.prepare (fs);
        chorusLine.init ((int) (0.05 * sr));
        tapeL.init ((int) (0.03 * sr)); tapeR.init ((int) (0.03 * sr));
        delayL.init ((int) (3.2 * sr)); delayR.init ((int) (3.2 * sr));
        for (auto* f : { &lowCutL, &lowCutR, &filtL, &filtR, &sideHp }) f->reset();
        sat = {};
        resetState();
        needsSnap = true;
    }

    void setParams (const Params& np) { p = np; }
    const Params& params() const { return p; }

    // The bank must stay alive while the engine uses it (the loader keeps it for the app's lifetime).
    void setSampleBank (const SampleBank* b) { bank = (b != nullptr && ! b->empty()) ? b : nullptr; }
    bool hasSamples() const { return bank != nullptr; }
    // the definition used by the "AI-lyd" sound (kept alive by the owner)
    void setCustomSound (const SoundDef* d) { custom = d; }
    void setUserBank (const SampleBank* b) { userBank = (b != nullptr && ! b->empty()) ? b : nullptr; }

    //--------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        noteOffInternal (note, true);
        const SoundDef& d = cur();
        const bool vaOnly = d.organ.level <= 0.0f && d.fm.level <= 0.0f && d.keys.level <= 0.0f && d.sampler.level <= 0.0f
                         && d.str.level <= 0.0f;
        if (d.mono && p.chord == kChordOff && vaOnly)
        {
            // mono legato: keep the oscillators running and only retrigger the envelopes -> no clicks
            Voice* held = nullptr;
            for (auto& v : voices)
                if (v.active && ! v.released && v.sound == p.sound && v.vaOn && (held == nullptr || v.order > held->order))
                    held = &v;
            for (auto& v : voices)
                if (v.active && &v != held && ! v.fastRelease) { v.fastRelease = true; v.released = true; }
            if (held != nullptr) { retrigger (*held, note, clamp01 (velocity)); return; }
        }
        else if (d.mono || stabMode (d))
            for (auto& v : voices)
                if (v.active && ! v.fastRelease) { v.fastRelease = true; v.released = true; }

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

    // dropped chord samples play one at a time (classic house chord-stab style)
    bool stabMode (const SoundDef& d) const
    {
        return d.sampler.user && userBank != nullptr
            && (p.smpMode == 2 || (p.smpMode == 0 && userBank->chordSample));
    }

    void setModWheel (float v) { modWheel = clamp01 (v); }   // CC1 adds vibrato

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

    // bpm / ppq / playing come from the host; when stopped, tempo effects use an internal clock.
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
        float re, im, c, s, re2, im2, c2, s2, e1, e2, d1, d2;
        bool  beat, on;
    };

    struct Voice
    {
        bool active = false, sustained = false, released = false, fastRelease = false;
        int note = -1, trigger = -1, sound = 0;
        uint64_t order = 0;
        float vel = 0, gain = 1;

        // master amplitude envelope (Attack / Release controls)
        float env = 0, attInc = 1;
        float extraDecay = 1.0f, extra = 1.0f;     // Decay control on samples

        // sampler
        const SampleZone* za = nullptr;
        float smpGain = 0;
        const SampleZone* zo = nullptr;            // octave-up layer
        float octGain = 0;
        double posO = 0, incO = 1;
        float glide = 1.0f, glideC = 0.0f;         // pitch ratio gliding toward 1 (mono legato)
        float vibPh = 0.0f, age = 0.0f;            // vibrato phase, seconds since note start
        double pos = 0, inc = 1;
        bool undamped = false;

        // acoustic (fallback) and keys
        int numPartials = 0, numKeys = 0;
        std::array<Partial, kMaxPartials> part {}, keys {};
        float hamEnv = 0, hamB1 = 0, hamB2 = 0, hamA = 0, hamR = 0, hamGain = 0;
        Rng rng;

        // organ
        std::array<float, 5> orRe {}, orIm {}, orC {}, orS {};
        float orAmp = 0, orDecay = 1, orClick = 0;

        // fm
        double ph[4] {};
        float fmAmp = 0, fmIdxA = 1, fmIdxB = 1, fbState = 0;
        bool fmOn = false;

        // va
        bool vaOn = false;
        int uni = 1;
        float vph[2][kMaxUnison] {}, vinc[2][kMaxUnison] {}, vpan[kMaxUnison] {};
        float subPh = 0, subInc = 0;
        Svf vf[2][2];
        float fenv = 0, vamp = 1, driftPh = 0, driftInc = 0, driftAmt = 0;
        int fStage = 0, cutCounter = 0;
        bool ampRise = false;
        float lfoPh = 0, penv = 0;                 // LFO phase, pitch envelope (1 -> 0)
        Svf fmt[2][2];                             // vowel filter: two formants per channel
        float vowelPos = -1.0f;

        // plucked string
        bool strOn = false;
        int ksW = 0, ksLife = 0;
        float ksDelay = 0, ksG = 0, ksAmp = 0, ksPrev = 0, ksB = 0.5f;
        std::array<float, 4096> ks {};
    };

    struct RelVoice { const SampleZone* z = nullptr; double pos = 0, inc = 1; float gain = 0; };

    const SoundDef& def (int i) const
    {
        i = std::clamp (i, 0, kNumSounds - 1);
        return (i == kAiSound && custom != nullptr) ? *custom : sounds()[(size_t) i];
    }
    const SoundDef& cur() const { return def (p.sound); }
    static float noteHz (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); }
    float decayScale() const { return std::pow (2.0f, (p.decay - 0.5f) * 4.0f); }   // 0.25x .. 4x
    float toneOffset() const { return (p.tone - 0.5f) * 2.0f; }                      // -1 .. +1

    //--------------------------------------------------------------------------
    void resetState()
    {
        for (float* f : { &loL, &loR, &hiL, &hiR, &compEnv, &tapeLpL, &tapeLpR, &hissLp, &dlFbL, &dlFbR,
                          &dlHpL, &dlHpR, &dcL, &dcR, &dcInL, &dcInR })
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

    //--------------------------------------------------------------------------
    void startVoice (Voice& v, int pitch, int trigger, float vel, float gain)
    {
        const SoundDef& d = cur();
        v = Voice {};
        v.active = true; v.note = pitch; v.trigger = trigger; v.sound = p.sound;
        v.order = ++counter; v.vel = vel; v.gain = gain;
        v.rng.s = 0x9E3779B9u ^ (uint32_t) (counter * 2654435761u);
        v.attInc = 1.0f / (std::max (0.3f, p.attackMs) * 0.001f * fs);

        // Decay control on recorded samples: below 50 % it adds an extra fade
        if (p.decay < 0.5f)
        {
            const float T = 0.12f * std::pow (40.0f, p.decay * 2.0f);
            v.extraDecay = std::exp (-1.0f / (T * fs));
        }

        const float f  = noteHz (pitch);
        const float ds = decayScale();
        const float kt = std::clamp (std::pow (2.0f, -(pitch - 60) / 18.0f), 0.12f, 3.0f);

        //---- sampler (or modelled fallback while samples load)
        const SampleBank* sb = d.sampler.user ? userBank : bank;
        if (d.sampler.level > 0.0f && sb != nullptr)
        {
            const int ri = sb->rootIndexFor (pitch);
            const float x = std::pow (vel, d.sampler.velCurve);
            const int layer = std::clamp ((int) (x * (float) sb->numLayers), 0, sb->numLayers - 1);
            v.za = sb->zone (ri, layer);
            if (v.za != nullptr)
            {
                v.inc = std::pow (2.0, (pitch - v.za->root - v.za->tuneCents / 100.0) / 12.0) * v.za->sampleRate / sr;
                // layers are level-matched, so loudness follows velocity smoothly over the whole range
                v.smpGain = d.sampler.level * v.za->gain * dbToGain (d.sampler.dynamics * (std::sqrt (vel) - 1.0f));
            }
            v.undamped = pitch >= 89 && ! d.sampler.user;   // the top strings of a grand have no dampers
            if (d.sampler.octave > 0.0f && pitch + 12 <= 108)
            {
                const int ro = sb->rootIndexFor (pitch + 12);
                v.zo = sb->zone (ro, std::max (0, layer - 1));
                if (v.zo != nullptr)
                {
                    v.incO = std::pow (2.0, (pitch + 12 - v.zo->root - v.zo->tuneCents / 100.0) / 12.0) * v.zo->sampleRate / sr;
                    v.octGain = d.sampler.octave * d.sampler.level * v.zo->gain * dbToGain (d.sampler.dynamics * (std::sqrt (vel) - 1.0f));
                }
            }
        }
        const bool useFallback = d.sampler.level > 0.0f && ! d.sampler.user && bank == nullptr && d.fallback.level > 0.0f;
        const AcousticDef* ac = useFallback ? &d.fallback : nullptr;

        if (ac != nullptr)
        {
            const AcousticDef& a = *ac;
            const float hard = clamp01 (a.hardness + 0.3f * toneOffset()) * (0.55f + 0.45f * vel);
            const float B    = std::clamp (0.00012f * std::exp ((pitch - 48) * 0.055f) * a.inharm, 0.00002f, 0.02f);
            const float kc   = 2.5f + 34.0f * hard * hard;
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
                const float ph0 = 2.39996f * (float) (k * k);
                pt.re = std::cos (ph0); pt.im = std::sin (ph0);
                pt.beat = k <= kBeatPartials && a.detune > 0.05f;
                const float w2 = w * (k % 2 ? detR : 1.0f / detR);
                pt.c2 = std::cos (w2); pt.s2 = std::sin (w2);
                pt.re2 = std::cos (ph0 + 1.3f * (float) k); pt.im2 = std::sin (ph0 + 1.3f * (float) k);
                const float tk = tau / (1.0f + 0.045f * std::pow ((float) (k - 1), 1.35f) * (0.6f + hard));
                pt.d1 = std::exp (-1.0f / (std::max (0.02f, tk * 0.22f) * fs));
                pt.d2 = std::exp (-1.0f / (std::max (0.04f, tk * 1.4f) * fs));
                pt.e1 = amp * 0.65f; pt.e2 = amp * 0.35f;
                pt.on = true;
                norm += amp;
            }
            v.numPartials = n;
            const float lvl = a.level * (0.35f + 0.65f * vel) * 0.8f / std::max (1.0f, std::sqrt (norm) * 1.3f);
            for (int i = 0; i < n; ++i) { v.part[(size_t) i].e1 *= lvl; v.part[(size_t) i].e2 *= lvl; }
            const float fc = std::clamp (f * (1.5f + 4.0f * hard), 300.0f, 6000.0f);
            const float rr = std::exp (-kPi * 0.7f * fc / fs);
            v.hamA = 2.0f * rr * std::cos (kTwoPi * fc / fs); v.hamR = rr * rr;
            v.hamEnv = 1.0f;
            v.hamGain = a.hammer * a.level * (0.15f + 0.85f * vel) * (0.25f + 0.75f * hard) * 0.06f;
        }

        //---- keys
        if (d.keys.level > 0.0f)
        {
            const KeysDef& kd = d.keys;
            const float kc   = kd.kc * std::pow (2.0f, toneOffset()) * (0.7f + 0.3f * vel);
            const float detR = std::pow (2.0f, kd.detune / 2400.0f);
            float norm = 0.0f;
            int n = 0;
            for (int k = 1; k <= kMaxPartials; ++k)
            {
                const float fk = f * (float) k;
                if (fk * detR > 0.45f * fs) break;
                float a = std::pow ((float) k, -kd.tilt) * std::exp (-(float) (k - 1) / kc);
                if (k > 1 && (k & 1)) a *= kd.odd;
                if (k == 2) a *= kd.h2;
                if (k == 3) a *= kd.h3;
                if (a < 1.0e-4f) continue;
                Partial& pt = v.keys[(size_t) n++];
                const float w1 = kTwoPi * fk * detR / fs, w2 = kTwoPi * fk / detR / fs;
                const float ph1 = 2.39996f * (float) (k * k + 3), ph2 = ph1 + 1.7f * (float) k;
                pt.c = std::cos (w1);  pt.s = std::sin (w1);  pt.re = std::cos (ph1);  pt.im = std::sin (ph1);
                pt.c2 = std::cos (w2); pt.s2 = std::sin (w2); pt.re2 = std::cos (ph2); pt.im2 = std::sin (ph2);
                const float tk = kd.tau * ds / (1.0f + kd.bright * (float) (k - 1));
                pt.d1 = std::exp (-1.0f / (std::max (0.01f, tk) * fs));
                pt.d2 = std::exp (-1.0f / (std::max (0.02f, kd.tauS * ds / (1.0f + kd.brightS * (float) (k - 1))) * fs));
                pt.e1 = a * (1.0f - kd.sus); pt.e2 = a * kd.sus;
                pt.beat = true; pt.on = true;
                norm += a;
            }
            v.numKeys = n;
            const float lvl = kd.level * (0.4f + 0.6f * vel) * 0.4f / std::max (1.0f, std::sqrt (norm) * 1.3f);
            for (int i = 0; i < n; ++i) { v.keys[(size_t) i].e1 *= lvl; v.keys[(size_t) i].e2 *= lvl; }
        }

        //---- organ
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
            v.orClick = d.organ.click * d.organ.level * 0.25f;
        }

        //---- fm
        if (d.fm.level > 0.0f) { v.fmOn = true; v.fmAmp = d.fm.level; }

        //---- va
        if (d.va.level > 0.0f)
        {
            const VaDef& va = d.va;
            v.vaOn = true;
            v.uni = std::clamp (va.unison, 1, kMaxUnison);
            for (int u = 0; u < v.uni; ++u)
            {
                const float t = v.uni > 1 ? (float) u / (float) (v.uni - 1) * 2.0f - 1.0f : 0.0f;  // -1..1
                const float cents = 0.5f * va.detune * t;
                v.vpan[u] = t * va.spread;
                v.vinc[0][u] = f * std::pow (2.0f, cents / 1200.0f) / fs;
                v.vinc[1][u] = f * std::pow (2.0f, (va.semi2 * 100.0f + va.fine2 + cents * 1.1f) / 1200.0f) / fs;
                // mono (bass) sounds start each waveform at its zero crossing -> clean, click-free starts
                static const float zeroPh[4] = { 0.5f, 0.0f, 0.25f, 0.0f };   // saw, square, tri, sine
                const bool clean = d.mono && v.uni == 1;
                v.vph[0][u] = clean ? zeroPh[va.wave1] : 0.5f + 0.5f * v.rng.next();
                v.vph[1][u] = clean ? zeroPh[va.wave2] : 0.5f + 0.5f * v.rng.next();
            }
            v.subInc = 0.5f * f / fs;
            v.driftInc = (0.12f + 0.2f * (0.5f + 0.5f * v.rng.next())) / fs;   // 0.12-0.32 Hz, different per voice
            v.driftPh = 0.5f + 0.5f * v.rng.next();
            v.driftAmt = va.drift;
            v.fenv = 0.0f; v.fStage = 0; v.vamp = 1.0f;
            for (auto& ch : v.vf) for (auto& st : ch) st.reset();
            for (auto& ch : v.fmt) for (auto& st : ch) st.reset();
            v.lfoPh = 0.0f; v.vowelPos = -1.0f;
            v.penv = va.pEnvSemi != 0.0f ? 1.0f : 0.0f;
        }

        //---- plucked string: a delay line one period long, filled with a noise burst
        if (d.str.level > 0.0f)
        {
            const StrDef& st = d.str;
            const float period = fs / f;
            if (period > 2.0f && period < 4000.0f)
            {
                v.strOn = true;
                v.ksB = 0.15f + 0.85f * clamp01 (st.damp + 0.25f * toneOffset());
                v.ksDelay = period - 0.5f * (1.0f - v.ksB);
                v.ksG = std::pow (0.001f, (1.0f / f) / std::max (0.05f, st.decay * kt * ds));
                v.ksAmp = st.level * (0.35f + 0.65f * vel) * 0.9f;
                v.ksLife = (int) ((st.decay * kt * ds * 1.6f + 0.2f) * fs);
                const float pc = 0.08f + 0.9f * clamp01 (st.pick * (0.6f + 0.4f * vel));
                float lp = 0.0f, mean = 0.0f;
                const int len = (int) period + 2;
                for (int i = 0; i < len; ++i) { lp += pc * (v.rng.next() - lp); v.ks[(size_t) i] = lp; mean += lp; }
                mean /= (float) len;
                for (int i = 0; i < len; ++i) v.ks[(size_t) i] -= mean;   // no DC thump
                v.ksW = len % (int) v.ks.size();
                v.ksPrev = 0.0f;
            }
        }
    }

    void retrigger (Voice& v, int note, float vel)
    {
        const float f = noteHz (note);
        const VaDef& va = def (v.sound).va;
        for (int u = 0; u < v.uni; ++u)
        {
            const float t = v.uni > 1 ? (float) u / (float) (v.uni - 1) * 2.0f - 1.0f : 0.0f;
            const float cents = 0.5f * va.detune * t;
            v.vinc[0][u] = f * std::pow (2.0f, cents / 1200.0f) / fs;
            v.vinc[1][u] = f * std::pow (2.0f, (va.semi2 * 100.0f + va.fine2 + cents * 1.1f) / 1200.0f) / fs;
        }
        const SoundDef& sd = def (v.sound);
        if (sd.glideMs > 0.0f)
        {
            // continue from the pitch we are at now, then glide to the new note
            v.glide *= noteHz (v.note) / f;
            v.glideC = std::exp (-1.0f / (sd.glideMs * 0.001f * fs));
        }
        v.subInc = 0.5f * f / fs;
        v.note = note; v.trigger = note; v.vel = vel; v.order = ++counter;
        v.fStage = 0; v.ampRise = true;
        if (sd.va.pEnvSemi != 0.0f && sd.glideMs <= 0.0f) v.penv = 1.0f;
    }

    void releaseVoice (Voice& v)
    {
        if (v.released) return;
        v.released = true;
        const SoundDef& d = def (v.sound);
        // damper noise
        if (bank != nullptr && v.za != nullptr && d.sampler.relNoise > 0.0f && ! d.sampler.user)
            if (const SampleZone* z = bank->release (v.note))
                for (auto& r : relVoices)
                    if (r.z == nullptr)
                    {
                        r.z = z; r.pos = 0; r.inc = z->sampleRate / sr;
                        r.gain = d.sampler.relNoise * d.sampler.level * 0.5f * (0.3f + 0.7f * v.vel) * v.env;
                        break;
                    }
    }

    //--------------------------------------------------------------------------
    static inline float hermite (float xm1, float x0, float x1, float x2, float t)
    {
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b = w + a;
        return ((a * t - b) * t + c) * t + x0;
    }

    static inline float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    static inline float osc (int wave, float t, float dt)
    {
        switch (wave)
        {
            case kSaw:    return 2.0f * t - 1.0f - polyBlep (t, dt);
            case kSquare: { float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                            return (t < 0.5f ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt); }
            case kTri:    return 1.0f - 4.0f * std::abs (t - 0.5f);
            default:      return std::sin (kTwoPi * t);
        }
    }

    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const SoundDef& d = def (v.sound);
        std::fill (mono.begin(), mono.begin() + n, 0.0f);
        std::fill (side.begin(), side.begin() + n, 0.0f);
        float* out = mono.data();
        float* sd  = side.data();
        bool alive = false;

        // vibrato: pitch ratio at the start and end of this chunk, interpolated per sample
        const float knobDepth = p.vibOn ? p.vibDepth * std::min (1.0f, v.age / 0.25f) : 0.0f;   // fades in
        const float depth = std::max (knobDepth, modWheel) * 60.0f / 1200.0f;                  // octaves
        float vib0 = 1.0f, vibStep = 0.0f;
        if (depth > 0.0f)
        {
            const float ph1 = v.vibPh + p.vibRate * (float) n / fs;
            vib0 = std::exp2 (depth * std::sin (kTwoPi * v.vibPh));
            const float vib1 = std::exp2 (depth * std::sin (kTwoPi * ph1));
            vibStep = (vib1 - vib0) / (float) n;
            v.vibPh = ph1 - std::floor (ph1);
        }
        v.age += (float) n / fs;

        //---- sampler (stereo)
        if (v.za != nullptr)
        {
            const SampleZone& a = *v.za;
            const float ga = v.smpGain;
            double pos = v.pos;
            float ex = v.extra;
            for (int i = 0; i < n; ++i)
            {
                const int ip = (int) pos;
                const float t = (float) (pos - ip);
                if (ip + 2 >= a.length) { v.za = nullptr; break; }
                const int i0 = std::max (0, ip - 1);
                float l = hermite (a.l (i0), a.l (ip), a.l (ip + 1), a.l (ip + 2), t) * ga;
                float r = hermite (a.r (i0), a.r (ip), a.r (ip + 1), a.r (ip + 2), t) * ga;
                l *= ex; r *= ex; ex *= v.extraDecay;
                out[i] += 0.5f * (l + r);
                sd[i]  += 0.5f * (l - r);
                pos += v.inc * (vib0 + vibStep * (float) i);
            }
            v.pos = pos;
            if (v.zo != nullptr)
            {
                const SampleZone& o = *v.zo;
                double po = v.posO;
                float exo = v.extra;
                for (int i = 0; i < n; ++i)
                {
                    const int ip = (int) po;
                    if (ip + 2 >= o.length) { v.zo = nullptr; break; }
                    const float t = (float) (po - ip);
                    const int i0 = std::max (0, ip - 1);
                    const float g = v.octGain * exo;
                    const float l = hermite (o.l (i0), o.l (ip), o.l (ip + 1), o.l (ip + 2), t) * g;
                    const float r = hermite (o.r (i0), o.r (ip), o.r (ip + 1), o.r (ip + 2), t) * g;
                    out[i] += 0.5f * (l + r);
                    sd[i]  += 0.5f * (l - r);
                    exo *= v.extraDecay;
                    po += v.incO * (vib0 + vibStep * (float) i);
                }
                v.posO = po;
            }
            v.extra = ex;
            if (v.za != nullptr && ex > 1.0e-4f) alive = true; else { v.za = nullptr; v.zo = nullptr; }
        }

        //---- keys
        if (v.numKeys > 0)
        {
            const float sp = d.keys.spread * std::clamp (p.width * 2.0f, 0.0f, 1.5f);
            for (int k = 0; k < v.numKeys; ++k)
            {
                Partial& pt = v.keys[(size_t) k];
                if (! pt.on) continue;
                float re = pt.re, im = pt.im, re2 = pt.re2, im2 = pt.im2, e1 = pt.e1, e2 = pt.e2;
                const float c = pt.c, s = pt.s, c2 = pt.c2, s2 = pt.s2, d1 = pt.d1, d2 = pt.d2;
                for (int i = 0; i < n; ++i)
                {
                    const float e = e1 + e2, aa = im * e, bb = im2 * e;
                    out[i] += (aa + bb) * 0.5f;
                    sd[i]  += (aa - bb) * 0.5f * sp;
                    const float nr = re * c - im * s;      im = re * s + im * c;      re = nr;
                    const float nr2 = re2 * c2 - im2 * s2; im2 = re2 * s2 + im2 * c2; re2 = nr2;
                    e1 *= d1; e2 *= d2;
                }
                const float g = 1.5f - 0.5f * (re * re + im * im), g2 = 1.5f - 0.5f * (re2 * re2 + im2 * im2);
                pt.re = re * g; pt.im = im * g; pt.re2 = re2 * g2; pt.im2 = im2 * g2; pt.e1 = e1; pt.e2 = e2;
                if (e1 + e2 < 1.0e-5f) pt.on = false; else alive = true;
            }
        }

        //---- acoustic fallback
        if (v.numPartials > 0)
        {
            for (int k = 0; k < v.numPartials; ++k)
            {
                Partial& pt = v.part[(size_t) k];
                if (! pt.on) continue;
                float re = pt.re, im = pt.im, e1 = pt.e1, e2 = pt.e2, re2 = pt.re2, im2 = pt.im2;
                const float c = pt.c, s = pt.s, d1 = pt.d1, d2 = pt.d2, c2 = pt.c2, s2 = pt.s2;
                for (int i = 0; i < n; ++i)
                {
                    out[i] += pt.beat ? im * e1 + im2 * e2 : im * (e1 + e2);
                    const float nr = re * c - im * s; im = re * s + im * c; re = nr;
                    if (pt.beat) { const float nr2 = re2 * c2 - im2 * s2; im2 = re2 * s2 + im2 * c2; re2 = nr2; }
                    e1 *= d1; e2 *= d2;
                }
                const float g = 1.5f - 0.5f * (re * re + im * im), g2 = 1.5f - 0.5f * (re2 * re2 + im2 * im2);
                pt.re = re * g; pt.im = im * g; pt.re2 = re2 * g2; pt.im2 = im2 * g2; pt.e1 = e1; pt.e2 = e2;
                if (e1 + e2 < 1.0e-5f) pt.on = false; else alive = true;
            }
            if (v.hamEnv > 1.0e-4f)
            {
                const float hd = std::exp (-1.0f / (0.006f * fs));
                for (int i = 0; i < n; ++i)
                {
                    const float y = v.rng.next() * (1.0f - v.hamR) + v.hamA * v.hamB1 - v.hamR * v.hamB2;
                    v.hamB2 = v.hamB1; v.hamB1 = y;
                    out[i] += y * v.hamEnv * v.hamGain;
                    v.hamEnv *= hd;
                }
                alive = true;
            }
        }

        //---- organ
        if (v.orAmp > 1.0e-5f)
        {
            const auto& bars = d.organ.bars;
            float amp = v.orAmp;
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (size_t bi = 0; bi < 5; ++bi)
                {
                    if (bars[bi] <= 0.0f) continue;
                    s += v.orIm[bi] * bars[bi];
                    const float nr = v.orRe[bi] * v.orC[bi] - v.orIm[bi] * v.orS[bi];
                    v.orIm[bi] = v.orRe[bi] * v.orS[bi] + v.orIm[bi] * v.orC[bi];
                    v.orRe[bi] = nr;
                }
                out[i] += s * amp * 0.38f;
                amp *= v.orDecay;
            }
            for (size_t bi = 0; bi < 5; ++bi)
            {
                const float g = 1.5f - 0.5f * (v.orRe[bi] * v.orRe[bi] + v.orIm[bi] * v.orIm[bi]);
                v.orRe[bi] *= g; v.orIm[bi] *= g;
            }
            if (v.orClick > 1.0e-4f)
            {
                const float cd = std::exp (-1.0f / (0.003f * fs));
                for (int i = 0; i < n; ++i) { out[i] += v.rng.next() * v.orClick; v.orClick *= cd; }
            }
            v.orAmp = amp;
            alive = alive || amp > 1.0e-5f;
        }

        //---- fm
        if (v.fmOn)
        {
            const FmDef& m = d.fm;
            const float kt   = std::clamp (std::pow (2.0f, -(v.note - 60) / 24.0f), 0.25f, 2.5f);
            const float idxK = std::clamp (std::pow (2.0f, -(v.note - 72) / 36.0f), 0.35f, 1.3f);
            const float bright = std::max (0.15f, 1.0f + 0.6f * toneOffset());
            const float velI = 0.45f + 0.55f * v.vel;
            const float peakA = m.indexA * bright * velI * idxK, peakB = m.indexB * bright * velI * idxK;
            const float decC = std::exp (-1.0f / (std::max (0.02f, m.decay * kt * decayScale()) * fs * 0.3f));
            const float iCA = std::exp (-1.0f / (m.decA * kt * fs)), iCB = std::exp (-1.0f / (m.decB * kt * fs));
            const double f0 = noteHz (v.note) / sr;
            const double inc[4] = { f0, f0 * m.ratioA, f0 * m.octB, f0 * m.octB * m.ratioB };
            const float barkDrive = 1.0f + 4.0f * m.bark * v.vel * v.vel;
            const float lvl = (0.35f + 0.65f * v.vel) * 0.8f;
            const float susL = m.sustain * m.level;
            float amp = v.fmAmp;
            for (int i = 0; i < n; ++i)
            {
                amp = susL + (amp - susL) * decC;
                const float iA = peakA * (m.susA + (1.0f - m.susA) * v.fmIdxA);
                const float iB = peakB * (m.susB + (1.0f - m.susB) * v.fmIdxB);
                v.fmIdxA *= iCA; v.fmIdxB *= iCB;
                const float modA = std::sin (kTwoPi * (float) v.ph[1] + m.feedback * v.fbState);
                v.fbState = modA;
                const float a = std::sin (kTwoPi * (float) v.ph[0] + iA * modA);
                const float b = std::sin (kTwoPi * (float) v.ph[2] + iB * std::sin (kTwoPi * (float) v.ph[3]));
                const double vf = vib0 + vibStep * (float) i;
                for (int j = 0; j < 4; ++j) { v.ph[j] += inc[j] * vf; v.ph[j] -= std::floor (v.ph[j]); }
                float s = a * (1.0f - 0.5f * m.mixB) + b * m.mixB;
                if (m.bark > 0.0f)
                {
                    const float x = s * barkDrive;
                    s = (x > 0.0f ? std::tanh (x) : std::tanh (x * 0.6f) / 0.6f) / barkDrive * (1.0f + 0.3f * m.bark);
                }
                out[i] += s * amp * lvl;
            }
            v.fmAmp = amp;
            if (amp > 1.0e-5f) alive = true; else v.fmOn = false;
        }

        //---- va
        if (v.vaOn)
        {
            const VaDef& va = d.va;
            const float ds = decayScale();
            const float fAinc = 1.0f / (std::max (0.5f, va.fA) * 0.001f * fs);
            const float fDc = std::exp (-1.0f / (std::max (1.0f, va.fD * ds) * 0.001f * fs));
            const float fRc = std::exp (-1.0f / (std::max (1.0f, va.fR) * 0.001f * fs));
            const float aDc = va.decayMs > 0.0f ? std::exp (-1.0f / (va.decayMs * ds * 0.001f * fs)) : 1.0f;
            const float sus = va.decayMs > 0.0f ? va.sustain : 1.0f;
            const float riseC = 1.0f - std::exp (-1.0f / (0.003f * fs));
            const float baseOct = std::log2 (std::max (20.0f, va.cutoff)) + va.keytrack * (float) (v.note - 60) / 12.0f
                                + va.velAmt * (v.vel - 0.7f) + 1.5f * toneOffset();
            const float q = 0.6f + va.reso * va.reso * 6.0f;
            const float lvl = va.level * (1.0f - va.velSens + va.velSens * v.vel) * 0.55f / std::sqrt ((float) v.uni);
            const float m2 = va.mix2;
            // extra blocks
            static const double divBeats[6] = { 0.0, 2.0, 1.0, 0.5, 0.25, 1.0 / 3.0 };
            const float lfoHz = va.lfoDiv > 0 ? (float) (curBpm / 60.0 / divBeats[std::clamp (va.lfoDiv, 1, 5)]) : va.lfoRate;
            const bool hasLfo = lfoHz > 0.0f && (va.lfoCut > 0.0f || va.lfoPitch > 0.0f || va.lfoVowel > 0.0f || va.lfoAmp > 0.0f);
            const float lfoInc = lfoHz / fs;
            const bool hasPenv = va.pEnvSemi != 0.0f;
            const float pDec = std::exp (-1.0f / (std::max (1.0f, va.pEnvMs) * 0.001f * fs));
            const float ringAmt = va.ring, foldK = 1.0f + va.fold * 5.0f;
            const float oscNorm = 1.0f / (std::sqrt ((float) v.uni) * (1.0f + m2));
            const bool hasFormant = va.formant > 0.0f;
            float lfo = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                if (hasLfo)
                {
                    lfo = std::sin (kTwoPi * v.lfoPh);
                    v.lfoPh += lfoInc; if (v.lfoPh >= 1.0f) v.lfoPh -= 1.0f;
                }
                // filter envelope
                if (v.released)       v.fenv *= fRc;
                else if (v.fStage == 0) { v.fenv += fAinc; if (v.fenv >= 1.0f) { v.fenv = 1.0f; v.fStage = 1; } }
                else                  v.fenv = va.fS + (v.fenv - va.fS) * fDc;
                if (v.ampRise) { v.vamp += (1.0f - v.vamp) * riseC; if (v.vamp > 0.995f) v.ampRise = false; }
                else           v.vamp = sus + (v.vamp - sus) * aDc;

                if (v.cutCounter-- <= 0)
                {
                    v.cutCounter = 8;
                    v.driftPh += v.driftInc * 8.0f; v.driftPh -= std::floor (v.driftPh);
                    const float mo = va.motion * std::sin (kTwoPi * v.driftPh + 1.3f * (float) v.note);
                    const float fc = std::exp2 (std::clamp (baseOct + va.envAmt * v.fenv + mo + va.lfoCut * lfo, 4.3f, 14.2f));
                    for (auto& ch : v.vf) for (auto& st : ch) st.set (fc, q, fs);
                    if (hasFormant)
                    {
                        // a - e - i - o - u
                        static const float F1[5] = { 800.0f, 420.0f, 280.0f, 480.0f, 330.0f };
                        static const float F2[5] = { 1200.0f, 1900.0f, 2300.0f, 820.0f, 700.0f };
                        const float vp = clamp01 (va.vowel + 0.5f * va.lfoVowel * lfo) * 3.999f;
                        if (std::abs (vp - v.vowelPos) > 0.002f)
                        {
                            v.vowelPos = vp;
                            const int k0 = (int) vp; const float t = vp - (float) k0;
                            const float f1 = F1[k0] + (F1[k0 + 1] - F1[k0]) * t, f2 = F2[k0] + (F2[k0 + 1] - F2[k0]) * t;
                            for (int ch = 0; ch < 2; ++ch) { v.fmt[ch][0].set (f1, 7.0f, fs); v.fmt[ch][1].set (f2, 9.0f, fs); }
                        }
                    }
                }
                v.glide = 1.0f + (v.glide - 1.0f) * v.glideC;
                float drift = (1.0f + v.driftAmt * 0.000578f * std::sin (kTwoPi * v.driftPh)) * v.glide
                            * (vib0 + vibStep * (float) i);
                if (hasPenv) { drift *= std::exp2 (va.pEnvSemi * v.penv / 12.0f); v.penv *= pDec; }
                if (va.lfoPitch > 0.0f && hasLfo) drift *= 1.0f + va.lfoPitch * 0.000578f * lfo;
                float l = 0.0f, r = 0.0f;
                for (int u = 0; u < v.uni; ++u)
                {
                    const float d0 = v.vinc[0][u] * drift, d1 = v.vinc[1][u] * drift;
                    const float o1 = osc (va.wave1, v.vph[0][u], d0);
                    float x = o1;
                    if (m2 > 0.0f)
                    {
                        const float o2 = osc (va.wave2, v.vph[1][u], d1);
                        x += m2 * o2;
                        if (ringAmt > 0.0f) x += ringAmt * ((1.0f + m2) * o1 * o2 - x);
                    }
                    v.vph[0][u] += d0; if (v.vph[0][u] >= 1.0f) v.vph[0][u] -= 1.0f;
                    v.vph[1][u] += d1; if (v.vph[1][u] >= 1.0f) v.vph[1][u] -= 1.0f;
                    l += x * (1.0f + v.vpan[u]);
                    r += x * (1.0f - v.vpan[u]);
                }
                if (va.sub > 0.0f || va.noise > 0.0f)
                {
                    const float sb = va.sub * std::sin (kTwoPi * v.subPh) * std::sqrt ((float) v.uni);
                    v.subPh += v.subInc; if (v.subPh >= 1.0f) v.subPh -= 1.0f;
                    const float nz = va.noise * v.rng.next() * std::sqrt ((float) v.uni);
                    l += sb + nz; r += sb + nz;
                }
                if (va.fold > 0.0f)
                {
                    l = std::sin (l * oscNorm * foldK * 1.5708f) / oscNorm * 0.75f;
                    r = std::sin (r * oscNorm * foldK * 1.5708f) / oscNorm * 0.75f;
                }
                if (va.fType == 0)
                {
                    l = v.vf[0][0].process (l); r = v.vf[1][0].process (r);
                    if (va.poles >= 4) { l = v.vf[0][1].process (l); r = v.vf[1][1].process (r); }
                }
                else if (va.fType == 1)
                {
                    l = 1.6f * v.vf[0][0].bp (l); r = 1.6f * v.vf[1][0].bp (r);
                    if (va.poles >= 4) { l = 1.4f * v.vf[0][1].bp (l); r = 1.4f * v.vf[1][1].bp (r); }
                }
                else
                {
                    l = v.vf[0][0].hp (l); r = v.vf[1][0].hp (r);
                    if (va.poles >= 4) { l = v.vf[0][1].hp (l); r = v.vf[1][1].hp (r); }
                }
                if (hasFormant)
                {
                    const float yl = 2.2f * v.fmt[0][0].bp (l) + 1.6f * v.fmt[0][1].bp (l);
                    const float yr = 2.2f * v.fmt[1][0].bp (r) + 1.6f * v.fmt[1][1].bp (r);
                    l += va.formant * (yl - l); r += va.formant * (yr - r);
                }
                float g = lvl * v.vamp;
                if (va.lfoAmp > 0.0f && hasLfo) g *= 1.0f - va.lfoAmp * (0.5f + 0.5f * lfo);
                out[i] += 0.5f * (l + r) * g;
                sd[i]  += 0.5f * (l - r) * g;
            }
            if (v.vamp < 1.0e-4f && sus <= 0.0f) v.vaOn = false; else alive = true;
        }

        //---- plucked string
        if (v.strOn)
        {
            const int N = (int) v.ks.size();
            const float dl = v.ksDelay;
            const int di = (int) dl;
            const float fr = dl - (float) di;
            const float b = v.ksB, gk = v.ksG, amp = v.ksAmp;
            for (int i = 0; i < n; ++i)
            {
                int r0 = v.ksW - di;     if (r0 < 0) r0 += N;
                int r1 = r0 - 1;         if (r1 < 0) r1 += N;
                const float y = v.ks[(size_t) r0] + (v.ks[(size_t) r1] - v.ks[(size_t) r0]) * fr;
                const float z = (b * y + (1.0f - b) * 0.5f * (y + v.ksPrev)) * gk;
                v.ksPrev = y;
                v.ks[(size_t) v.ksW] = z;
                if (++v.ksW >= N) v.ksW = 0;
                out[i] += y * amp;
            }
            v.ksLife -= n;
            if (v.ksLife <= 0) v.strOn = false; else alive = true;
        }

        //---- master envelope + output
        const float relT = v.fastRelease ? (v.za != nullptr ? 0.015f : 0.006f)
                         : (v.za != nullptr && v.undamped) ? std::max (p.releaseS, 3.0f) : std::max (0.01f, p.releaseS);
        const float relC = std::exp (-5.0f / (relT * fs));
        const float g = v.gain;
        for (int i = 0; i < n; ++i)
        {
            if (v.released)        v.env *= relC;
            else if (v.env < 1.0f) v.env = std::min (1.0f, v.env + v.attInc);
            const float e = g * v.env;
            const float s = out[i] * e, sdv = sd[i] * e;
            L[i] += s + sdv;
            R[i] += s - sdv;
        }
        if (! alive || (v.released && v.env < 2.0e-5f)) v.active = false;
    }

    void renderReleaseNoises (float* L, float* R, int n)
    {
        for (auto& r : relVoices)
        {
            if (r.z == nullptr) continue;
            const SampleZone& z = *r.z;
            for (int i = 0; i < n; ++i)
            {
                const int ip = (int) r.pos;
                if (ip + 1 >= z.length) { r.z = nullptr; break; }
                const float t = (float) (r.pos - ip);
                L[i] += (z.l (ip) + (z.l (ip + 1) - z.l (ip)) * t) * r.gain;
                R[i] += (z.r (ip) + (z.r (ip + 1) - z.r (ip)) * t) * r.gain;
                r.pos += r.inc;
            }
        }
    }

    //--------------------------------------------------------------------------
    void snap (const SoundDef& d, double bpm)
    {
        needsSnap = false;
        sVol.init (dbToGain (p.volumeDb + d.gainDb), sr, 20.0f);
        sCutoff.init (std::log (p.cutoffHz), sr, 25.0f);
        for (auto* s : { &sFilterOn, &sSatOn, &sChorusOn, &sDelayOn, &sReverbOn, &sPumpOn }) s->init (0.0f, sr, 15.0f);
        sFilterOn.value = p.filterOn; sSatOn.value = p.satOn; sChorusOn.value = p.chorusOn; sDelayOn.value = p.delayOn;
        sReverbOn.value = p.reverbOn; sPumpOn.value = p.pumpOn;
        sChorusMix.init (p.chorusMix, sr, 20.0f); sDelayMix.init (p.delayMix, sr, 20.0f);
        sReverbMix.init (p.reverbMix, sr, 20.0f); sWidth.init (p.width, sr, 20.0f);
        sAir.init (p.air, sr, 30.0f); sSatDrive.init (p.satDrive, sr, 30.0f);
        sTape.init (d.tape, sr, 50.0f); sDrive.init (d.drive, sr, 50.0f); sGlue.init (d.glue, sr, 50.0f);
        sLow.init (dbToGain (d.lowDb) - 1.0f, sr, 50.0f);
        sHigh.init (dbToGain (d.highDb + 6.0f * toneOffset()) - 1.0f, sr, 50.0f);
        lowCutLog = std::log (d.lowCutHz);
        sDelayTime.init ((float) (delayBeats (p.delayTime) * 60.0 / bpm * sr), sr, 80.0f);
        sPumpGain.init (1.0f, sr, 1.5f);
    }

    void processChunk (float* L, float* R, int n, double bpm, double ppq)
    {
        const SoundDef& d = cur();
        if (needsSnap) snap (d, bpm);
        curBpm = bpm;

        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        for (auto& v : voices) if (v.active) renderVoice (v, L, R, n);
        renderReleaseNoises (L, R, n);

        const float loC = onePoleCoef (220.0f, fs), hiC = onePoleCoef (3200.0f, fs);
        const float q = 0.707f + p.resonance * p.resonance * 9.0f;
        const float logCut = std::log (std::clamp (p.cutoffHz, 40.0f, 20000.0f));
        const float cAtt = std::exp (-1.0f / (0.0002f * fs)), cRel = std::exp (-1.0f / (0.15f * fs));
        const double pumpDiv = p.pumpRate == 2 ? 4.0 : (p.pumpRate == 1 ? 2.0 : 1.0);
        const double ppqInc = bpm / 60.0 / sr;
        const float delayTarget = (float) std::min (delayBeats (p.delayTime) * 60.0 / bpm * sr, 3.0 * sr);
        const float dlLp = onePoleCoef (4500.0f, fs), dlHp = onePoleCoef (180.0f, fs);
        const float dcCoef = 1.0f - kTwoPi * 12.0f / fs;
        const float fb = std::clamp (p.delayFeedback, 0.0f, 0.95f);
        sideHp.set (120.0f, 0.707f, fs);

        // low cut changes slowly (only when the sound changes), so glide it once per block
        lowCutLog += (std::log (d.lowCutHz) - lowCutLog) * (1.0f - std::exp (-(float) n / (0.05f * fs)));
        lowCutL.set (std::exp (lowCutLog), 0.707f, fs); lowCutR.set (std::exp (lowCutLog), 0.707f, fs);

        const float lowTarget  = dbToGain (d.lowDb) - 1.0f;
        const float highTarget = dbToGain (d.highDb + 6.0f * toneOffset()) - 1.0f;
        const float volTarget  = dbToGain (p.volumeDb + d.gainDb);

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            //---- colour: low cut + shelves (Tone drives the high shelf)
            l = lowCutL.hp (l); r = lowCutR.hp (r);
            const float lowG  = sLow.next (lowTarget);
            const float highG = sHigh.next (highTarget);
            loL += loC * (l - loL); loR += loC * (r - loR);
            hiL += hiC * (l - hiL); hiR += hiC * (r - hiR);
            l += lowG * loL + highG * (l - hiL);
            r += lowG * loR + highG * (r - hiR);

            const float drive = sDrive.next (d.drive);
            if (drive > 0.001f)
            {
                const float k = 1.0f + 3.0f * drive;
                l = std::tanh (l * k) / k; r = std::tanh (r * k) / k;
            }

            const float glue = sGlue.next (d.glue);
            if (glue > 0.001f)
            {
                // instant peak detection (no "first note pops" after silence), smoothed gain
                const float thr = -6.0f - 20.0f * glue, ratio = 1.5f + 4.5f * glue;
                const float lvl = std::max (std::abs (l), std::abs (r));
                compEnv = lvl > compEnv ? lvl : lvl + (compEnv - lvl) * cRel;
                const float over = 20.0f * std::log10 (compEnv + 1.0e-9f) - thr;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                const float target = dbToGain (-gr + glue * 8.0f);
                compGain = target < compGain ? target + (compGain - target) * cAtt : target;
                l *= compGain; r *= compGain;
            }

            const float tape = sTape.next (d.tape);
            tapeL.push (l); tapeR.push (r);
            if (tape > 0.001f)
            {
                wowPh += 0.5 / sr; wowPh -= std::floor (wowPh);
                flutterPh += 6.1 / sr; flutterPh -= std::floor (flutterPh);
                const float md = (0.004f + tape * (0.0016f * std::sin (kTwoPi * (float) wowPh)
                                                   + 0.00025f * std::sin (kTwoPi * (float) flutterPh))) * fs;
                const float wl = tapeL.read (md), wr = tapeR.read (md);
                hissLp += 0.45f * (hissRng.next() - hissLp);
                const float lpC = onePoleCoef (16000.0f - 11000.0f * tape, fs);
                tapeLpL += lpC * (wl - tapeLpL); tapeLpR += lpC * (wr - tapeLpR);
                const float mixT = std::min (1.0f, tape * 4.0f);
                l = l + mixT * (tapeLpL + hissLp * 0.004f * tape - l);
                r = r + mixT * (tapeLpR + hissLp * 0.004f * tape - r);
            }

            //---- FILTER
            {
                const float on = sFilterOn.next (p.filterOn ? 1.0f : 0.0f);
                const float fc = std::exp (sCutoff.next (logCut));
                if ((i & 7) == 0) { filtL.set (fc, q, fs); filtR.set (fc, q, fs); }
                const float fl = filtL.process (l), fr = filtR.process (r);
                l += on * (fl - l); r += on * (fr - r);
            }

            //---- SATURATION
            {
                const float on = sSatOn.next (p.satOn ? 1.0f : 0.0f);
                float sl = l, sr2 = r;
                sat.process (sl, sr2, sSatDrive.next (p.satDrive), fs);
                l += on * (sl - l); r += on * (sr2 - r);
            }

            //---- AIR
            air.process (l, r, sAir.next (p.air));

            //---- PUMP
            {
                const float on = sPumpOn.next (p.pumpOn ? 1.0f : 0.0f);
                const double ph = (ppq + i * ppqInc) * pumpDiv;
                const float frac = (float) (ph - std::floor (ph));
                const float c = std::max (0.0f, 1.0f - frac / 0.75f);
                const float duck = c * c * (3.0f - 2.0f * c);
                const float pg = sPumpGain.next (1.0f - on * p.pumpDepth * duck);
                l *= pg; r *= pg;
            }

            //---- CHORUS
            {
                const float on = sChorusOn.next (p.chorusOn ? 1.0f : 0.0f) * sChorusMix.next (p.chorusMix);
                chorusLine.push (0.5f * (l + r));
                chPh += 0.55 / sr; chPh -= std::floor (chPh);
                const float a = std::sin (kTwoPi * (float) chPh), b = std::sin (kTwoPi * (float) chPh + 2.1f);
                const float wl = chorusLine.read ((0.011f + 0.0032f * a) * fs);
                const float wr = chorusLine.read ((0.013f + 0.0032f * b) * fs);
                l = l * (1.0f - 0.3f * on) + wl * 0.75f * on;
                r = r * (1.0f - 0.3f * on) + wr * 0.75f * on;
            }

            //---- DELAY (tempo-synced ping-pong, darker repeats)
            {
                const float on = sDelayOn.next (p.delayOn ? 1.0f : 0.0f);
                const float t = sDelayTime.next (delayTarget);
                const float dl = delayL.read (t), dr = delayR.read (t);
                dlFbL += dlLp * (dr - dlFbL); dlHpL += dlHp * (dlFbL - dlHpL);
                dlFbR += dlLp * (dl - dlFbR); dlHpR += dlHp * (dlFbR - dlHpR);
                delayL.push (0.5f * (l + r) * on + fb * (dlFbL - dlHpL));
                delayR.push (fb * (dlFbR - dlHpR));
                const float mix = sDelayMix.next (p.delayMix) * 1.2f;
                l += dl * mix; r += dr * mix;
            }

            //---- REVERB
            {
                const float on = sReverbOn.next (p.reverbOn ? 1.0f : 0.0f);
                float wl = 0.0f, wr = 0.0f;
                reverb.process (l * on, r * on, p.reverbSize, wl, wr);
                const float mix = sReverbMix.next (p.reverbMix);
                l = l * (1.0f - 0.25f * mix * on) + wl * mix * 1.3f;
                r = r * (1.0f - 0.25f * mix * on) + wr * mix * 1.3f;
            }

            //---- WIDTH (side below 120 Hz removed: the low end stays mono and tight)
            {
                const float w = sWidth.next (p.width) * 2.0f;
                const float m = 0.5f * (l + r), s = sideHp.hp (0.5f * (l - r)) * w;
                l = m + s; r = m - s;
            }

            //---- DC block, volume, soft limiter
            const float yl = l - dcInL + dcCoef * dcL; dcInL = l; dcL = yl;
            const float yr = r - dcInR + dcCoef * dcR; dcInR = r; dcR = yr;
            const float vol = sVol.next (volTarget);
            L[i] = softClip (yl * vol);
            R[i] = softClip (yr * vol);
        }
    }

    static inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.75f) return x;
        const float y = 0.75f + 0.25f * std::tanh ((a - 0.75f) / 0.25f);
        return x < 0.0f ? -y : y;
    }

    //--------------------------------------------------------------------------
    double sr = 44100.0;
    float fs = 44100.0f;
    int block = 512;
    Params p;
    const SampleBank* bank = nullptr;
    const SampleBank* userBank = nullptr;
    const SoundDef* custom = nullptr;
    std::array<Voice, kMaxVoices> voices {};
    std::array<RelVoice, kRelVoices> relVoices {};
    std::vector<float> mono, side;
    uint64_t counter = 0;
    double curBpm = 120.0;
    bool pedal = false, needsSnap = true;
    float modWheel = 0.0f;

    FdnReverb reverb;
    AirProcessor air;
    Saturator sat;
    Svf lowCutL, lowCutR, filtL, filtR, sideHp;
    DelayLine chorusLine, tapeL, tapeR, delayL, delayR;
    float loL = 0, loR = 0, hiL = 0, hiR = 0, compEnv = 0, compGain = 1;
    float tapeLpL = 0, tapeLpR = 0, hissLp = 0, dlFbL = 0, dlFbR = 0, dlHpL = 0, dlHpR = 0;
    float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0;
    Rng hissRng { 12345u };
    double wowPh = 0, flutterPh = 0, chPh = 0, freePpq = 0;
    Smoother sVol, sCutoff, sFilterOn, sSatOn, sChorusOn, sDelayOn, sReverbOn, sPumpOn, sChorusMix, sDelayMix,
             sReverbMix, sWidth, sAir, sSatDrive, sTape, sDrive, sGlue, sLow, sHigh, sDelayTime, sPumpGain;
    float lowCutLog = 3.0f;
};
} // namespace hk
