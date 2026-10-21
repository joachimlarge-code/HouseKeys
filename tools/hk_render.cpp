// Offline render tool: plays HouseKeys sounds through the real engine (with the real samples)
// and writes a WAV. Build with -DHK_BUILD_TOOLS=ON.
//
//   hk_render out.wav            all sounds, a fitting phrase for each category
//   hk_render out.wav 3          only sound #3
//   hk_render out.wav 3 notes    chord test: hk_render out.wav <sound> <gate s> <note> <note> ...
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "SampleLoader.h"
#include <chrono>

namespace
{
struct Ev { int t; bool on; int note; float vel; };

void addNote (std::vector<Ev>& e, int t0, int t1, int note, float vel)
{
    e.push_back ({ t0, true, note, vel });
    e.push_back ({ t1, false, note, 0.0f });
}

std::vector<Ev> phraseFor (int category, int sr, double bpm, int& totalSamples)
{
    const int s16 = (int) std::lround (sr * 60.0 / bpm / 4.0);
    std::vector<Ev> e;
    // A minor house progression: Am9 - Fmaj7 - Dm9 - Em7, left hand + right hand
    const std::vector<std::vector<int>> chords = {
        { 45, 60, 64, 67, 71 }, { 41, 60, 64, 65, 69 }, { 38, 60, 62, 65, 69 }, { 40, 59, 62, 64, 67 } };
    const std::vector<std::vector<int>> rh = {
        { 60, 64, 67, 71 }, { 60, 64, 65, 69 }, { 60, 62, 65, 69 }, { 59, 62, 64, 67 } };
    const int bars = 4;

    switch (category)
    {
        case hk::kCatRealPiano:
            for (int b = 0; b < bars; ++b)
            {
                const int t0 = b * 16 * s16;
                const auto& c = chords[(size_t) b];
                for (size_t k = 0; k < c.size(); ++k)
                    addNote (e, t0 + (int) k * s16 / 3, t0 + 15 * s16, c[k], k == 0 ? 0.55f : 0.42f + 0.05f * (float) k);
                const int mel[] = { 76, 74, 72, 71 };
                addNote (e, t0 + 6 * s16, t0 + 9 * s16, mel[b], 0.62f);
                addNote (e, t0 + 10 * s16, t0 + 15 * s16, mel[b] - 3, 0.5f);
            }
            break;

        case hk::kCatPads:
            for (int b = 0; b < bars; ++b)
                for (int n : rh[(size_t) b]) addNote (e, b * 16 * s16, (b + 1) * 16 * s16 - s16 / 2, n, 0.7f);
            break;

        case hk::kCatBass:
        {
            const int roots[] = { 33, 29, 26, 28 };
            for (int b = 0; b < bars; ++b)
                for (int s : { 2, 6, 10, 14, 15 })
                {
                    const int t0 = (b * 16 + s) * s16;
                    addNote (e, t0, t0 + (s == 15 ? s16 / 2 : s16 + s16 / 2), roots[b] + (s == 15 ? 12 : 0), s == 2 ? 0.95f : 0.8f);
                }
            break;
        }

        case hk::kCatLeads:
        {
            // legato melody (overlapping notes -> glide)
            const int mel[] = { 69, 72, 74, 76, 74, 72, 71, 72,   69, 72, 74, 79, 76, 74, 72, 74 };
            const int len[] = { 3, 2, 3, 4, 2, 2, 2, 6,           3, 2, 3, 4, 2, 2, 2, 6 };
            int t = 0;
            for (int k = 0; k < 16; ++k)
            {
                const int t0 = t * s16, t1 = (t + len[k]) * s16 + s16 / 3;   // slight overlap
                addNote (e, t0, t1, mel[k], 0.85f);
                t += len[k];
            }
            break;
        }

        case hk::kCatPlucks:
            for (int b = 0; b < bars; ++b)
            {
                const auto& c = rh[(size_t) b];
                for (int s = 0; s < 16; ++s)
                {
                    if (s % 4 == 1) continue;
                    const int t0 = (b * 16 + s) * s16;
                    addNote (e, t0, t0 + s16 - s16 / 4, c[(size_t) (s * 3 % 4)] + 12 * (s % 8 == 6), 0.6f + 0.3f * (s % 4 == 0));
                }
            }
            break;

        default:   // house groove chords
        {
            struct Hit { int step, len; float vel; };
            const std::vector<Hit> groove = { { 0, 2, 0.95f }, { 3, 2, 0.8f }, { 6, 3, 0.85f }, { 10, 2, 0.8f }, { 12, 3, 0.85f } };
            for (int b = 0; b < bars; ++b)
                for (auto& h : groove)
                {
                    const int t0 = (b * 16 + h.step) * s16, t1 = t0 + h.len * s16 - s16 / 4;
                    for (int n : chords[(size_t) b]) addNote (e, t0, t1, n, h.vel * (category == hk::kCatRhodes ? 0.8f : 1.0f));
                }
        }
    }
    totalSamples = bars * 16 * s16 + sr * 3;
    std::stable_sort (e.begin(), e.end(), [] (const Ev& a, const Ev& b) { return a.t != b.t ? a.t < b.t : (! a.on && b.on); });
    return e;
}

void render (const hk::SampleBank* bank, const hk::Params& prm, const std::vector<Ev>& evs, int total, int sr, double bpm,
             std::vector<float>& L, std::vector<float>& R, double& cpu)
{
    hk::Engine eng;
    eng.setParams (prm);
    eng.prepare (sr, 256);
    eng.setSampleBank (bank);
    L.assign ((size_t) total, 0.0f); R.assign ((size_t) total, 0.0f);
    size_t ei = 0;
    int pos = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (pos < total)
    {
        int end = std::min (total, pos + 256);
        while (ei < evs.size() && evs[ei].t <= pos)
        {
            if (evs[ei].on) eng.noteOn (evs[ei].note, evs[ei].vel); else eng.noteOff (evs[ei].note);
            ++ei;
        }
        if (ei < evs.size() && evs[ei].t < end) end = evs[ei].t;
        eng.process (L.data() + pos, R.data() + pos, end - pos, bpm, pos * bpm / 60.0 / sr, true);
        pos = end;
    }
    cpu += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
}

void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return;
    os.release();
    const float* ch[2] = { L.data(), R.data() };
    w->writeFromFloatArrays (ch, 2, (int) L.size());
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: hk_render out.wav [sound] [gate notes...]\n"); return 1; }
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    const int only = argc > 2 ? std::atoi (argv[2]) : -1;
    const int sr = 44100;
    const double bpm = 124.0;

    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<hk::SampleBank> bank;
    if (const char* dir = std::getenv ("HK_SAMPLES_DIR")) bank = SampleLibrary::loadFromFolder (juce::File (dir));
    else bank = SampleLibrary::loadBuiltIn();
    std::printf ("samples: %d zones, %d layers, %.0f MB, loaded in %.2f s\n", (int) bank->zones.size(), bank->numLayers,
                 bank->memoryBytes() / 1048576.0, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());

