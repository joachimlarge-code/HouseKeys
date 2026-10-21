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
