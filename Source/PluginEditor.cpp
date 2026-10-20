#include "PluginEditor.h"

using namespace hkui;

namespace
{
juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

void styleCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (uiFont (11.5f, true));
    l.setColour (juce::Label::textColourId, col::dim);
    l.setInterceptsMouseClicks (false, false);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour titleColour)
{
    auto r = area.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (titleColour);
        g.setFont (uiFont (12.5f, true));
        g.drawText (title.toUpperCase(), area.getX() + 16, area.getY() + 10, 300, 18, juce::Justification::centredLeft);
    }
}

juce::Colour pianoColour (int piano)
{
    return hk::pianos()[(size_t) juce::jlimit (0, hk::kNumPianos - 1, piano)].energetic ? col::hot2 : col::calm;
}

constexpr int kW = 980, kH = 540;
} // namespace

//==============================================================================
void hkui::showParameterMenu (juce::Component& c, const juce::String& paramID)
{
    auto* ed = c.findParentComponentOfClass<HouseKeysEditor>();
    if (ed == nullptr || paramID.isEmpty()) return;
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (ed->getProcessor().apvts.getParameter (paramID));
    if (param == nullptr) return;

    // 1) the host's own menu (FL Studio: "Create automation clip", "Link to controller", ...)
    if (auto* ctx = ed->getHostContext())
        if (auto menu = ctx->getContextMenuForParameter (param))
        {
            menu->showNativeMenu (ed->getMouseXYRelative());
            return;
        }

    // 2) fallback when the host doesn't offer one
    juce::PopupMenu m;
    m.setLookAndFeel (&ed->getLookAndFeel());
    m.addSectionHeader (param->getName (64));
    m.addItem (1, utf8 ("Tilbakestill til pianoets standard"));
    m.addSeparator();
    m.addItem (2, utf8 ("Automation: vri på kontrollen, og velg i FL"), false, false);
    m.addItem (3, utf8 ("Tools > Last tweaked > Create automation clip"), false, false);
    juce::Component::SafePointer<HouseKeysEditor> safe (ed);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c),
                     [safe, paramID] (int r) { if (r == 1 && safe != nullptr) safe->getProcessor().resetToPianoDefault (paramID); });
}

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
    setColour (juce::PopupMenu::headerTextColourId, col::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float lw = juce::jmax (3.0f, r * 0.12f);
    const float arcR = r - lw * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (accent.withRotatedHue (-0.05f), c.x - r, c.y + r,
                                                 accent, c.x + r, c.y - r, false));
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float br = r - lw * 2.1f;
    const bool hover = s.isMouseOverOrDragging();
    g.setGradientFill (juce::ColourGradient (col::knob.brighter (hover ? 0.4f : 0.25f), c.x, c.y - br,
                                             col::knob.darker (0.5f), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (col::border.brighter (0.15f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    const juce::Point<float> tip   (c.x + (br - 5.0f) * std::sin (angle), c.y - (br - 5.0f) * std::cos (angle));
    const juce::Point<float> inner (c.x + br * 0.3f * std::sin (angle), c.y - br * 0.3f * std::cos (angle));
    g.setColour (col::text);
    g.drawLine ({ inner, tip }, 2.5f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? col::knob.brighter (0.15f) : col::knob);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::border.brighter (0.2f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? onCol : col::knob.brighter (highlighted ? 0.25f : 0.1f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? onCol.brighter (0.2f) : col::border.brighter (0.3f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    const float d = r.getHeight() - 6.0f;
    const float dx = on ? r.getRight() - d - 3.0f : r.getX() + 3.0f;
    g.setColour (on ? col::bg : col::dim);
    g.fillEllipse (dx, r.getY() + 3.0f, d, d);

    g.setFont (uiFont (10.5f, true));
    auto txt = on ? r.withTrimmedRight (d + 4.0f) : r.withTrimmedLeft (d + 4.0f);
    g.drawText (on ? "ON" : "OFF", txt, juce::Justification::centred);
}

juce::Font Look::getComboBoxFont (juce::ComboBox&) { return uiFont (14.0f, true); }
juce::Font Look::getPopupMenuFont()                { return uiFont (15.0f); }

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
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
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title, int maxSz)
    : maxSize (maxSz)
{
    slider.paramID = id;
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    styleCaption (label, title);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), maxSize), juce::jmin (r.getHeight(), maxSize + 20)));
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title)
{
    box.paramID = id;
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
    auto inner = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 100), 52);
    label.setBounds (inner.removeFromTop (18));
    inner.removeFromTop (4);
    box.setBounds (inner.removeFromTop (30));
}

