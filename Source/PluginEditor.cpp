#include "PluginEditor.h"

using namespace hkui;

namespace
{
juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

void styleCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (uiFont (11.5f, true));
    l.setColour (juce::Label::textColourId, col::dim);
    l.setInterceptsMouseClicks (false, false);
}

constexpr int kW = 920, kH = 486;
} // namespace

//==============================================================================
Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.35f));
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Label::textWhenEditingColourId, col::text);
    setColour (juce::Label::outlineWhenEditingColourId, col::accent);
    setColour (juce::TextEditor::backgroundColourId, col::bg2);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float lw = juce::jmax (3.0f, r * 0.11f);
    const float arcR = r - lw * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        juce::ColourGradient grad (col::accent2, c.x - r, c.y + r, col::accent, c.x + r, c.y - r, false);
        g.setGradientFill (grad);
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float br = r - lw * 2.1f;
    const bool hover = s.isMouseOverOrDragging();
    g.setGradientFill (juce::ColourGradient (col::knob.brighter (hover ? 0.35f : 0.22f), c.x, c.y - br,
                                             col::knob.darker (0.45f), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (col::border.brighter (0.1f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    const juce::Point<float> tip   (c.x + (br - 5.0f) * std::sin (angle), c.y - (br - 5.0f) * std::cos (angle));
    const juce::Point<float> inner (c.x + br * 0.25f * std::sin (angle), c.y - br * 0.25f * std::cos (angle));
    g.setColour (col::text);
    g.drawLine ({ inner, tip }, 2.4f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? col::knob.brighter (0.15f) : col::knob);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::border.brighter (0.15f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

juce::Font Look::getComboBoxFont (juce::ComboBox&) { return uiFont (14.0f, true); }
juce::Font Look::getPopupMenuFont()                { return uiFont (14.0f); }

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Label* Look::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.5f));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setDoubleClickReturnValue (true, s.getParameterRange (id).convertFrom0to1 (
        s.getParameter (id)->getDefaultValue()));
    styleCaption (label, title);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 86), r.getHeight()));
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    styleCaption (label, title);
    addAndMakeVisible (box);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);
    box.setBounds (r.removeFromTop (30));
}

//==============================================================================
HouseKeysEditor::HouseKeysEditor (HouseKeysProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    // preset browser
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    styleCaption (presetLabel, "Preset");
    addAndMakeVisible (presetBox);
    addAndMakeVisible (presetLabel);

    model    = std::make_unique<Choice> (s, "model", "Model");
    chord    = std::make_unique<Choice> (s, "chord", "Chord");
    pumpRate = std::make_unique<Choice> (s, "pumprate", "Rate");
    addAndMakeVisible (*model);
    addAndMakeVisible (*chord);
    addAndMakeVisible (*pumpRate);

    const std::pair<const char*, const char*> defs[] = {
        { "bright", "Bright" }, { "layer", "Layer" }, { "detune", "Detune" }, { "tape", "Tape" },
        { "attack", "Attack" }, { "decay", "Decay" }, { "release", "Release" }, { "vel", "Velocity" },
        { "cutoff", "Cutoff" }, { "reso", "Reso" },
        { "chorus", "Chorus" }, { "reverb", "Reverb" }, { "size", "Size" }, { "comp", "Comp" },
        { "pump", "Depth" }, { "volume", "Volume" },
    };
    for (auto& [id, title] : defs)
    {
        knobs.push_back (std::make_unique<Knob> (s, id, title));
        knobs.back()->setName (id);
        addAndMakeVisible (*knobs.back());
    }

    setSize (kW, kH);
    startTimerHz (8);
}

HouseKeysEditor::~HouseKeysEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob* HouseKeysEditor::knob (const juce::String& id)
{
    for (auto& k : knobs) if (k->getName() == id) return k.get();
    jassertfalse;
    return nullptr;
}

void HouseKeysEditor::timerCallback()
{
    const int id = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != id) presetBox.setSelectedId (id, juce::dontSendNotification);
}

