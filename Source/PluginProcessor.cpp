#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Range = juce::NormalisableRange<float>;
using Fmt   = std::function<juce::String (float, int)>;

Fmt pct() { return [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }; }
Fmt db()  { return [] (float v, int) { return juce::String (v, 1) + " dB"; }; }
Fmt hz()  { return [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                            : juce::String (juce::roundToInt (v)) + " Hz"; }; }

std::unique_ptr<juce::AudioParameterFloat> fparam (const char* id, const char* name, Range r, float def, Fmt f)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)));
}

std::unique_ptr<juce::AudioParameterBool> bparam (const char* id, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> cparam (const char* id, const char* name, juce::StringArray items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HouseKeysProcessor::createLayout()
{
    const hk::Params d = hk::pianos()[0].preset;   // first piano's defaults
    juce::StringArray pianoNames;
    for (auto& p : hk::pianos()) pianoNames.add (p.name);

    Range cutoff (80.0f, 18000.0f);
    cutoff.setSkewForCentre (1500.0f);

    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (cparam ("piano", "Piano", pianoNames, d.piano));
    l.add (fparam ("tone",   "Tone",   Range (0.0f, 1.0f), d.tone,  pct()));
    l.add (fparam ("decay",  "Decay",  Range (0.0f, 1.0f), d.decay, pct()));
    l.add (fparam ("width",  "Width",  Range (0.0f, 1.0f), d.width, pct()));
    l.add (fparam ("volume", "Volume", Range (-24.0f, 6.0f), d.volumeDb, db()));
    l.add (cparam ("chord",  "Chord",  { "Off", "Min7", "Min9", "Min11", "Maj7", "Maj9", "7sus4" }, d.chord));

    l.add (bparam ("filterOn", "Filter On", d.filterOn));
    l.add (fparam ("cutoff", "Filter Cutoff", cutoff, d.cutoffHz, hz()));
    l.add (fparam ("reso",   "Filter Reso",   Range (0.0f, 1.0f), d.resonance, pct()));

    l.add (bparam ("chorusOn", "Chorus On", d.chorusOn));
    l.add (fparam ("chorusMix", "Chorus Mix", Range (0.0f, 1.0f), d.chorusMix, pct()));

    l.add (bparam ("delayOn", "Delay On", d.delayOn));
    l.add (cparam ("delayTime", "Delay Time", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, d.delayTime));
    l.add (fparam ("delayFb",  "Delay Feedback", Range (0.0f, 0.9f), d.delayFeedback, pct()));
    l.add (fparam ("delayMix", "Delay Mix",      Range (0.0f, 1.0f), d.delayMix, pct()));

    l.add (bparam ("reverbOn", "Reverb On", d.reverbOn));
    l.add (fparam ("reverbSize", "Reverb Size", Range (0.0f, 1.0f), d.reverbSize, pct()));
    l.add (fparam ("reverbMix",  "Reverb Mix",  Range (0.0f, 1.0f), d.reverbMix, pct()));

    l.add (bparam ("pumpOn", "Pump On", d.pumpOn));
    l.add (cparam ("pumpRate", "Pump Rate", { "1/4", "1/8", "1/16" }, d.pumpRate));
    l.add (fparam ("pumpDepth", "Pump Depth", Range (0.0f, 1.0f), d.pumpDepth, pct()));
    return l;
}

HouseKeysProcessor::HouseKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HouseKeys", createLayout())
{
    apvts.addParameterListener ("piano", this);
}

HouseKeysProcessor::~HouseKeysProcessor()
{
    apvts.removeParameterListener ("piano", this);
    cancelPendingUpdate();
}

// The piano parameter can change from the UI, a preset or automation (possibly on the
// audio thread). The reset of all other controls happens on the message thread.
void HouseKeysProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

void HouseKeysProcessor::handleAsyncUpdate()
{
    const int cur = (int) raw ("piano");
    if (cur != appliedPiano.load()) applyPianoPreset (cur);
}

void HouseKeysProcessor::setParam (const char* id, float value)
{
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        const float norm = prm->convertTo0to1 (value);
        if (std::abs (prm->getValue() - norm) < 1.0e-6f) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (norm);
        prm->endChangeGesture();
    }
}

float HouseKeysProcessor::pianoDefault (const juce::String& id) const
{
    const hk::Params& q = hk::pianos()[(size_t) juce::jlimit (0, hk::kNumPianos - 1, (int) raw ("piano"))].preset;
    if (id == "tone") return q.tone;            if (id == "decay") return q.decay;
    if (id == "width") return q.width;          if (id == "volume") return q.volumeDb;
    if (id == "chord") return (float) q.chord;
    if (id == "filterOn") return q.filterOn;    if (id == "cutoff") return q.cutoffHz;   if (id == "reso") return q.resonance;
    if (id == "chorusOn") return q.chorusOn;    if (id == "chorusMix") return q.chorusMix;
    if (id == "delayOn") return q.delayOn;      if (id == "delayTime") return (float) q.delayTime;
    if (id == "delayFb") return q.delayFeedback; if (id == "delayMix") return q.delayMix;
    if (id == "reverbOn") return q.reverbOn;    if (id == "reverbSize") return q.reverbSize; if (id == "reverbMix") return q.reverbMix;
    if (id == "pumpOn") return q.pumpOn;        if (id == "pumpRate") return (float) q.pumpRate; if (id == "pumpDepth") return q.pumpDepth;
    if (id == "piano") return raw ("piano");
    return 0.0f;
}

void HouseKeysProcessor::resetToPianoDefault (const juce::String& id) { setParam (id.toRawUTF8(), pianoDefault (id)); }

void HouseKeysProcessor::applyPianoPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) hk::kNumPianos)) return;
    appliedPiano = index;
    const hk::Params& q = hk::pianos()[(size_t) index].preset;
    setParam ("tone", q.tone);              setParam ("decay", q.decay);
    setParam ("width", q.width);            setParam ("volume", q.volumeDb);
    setParam ("chord", (float) q.chord);
    setParam ("filterOn", q.filterOn ? 1.0f : 0.0f);
    setParam ("cutoff", q.cutoffHz);        setParam ("reso", q.resonance);
    setParam ("chorusOn", q.chorusOn ? 1.0f : 0.0f);
    setParam ("chorusMix", q.chorusMix);
    setParam ("delayOn", q.delayOn ? 1.0f : 0.0f);
    setParam ("delayTime", (float) q.delayTime);
    setParam ("delayFb", q.delayFeedback);  setParam ("delayMix", q.delayMix);
    setParam ("reverbOn", q.reverbOn ? 1.0f : 0.0f);
    setParam ("reverbSize", q.reverbSize);  setParam ("reverbMix", q.reverbMix);
    setParam ("pumpOn", q.pumpOn ? 1.0f : 0.0f);
    setParam ("pumpRate", (float) q.pumpRate);
    setParam ("pumpDepth", q.pumpDepth);
}

