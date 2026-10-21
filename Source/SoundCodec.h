// HouseKeys - JSON <-> sound definition (presets on disk, project state, AI answers).
// fromVar() only changes the fields that are present, so it can merge a partial design
// on top of a base sound.
#pragma once

#include <juce_core/juce_core.h>
#include "Sounds.h"

namespace codec
{
using juce::var;

inline void put (juce::DynamicObject& o, const char* k, float v) { o.setProperty (k, v); }
inline void put (juce::DynamicObject& o, const char* k, int v)   { o.setProperty (k, v); }
inline void put (juce::DynamicObject& o, const char* k, bool v)  { o.setProperty (k, v); }

inline void get (const var& o, const char* k, float& v) { if (o.hasProperty (k)) v = (float) o.getProperty (k, v); }
inline void get (const var& o, const char* k, int& v)   { if (o.hasProperty (k)) v = (int) o.getProperty (k, v); }
inline void get (const var& o, const char* k, bool& v)  { if (o.hasProperty (k)) v = (bool) o.getProperty (k, v); }

#define HK_FIELDS_SAMPLER(X) X(level) X(dynamics) X(velCurve) X(relNoise) X(octave)
#define HK_FIELDS_ORGAN(X)   X(level) X(decay) X(click)
#define HK_FIELDS_FM(X)      X(level) X(ratioA) X(indexA) X(susA) X(decA) X(octB) X(ratioB) X(indexB) X(decB) X(susB) \
                             X(mixB) X(feedback) X(decay) X(sustain) X(bark)
#define HK_FIELDS_KEYS(X)    X(level) X(tilt) X(kc) X(odd) X(h2) X(h3) X(tau) X(bright) X(sus) X(tauS) X(detune) X(spread) X(brightS)
#define HK_FIELDS_VA(X)      X(level) X(wave1) X(wave2) X(semi2) X(fine2) X(mix2) X(unison) X(detune) X(spread) X(sub) X(noise) \
                             X(cutoff) X(reso) X(envAmt) X(keytrack) X(velAmt) X(fA) X(fD) X(fS) X(fR) X(decayMs) X(sustain) \
                             X(poles) X(drift) X(motion) X(velSens)
#define HK_FIELDS_SOUND(X)   X(mono) X(glideMs) X(lowCutHz) X(lowDb) X(highDb) X(drive) X(glue) X(tape) X(gainDb)
#define HK_FIELDS_PARAMS(X)  X(tone) X(air) X(attackMs) X(decay) X(releaseS) X(width) X(volumeDb) X(chord) X(filterOn) \
                             X(cutoffHz) X(resonance) X(satOn) X(satDrive) X(chorusOn) X(chorusMix) X(delayOn) X(delayTime) \
                             X(delayFeedback) X(delayMix) X(reverbOn) X(reverbMix) X(reverbSize) X(pumpOn) X(pumpDepth) \
                             X(pumpRate) X(vibOn) X(vibRate) X(vibDepth)

template <class T, class F> var object (const T& t, F fields)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    fields (*o, t);
    return var (o.get());
}

inline var toVar (const hk::Params& p)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
#define X(f) put (*o, #f, p.f);
    HK_FIELDS_PARAMS (X)
#undef X
    return var (o.get());
}

inline void fromVar (const var& v, hk::Params& p)
{
#define X(f) get (v, #f, p.f);
    HK_FIELDS_PARAMS (X)
#undef X
}

inline var toVar (const hk::SoundDef& d)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("category", d.category);
    { juce::DynamicObject::Ptr s = new juce::DynamicObject();
#define X(f) put (*s, #f, d.sampler.f);
      HK_FIELDS_SAMPLER (X)
#undef X
      o->setProperty ("sampler", var (s.get())); }
    { juce::DynamicObject::Ptr s = new juce::DynamicObject();
#define X(f) put (*s, #f, d.organ.f);
      HK_FIELDS_ORGAN (X)
#undef X
      juce::Array<var> bars; for (auto b : d.organ.bars) bars.add (b);
      s->setProperty ("bars", bars);
      o->setProperty ("organ", var (s.get())); }
    { juce::DynamicObject::Ptr s = new juce::DynamicObject();
#define X(f) put (*s, #f, d.fm.f);
      HK_FIELDS_FM (X)
#undef X
      o->setProperty ("fm", var (s.get())); }
    { juce::DynamicObject::Ptr s = new juce::DynamicObject();
#define X(f) put (*s, #f, d.keys.f);
      HK_FIELDS_KEYS (X)
#undef X
      o->setProperty ("keys", var (s.get())); }
    { juce::DynamicObject::Ptr s = new juce::DynamicObject();
#define X(f) put (*s, #f, d.va.f);
      HK_FIELDS_VA (X)
#undef X
      o->setProperty ("va", var (s.get())); }
