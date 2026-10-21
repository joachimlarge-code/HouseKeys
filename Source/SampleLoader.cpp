#include "SampleLoader.h"

#if HK_HAS_SAMPLES
 #include "BinaryData.h"
#endif

#include <atomic>
#include <mutex>

namespace
{
std::atomic<const hk::SampleBank*> readyBank { nullptr };
std::mutex statusLock;
juce::String status = "Laster pianosamples ...";

void setStatus (const juce::String& s)
{
    std::lock_guard<std::mutex> g (statusLock);
    status = s;
}

// "A4v3.ogg" -> root 69, layer 2.   "rel40.ogg" -> release for midi 60.
struct ParsedName { bool sustain = false, release = false; int midi = -1, layer = 0; };

ParsedName parseName (const juce::String& fileName)
{
    ParsedName r;
    const auto base = fileName.upToLastOccurrenceOf (".", false, false);
    const auto ext  = fileName.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (! juce::StringArray { "ogg", "wav", "flac", "aif", "aiff" }.contains (ext)) return r;

    if (base.startsWithIgnoreCase ("rel"))
    {
        const auto num = base.substring (3);
        if (num.containsOnly ("0123456789") && num.isNotEmpty())
        {
            r.release = true;
            r.midi = 20 + num.getIntValue();
        }
        return r;
    }

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int v = base.lastIndexOfChar ('v');
    if (v <= 1) return r;
    const auto notePart = base.substring (0, v), layerPart = base.substring (v + 1);
    if (layerPart.isEmpty() || ! layerPart.containsOnly ("0123456789")) return r;

    int nameLen = (notePart.length() >= 2 && notePart[1] == '#') ? 2 : 1;
    const auto letter = notePart.substring (0, nameLen).toUpperCase();
    const auto octave = notePart.substring (nameLen);
    if (octave.isEmpty() || ! octave.containsOnly ("-0123456789")) return r;
    for (int i = 0; i < 12; ++i)
        if (letter == names[i])
        {
            r.sustain = true;
            r.midi = i + (octave.getIntValue() + 1) * 12;
            r.layer = juce::jmax (0, layerPart.getIntValue() - 1);
        }
    return r;
}

bool decodeInto (juce::AudioFormatReader* reader, hk::SampleZone& z)
{
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    std::unique_ptr<juce::AudioFormatReader> owned (reader);
    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, 48000 * 30);
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);
    z.sampleRate = reader->sampleRate;
    z.length = len;
    z.data.resize ((size_t) len * 2);
    const float* l = buf.getReadPointer (0);
    const float* r = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < len; ++i)
    {
        z.data[(size_t) i * 2]     = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (l[i] * 32767.0f));
        z.data[(size_t) i * 2 + 1] = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (r[i] * 32767.0f));
    }
    return true;
}

void addZone (hk::SampleBank& bank, const ParsedName& n, juce::AudioFormatReader* reader)
{
    hk::SampleZone z;
    if (! decodeInto (reader, z)) return;
    z.root = n.midi;
    z.layer = n.layer;
    if (n.sustain) bank.zones.push_back (std::move (z));
    else if (n.release && n.midi >= 0 && n.midi < 128)
    {
        if (bank.releases.size() < 128) bank.releases.resize (128);
        bank.releases[(size_t) n.midi] = std::move (z);
    }
}

class LoaderThread : public juce::Thread
{
public:
    LoaderThread() : juce::Thread ("HouseKeys sample loader") {}
    ~LoaderThread() override { stopThread (10000); }

    void run() override
    {
        std::unique_ptr<hk::SampleBank> bank;
        const auto user = SampleLibrary::userFolder();
        if (user.isDirectory())
        {
            bank = SampleLibrary::loadFromFolder (user);
            if (bank != nullptr && ! bank->empty()) bank->name = "Egne samples";
            else bank.reset();
        }
        if (bank == nullptr && ! threadShouldExit()) bank = SampleLibrary::loadBuiltIn();
        if (threadShouldExit()) return;

        if (bank != nullptr && ! bank->empty())
        {
            setStatus (juce::String (bank->name) + "  -  " + juce::String (bank->zones.size()) + " samples, "
                       + juce::String ((int) (bank->memoryBytes() / (1024 * 1024))) + " MB");
            keep = std::move (bank);
            readyBank.store (keep.get());
        }
        else
            setStatus ("Fant ingen pianosamples - spiller modellert piano");
    }