//==============================================================================
PianoSelector::PianoSelector (HouseKeysProcessor& p) : proc (p)
{
    name.paramID = "piano";
    addAndMakeVisible (prev);
    addAndMakeVisible (next);
    addAndMakeVisible (name);
    prev.onClick = [this] { proc.selectPiano (proc.getCurrentProgram() - 1); refresh(); };
    next.onClick = [this] { proc.selectPiano (proc.getCurrentProgram() + 1); refresh(); };
    name.onClick = [this] { showMenu(); };
    for (juce::Component* c : { (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &name })
        c->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    prev.setTooltip ("Forrige piano");
    next.setTooltip ("Neste piano");
    refresh();
}

void PianoSelector::refresh()
{
    const int cur = proc.getCurrentProgram();
    if (name.piano != cur) { name.piano = cur; name.repaint(); }
}

void PianoSelector::showMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    const int cur = proc.getCurrentProgram();
    for (bool energetic : { true, false })
    {
        m.addSectionHeader (energetic ? "ENERGISK" : "ROLIG");
        for (int i = 0; i < hk::kNumPianos; ++i)
        {
            const auto& d = hk::pianos()[(size_t) i];
            if (d.energetic != energetic) continue;
            juce::PopupMenu::Item item (utf8 (d.name) + "   -   " + utf8 (d.tag));
            item.setID (i + 1).setTicked (i == cur).setColour (i == cur ? pianoColour (i) : col::text);
            m.addItem (item);
        }
    }
    juce::Component::SafePointer<PianoSelector> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name).withMinimumWidth (name.getWidth()),
                     [safe] (int r)
                     {
                         if (r > 0 && safe != nullptr) { safe->proc.selectPiano (r - 1); safe->refresh(); }
                     });
}

void PianoSelector::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (44));
    next.setBounds (r.removeFromRight (44));
    name.setBounds (r.reduced (8, 0));
}

void PianoSelector::Arrow::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::panel2.brighter (highlighted ? 0.18f : 0.0f).darker (down ? 0.15f : 0.0f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col::border.brighter (0.2f));
    g.drawRoundedRectangle (r, 10.0f, 1.0f);
    const auto c = r.getCentre();
    juce::Path tri;
    if (isLeft) tri.addTriangle (c.x + 5.0f, c.y - 8.0f, c.x + 5.0f, c.y + 8.0f, c.x - 6.0f, c.y);
    else        tri.addTriangle (c.x - 5.0f, c.y - 8.0f, c.x - 5.0f, c.y + 8.0f, c.x + 6.0f, c.y);
    g.setColour (highlighted ? col::text : col::accent);
    g.fillPath (tri);
}

