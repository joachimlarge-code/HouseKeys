#include "PluginEditor.h"
#include "SampleLoader.h"
#include "PresetAI.h"

using namespace hkui;

namespace
{
juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

const hk::SoundDef& soundDef (int i) { return hk::sounds()[(size_t) juce::jlimit (0, hk::kNumSounds - 1, i)]; }

void styleCaption (juce::Label& l, const juce::String& text, juce::Colour c = col::dim)
{
    l.setText (text.toUpperCase(), juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (uiFont (11.5f, true));
    l.setColour (juce::Label::textColourId, c);
    l.setInterceptsMouseClicks (false, false);
}

// glassy dark panel with an optional neon top line
void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour titleColour, bool glow)
{
    auto r = area.toFloat();
    g.setGradientFill (juce::ColourGradient (col::panel2.withAlpha (0.92f), r.getX(), r.getY(),
                                             juce::Colour (0xf0131019), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    if (glow)
    {
        auto line = r.reduced (18.0f, 0.0f).removeFromTop (2.0f);
        g.setGradientFill (juce::ColourGradient (titleColour.withAlpha (0.0f), line.getX(), 0,
                                                 titleColour.withAlpha (0.0f), line.getRight(), 0, false));
        juce::ColourGradient cg (titleColour.withAlpha (0.0f), line.getX(), 0, titleColour.withAlpha (0.0f), line.getRight(), 0, false);
        cg.addColour (0.5, titleColour);
        g.setGradientFill (cg);
        g.fillRect (line);
        cg = juce::ColourGradient (titleColour.withAlpha (0.0f), line.getX(), 0, titleColour.withAlpha (0.0f), line.getRight(), 0, false);
        cg.addColour (0.5, titleColour.withAlpha (0.18f));
        g.setGradientFill (cg);
        g.fillRect (line.withHeight (14.0f));
    }
    if (title.isNotEmpty())
    {
        g.setColour (titleColour);
        g.setFont (uiFont (12.5f, true));
        g.drawText (title.toUpperCase(), area.getX() + 18, area.getY() + 12, 300, 18, juce::Justification::centredLeft);
    }
}

constexpr int kW = 1220, kH = 660;
} // namespace

//==============================================================================
void hkui::showParameterMenu (juce::Component& c, const juce::String& paramID)
{
    auto* ed = c.findParentComponentOfClass<HouseKeysEditor>();
    if (ed == nullptr || paramID.isEmpty()) return;
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (ed->getProcessor().apvts.getParameter (paramID));
    if (param == nullptr) return;

    // 1) "touch" the parameter: the host now sees it as the last tweaked control, so in FL Studio
    //    Tools > Last tweaked > Create automation clip points straight at it.
    {
        const float v = param->getValue();
        const float nudge = v < 0.5f ? 1.0e-4f : -1.0e-4f;
        param->beginChangeGesture();
        param->setValueNotifyingHost (v + nudge);
        param->setValueNotifyingHost (v);
        param->endChangeGesture();
    }

    // 2) the host's own menu, when it offers one ("Create automation clip", "Link to controller", ...)
    if (auto* ctx = ed->getHostContext())
        if (auto menu = ctx->getContextMenuForParameter (param))
            if (menu->getEquivalentPopupMenu().getNumItems() > 0)
            {
                menu->showNativeMenu (ed->getMouseXYRelative());
                return;
            }

    // 3) our own menu
    juce::PopupMenu m;
    m.setLookAndFeel (&ed->getLookAndFeel());
    m.addSectionHeader (param->getName (64).toUpperCase() + utf8 ("  \xe2\x80\x93  klar for automation"));
    m.addItem (2, utf8 ("Lag automation clip i FL:  Tools  \xe2\x96\xb8  Last tweaked  \xe2\x96\xb8  Create automation clip"), false, false);
    m.addItem (3, utf8 ("eller:  Add  \xe2\x96\xb8  Automation from last tweaked parameter"), false, false);
    m.addSeparator();
    m.addItem (1, utf8 ("Tilbakestill til lydens standard"));
    juce::Component::SafePointer<HouseKeysEditor> safe (ed);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c),
                     [safe, paramID] (int r) { if (r == 1 && safe != nullptr) safe->getProcessor().resetToSoundDefault (paramID); });
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
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1825));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float lw = juce::jmax (3.5f, r * 0.11f);
    const float arcR = r - lw * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled() && s.getAlpha() > 0.5f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        if (enabled)   // neon glow
        {
            g.setColour (accent.withAlpha (0.10f));
            g.strokePath (val, juce::PathStrokeType (lw * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (0.18f));
            g.strokePath (val, juce::PathStrokeType (lw * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setGradientFill (juce::ColourGradient (accent.darker (0.3f), c.x - r, c.y + r, accent.brighter (0.25f), c.x + r, c.y - r, false));
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float br = r - lw * 2.2f;
    const bool hover = s.isMouseOverOrDragging();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - br, c.y - br + 3.0f, br * 2.0f, br * 2.0f);
    g.setGradientFill (juce::ColourGradient (col::knob.brighter (hover ? 0.5f : 0.32f), c.x, c.y - br,
                                             col::knob.darker (0.6f), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - br + 1.0f, c.y - br + 1.0f, br * 2.0f - 2.0f, br * 2.0f - 2.0f, 1.0f);

    const juce::Point<float> tip   (c.x + (br - 6.0f) * std::sin (angle), c.y - (br - 6.0f) * std::cos (angle));
    const juce::Point<float> inner (c.x + br * 0.35f * std::sin (angle), c.y - br * 0.35f * std::cos (angle));
    g.setColour (enabled ? accent.brighter (0.4f) : col::text);
    g.drawLine ({ inner, tip }, 3.0f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? col::knob.brighter (0.2f) : col::knob);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::border.brighter (0.25f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
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
    if (on)
    {
        g.setColour (onCol.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), r.getHeight() * 0.5f + 2.0f);
    }
    g.setColour (on ? onCol : col::knob.brighter (highlighted ? 0.25f : 0.1f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? onCol.brighter (0.3f) : col::border.brighter (0.3f));
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
juce::Font Look::getPopupMenuFont()                { return uiFont (14.5f); }

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Label* Look::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (13.5f, true));
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    styleCaption (label, title);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int size = juce::jmin (r.getWidth(), maxSize, r.getHeight() - 18 - 22);
    auto block = r.withSizeKeepingCentre (r.getWidth(), 18 + size + 22);
    label.setBounds (block.removeFromTop (18));
    slider.setBounds (block.withSizeKeepingCentre (juce::jmax (size, 84), size + 22));
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
    auto inner = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 104), 54);
    label.setBounds (inner.removeFromTop (18));
    inner.removeFromTop (4);
    box.setBounds (inner.removeFromTop (32));
}

//==============================================================================
SoundBrowser::SoundBrowser (HouseKeysProcessor& p) : proc (p) {}

void SoundBrowser::open()
{
    shownCategory = soundDef (proc.getCurrentProgram()).category;
    hover = -1;
    layoutHits();
    setVisible (true);
    toFront (false);
    repaint();
}

void SoundBrowser::resized() { layoutHits(); }

void SoundBrowser::layoutHits()
{
    hits.clear();
    auto r = getLocalBounds().reduced (20, 0);
    closeArea = { r.getRight() - 40, 14, 30, 30 };
    hits.push_back ({ closeArea, -1, -1, true });

    catArea = r.withTrimmedTop (56).withTrimmedBottom (16).removeFromLeft (250);
    tileArea = r.withTrimmedTop (56).withTrimmedBottom (16).withTrimmedLeft (250 + 18);

    const int ch = juce::jmin (44, catArea.getHeight() / hk::kNumCategories);
    for (int c = 0; c < hk::kNumCategories; ++c)
        hits.push_back ({ catArea.withHeight (ch - 6).translated (0, c * ch), c, -1, false });

    int n = 0;
    const int cols = 3, tw = (tileArea.getWidth() - (cols - 1) * 12) / cols, th = 58;
    auto cell = [&] { const int cx = n % cols, cy = n / cols; ++n;
                      return juce::Rectangle<int> (tileArea.getX() + cx * (tw + 12), tileArea.getY() + cy * (th + 12), tw, th); };
    if (shownCategory == hk::kCatMine)
    {
        const auto saved = HouseKeysProcessor::listCustomPresets();
        bool isSaved = false;
        for (auto& f : saved) isSaved |= f.getFileNameWithoutExtension() == proc.getCustomName();
        if (proc.hasCustomSound() && ! isSaved) hits.push_back ({ cell(), -1, hk::kAiSound, false, {}, proc.getCustomName() });
        for (auto& f : saved)
            hits.push_back ({ cell(), -1, -2, false, f, f.getFileNameWithoutExtension() });
        return;
    }
    for (int i = 0; i < hk::kNumSounds; ++i)
    {
        if (hk::sounds()[(size_t) i].category != shownCategory) continue;
        hits.push_back ({ cell(), -1, i, false });
    }
}

int SoundBrowser::hitAt (juce::Point<int> p) const
{
    for (size_t i = 0; i < hits.size(); ++i) if (hits[i].r.contains (p)) return (int) i;
    return -1;
}

void SoundBrowser::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SoundBrowser::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void SoundBrowser::mouseUp (const juce::MouseEvent& e)
{
    const int h = hitAt (e.getPosition());
    if (h < 0) return;
    const auto hit = hits[(size_t) h];
    if (hit.close) { setVisible (false); if (onClose) onClose(); return; }
    if (hit.category >= 0) { shownCategory = hit.category; hover = -1; layoutHits(); repaint(); return; }
    if (hit.sound == -2 && hit.preset.existsAsFile())
    {
        proc.loadCustomPreset (hit.preset);
        setVisible (false);
        if (onClose) onClose();
        return;
    }
    if (hit.sound >= 0)
    {
        proc.selectSound (hit.sound);
        setVisible (false);
        if (onClose) onClose();
    }
}

void SoundBrowser::paint (juce::Graphics& g)
{
    struct EmptyHint {};
    auto all = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0a13));
    g.fillRoundedRectangle (all, 16.0f);
    g.setColour (col::border.brighter (0.2f));
    g.drawRoundedRectangle (all.reduced (0.5f), 16.0f, 1.0f);

    g.setColour (col::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText ("VELG LYD", 26, 14, 300, 30, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (uiFont (12.5f));
    g.drawText (juce::String (hk::kNumSounds) + " lyder i " + juce::String (hk::kNumCategories) + " kategorier",
                150, 14, 300, 30, juce::Justification::centredLeft);

    const int cur = proc.getCurrentProgram();
    if (shownCategory == hk::kCatMine && hits.size() <= (size_t) hk::kNumCategories + 1)
    {
        g.setColour (col::faint);
        g.setFont (uiFont (15.0f));
        g.drawMultiLineText (utf8 ("Her havner lydene du lager med \u2726 AI og lagrer.\nTrykk \u2726 AI, beskriv lyden, og trykk \u00abLagre som preset\u00bb."),
                             tileArea.getX(), tileArea.getY() + 24, tileArea.getWidth());
    }
    for (size_t i = 0; i < hits.size(); ++i)
    {
        const auto& h = hits[i];
        const bool hov = (int) i == hover;
        if (h.close)
        {
            g.setColour (hov ? col::text : col::dim);
            const auto c = h.r.toFloat().reduced (9.0f);
            g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 2.2f);
            g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 2.2f);
            continue;
        }
        if (h.category >= 0)
        {
            const auto c = col::category (h.category);
            const bool sel = h.category == shownCategory;
            auto r = h.r.toFloat();
            if (sel)
            {
                g.setGradientFill (juce::ColourGradient (c.withAlpha (0.28f), r.getX(), 0, c.withAlpha (0.04f), r.getRight(), 0, false));
                g.fillRoundedRectangle (r, 9.0f);
            }
            else if (hov)
            {
                g.setColour (col::panel2);
                g.fillRoundedRectangle (r, 9.0f);
            }
            g.setColour (c);
            g.fillRoundedRectangle (r.getX() + 10.0f, r.getCentreY() - 7.0f, 4.0f, 14.0f, 2.0f);
            g.setColour (sel ? col::text : (hov ? col::text : col::dim));
            g.setFont (uiFont (14.5f, true));
            g.drawText (hk::categoryName (h.category), r.withTrimmedLeft (26).toNearestInt(), juce::Justification::centredLeft);
            int count = 0;
            for (auto& s : hk::sounds()) count += s.category == h.category;
            if (h.category == hk::kCatMine)
            {
                const auto saved = HouseKeysProcessor::listCustomPresets();
                bool isSaved = false;
                for (auto& f : saved) isSaved |= f.getFileNameWithoutExtension() == proc.getCustomName();
                count = saved.size() + (proc.hasCustomSound() && ! isSaved ? 1 : 0);
            }
            g.setFont (uiFont (12.0f, true));
            g.setColour (sel ? c : col::faint);
            g.drawText (juce::String (count), r.withTrimmedRight (12).toNearestInt(), juce::Justification::centredRight);
            continue;
        }
        // sound tile
        const auto& d = hk::sounds()[(size_t) juce::jmax (0, h.sound)];
        const auto c = col::category (h.sound < 0 || h.sound == hk::kAiSound ? hk::kCatMine : d.category);
        const bool isCur = h.sound == cur || (h.sound == -2 && cur == hk::kAiSound && h.label == proc.getCustomName());
        auto r = h.r.toFloat();
        if (isCur)
        {
            g.setColour (c.withAlpha (0.18f));
            g.fillRoundedRectangle (r.expanded (3.0f), 14.0f);
        }
        g.setGradientFill (juce::ColourGradient (col::panel2.brighter (hov ? 0.15f : 0.0f), r.getX(), r.getY(),
                                                 juce::Colour (0xff17131e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (isCur ? c : (hov ? c.withAlpha (0.6f) : col::border.brighter (0.15f)));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, isCur ? 2.0f : 1.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r.getX() + 12.0f, r.getY() + 14.0f, 4.0f, r.getHeight() - 28.0f, 2.0f);
        g.setColour (col::text);
        g.setFont (uiFont (17.0f, true));
        auto tileName = h.label.isNotEmpty() ? (h.sound == hk::kAiSound ? utf8 ("\u2726 ") + h.label : h.label) : utf8 (d.name);
        if (d.sampler.user) tileName = proc.getUserSampleName().isNotEmpty() ? proc.getUserSampleName()
                                                                             : utf8 ("Dra en lydfil inn i pluginen");
        g.drawFittedText (tileName, r.reduced (28.0f, 8.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }
}

//==============================================================================
AiPanel::AiPanel (HouseKeysProcessor& p) : proc (p)
{
    for (auto* t : { &input, &keyInput })
    {
        t->setFont (uiFont (t == &input ? 18.0f : 13.0f));
        t->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b1724));
        t->setColour (juce::TextEditor::outlineColourId, col::border.brighter (0.3f));
        t->setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
        t->setColour (juce::TextEditor::textColourId, col::text);
        t->setIndents (14, t == &input ? 13 : 7);
        addAndMakeVisible (*t);
    }
    input.setTextToShowWhenEmpty (utf8 ("Beskriv lyden du vil ha, f.eks. \u00abfet house piano med mye luft og pump\u00bb"), col::faint);
    input.onReturnKey = [this] { run (input.getText()); };
    keyInput.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    keyInput.setTextToShowWhenEmpty (utf8 ("Valgfritt: Anthropic API-n\u00f8kkel for Claude"), col::faint);
    keyInput.setText (PresetAI::loadApiKey(), false);

    makeButton.setButtonText (utf8 ("LAG NY LYD"));
    saveKey.setButtonText ("Lagre");
    savePreset.setButtonText (utf8 ("Lagre som preset"));
    nameInput.setFont (uiFont (15.0f, true));
    nameInput.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b1724));
    nameInput.setColour (juce::TextEditor::outlineColourId, col::border.brighter (0.3f));
    nameInput.setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    nameInput.setColour (juce::TextEditor::textColourId, col::text);
    nameInput.setIndents (12, 9);
    addChildComponent (nameInput);
    addChildComponent (savePreset);
    savePreset.onClick = [this]
    {
        juce::String msg;
        proc.saveCustomPreset (nameInput.getText(), msg);
        notes.clear();
        notes.add (msg);
        resultTitle = utf8 ("\u2713  ") + nameInput.getText().trim();
        repaint();
    };
    for (auto* b : { &makeButton, &saveKey, &savePreset })
    {
        b->setColour (juce::TextButton::buttonColourId, b == &makeButton ? col::accent : (b == &savePreset ? col::violet : col::panel2));
        b->setColour (juce::TextButton::textColourOffId, b == &makeButton ? col::bg : col::text);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        if (b == &savePreset) addChildComponent (*b); else addAndMakeVisible (*b);
    }
    makeButton.onClick = [this] { run (input.getText()); };
    saveKey.onClick = [this]
    {
        PresetAI::saveApiKey (keyInput.getText());
        notes.clear();
        resultTitle = keyInput.getText().trim().isEmpty() ? utf8 ("N\u00f8kkel fjernet \u2013 bruker innebygd AI")
                                                          : utf8 ("N\u00f8kkel lagret \u2013 bruker Claude");
        repaint();
    };
}

