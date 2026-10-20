#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
using Range = juce::NormalisableRange<float>;
using Fmt   = std::function<juce::String (float, int)>;

Fmt pct()   { return [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }; }
Fmt ms()    { return [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }; }
Fmt secs()  { return [] (float v, int) { return juce::String (v, 2) + " s"; }; }
Fmt cents() { return [] (float v, int) { return juce::String (v, 1) + " ct"; }; }
Fmt db()    { return [] (float v, int) { return juce::String (v, 1) + " dB"; }; }
Fmt hz()    { return [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                              : juce::String (juce::roundToInt (v)) + " Hz"; }; }

std::unique_ptr<juce::AudioParameterFloat> fparam (const char* id, const char* name, Range r, float def, Fmt f)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)));
}

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HouseKeysProcessor::createLayout()
{
    const hk::Params d = hk::factoryPresets().front().p;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "model", 1 }, "Model",
           juce::StringArray { "House Piano", "Deep Keys", "Organ", "Stab" }, d.model));
    l.add (fparam ("bright", "Brightness", Range (0.0f, 1.0f), d.brightness, pct()));
    l.add (fparam ("layer",  "Layer",      Range (0.0f, 1.0f), d.layer,      pct()));
    l.add (fparam ("detune", "Detune",     Range (0.0f, 30.0f), d.detune,    cents()));
    l.add (fparam ("tape",   "Tape",       Range (0.0f, 1.0f), d.tape,       pct()));

    l.add (fparam ("attack",  "Attack",   skewed (0.0f, 300.0f, 30.0f), d.attackMs, ms()));
    l.add (fparam ("decay",   "Decay",    skewed (0.1f, 10.0f, 2.0f),   d.decayS,   secs()));
    l.add (fparam ("release", "Release",  skewed (0.02f, 5.0f, 0.5f),   d.releaseS, secs()));
    l.add (fparam ("vel",     "Velocity", Range (0.0f, 1.0f),           d.velSens,  pct()));

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "chord", 1 }, "Chord",
           juce::StringArray { "Off", "Min7", "Min9", "Min11", "Maj7", "Maj9", "7sus4" }, d.chord));

    l.add (fparam ("cutoff", "Cutoff",    skewed (80.0f, 20000.0f, 1500.0f), d.cutoffHz,  hz()));
    l.add (fparam ("reso",   "Resonance", Range (0.0f, 1.0f),                d.resonance, pct()));
    l.add (fparam ("chorus", "Chorus",    Range (0.0f, 1.0f),                d.chorus,    pct()));
    l.add (fparam ("reverb", "Reverb",    Range (0.0f, 1.0f),                d.reverb,    pct()));
    l.add (fparam ("size",   "Room Size", Range (0.0f, 1.0f),                d.roomSize,  pct()));
    l.add (fparam ("comp",   "Compressor", Range (0.0f, 1.0f),               d.comp,      pct()));
    l.add (fparam ("pump",   "Pump",      Range (0.0f, 1.0f),                d.pump,      pct()));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pumprate", 1 }, "Pump Rate",
           juce::StringArray { "1/4", "1/8", "1/16" }, d.pumpRate));
    l.add (fparam ("volume", "Volume",    Range (-24.0f, 6.0f),              d.volumeDb,  db()));
    return l;
}

HouseKeysProcessor::HouseKeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HouseKeys", createLayout())
{
    auto r = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw = { r ("model"), r ("bright"), r ("layer"), r ("detune"), r ("tape"), r ("attack"), r ("decay"),
            r ("release"), r ("vel"), r ("chord"), r ("cutoff"), r ("reso"), r ("chorus"), r ("reverb"),
            r ("size"), r ("comp"), r ("pump"), r ("pumprate"), r ("volume") };
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
    p.model      = (int) raw.model->load();
    p.brightness = raw.bright->load();
    p.layer      = raw.layer->load();
    p.detune     = raw.detune->load();
    p.tape       = raw.tape->load();
    p.attackMs   = raw.attack->load();
    p.decayS     = raw.decay->load();
    p.releaseS   = raw.release->load();
    p.velSens    = raw.vel->load();
    p.chord      = (int) raw.chord->load();
    p.cutoffHz   = raw.cutoff->load();
    p.resonance  = raw.reso->load();
    p.chorus     = raw.chorus->load();
    p.reverb     = raw.reverb->load();
    p.roomSize   = raw.size->load();
    p.comp       = raw.comp->load();
    p.pump       = raw.pump->load();
    p.pumpRate   = (int) raw.pumpRate->load();
    p.volumeDb   = raw.volume->load();
    return p;
}

void HouseKeysProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                     engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())               engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())        engine.setSustainPedal (true);
    else if (m.isSustainPedalOff())       engine.setSustainPedal (false);
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
    bpm = juce::jlimit (20.0, 400.0, bpm);

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

    if (buffer.getNumChannels() == 1)  // mono output: fold the right channel in
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
int HouseKeysProcessor::getNumPrograms() { return (int) hk::factoryPresets().size(); }

const juce::String HouseKeysProcessor::getProgramName (int index)
{
    const auto& ps = hk::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? juce::String (ps[(size_t) index].name) : juce::String();
}

void HouseKeysProcessor::setCurrentProgram (int index)
{
    const auto& ps = hk::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;
    const hk::Params& p = ps[(size_t) index].p;

    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set ("model", (float) p.model);   set ("bright", p.brightness); set ("layer", p.layer);
    set ("detune", p.detune);         set ("tape", p.tape);        set ("attack", p.attackMs);
    set ("decay", p.decayS);          set ("release", p.releaseS); set ("vel", p.velSens);
    set ("chord", (float) p.chord);   set ("cutoff", p.cutoffHz);  set ("reso", p.resonance);
    set ("chorus", p.chorus);         set ("reverb", p.reverb);    set ("size", p.roomSize);
    set ("comp", p.comp);             set ("pump", p.pump);        set ("pumprate", (float) p.pumpRate);
    set ("volume", p.volumeDb);
}

//==============================================================================
void HouseKeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void HouseKeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* HouseKeysProcessor::createEditor() { return new HouseKeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HouseKeysProcessor(); }
