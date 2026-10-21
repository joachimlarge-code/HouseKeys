// HouseKeys - multisample bank (no JUCE dependency).
// Holds decoded sustain samples (several velocity layers per root note) and optional
// key-release noises. Decoding/loading lives in SampleLoader (JUCE side).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hk
{
struct SampleZone
{
    int root = 60;                 // MIDI note the sample was recorded at
    int layer = 0;                 // 0 = softest
    double sampleRate = 48000.0;
    int length = 0;                // frames
    std::vector<int16_t> data;     // interleaved stereo
    float gain = 1.0f;             // makes every layer as loud as the hardest layer of the same note

    float l (int i) const { return data[(size_t) i * 2] * (1.0f / 32768.0f); }
    float r (int i) const { return data[(size_t) i * 2 + 1] * (1.0f / 32768.0f); }
};

class SampleBank
{
public:
    std::string name;
    std::vector<SampleZone> zones;
    std::vector<SampleZone> releases;   // index = midi note (empty zone = none)
    int numLayers = 0;

    // Call after all zones are added.
    void finalise()
    {
        numLayers = 0;
        roots.clear();
        for (auto& z : zones)
        {
            numLayers = std::max (numLayers, z.layer + 1);
            if (std::find (roots.begin(), roots.end(), z.root) == roots.end()) roots.push_back (z.root);
        }
        std::sort (roots.begin(), roots.end());
        table.assign ((size_t) roots.size() * (size_t) std::max (1, numLayers), -1);
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto ri = (size_t) (std::lower_bound (roots.begin(), roots.end(), zones[i].root) - roots.begin());
            table[ri * (size_t) numLayers + (size_t) zones[i].layer] = (int) i;
        }
        // fill gaps with the nearest existing layer of the same root
        for (size_t ri = 0; ri < roots.size(); ++ri)
            for (int L = 0; L < numLayers; ++L)
                if (table[ri * (size_t) numLayers + (size_t) L] < 0)
                    for (int d = 1; d < numLayers; ++d)
                    {
                        for (int c : { L - d, L + d })
                            if (c >= 0 && c < numLayers && table[ri * (size_t) numLayers + (size_t) c] >= 0)
                            { table[ri * (size_t) numLayers + (size_t) L] = table[ri * (size_t) numLayers + (size_t) c]; break; }
                        if (table[ri * (size_t) numLayers + (size_t) L] >= 0) break;
                    }
        // Level-match the layers of each note to its hardest layer. Velocity then controls loudness
        // smoothly, while the layers only change the timbre (no phasey layer crossfades).
        for (size_t ri = 0; ri < roots.size(); ++ri)
        {
            const SampleZone* top = zone ((int) ri, numLayers - 1);
            const float ref = top != nullptr ? attackRms (*top) : 0.0f;
            for (int L = 0; L < numLayers; ++L)
            {
                const int zi = table[ri * (size_t) numLayers + (size_t) L];
                if (zi < 0) continue;
                auto& z = zones[(size_t) zi];
                const float r = attackRms (z);
                z.gain = (ref > 0.0f && r > 0.0f) ? std::clamp (ref / r, 0.25f, 8.0f) : 1.0f;
            }
        }
        bytes = 0;
        for (auto& z : zones) bytes += z.data.size() * sizeof (int16_t);
        for (auto& z : releases) bytes += z.data.size() * sizeof (int16_t);
    }

    bool empty() const { return roots.empty() || numLayers == 0; }
    size_t memoryBytes() const { return bytes; }

    // nearest recorded root for a note
    int rootIndexFor (int note) const
    {
        auto it = std::lower_bound (roots.begin(), roots.end(), note);
        if (it == roots.end()) return (int) roots.size() - 1;
        if (it == roots.begin()) return 0;
        const int hi = (int) (it - roots.begin()), lo = hi - 1;
        return (note - roots[(size_t) lo] <= roots[(size_t) hi] - note) ? lo : hi;
    }

    const SampleZone* zone (int rootIndex, int layer) const
    {
        const int i = table[(size_t) rootIndex * (size_t) numLayers + (size_t) std::clamp (layer, 0, numLayers - 1)];
        return i >= 0 ? &zones[(size_t) i] : nullptr;
    }

    const SampleZone* release (int note) const
    {
        if (note < 0 || note >= (int) releases.size() || releases[(size_t) note].length == 0) return nullptr;
        return &releases[(size_t) note];
    }

private:
    static float attackRms (const SampleZone& z)
    {
        const int n = std::min (z.length, (int) (0.25 * z.sampleRate));
        double sum = 0.0;
        for (int i = 0; i < n; ++i) sum += 0.5 * ((double) z.l (i) * z.l (i) + (double) z.r (i) * z.r (i));
        return n > 0 ? (float) std::sqrt (sum / n) : 0.0f;
    }

    std::vector<int> roots, table;
    size_t bytes = 0;
};
} // namespace hk