bool HouseKeysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HouseKeysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.setSize (1, juce::jmax (1, samplesPerBlock));
}

hk::Params HouseKeysProcessor::readParams() const
{
    hk::Params p;
    p.piano = (int) raw ("piano");
    p.tone = raw ("tone"); p.decay = raw ("decay"); p.width = raw ("width"); p.volumeDb = raw ("volume");
    p.chord = (int) raw ("chord");
    p.filterOn = raw ("filterOn") > 0.5f; p.cutoffHz = raw ("cutoff"); p.resonance = raw ("reso");
    p.chorusOn = raw ("chorusOn") > 0.5f; p.chorusMix = raw ("chorusMix");
    p.delayOn  = raw ("delayOn") > 0.5f;  p.delayTime = (int) raw ("delayTime");
    p.delayFeedback = raw ("delayFb");    p.delayMix = raw ("delayMix");
    p.reverbOn = raw ("reverbOn") > 0.5f; p.reverbSize = raw ("reverbSize"); p.reverbMix = raw ("reverbMix");
    p.pumpOn   = raw ("pumpOn") > 0.5f;   p.pumpRate = (int) raw ("pumpRate"); p.pumpDepth = raw ("pumpDepth");
    return p;
}

void HouseKeysProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())          engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())   engine.setSustainPedal (true);
    else if (m.isSustainPedalOff())  engine.setSustainPedal (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void HouseKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    engine.setParams (readParams());

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    bpm = juce::jlimit (40.0, 300.0, bpm);

    if (scratch.getNumSamples() < numSamples) scratch.setSize (1, numSamples, false, false, true);
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : scratch.getWritePointer (0);
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;

    auto render = [&] (int from, int to)
    {
        if (to > from)
            engine.process (L + from, R + from, to - from, bpm, ppq + from * bpm / 60.0 / sr, playing);
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        render (pos, t);
        handleMidi (meta.getMessage());
        pos = t;
    }
    render (pos, numSamples);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
int HouseKeysProcessor::getCurrentProgram() { return (int) raw ("piano"); }

const juce::String HouseKeysProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) hk::kNumPianos) ? juce::String (hk::pianos()[(size_t) index].name)
                                                                  : juce::String();
}

void HouseKeysProcessor::setCurrentProgram (int index) { selectPiano (index); }

void HouseKeysProcessor::selectPiano (int index)
{
    index = (index % hk::kNumPianos + hk::kNumPianos) % hk::kNumPianos;   // arrows wrap around
    setParam ("piano", (float) index);
    applyPianoPreset (index);
}

void HouseKeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void HouseKeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            // Loading a saved project: keep the user's tweaks, don't reset to piano defaults.
            auto tree = juce::ValueTree::fromXml (*xml);
            for (auto child : tree)
                if (child.getProperty ("id").toString() == "piano")
                    appliedPiano = (int) child.getProperty ("value");
            apvts.replaceState (tree);
            cancelPendingUpdate();
        }
}

juce::AudioProcessorEditor* HouseKeysProcessor::createEditor() { return new HouseKeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HouseKeysProcessor(); }