void PianoSelector::NameBox::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& d = hk::pianos()[(size_t) juce::jlimit (0, hk::kNumPianos - 1, piano)];
    const auto c = pianoColour (piano);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::panel2.brighter (highlighted ? 0.12f : 0.0f).darker (down ? 0.1f : 0.0f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (c.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 10.0f, 1.5f);
    g.fillRoundedRectangle (r.getX() + 8.0f, r.getY() + 10.0f, 4.0f, r.getHeight() - 20.0f, 2.0f);

    auto t = r.reduced (22.0f, 6.0f).withTrimmedRight (20.0f);
    auto top = t.removeFromTop (t.getHeight() * 0.42f);
    g.setFont (uiFont (11.0f, true));
    g.setColour (c);
    g.drawText (juce::String (d.energetic ? "ENERGISK" : "ROLIG") + "   " + utf8 (d.tag).toUpperCase(),
                top, juce::Justification::bottomLeft);
    g.setFont (uiFont (21.0f, true));
    g.setColour (col::text);
    g.drawFittedText (utf8 (d.name), t.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    const float ax = r.getRight() - 20.0f, ay = r.getCentreY();
    juce::Path arrow;
    arrow.addTriangle (ax - 6.0f, ay - 3.0f, ax + 6.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

//==============================================================================
FxCard::FxCard (juce::AudioProcessorValueTreeState& s, const juce::String& t, const juce::String& onId) : title (t)
{
    power.paramID = onId;
    power.setColour (juce::ToggleButton::tickColourId, col::accent);
    addAndMakeVisible (power);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, onId, power);
    power.onStateChange = [this] { refresh(); };
}

void FxCard::add (std::unique_ptr<juce::Component> c)
{
    addAndMakeVisible (*c);
    items.push_back (std::move (c));
}

void FxCard::refresh()
{
    const bool on = power.getToggleState();
    if ((int) on == lastOn) return;
    lastOn = on;
    for (auto& c : items) c->setAlpha (on ? 1.0f : 0.38f);
    repaint();
}

void FxCard::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), title, power.getToggleState() ? col::accent : col::dim);
}

void FxCard::resized()
{
    auto r = getLocalBounds();
    power.setBounds (r.getRight() - 62, 9, 50, 22);
    auto inner = r.reduced (10, 0).withTrimmedTop (40).withTrimmedBottom (10);
    const int n = juce::jmax (1, (int) items.size());
    const int w = inner.getWidth() / n;
    for (int i = 0; i < (int) items.size(); ++i)
        items[(size_t) i]->setBounds (inner.getX() + i * w, inner.getY(), w, inner.getHeight());
}

//==============================================================================
HouseKeysEditor::HouseKeysEditor (HouseKeysProcessor& p)
    : AudioProcessorEditor (p), proc (p), selector (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    addAndMakeVisible (selector);

    for (auto [id, title] : { std::pair { "tone", "Tone" }, std::pair { "decay", "Decay" },
                              std::pair { "width", "Width" }, std::pair { "volume", "Volume" } })
    {
        mainKnobs.push_back (std::make_unique<Knob> (s, id, title, 104));
        addAndMakeVisible (*mainKnobs.back());
    }
    chord = std::make_unique<Choice> (s, "chord", "Chord");
    addAndMakeVisible (*chord);

    auto knob   = [&s] (const char* id, const char* t) { return std::make_unique<Knob> (s, id, t); };
    auto choice = [&s] (const char* id, const char* t) { return std::make_unique<Choice> (s, id, t); };

    auto filter = std::make_unique<FxCard> (s, "Filter", "filterOn");
    filter->add (knob ("cutoff", "Cutoff"));
    filter->add (knob ("reso", "Reso"));
    auto chorus = std::make_unique<FxCard> (s, "Chorus", "chorusOn");
    chorus->add (knob ("chorusMix", "Mix"));
    auto delay = std::make_unique<FxCard> (s, "Delay", "delayOn");
    delay->add (choice ("delayTime", "Time"));
    delay->add (knob ("delayFb", "Feedback"));
    delay->add (knob ("delayMix", "Mix"));
    auto reverb = std::make_unique<FxCard> (s, "Reverb", "reverbOn");
    reverb->add (knob ("reverbSize", "Size"));
    reverb->add (knob ("reverbMix", "Mix"));
    auto pump = std::make_unique<FxCard> (s, "Pump", "pumpOn");
    pump->add (choice ("pumpRate", "Rate"));
    pump->add (knob ("pumpDepth", "Depth"));

    for (auto* c : { &filter, &chorus, &delay, &reverb, &pump })
    {
        cards.push_back (std::move (*c));
        addAndMakeVisible (*cards.back());
        cards.back()->refresh();
    }

    setSize (kW, kH);
    timerCallback();
    startTimerHz (15);
}

