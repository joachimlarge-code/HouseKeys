// Renders every factory preset through the HouseKeys engine to a WAV file,
// playing a short house progression. No JUCE needed:
//   g++ -O2 -std=c++17 tools/render_demo.cpp -o render_demo && ./render_demo demo.wav
#include "../Source/Engine.h"
#include "../Source/Presets.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
struct Hit { int step16; int len16; };

void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::perror ("fopen"); return; }
    const uint32_t frames = (uint32_t) L.size(), dataBytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        int16_t s[2] = { (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f),
                         (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
}
} // namespace

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "housekeys_demo.wav";
    const int sr = 44100, blockSize = 256;
    const double bpm = 124.0;
    const int samplesPer16 = (int) std::lround (sr * 60.0 / bpm / 4.0);

    // classic syncopated house piano rhythm (one bar of 16ths)
    const std::vector<Hit> rhythm = { { 0, 2 }, { 3, 2 }, { 6, 3 }, { 10, 2 }, { 12, 3 } };

    // full voicings (left-hand root + right-hand chord), A minor
    const std::vector<std::vector<int>> voiced = {
        { 45, 57, 60, 64, 67 },  // Am7
        { 41, 53, 57, 60, 64 },  // Fmaj7
        { 38, 53, 57, 60, 62 },  // Dm7
        { 40, 55, 59, 62, 64 },  // Em7
    };
    // single roots for presets that use the built-in chord mode (minor chords)
    const std::vector<int> roots = { 57, 50, 52, 50 };

    std::vector<float> L, R;
    const auto& presets = hk::factoryPresets();

    for (const auto& pr : presets)
    {
        hk::Engine eng;
        eng.prepare (sr, blockSize);
        eng.setParams (pr.p);

        const int bars = 4;
        const int total = bars * 16 * samplesPer16 + sr * 2; // + tail
        struct Ev { int t; bool on; int note; float vel; };
        std::vector<Ev> evs;
        for (int bar = 0; bar < bars; ++bar)
            for (size_t h = 0; h < rhythm.size(); ++h)
            {
                const int t0 = (bar * 16 + rhythm[h].step16) * samplesPer16;
                const int t1 = t0 + rhythm[h].len16 * samplesPer16 - samplesPer16 / 4;
                const float vel = h == 0 ? 0.95f : 0.8f;
                if (pr.p.chord != hk::kChordOff)
                    { evs.push_back ({ t0, true, roots[(size_t) bar], vel }); evs.push_back ({ t1, false, roots[(size_t) bar], 0 }); }
                else
                    for (int nn : voiced[(size_t) bar])
                        { evs.push_back ({ t0, true, nn, vel }); evs.push_back ({ t1, false, nn, 0 }); }
            }
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
                          { return a.t != b.t ? a.t < b.t : (! a.on && b.on); });

        std::vector<float> bl ((size_t) total), br ((size_t) total);
        size_t ei = 0;
        int pos = 0;
        while (pos < total)
        {
            int end = std::min (total, pos + blockSize);
            while (ei < evs.size() && evs[ei].t <= pos)
            {
                if (evs[ei].on) eng.noteOn (evs[ei].note, evs[ei].vel); else eng.noteOff (evs[ei].note);
                ++ei;
            }
            if (ei < evs.size() && evs[ei].t < end) end = evs[ei].t;
            const double ppq = pos * bpm / 60.0 / sr;
            eng.process (bl.data() + pos, br.data() + pos, end - pos, bpm, ppq, true);
            pos = end;
        }

        float peak = 0.0f; double rms = 0.0;
        for (int i = 0; i < total; ++i)
        {
            peak = std::max (peak, std::max (std::abs (bl[(size_t) i]), std::abs (br[(size_t) i])));
            rms += bl[(size_t) i] * bl[(size_t) i];
        }
        std::printf ("%-18s peak %.3f  rms %.4f\n", pr.name, peak, std::sqrt (rms / total));

        L.insert (L.end(), bl.begin(), bl.end());
        R.insert (R.end(), br.begin(), br.end());
    }

    writeWav (out, L, R, sr);
    std::printf ("wrote %s (%.1f s)\n", out.c_str(), L.size() / (double) sr);
    return 0;
}