void AiPanel::open()
{
    setVisible (true);
    toFront (true);
    input.grabKeyboardFocus();
    repaint();
}

void AiPanel::run (const juce::String& text)
{
    if (busy || text.trim().isEmpty()) return;
    lastPrompt = text.trim();
    const auto key = keyInput.getText().trim();
    usingClaude = key.isNotEmpty();
    auto apply = [this] (const PresetAI::Result& r)
    {
        busy = false;
        makeButton.setEnabled (true);
        notes = r.notes;
        if (r.ok)
        {
            proc.applyCustomSound (r.def, r.name);
            resultTitle = utf8 ("\u2726  Ny lyd: ") + r.name
                        + (r.fromClaude ? utf8 ("   (laget av Claude)") : utf8 ("   (innebygd AI)"));
            nameInput.setText (r.name, false);
            hasResult = true;
            nameInput.setVisible (true);
            savePreset.setVisible (true);
            resized();
        }
        else
            resultTitle = r.error;
        repaint();
    };
    if (usingClaude)
    {
        busy = true;
        makeButton.setEnabled (false);
        resultTitle = utf8 ("Claude designer lyden \u2026");
        notes.clear();
        repaint();
        juce::Component::SafePointer<AiPanel> safe (this);
        PresetAI::askClaude (lastPrompt, key, proc.getCurrentProgram(), [safe, apply] (PresetAI::Result r) { if (safe != nullptr) apply (r); });
    }
    else
        apply (PresetAI::interpretLocal (lastPrompt, proc.getCurrentProgram()));
}

