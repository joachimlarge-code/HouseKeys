// Renders every HouseKeys piano to a WAV file (no JUCE needed):
//   g++ -O2 -std=c++17 tools/render_demo.cpp -o render_demo && ./render_demo demo.wav
// Energetic pianos play a syncopated house groove, calm pianos play slow chords.
// Optional 2nd argument: a single piano index to render.
#include "../Source/Engine.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace
{
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

struct Ev { int t; bool on; int note; float vel; };
} // namespace

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "housekeys_demo.wav";
    const int only = argc > 2 ? std::atoi (argv[2]) : -1;
    const int sr = 44100, blockSize = 256;
    const double bpm = 124.0;
    const int s16 = (int) std::lround (sr * 60.0 / bpm / 4.0);

    // A minor progression: Am7 - Fmaj7 - Dm7 - Em7 (left-hand root + right-hand chord)
    const std::vector<std::vector<int>> chords = {
        { 45, 57, 60, 64, 67 }, { 41, 53, 57, 60, 64 }, { 38, 53, 57, 60, 62 }, { 40, 55, 59, 62, 64 } };
    struct Hit { int step, len; float vel; };
    const std::vector<Hit> groove = { { 0, 2, 0.95f }, { 3, 2, 0.8f }, { 6, 3, 0.85f }, { 10, 2, 0.8f }, { 12, 3, 0.85f } };

    std::vector<float> L, R;
    double cpuSec = 0, audioSec = 0;
    std::printf ("%-16s %7s %7s\n", "piano", "peak", "rms(dB)");

    for (int pi = 0; pi < hk::kNumPianos; ++pi)
    {
        if (only >= 0 && pi != only) continue;
        const auto& def = hk::pianos()[(size_t) pi];
        hk::Params prm;
        prm.piano = pi;

        hk::Engine eng;
        eng.setParams (prm);
        eng.prepare (sr, blockSize);

        const int bars = 4;
        std::vector<Ev> evs;
        for (int bar = 0; bar < bars; ++bar)
        {
            const auto& ch = chords[(size_t) bar];
            if (def.energetic)
                for (auto& h : groove)
                {
                    const int t0 = (bar * 16 + h.step) * s16, t1 = t0 + h.len * s16 - s16 / 4;
                    for (int nn : ch) { evs.push_back ({ t0, true, nn, h.vel }); evs.push_back ({ t1, false, nn, 0 }); }
                }
            else
            {
                // gentle: bass + rolled chord, then a small melody note
                const int t0 = bar * 16 * s16;
                for (size_t k = 0; k < ch.size(); ++k)
                {
                    const int t = t0 + (int) k * (s16 / 3);
                    evs.push_back ({ t, true, ch[k], k == 0 ? 0.6f : 0.45f });
                    evs.push_back ({ t0 + 15 * s16, false, ch[k], 0 });
                }
                const int mel = ch.back() + 5;
                evs.push_back ({ t0 + 8 * s16, true, mel, 0.5f });
                evs.push_back ({ t0 + 14 * s16, false, mel, 0 });
            }
        }
        std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
                          { return a.t != b.t ? a.t < b.t : (! a.on && b.on); });

        const int total = bars * 16 * s16 + sr * 2;
        std::vector<float> bl ((size_t) total), br ((size_t) total);
        size_t ei = 0;
        int pos = 0;
        const auto t0 = std::chrono::steady_clock::now();
        while (pos < total)
        {
            int end = std::min (total, pos + blockSize);
            while (ei < evs.size() && evs[ei].t <= pos)
            {
                if (evs[ei].on) eng.noteOn (evs[ei].note, evs[ei].vel); else eng.noteOff (evs[ei].note);
                ++ei;
            }
            if (ei < evs.size() && evs[ei].t < end) end = evs[ei].t;
            eng.process (bl.data() + pos, br.data() + pos, end - pos, bpm, pos * bpm / 60.0 / sr, true);
            pos = end;
        }
        cpuSec += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        audioSec += total / (double) sr;

        float peak = 0.0f; double sum = 0.0;
        const int active = bars * 16 * s16;
        for (int i = 0; i < total; ++i)
            peak = std::max (peak, std::max (std::abs (bl[(size_t) i]), std::abs (br[(size_t) i])));
        for (int i = 0; i < active; ++i)
            sum += 0.5 * (bl[(size_t) i] * bl[(size_t) i] + br[(size_t) i] * br[(size_t) i]);
        std::printf ("%-16s %7.3f %7.1f\n", def.name, peak, 10.0 * std::log10 (sum / active + 1e-12));

        L.insert (L.end(), bl.begin(), bl.end());
        R.insert (R.end(), br.begin(), br.end());
    }

    writeWav (out, L, R, sr);
    std::printf ("wrote %s (%.1f s), realtime factor %.1fx\n", out.c_str(), audioSec, audioSec / cpuSec);
    return 0;
}
