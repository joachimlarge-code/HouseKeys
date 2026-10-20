// HouseKeys factory presets (no JUCE dependency).
#pragma once

#include "Engine.h"

namespace hk
{
struct Preset
{
    const char* name;
    Params      p;
};

inline Params makeParams (int model, float bright, float layer, float detune, float tape,
                          float attackMs, float decayS, float releaseS, float velSens, int chord,
                          float cutoff, float reso, float chorus, float reverb, float size,
                          float comp, float pump, int pumpRate, float volumeDb)
{
    Params p;
    p.model = model; p.brightness = bright; p.layer = layer; p.detune = detune; p.tape = tape;
    p.attackMs = attackMs; p.decayS = decayS; p.releaseS = releaseS; p.velSens = velSens;
    p.chord = chord; p.cutoffHz = cutoff; p.resonance = reso; p.chorus = chorus;
    p.reverb = reverb; p.roomSize = size; p.comp = comp; p.pump = pump; p.pumpRate = pumpRate;
    p.volumeDb = volumeDb;
    return p;
}

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = {
        //                          model     brt    lay   det  tape  atk   dec   rel   vel   chord      cutoff   res   cho   rev   size  comp  pump rate  vol
        { "90s House Piano",  makeParams (kClassic, 0.75f, 0.45f, 6.0f, 0.0f, 0.5f, 2.5f, 0.35f, 0.5f, kChordOff, 16000.f, 0.10f, 0.25f, 0.22f, 0.55f, 0.45f, 0.0f, 0, -4.0f) },
        { "Deep House Keys",  makeParams (kDeep,    0.45f, 0.50f, 4.0f, 0.0f, 2.0f, 4.0f, 0.60f, 0.6f, kChordOff,  9000.f, 0.10f, 0.45f, 0.30f, 0.70f, 0.30f, 0.0f, 0, -8.0f) },
        { "Garage Organ",     makeParams (kOrgan,   0.55f, 0.60f, 8.0f, 0.0f, 3.0f, 1.0f, 0.12f, 0.2f, kChordOff, 12000.f, 0.15f, 0.50f, 0.18f, 0.45f, 0.40f, 0.0f, 0, -3.5f) },
        { "Lo-fi Piano",      makeParams (kClassic, 0.40f, 0.35f, 5.0f, 0.7f, 1.0f, 3.0f, 0.40f, 0.5f, kChordOff,  5000.f, 0.10f, 0.20f, 0.30f, 0.60f, 0.35f, 0.0f, 0, -8.5f) },
        { "Min7 Stab",        makeParams (kStab,    0.85f, 0.40f, 7.0f, 0.0f, 0.5f, 0.6f, 0.15f, 0.4f, kMin7,     15000.f, 0.20f, 0.20f, 0.25f, 0.50f, 0.60f, 0.0f, 0, -1.5f) },
        { "Pumping Min9",     makeParams (kClassic, 0.70f, 0.45f, 6.0f, 0.0f, 0.5f, 2.0f, 0.30f, 0.5f, kMin9,     14000.f, 0.10f, 0.30f, 0.30f, 0.60f, 0.50f, 0.7f, 0, -1.5f) },
        { "Dark Filtered",    makeParams (kDeep,    0.60f, 0.50f, 5.0f, 0.2f, 1.0f, 3.5f, 0.50f, 0.5f, kMin11,     1200.f, 0.50f, 0.35f, 0.35f, 0.75f, 0.40f, 0.0f, 0, -6.0f) },
    };
    return presets;
}
} // namespace hk