    std::vector<float> AL, AR, L, R;
    double cpu = 0, audio = 0;

    if (const char* us = std::getenv ("HK_USER_SAMPLE"))   // test the drag & drop pipeline
    {
        juce::AudioBuffer<float> a; double usr = 44100; juce::String err;
        if (! UserSample::decode (juce::File (us), a, usr, err)) { std::printf ("decode failed: %s\n", err.toRawUTF8()); return 1; }
        const int inLen = a.getNumSamples();
        UserSample::prepare (a, usr);
        const auto an = UserSample::analyse (a, usr);
        const int root = an.pitched ? an.root : -1;
        std::printf ("analysis: %s  (root %d, cents %.1f, chord %d, notes %d)\n", an.describe().toRawUTF8(), an.root, an.cents, (int) an.chord, an.notes);
        auto flac = UserSample::encodeFlac (a, usr);
        juce::AudioBuffer<float> back; double bsr = 0;
        const bool rt = UserSample::decodeFlac (flac, back, bsr);
        std::printf ("user sample: %d -> %d frames, root %d (%s), flac %d bytes, roundtrip %s (%d frames, %.0f Hz)\n",
                     inLen, a.getNumSamples(), root, root >= 0 ? UserSample::noteName (root).toRawUTF8() : "none",
                     (int) flac.getSize(), rt ? "ok" : "FAILED", back.getNumSamples(), bsr);
        auto ub = UserSample::makeBank (back, bsr, root >= 0 ? root : 60);
        for (auto& z : ub->zones) z.tuneCents = an.cents;
        ub->chordSample = an.chord;
        hk::Params prm = hk::sounds()[(size_t) hk::kDroppedSample].preset;
        if (const char* m = std::getenv ("HK_SMP_MODE")) prm.smpMode = std::atoi (m);
        std::vector<Ev> e;
        // house chords: Am7, Fmaj7, Dm7, Em7 (4 notes each), half a second apart
        const int ch[4][4] = { { 57, 60, 64, 67 }, { 53, 57, 60, 64 }, { 50, 53, 57, 60 }, { 52, 55, 59, 62 } };
        for (int k = 0; k < 4; ++k) for (int j = 0; j < 4; ++j) addNote (e, k * sr / 2, k * sr / 2 + sr / 3, ch[k][j], 0.9f);
        std::stable_sort (e.begin(), e.end(), [] (const Ev& x, const Ev& y) { return x.t != y.t ? x.t < y.t : (! x.on && y.on); });
        hk::Engine eng; eng.setParams (prm); eng.prepare (sr, 256); eng.setUserBank (ub.get());
        const int total = 4 * sr;
        L.assign ((size_t) total, 0.0f); R.assign ((size_t) total, 0.0f);
        size_t ei = 0; int pos = 0;
        while (pos < total)
        {
            int end = std::min (total, pos + 256);
            while (ei < e.size() && e[ei].t <= pos) { if (e[ei].on) eng.noteOn (e[ei].note, e[ei].vel); else eng.noteOff (e[ei].note); ++ei; }
            if (ei < e.size() && e[ei].t < end) end = e[ei].t;
            eng.process (L.data() + pos, R.data() + pos, end - pos, bpm, 0, true);
            pos = end;
        }
        writeWav (out, L, R, sr);
        return 0;
    }