void AiPanel::resized()
{
    auto r = getLocalBounds().reduced (26, 0);
    closeArea = { getWidth() - 50, 14, 30, 30 };
    auto row = r.withTrimmedTop (64).withHeight (52);
    makeButton.setBounds (row.removeFromRight (150));
    row.removeFromRight (12);
    input.setBounds (row);

    // example chips
    chips.clear();
    int x = r.getX(), y = 134;
    for (auto& ex : PresetAI::examples())
    {
        const int w = juce::GlyphArrangement::getStringWidthInt (uiFont (13.5f), ex) + 30;
        if (x + w > r.getRight()) { x = r.getX(); y += 40; }
        chips.push_back ({ { x, y, w, 30 }, ex });
        x += w + 10;
    }

    auto save = r.withTrimmedTop (getHeight() - 112).withHeight (40);
    savePreset.setBounds (save.removeFromRight (180));
    save.removeFromRight (10);
    nameInput.setBounds (save.removeFromRight (360));

    auto bottom = r.withTrimmedTop (getHeight() - 60).withHeight (34);
    saveKey.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (10);
    keyInput.setBounds (bottom.removeFromRight (420));
}

void AiPanel::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < chips.size(); ++i) if (chips[i].first.contains (e.getPosition())) h = (int) i;
    if (closeArea.contains (e.getPosition())) h = 1000;
    if (h != hoverChip) { hoverChip = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void AiPanel::mouseUp (const juce::MouseEvent& e)
{
    if (closeArea.contains (e.getPosition())) { setVisible (false); if (onClose) onClose(); return; }
    for (auto& c : chips)
        if (c.first.contains (e.getPosition())) { input.setText (c.second, false); run (c.second); return; }
}

void AiPanel::paint (juce::Graphics& g)
{
    auto all = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0a13));
    g.fillRoundedRectangle (all, 16.0f);
    juce::ColourGradient glow (col::violet.withAlpha (0.22f), 0, 0, col::violet.withAlpha (0.0f), all.getWidth() * 0.6f, all.getHeight(), true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (all, 16.0f);
    g.setColour (col::border.brighter (0.2f));
    g.drawRoundedRectangle (all.reduced (0.5f), 16.0f, 1.0f);

    // title with sparkle
    juce::ColourGradient tg (col::accent, 26.0f, 0.0f, col::violet, 360.0f, 0.0f, false);
    tg.addColour (0.5, col::hot);
    g.setGradientFill (tg);
    g.setFont (uiFont (22.0f, true));
    g.drawText (utf8 ("\u2726 AI  \u00b7  LAG LYDEN DU VIL HA"), 26, 16, 600, 32, juce::Justification::centredLeft);

    g.setColour (hoverChip == 1000 ? col::text : col::dim);
    const auto c = closeArea.toFloat().reduced (9.0f);
    g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 2.2f);
    g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 2.2f);

    g.setFont (uiFont (13.5f));
    for (size_t i = 0; i < chips.size(); ++i)
    {
        auto r = chips[i].first.toFloat();
        const bool hov = (int) i == hoverChip;
        g.setColour (hov ? col::accent.withAlpha (0.25f) : col::panel2);
        g.fillRoundedRectangle (r, 15.0f);
        g.setColour (hov ? col::accent : col::border.brighter (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 15.0f, 1.0f);
        g.setColour (hov ? col::text : col::dim);
        g.drawText (chips[i].second, r.toNearestInt(), juce::Justification::centred);
    }

    // result
    const int ry = (chips.empty() ? 134 : chips.back().first.getBottom()) + 30;
    if (resultTitle.isNotEmpty())
    {
        g.setColour (col::text);
        g.setFont (uiFont (20.0f, true));
        g.drawText (resultTitle, 26, ry, getWidth() - 52, 30, juce::Justification::centredLeft);
        g.setFont (uiFont (14.5f));
        int y = ry + 38;
        for (auto& n : notes)
        {
            g.setColour (col::accent);
            g.fillEllipse (30.0f, (float) y + 8.0f, 6.0f, 6.0f);
            g.setColour (col::dim.brighter (0.4f));
            g.drawText (n, 46, y, getWidth() - 80, 22, juce::Justification::centredLeft);
            y += 26;
        }
    }
    else
    {
        g.setColour (col::faint);
        g.setFont (uiFont (14.5f));
        g.drawMultiLineText (utf8 ("Skriv p\u00e5 norsk eller engelsk hva du vil ha \u2013 type lyd (piano, rhodes, pad, bass, lead, pluck, supersaw \u2026)\n"
                                   "og hvordan den skal l\u00e5te (fet, m\u00f8rk, lys, luftig, bred, kort, lo-fi, med reverb, delay, pump, vibrato \u2026).\n"
                                   "AI-en lager en NY lyd: den endrer selve lyddesignet (lag, oscillatorer, filter) og alle knappene.\n"
                                   "Liker du den, gi den et navn og trykk \u00abLagre som preset\u00bb \u2013 da ligger den under Mine lyder."),
                             26, ry + 20, getWidth() - 52);
    }

    g.setColour (col::faint);
    g.setFont (uiFont (12.0f));
    g.drawText (keyInput.getText().trim().isEmpty() ? utf8 ("Bruker innebygd AI (gratis, uten internett)")
                                                    : utf8 ("Bruker Claude (din API-n\u00f8kkel)"),
                26, getHeight() - 60, 400, 34, juce::Justification::centredLeft);
}