HouseKeysEditor::~HouseKeysEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HouseKeysEditor::timerCallback()
{
    selector.refresh();
    const int sel = proc.getCurrentProgram();
    if (sel != lastPiano)
    {
        lastPiano = sel;
        const auto c = pianoColour (sel);
        for (auto& k : mainKnobs) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, c); k->slider.repaint(); }
        repaint();
    }
    for (auto& c : cards) c->refresh();
}

//==============================================================================
void HouseKeysEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (col::bg2, 0, 0, col::bg, 0, (float) getHeight(), false));
    g.fillAll();

    // logo
    const float lx = 22.0f, ly = 20.0f;
    for (int i = 0; i < 4; ++i) { g.setColour (col::text); g.fillRoundedRectangle (lx + i * 9.0f, ly, 8.0f, 30.0f, 1.5f); }
    g.setColour (col::hot);  g.fillRoundedRectangle (lx + 5.5f, ly, 6.0f, 18.0f, 1.2f);
    g.setColour (col::hot2); g.fillRoundedRectangle (lx + 14.5f, ly, 6.0f, 18.0f, 1.2f);
    g.setColour (col::calm); g.fillRoundedRectangle (lx + 23.5f, ly, 6.0f, 18.0f, 1.2f);

    g.setColour (col::text);
    g.setFont (uiFont (30.0f, true));
    g.drawText ("HOUSEKEYS", 68, 12, 220, 34, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.fillRoundedRectangle (262.0f, 18.0f, 26.0f, 22.0f, 6.0f);
    g.setColour (col::bg);
    g.setFont (uiFont (15.0f, true));
    g.drawText ("2", 262, 18, 26, 22, juce::Justification::centred);
    g.setColour (col::dim);
    g.setFont (uiFont (12.0f, true));
    g.drawText ("HOUSE & CHILL PIANOS", 70, 44, 300, 14, juce::Justification::centredLeft);

    drawPanel (g, soundArea, "Sound", pianoColour (proc.getCurrentProgram()));
}

void HouseKeysEditor::resized()
{
    const int margin = 20, gap = 12;

    // piano selector, top right
    selector.setBounds (getWidth() - margin - 470, 12, 470, 52);

    // main sound knobs
    soundArea = { margin, 78, getWidth() - margin * 2, 180 };
    {
        auto inner = soundArea.reduced (20, 0).withTrimmedTop (32).withTrimmedBottom (10);
        const int w = inner.getWidth() / 5;
        for (int i = 0; i < 4; ++i)
            mainKnobs[(size_t) i]->setBounds (inner.getX() + i * w, inner.getY(), w, inner.getHeight());
        chord->setBounds (inner.getX() + 4 * w, inner.getY(), w, inner.getHeight());
    }

    // effect cards
    {
        auto row = juce::Rectangle<int> (margin, soundArea.getBottom() + gap, getWidth() - margin * 2,
                                         getHeight() - soundArea.getBottom() - gap - margin);
        int units = 0;
        for (auto& c : cards) units += c->units();
        const int pad = 20;
        const float unitW = (float) (row.getWidth() - (int) (cards.size() - 1) * gap - (int) cards.size() * pad) / (float) units;
        float x = (float) row.getX();
        for (size_t i = 0; i < cards.size(); ++i)
        {
            const int w = (i + 1 == cards.size()) ? row.getRight() - juce::roundToInt (x)
                                                  : juce::roundToInt (unitW * (float) cards[i]->units()) + pad;
            cards[i]->setBounds (juce::roundToInt (x), row.getY(), w, row.getHeight());
            x += (float) (w + gap);
        }
    }
}