    if (argc > 4)   // chord test
    {
        hk::Params prm = hk::sounds()[(size_t) only].preset;
        if (const char* e = std::getenv ("HK_DRY")) if (*e == '1') { prm.reverbOn = prm.chorusOn = prm.delayOn = false; }
        if (const char* e = std::getenv ("HK_AIR")) prm.air = (float) std::atof (e);
        if (const char* e = std::getenv ("HK_WIDTH")) prm.width = (float) std::atof (e);
        const double gate = std::atof (argv[3]);
        std::vector<Ev> e;
        for (int a = 4; a < argc; ++a) addNote (e, 0, (int) (gate * sr), std::atoi (argv[a]), 1.0f);
        std::stable_sort (e.begin(), e.end(), [] (const Ev& a, const Ev& b) { return a.t != b.t ? a.t < b.t : (! a.on && b.on); });
        render (bank.get(), prm, e, (int) ((gate + 2.0) * sr), sr, bpm, L, R, cpu);
        writeWav (out, L, R, sr);
        return 0;
    }

    std::printf ("%-20s %7s %8s\n", "sound", "peak", "rms dB");
    for (int si = 0; si < hk::kNumSounds; ++si)
    {
        if (only >= 0 && si != only) continue;
        const auto& d = hk::sounds()[(size_t) si];
        int total = 0;
        auto evs = phraseFor (d.category, sr, bpm, total);
        render (bank.get(), d.preset, evs, total, sr, bpm, L, R, cpu);
        audio += total / (double) sr;
        float peak = 0; double sum = 0;
        const int active = total - sr * 3;
        for (int i = 0; i < total; ++i) peak = std::max (peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
        for (int i = 0; i < active; ++i) sum += 0.5 * (L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]);
        std::printf ("%-20s %7.3f %8.1f\n", d.name, peak, 10.0 * std::log10 (sum / active + 1e-12));
        AL.insert (AL.end(), L.begin(), L.end());
        AR.insert (AR.end(), R.begin(), R.end());
    }
    writeWav (out, AL, AR, sr);
    std::printf ("wrote %s (%.1f s audio), realtime factor %.1fx\n", out.getFullPathName().toRawUTF8(), audio, audio / cpu);
    return 0;
}