//==============================================================================
SoundSelector::SoundSelector (HouseKeysProcessor& p) : proc (p)
{
    name.paramID = "sound";
    for (juce::Component* c : { (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &name, (juce::Component*) &ai })
    {
        addAndMakeVisible (c);
        c->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    prev.onClick = [this] { proc.selectSound (proc.getCurrentProgram() - 1); refresh(); };
    next.onClick = [this] { proc.selectSound (proc.getCurrentProgram() + 1); refresh(); };
    name.onClick   = [this] { if (onBrowse) onBrowse(); };
    browse.onClick = [this] { if (onBrowse) onBrowse(); };
    ai.onClick = [this] { if (onAi) onAi(); };
    refresh();
}

void SoundSelector::refresh()
{
    const int cur = proc.getCurrentProgram();
    const auto un = proc.getUserSampleName(), cn = proc.getCustomName();
    if (name.sound != cur || name.userName != un || name.customName != cn)
    { name.sound = cur; name.userName = un; name.customName = cn; name.repaint(); }
}

void SoundSelector::resized()
{
    auto r = getLocalBounds();
    ai.setBounds (r.removeFromRight (84));
    r.removeFromRight (10);
    prev.setBounds (r.removeFromLeft (48));
    next.setBounds (r.removeFromRight (48));
    name.setBounds (r.reduced (8, 0));
}

void SoundSelector::Arrow::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::panel2.brighter (highlighted ? 0.2f : 0.0f).darker (down ? 0.15f : 0.0f));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (col::border.brighter (0.25f));
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
    const auto c = r.getCentre();
    juce::Path tri;
    if (isLeft) tri.addTriangle (c.x + 5.0f, c.y - 8.0f, c.x + 5.0f, c.y + 8.0f, c.x - 6.0f, c.y);
    else        tri.addTriangle (c.x - 5.0f, c.y - 8.0f, c.x - 5.0f, c.y + 8.0f, c.x + 6.0f, c.y);
    g.setColour (highlighted ? col::text : col::accent);
    g.fillPath (tri);
}

