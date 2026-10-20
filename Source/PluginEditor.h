#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace hkui
{
namespace col
{
    const juce::Colour bg      { 0xff121016 };
    const juce::Colour bg2     { 0xff1b1822 };
    const juce::Colour panel   { 0xff211e29 };
    const juce::Colour panel2  { 0xff2a2634 };
    const juce::Colour border  { 0xff35303f };
    const juce::Colour track   { 0xff3b3647 };
    const juce::Colour knob    { 0xff2e2a38 };
    const juce::Colour hot     { 0xffff7a45 };   // energetic
    const juce::Colour hot2    { 0xffffb43c };
    const juce::Colour calm    { 0xff5ec8ff };   // calm
    const juce::Colour calm2   { 0xff9a86ff };
    const juce::Colour accent  { 0xffffb43c };
    const juce::Colour text    { 0xfff1ebf5 };
    const juce::Colour dim     { 0xff8d859b };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Right-click on any control shows the host's parameter menu (in FL Studio this is where
// "Create automation clip" lives). Falls back to a small menu of our own.
void showParameterMenu (juce::Component& c, const juce::String& paramID);

template <class Base>
class HostMenu : public Base
{
public:
    using Base::Base;
    juce::String paramID;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showParameterMenu (*this, paramID); return; }
        Base::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Base::mouseDrag (e); }
    void mouseUp   (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Base::mouseUp (e); }
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title, int maxSize = 92);
    void resized() override;
    HostMenu<juce::Slider> slider;
    juce::Label label;
private:
    int maxSize;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title);
    void resized() override;
    HostMenu<juce::ComboBox> box;
    juce::Label label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Top bar: [<]  [ piano name  v ]  [>]
class PianoSelector : public juce::Component
{
public:
    explicit PianoSelector (HouseKeysProcessor&);
    void resized() override;
    void refresh();

private:
    struct Arrow : public juce::Button
    {
        explicit Arrow (bool left) : juce::Button (left ? "prev" : "next"), isLeft (left) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool isLeft;
    };
    struct NameBox : public juce::Button
    {
        NameBox() : juce::Button ("piano") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        int piano = 0;
    };

    void showMenu();

    HouseKeysProcessor& proc;
    Arrow prev { true }, next { false };
    HostMenu<NameBox> name;
};

// Effect section card with an on/off switch
class FxCard : public juce::Component
{
public:
    FxCard (juce::AudioProcessorValueTreeState& s, const juce::String& title, const juce::String& onId);
    void add (std::unique_ptr<juce::Component> c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    int units() const { return juce::jmax (2, (int) items.size()); }   // narrow cards still fit the title

private:
    juce::String title;
    HostMenu<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    std::vector<std::unique_ptr<juce::Component>> items;
    int lastOn = -1;
};
} // namespace hkui

//==============================================================================
class HouseKeysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HouseKeysEditor (HouseKeysProcessor&);
    ~HouseKeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    HouseKeysProcessor& getProcessor() { return proc; }

private:
    void timerCallback() override;

    HouseKeysProcessor& proc;
    hkui::Look look;

    hkui::PianoSelector selector;
    std::vector<std::unique_ptr<hkui::Knob>> mainKnobs;
    std::unique_ptr<hkui::Choice> chord;
    std::vector<std::unique_ptr<hkui::FxCard>> cards;

    juce::Rectangle<int> soundArea;
    int lastPiano = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysEditor)
};