//==============================================================================
void HouseKeysEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (col::bg2, 0, 0, col::bg, 0, (float) getHeight(), false));
    g.fillAll();

    // header: little keyboard logo + name
    const float lx = 22.0f, ly = 20.0f;
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (col::text);
        g.fillRoundedRectangle (lx + i * 9.0f, ly, 8.0f, 30.0f, 1.5f);
    }
    g.setColour (col::accent);
    for (int i : { 0, 1 })
        g.fillRoundedRectangle (lx + 5.5f + i * 9.0f, ly, 6.0f, 18.0f, 1.2f);
    g.setColour (col::accent2);
    g.fillRoundedRectangle (lx + 5.5f + 2 * 9.0f, ly, 6.0f, 18.0f, 1.2f);

    g.setColour (col::text);
    g.setFont (uiFont (30.0f, true));
    g.drawText ("HOUSEKEYS", 68, 12, 260, 34, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.setFont (uiFont (11.5f, true));
    g.drawText ("FM HOUSE PIANO", 70, 44, 260, 14, juce::Justification::centredLeft);

    // section panels
    for (auto& sec : sections)
    {
        auto r = sec.area.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (col::accent);
        g.setFont (uiFont (11.5f, true));
        g.drawText (sec.title.toUpperCase(), sec.area.getX() + 14, sec.area.getY() + 8, 200, 16,
                    juce::Justification::centredLeft);
    }
}

void HouseKeysEditor::resized()
{
    sections.clear();
    const int margin = 20, gap = 12, pad = 12;

    // header controls (right side)
    {
        auto hdr = juce::Rectangle<int> (0, 10, getWidth() - margin, 56);
        auto chordR = hdr.removeFromRight (120);
        hdr.removeFromRight (gap);
        auto modelR = hdr.removeFromRight (150);
        hdr.removeFromRight (gap);
        auto presetR = hdr.removeFromRight (220);
        presetLabel.setBounds (presetR.removeFromTop (18));
        presetR.removeFromTop (4);
        presetBox.setBounds (presetR.removeFromTop (30));
        model->setBounds (modelR);
        chord->setBounds (chordR);
    }

    // rows of panels. Each entry: title + list of control ids ("#rate" = pump rate selector)
    using Panel = std::pair<juce::String, std::vector<juce::String>>;
    auto layoutRow = [&] (juce::Rectangle<int> row, const std::vector<Panel>& panels)
    {
        int cells = 0;
        for (auto& p : panels) cells += (int) p.second.size();
        const int n = (int) panels.size();
        const float cellW = (float) (row.getWidth() - n * pad * 2 - (n - 1) * gap) / (float) cells;
        float x = (float) row.getX();
        for (auto& [title, ids] : panels)
        {
            const int w = juce::roundToInt (cellW * (float) ids.size()) + pad * 2;
            juce::Rectangle<int> area (juce::roundToInt (x), row.getY(), w, row.getHeight());
            sections.push_back ({ title, area });
            auto inner = area.reduced (pad, 0).withTrimmedTop (30).withTrimmedBottom (10);
            for (size_t i = 0; i < ids.size(); ++i)
            {
                auto cell = inner.withX (inner.getX() + juce::roundToInt (cellW * (float) i))
                                 .withWidth (juce::roundToInt (cellW));
                if (ids[i] == "#rate")
                    pumpRate->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 80), 52)
                                             .translated (0, -8));
                else if (auto* k = knob (ids[i]))
                    k->setBounds (cell);
            }
            x += (float) w + (float) gap;
        }
    };

    const int rowH = 190;
    auto row1 = juce::Rectangle<int> (margin, 80, getWidth() - margin * 2, rowH);
    auto row2 = row1.translated (0, rowH + gap);

    layoutRow (row1, { { "Sound",    { "bright", "layer", "detune", "tape" } },
                       { "Envelope", { "attack", "decay", "release", "vel" } } });
    layoutRow (row2, { { "Filter",  { "cutoff", "reso" } },
                       { "Effects", { "chorus", "reverb", "size", "comp" } },
                       { "Pump",    { "pump", "#rate" } },
                       { "Output",  { "volume" } } });
}