void SoundSelector::NameBox::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& d = soundDef (sound);
    const auto c = col::category (d.category);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (c.withAlpha (highlighted ? 0.22f : 0.12f));
    g.fillRoundedRectangle (r.expanded (1.0f), 13.0f);
    g.setGradientFill (juce::ColourGradient (col::panel2.brighter (highlighted ? 0.12f : 0.0f).darker (down ? 0.1f : 0.0f), r.getX(), r.getY(),
                                             juce::Colour (0xff18141f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r.reduced (1.5f), 12.0f);
    g.setColour (c);
    g.drawRoundedRectangle (r.reduced (1.5f), 12.0f, 1.6f);
    g.fillRoundedRectangle (r.getX() + 10.0f, r.getY() + 11.0f, 4.0f, r.getHeight() - 22.0f, 2.0f);

    auto t = r.reduced (26.0f, 6.0f);
    auto top = t.removeFromTop (t.getHeight() * 0.40f);
    g.setFont (uiFont (11.0f, true));
    g.setColour (c);
    g.drawText (juce::String (hk::categoryName (d.category)).toUpperCase(), top, juce::Justification::bottomLeft);
    auto nameArea = t.toNearestInt();
    auto nm = utf8 (d.name);
    if (d.sampler.user) nm = userName.isNotEmpty() ? userName : utf8 ("Dra en lydfil inn hit");
    if (sound == hk::kAiSound) nm = customName.isNotEmpty() ? customName : utf8 ("Lag en lyd med \u2726 AI");
    g.setFont (uiFont (23.0f, true));
    g.setColour (col::text);
    g.drawText (nm, nameArea.withTrimmedRight (30), juce::Justification::centredLeft);
    const float ax = r.getRight() - 22.0f, ay = r.getCentreY();
    juce::Path arrow;
    arrow.addTriangle (ax - 6.0f, ay - 3.0f, ax + 6.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

void SoundSelector::BrowseButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient cg (col::accent, r.getX(), r.getY(), col::hot, r.getRight(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.setOpacity (down ? 0.8f : 1.0f);
    g.fillRoundedRectangle (r, 12.0f);
    if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r, 12.0f); }
    // grid icon
    g.setColour (col::bg);
    const float gx = r.getX() + 16.0f, gy = r.getCentreY() - 8.0f;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillRoundedRectangle (gx + (float) i * 9.0f, gy + (float) j * 9.0f, 7.0f, 7.0f, 1.5f);
    g.setFont (uiFont (13.5f, true));
    g.drawText ("LYDER", r.withTrimmedLeft (38.0f), juce::Justification::centredLeft);
}

void SoundSelector::AiButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient cg (col::violet, r.getX(), r.getY(), col::hot, r.getRight(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.setOpacity (down ? 0.8f : 1.0f);
    g.fillRoundedRectangle (r, 12.0f);
    if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.14f)); g.fillRoundedRectangle (r, 12.0f); }
    g.setColour (juce::Colours::white);
    g.setFont (uiFont (15.0f, true));
    g.drawText (utf8 ("\u2726 AI"), r, juce::Justification::centred);
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
    for (auto& c : items) c->setAlpha (on ? 1.0f : 0.35f);
    repaint();
}