    std::unique_ptr<hk::SampleBank> keep;
};

LoaderThread& loader()
{
    static LoaderThread t;
    return t;
}

void startOnce()
{
    static std::once_flag once;
    std::call_once (once, [] { loader().startThread (juce::Thread::Priority::low); });
}
} // namespace

namespace SampleLibrary
{
juce::File userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Application Support").getChildFile ("HouseKeys").getChildFile ("Piano");
}

const hk::SampleBank* get()
{
    startOnce();
    return readyBank.load();
}

const hk::SampleBank* getBlocking()
{
    startOnce();
    while (readyBank.load() == nullptr && loader().isThreadRunning())
        juce::Thread::sleep (20);
    return readyBank.load();
}

juce::String statusText()
{
    std::lock_guard<std::mutex> g (statusLock);
    return status;
}

std::unique_ptr<hk::SampleBank> loadFromFolder (const juce::File& folder)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto bank = std::make_unique<hk::SampleBank>();
    bank->name = folder.getFileName().toStdString();
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false))
    {
        const auto n = parseName (f.getFileName());
        if (n.sustain || n.release) addZone (*bank, n, fm.createReaderFor (f));
    }
    bank->finalise();
    return bank;
}

std::unique_ptr<hk::SampleBank> loadBuiltIn()
{
    auto bank = std::make_unique<hk::SampleBank>();
    bank->name = "Salamander Grand Piano";
#if HK_HAS_SAMPLES
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* resName = BinaryData::namedResourceList[i];
        const auto n = parseName (BinaryData::getNamedResourceOriginalFilename (resName));
        if (! (n.sustain || n.release)) continue;
        int size = 0;
        const char* data = BinaryData::getNamedResource (resName, size);
        if (data == nullptr || size <= 0) continue;
        auto stream = std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false);
        addZone (*bank, n, fm.createReaderFor (std::move (stream)));
    }
#endif
    bank->finalise();
    return bank;
}
} // namespace SampleLibrary

//==============================================================================
namespace UserSample
{
bool decode (const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Kan ikke lese denne filtypen"; return false; }
    if (reader->lengthInSamples <= 0) { error = "Filen er tom"; return false; }
    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    out.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&out, 0, len, 0, true, out.getNumChannels() > 1);
    sampleRate = reader->sampleRate;
    return true;
}

void prepare (juce::AudioBuffer<float>& audio, double sr)
{
    // stereo
    if (audio.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> st (2, audio.getNumSamples());
        st.copyFrom (0, 0, audio, 0, 0, audio.getNumSamples());
        st.copyFrom (1, 0, audio, 0, 0, audio.getNumSamples());
        audio = std::move (st);
    }
    const int n = audio.getNumSamples();
    const float peak = juce::jmax (audio.getMagnitude (0, 0, n), 1.0e-6f);

    // remove silence before the sound starts (keep 1 ms)
    int start = 0;
    const float thr = peak * 0.01f;
    while (start < n && std::abs (audio.getSample (0, start)) < thr && std::abs (audio.getSample (1, start)) < thr) ++start;
    start = juce::jmax (0, start - (int) (0.001 * sr));

    // remove silence at the end, max 20 s
    int end = n;
    const float endThr = peak * 0.0005f;
    while (end > start + 1 && std::abs (audio.getSample (0, end - 1)) < endThr && std::abs (audio.getSample (1, end - 1)) < endThr) --end;
    end = juce::jmin (end, start + (int) (20.0 * sr));

    const int len = juce::jmax (1, end - start);
    juce::AudioBuffer<float> out (2, len);
    for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, audio, c, start, len);

    // short fades so it never clicks
    const int fin = juce::jmin (len / 4, (int) (0.0015 * sr)), fout = juce::jmin (len / 4, (int) (0.02 * sr));
    out.applyGainRamp (0, fin, 0.0f, 1.0f);
    out.applyGainRamp (len - fout, fout, 1.0f, 0.0f);

    // consistent level: RMS of the first 300 ms to about -16 dBFS, peak never above -1 dBFS
    const int m = juce::jmin (len, (int) (0.3 * sr));
    double sum = 0.0;
    for (int c = 0; c < 2; ++c) { const float* p = out.getReadPointer (c); for (int i = 0; i < m; ++i) sum += (double) p[i] * p[i]; }
    const float rms = (float) std::sqrt (sum / (2.0 * juce::jmax (1, m)));
    float g = rms > 1.0e-6f ? 0.158f / rms : 1.0f;
    g = juce::jmin (g, 0.89f / juce::jmax (out.getMagnitude (0, 0, len), 1.0e-6f));
    out.applyGain (g);
    audio = std::move (out);
}