#define X(f) put (*o, #f, d.f);
    HK_FIELDS_SOUND (X)
#undef X
    o->setProperty ("params", toVar (d.preset));
    return var (o.get());
}

// keeps every value sane, whatever a file or the AI says
inline void sanitise (hk::SoundDef& d)
{
    auto c = [] (float& v, float lo, float hi) { v = juce::jlimit (lo, hi, std::isfinite (v) ? v : lo); };
    c (d.sampler.level, 0, 1.5f); c (d.sampler.octave, 0, 1); c (d.sampler.dynamics, 0, 30); c (d.sampler.velCurve, 0.3f, 3);
    c (d.sampler.relNoise, 0, 1);
    c (d.organ.level, 0, 1.5f); c (d.organ.decay, 0, 10); c (d.organ.click, 0, 1);
    for (auto& b : d.organ.bars) c (b, 0, 1);
    c (d.fm.level, 0, 1.5f); c (d.fm.ratioA, 0.25f, 16); c (d.fm.indexA, 0, 8); c (d.fm.ratioB, 0.25f, 20); c (d.fm.indexB, 0, 8);
    c (d.fm.decA, 0.01f, 10); c (d.fm.decB, 0.01f, 10); c (d.fm.decay, 0.05f, 20); c (d.fm.octB, 0.25f, 4); c (d.fm.feedback, 0, 1);
    c (d.keys.level, 0, 1.5f); c (d.keys.kc, 1, 60); c (d.keys.tau, 0.01f, 10); c (d.keys.tauS, 0.05f, 20); c (d.keys.detune, 0, 50);
    c (d.va.level, 0, 1.5f); c (d.va.mix2, 0, 1.5f); c (d.va.semi2, -24, 24); c (d.va.fine2, -50, 50);
    c (d.va.detune, 0, 60); c (d.va.spread, 0, 1); c (d.va.sub, 0, 1); c (d.va.noise, 0, 0.5f);
    c (d.va.cutoff, 40, 18000); c (d.va.reso, 0, 0.95f); c (d.va.envAmt, -2, 6); c (d.va.keytrack, 0, 1); c (d.va.velAmt, 0, 2);
    c (d.va.fA, 0.5f, 5000); c (d.va.fD, 1, 10000); c (d.va.fS, 0, 1); c (d.va.fR, 1, 10000);
    c (d.va.decayMs, 0, 10000); c (d.va.sustain, 0, 1); c (d.va.drift, 0, 15); c (d.va.motion, 0, 1); c (d.va.velSens, 0, 1);
    d.va.unison = juce::jlimit (1, 7, d.va.unison); d.va.poles = d.va.poles >= 3 ? 4 : 2;
    d.va.wave1 = juce::jlimit (0, 3, d.va.wave1); d.va.wave2 = juce::jlimit (0, 3, d.va.wave2);
    c (d.glideMs, 0, 1000); c (d.lowCutHz, 20, 600); c (d.lowDb, -12, 12); c (d.highDb, -12, 12);
    c (d.drive, 0, 1); c (d.glue, 0, 1); c (d.tape, 0, 1); c (d.gainDb, -24, 12);
    d.sampler.user = false;   // custom sounds never point at the dropped sample
}

inline void fromVar (const var& v, hk::SoundDef& d)
{
    if (v.hasProperty ("category")) d.category = (int) v.getProperty ("category", d.category);
    if (auto s = v.getProperty ("sampler", {}); s.isObject())
    {
#define X(f) get (s, #f, d.sampler.f);
        HK_FIELDS_SAMPLER (X)
#undef X
    }
    if (auto s = v.getProperty ("organ", {}); s.isObject())
    {
#define X(f) get (s, #f, d.organ.f);
        HK_FIELDS_ORGAN (X)
#undef X
        if (auto* bars = s.getProperty ("bars", {}).getArray())
            for (int i = 0; i < juce::jmin (5, bars->size()); ++i) d.organ.bars[(size_t) i] = (float) (*bars)[i];
    }
    if (auto s = v.getProperty ("fm", {}); s.isObject())
    {
#define X(f) get (s, #f, d.fm.f);
        HK_FIELDS_FM (X)
#undef X
    }
    if (auto s = v.getProperty ("keys", {}); s.isObject())
    {
#define X(f) get (s, #f, d.keys.f);
        HK_FIELDS_KEYS (X)
#undef X
    }
    if (auto s = v.getProperty ("va", {}); s.isObject())
    {
#define X(f) get (s, #f, d.va.f);
        HK_FIELDS_VA (X)
#undef X
    }
#define X(f) get (v, #f, d.f);
    HK_FIELDS_SOUND (X)
#undef X
    if (auto pv = v.getProperty ("params", {}); pv.isObject()) fromVar (pv, d.preset);
    sanitise (d);
}
} // namespace codec