void FxCard::paint (juce::Graphics& g)
{
    const bool on = power.getToggleState();
    drawPanel (g, getLocalBounds(), title, on ? col::accent : col::dim, on);
}

void FxCard::resized()
{
    auto r = getLocalBounds();
    power.setBounds (r.getRight() - 64, 11, 50, 22);
    auto inner = r.reduced (10, 0).withTrimmedTop (42).withTrimmedBottom (10);
    const int n = juce::jmax (1, (int) items.size());
    const int w = inner.getWidth() / n;
    for (int i = 0; i < (int) items.size(); ++i)
        items[(size_t) i]->setBounds (inner.getX() + i * w, inner.getY(), w, inner.getHeight());
}

//==============================================================================
HouseKeysEditor::HouseKeysEditor (HouseKeysProcessor& p)
    : AudioProcessorEditor (p), proc (p), selector (p), browser (p), aiPanel (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    addAndMakeVisible (selector);
    selector.onBrowse = [this] { aiPanel.setVisible (false); if (browser.isVisible()) browser.setVisible (false); else browser.open(); };
    selector.onAi     = [this] { browser.setVisible (false); if (aiPanel.isVisible()) aiPanel.setVisible (false); else aiPanel.open(); };

    for (auto [id, title] : { std::pair { "tone", "Tone" }, std::pair { "air", "Air" }, std::pair { "attack", "Attack" },
                              std::pair { "decay", "Decay" }, std::pair { "release", "Release" },
                              std::pair { "width", "Width" }, std::pair { "volume", "Volume" } })
    {
        mainKnobs.push_back (std::make_unique<Knob> (s, id, title, 112));
        addAndMakeVisible (*mainKnobs.back());
        if (juce::String (id) == "air") airKnob = mainKnobs.back().get();
    }
    airKnob->slider.setColour (juce::Slider::rotarySliderFillColourId, col::air);
    airKnob->label.setColour (juce::Label::textColourId, col::air);
    chord = std::make_unique<Choice> (s, "chord", "Chord");
    addAndMakeVisible (*chord);
    smpMode = std::make_unique<Choice> (s, "smpMode", "Modus");
    addChildComponent (*smpMode);

    auto knob   = [&s] (const char* id, const char* t) { return std::make_unique<Knob> (s, id, t, 80); };
    auto choice = [&s] (const char* id, const char* t) { return std::make_unique<Choice> (s, id, t); };

    auto filter = std::make_unique<FxCard> (s, "Filter", "filterOn");
    filter->add (knob ("cutoff", "Cutoff"));
    filter->add (knob ("reso", "Reso"));
    auto sat = std::make_unique<FxCard> (s, "Saturate", "satOn");
    sat->add (knob ("satDrive", "Drive"));
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
    auto vib = std::make_unique<FxCard> (s, "Vibrato", "vibOn");
    vib->add (knob ("vibRate", "Rate"));
    vib->add (knob ("vibDepth", "Depth"));

    for (auto* c : { &filter, &sat, &chorus, &vib, &delay, &reverb, &pump })
    {
        cards.push_back (std::move (*c));
        addAndMakeVisible (*cards.back());
        cards.back()->refresh();
    }

    addChildComponent (browser);   // hidden until opened
    addChildComponent (aiPanel);
    for (auto& k : mainKnobs) k->slider.sendLookAndFeelChange();

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
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
    if (sel != lastSound)
    {
        lastSound = sel;
        const auto c = col::category (soundDef (sel).category);
        for (auto& k : mainKnobs)
            if (k.get() != airKnob) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, c); k->slider.repaint(); }
        if (browser.isVisible()) browser.repaint();
        resized();
        repaint();
    }
    for (auto& c : cards) c->refresh();

    // meter: peak hold with smooth fall
    const float pl = proc.meterL.exchange (0.0f), pr = proc.meterR.exchange (0.0f);
    levL = std::max (pl, levL * 0.86f);
    levR = std::max (pr, levR * 0.86f);
    repaint (meterArea);

    const auto st = SampleLibrary::statusText();
    if (st != status) { status = st; repaint (footer); }
    if (dropMessageTicks > 0 && --dropMessageTicks == 0) repaint (footer);
}