int detectRootNote (const juce::AudioBuffer<float>& audio, double sr)
{
    // analyse ~100 ms of the steady part (skip the attack)
    const int skip = (int) (0.04 * sr), W = juce::jmin (4096, (int) (0.1 * sr));
    if (audio.getNumSamples() < skip + 2 * W) return -1;
    std::vector<float> x ((size_t) (2 * W));
    for (int i = 0; i < 2 * W; ++i)
        x[(size_t) i] = 0.5f * (audio.getSample (0, skip + i) + audio.getSample (audio.getNumChannels() > 1 ? 1 : 0, skip + i));

    const int tauMin = (int) (sr / 2000.0), tauMax = juce::jmin (W - 1, (int) (sr / 30.0));
    std::vector<float> d ((size_t) tauMax + 1, 0.0f), cmnd ((size_t) tauMax + 1, 1.0f);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double s = 0.0;
        for (int i = 0; i < W; ++i) { const float diff = x[(size_t) i] - x[(size_t) (i + tau)]; s += (double) diff * diff; }
        d[(size_t) tau] = (float) s;
    }
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += d[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (cmnd[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0) return -1;   // noise / drums: no clear pitch
    // parabolic interpolation
    float t = (float) best;
    if (best > 1 && best < tauMax)
    {
        const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        if (std::abs (den) > 1.0e-9f) t += 0.5f * (a - c) / den;
    }
    const double f0 = sr / t;
    return juce::jlimit (12, 120, (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0)));
}

std::unique_ptr<hk::SampleBank> makeBank (const juce::AudioBuffer<float>& audio, double sr, int rootNote)
{
    auto bank = std::make_unique<hk::SampleBank>();
    bank->name = "Ditt sample";
    hk::SampleZone z;
    z.root = rootNote; z.layer = 0; z.sampleRate = sr; z.length = audio.getNumSamples();
    z.data.resize ((size_t) z.length * 2);
    const float* l = audio.getReadPointer (0);
    const float* r = audio.getReadPointer (audio.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < z.length; ++i)
    {
        z.data[(size_t) i * 2]     = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (l[i] * 32767.0f));
        z.data[(size_t) i * 2 + 1] = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (r[i] * 32767.0f));
    }
    bank->zones.push_back (std::move (z));
    bank->finalise();
    return bank;
}

juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& audio, double sr)
{
    juce::MemoryBlock mb;
    juce::FlacAudioFormat flac;
    auto* os = new juce::MemoryOutputStream (mb, false);
    std::unique_ptr<juce::AudioFormatWriter> w (flac.createWriterFor (os, sr, (unsigned int) audio.getNumChannels(), 16, {}, 5));
    if (w == nullptr) { delete os; return {}; }
    w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    w.reset();   // flushes and deletes the stream
    return mb;
}

bool decodeFlac (const juce::MemoryBlock& data, juce::AudioBuffer<float>& out, double& sr)
{
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, false), true));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    out.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&out, 0, out.getNumSamples(), 0, true, true);
    sr = reader->sampleRate;
    return true;
}

juce::String noteName (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}
} // namespace UserSample