void HouseKeysEditor::drawMeter (juce::Graphics& g)
{
    auto r = meterArea.toFloat();
    const float bw = (r.getWidth() - 6.0f) / 2.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (r.getX() + (float) ch * (bw + 6.0f), r.getY(), bw, r.getHeight());
        g.setColour (col::track);
        g.fillRoundedRectangle (bar, 3.0f);
        const float lev = ch == 0 ? levL : levR;
        const float db = lev > 0.0f ? 20.0f * std::log10 (lev) : -60.0f;
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto fill = bar.withTrimmedTop (bar.getHeight() * (1.0f - frac));
        juce::ColourGradient cg (col::violet, 0, bar.getBottom(), col::hot, 0, bar.getY(), false);
        cg.addColour (0.55, col::accent);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (fill, 3.0f);
    }
}

//==============================================================================
bool HouseKeysEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;m4a;caf")) return true;
    return false;
}

void HouseKeysEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    for (auto& f : files)
    {
        juce::File file (f);
        if (! file.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;m4a;caf")) continue;
        juce::String msg;
        const bool ok = proc.loadUserSample (file, msg);
        dropMessage = ok ? file.getFileName() + "  \xc2\xb7  " + msg : "Kunne ikke laste " + file.getFileName() + ": " + msg;
        dropMessage = juce::String::fromUTF8 (dropMessage.toRawUTF8());
        dropMessageTicks = 30 * 6;
        break;
    }
    selector.refresh();
    repaint();
}

void HouseKeysEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragging) return;
    auto r = getLocalBounds().toFloat().reduced (14.0f);
    g.setColour (juce::Colour (0xe00a0810));
    g.fillRoundedRectangle (r, 18.0f);
    const float dash[] = { 10.0f, 8.0f };
    juce::Path p; p.addRoundedRectangle (r.reduced (6.0f), 16.0f);
    juce::Path dashed; juce::PathStrokeType (2.5f).createDashedStroke (dashed, p, dash, 2);
    g.setColour (col::accent);
    g.fillPath (dashed);
    g.setFont (uiFont (32.0f, true));
    g.drawText (utf8 ("Slipp lyden her"), r.withTrimmedBottom (40.0f).toNearestInt(), juce::Justification::centred);
    g.setColour (col::dim);
    g.setFont (uiFont (15.0f));
    g.drawText (utf8 ("Grunntonen finnes automatisk, og lyden kan spilles over hele klaviaturet"),
                r.withTrimmedTop (50.0f).toNearestInt(), juce::Justification::centred);
}

void HouseKeysEditor::paint (juce::Graphics& g)
{
    const auto w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (col::bg);
    // neon club glow
    g.setGradientFill (juce::ColourGradient (col::violet.withAlpha (0.30f), w * 0.12f, h * 0.05f,
                                             col::violet.withAlpha (0.0f), w * 0.55f, h * 0.65f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (col::hot.withAlpha (0.20f), w * 0.92f, h * 0.98f,
                                             col::hot.withAlpha (0.0f), w * 0.45f, h * 0.35f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.10f), w * 0.95f, 0.0f,
                                             col::accent.withAlpha (0.0f), w * 0.65f, h * 0.3f, true));
    g.fillAll();
    // subtle dotted grid
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    for (float y = 8.0f; y < h; y += 16.0f)
        for (float x = 8.0f; x < w; x += 16.0f)
            g.fillRect (x, y, 1.2f, 1.2f);

    // logo
    juce::ColourGradient lg (col::accent, 22.0f, 0.0f, col::violet, 280.0f, 0.0f, false);
    lg.addColour (0.5, col::hot);
    g.setGradientFill (lg);
    g.setFont (uiFont (36.0f, true));
    g.drawText ("HOUSEKEYS", 22, 12, 300, 40, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText (utf8 ("P I A N O   \xc2\xb7   K E Y S   \xc2\xb7   S Y N T H S"), 24, 52, 320, 16, juce::Justification::centredLeft);

    drawPanel (g, soundArea, "Sound", col::category (soundDef (proc.getCurrentProgram()).category), true);
    drawMeter (g);

    g.setFont (uiFont (11.5f));
    g.setColour (col::faint);
    juce::String left = status;
    if (dropMessageTicks > 0) { left = dropMessage; g.setColour (col::accent); }
    else if (soundDef (proc.getCurrentProgram()).sampler.user)
        left = proc.getUserSampleName().isNotEmpty()
                 ? utf8 ("Ditt sample: ") + proc.getUserSampleName() + utf8 ("  \xc2\xb7  ") + proc.getUserSampleInfo()
                 : utf8 ("Dra en lydfil (wav, aiff, flac, mp3) inn i pluginen for \xc3\xa5 lage din egen lyd");
    g.drawText (left, footer, juce::Justification::centredLeft);
    g.setColour (col::faint);
    g.drawText (utf8 ("Piano: Salamander Grand Piano av Alexander Holm (CC-BY 3.0)"), footer, juce::Justification::centredRight);
}

void HouseKeysEditor::resized()
{
    const int margin = 20, gap = 12;

    meterArea = { getWidth() - margin - 16, 16, 16, 52 };
    selector.setBounds (330, 14, meterArea.getX() - 14 - 330, 56);

    soundArea = { margin, 86, getWidth() - margin * 2, 236 };
    {
        auto inner = soundArea.reduced (16, 0).withTrimmedTop (36).withTrimmedBottom (12);
        const int n = (int) mainKnobs.size() + 1;
        const int wk = inner.getWidth() / n;
        for (int i = 0; i < (int) mainKnobs.size(); ++i)
            mainKnobs[(size_t) i]->setBounds (inner.getX() + i * wk, inner.getY(), wk, inner.getHeight());
        auto cell = juce::Rectangle<int> (inner.getX() + (n - 1) * wk, inner.getY(), wk, inner.getHeight());
        const bool user = soundDef (proc.getCurrentProgram()).sampler.user;
        smpMode->setVisible (user);
        if (user)
        {
            auto top = cell.removeFromTop (cell.getHeight() / 2);
            chord->setBounds (top.withTrimmedTop (8));
            smpMode->setBounds (cell.withTrimmedBottom (8));
        }
        else
            chord->setBounds (cell);
    }

    footer = { margin + 4, getHeight() - 26, getWidth() - margin * 2 - 8, 18 };

    {
        auto row = juce::Rectangle<int> (margin, soundArea.getBottom() + gap, getWidth() - margin * 2,
                                         footer.getY() - 6 - soundArea.getBottom() - gap);
        int units = 0;
        for (auto& c : cards) units += c->units();
        const int pad = 20;
        const float unitW = (float) (row.getWidth() - (int) (cards.size() - 1) * gap - (int) cards.size() * pad) / (float) units;
        float x = (float) row.getX();
        for (size_t i = 0; i < cards.size(); ++i)
        {
            const int wc = (i + 1 == cards.size()) ? row.getRight() - juce::roundToInt (x)
                                                   : juce::roundToInt (unitW * (float) cards[i]->units()) + pad;
            cards[i]->setBounds (juce::roundToInt (x), row.getY(), wc, row.getHeight());
            x += (float) (wc + gap);
        }
    }

    browser.setBounds (margin, 86, getWidth() - margin * 2, footer.getY() - 6 - 86);
    aiPanel.setBounds (browser.getBounds());
}
