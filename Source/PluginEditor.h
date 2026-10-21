#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace hkui
{
namespace col
{
    const juce::Colour bg      { 0xff0a0810 };
    const juce::Colour bg2     { 0xff15111d };
    const juce::Colour panel   { 0xe01a1622 };
    const juce::Colour panel2  { 0xff25202f };
    const juce::Colour border  { 0xff332c40 };
    const juce::Colour track   { 0xff2f2a3b };
    const juce::Colour knob    { 0xff2a2533 };
    const juce::Colour accent  { 0xffff9f2e };
    const juce::Colour hot     { 0xffff4fa3 };
    const juce::Colour violet  { 0xff8a5cff };
    const juce::Colour air     { 0xff6fe8ff };
    const juce::Colour text    { 0xfff4eef8 };
    const juce::Colour dim     { 0xff8d84a0 };
    const juce::Colour faint   { 0xff5a5368 };

    inline juce::Colour category (int c)
    {
        // Real Piano, House Piano, Deep Keys, Rhodes, Supersaw, Synths, Leads, Pads, Bass, Plucks, Your sample
        static const juce::uint32 cc[] = { 0xfff3e2c0, 0xffff9f2e, 0xffa98bff, 0xffff7a5a, 0xffff4fa3,
                                           0xffffd84a, 0xff6fe8ff, 0xff45e0b0, 0xff5c8dff, 0xffd0f25f, 0xffffffff, 0xffb68bff };
        return juce::Colour (cc[juce::jlimit (0, 11, c)]);
    }
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

// Full-size sound browser: categories on the left, big sound tiles on the right.
class SoundBrowser : public juce::Component
{
public:
    explicit SoundBrowser (HouseKeysProcessor&);
    void open();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onClose;

private:
    struct Hit { juce::Rectangle<int> r; int category = -1, sound = -1; bool close = false; juce::File preset; juce::String label; };
    void layoutHits();
    int hitAt (juce::Point<int>) const;

    HouseKeysProcessor& proc;
    int shownCategory = 0, hover = -1;
    std::vector<Hit> hits;
    juce::Rectangle<int> catArea, tileArea, closeArea;
};

// AI panel: describe a sound, get a preset
class AiPanel : public juce::Component
{
public:
    explicit AiPanel (HouseKeysProcessor&);
    void open();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    std::function<void()> onClose;

private:
    void run (const juce::String& text);

    HouseKeysProcessor& proc;
    juce::TextEditor input, keyInput, nameInput;
    juce::TextButton makeButton, saveKey, savePreset;
    bool hasResult = false;
    juce::StringArray notes;
    juce::String resultTitle, lastPrompt;
    bool busy = false, usingClaude = false;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> chips;
    juce::Rectangle<int> closeArea;
    int hoverChip = -1;
};

// Top bar: [<]  [ CATEGORY  name ]  [>]  [AI]  [LYDER]
class SoundSelector : public juce::Component
{
public:
    explicit SoundSelector (HouseKeysProcessor&);
    void resized() override;
    void refresh();
    std::function<void()> onBrowse, onAi;

private:
    struct Arrow : public juce::Button
    {
        explicit Arrow (bool left) : juce::Button (left ? "prev" : "next"), isLeft (left) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool isLeft;
    };
    struct NameBox : public juce::Button
    {
        NameBox() : juce::Button ("sound") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        int sound = 0;
        juce::String userName, customName;
    };
    struct BrowseButton : public juce::Button
    {
        BrowseButton() : juce::Button ("browse") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };
    struct AiButton : public juce::Button
    {
        AiButton() : juce::Button ("ai") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    HouseKeysProcessor& proc;
    Arrow prev { true }, next { false };
    HostMenu<NameBox> name;
    BrowseButton browse;
    AiButton ai;
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
    int units() const { return juce::jmax (2, (int) items.size()); }

private:
    juce::String title;
    HostMenu<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    std::vector<std::unique_ptr<juce::Component>> items;
    int lastOn = -1;
};
} // namespace hkui

//==============================================================================
class HouseKeysEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void paintOverChildren (juce::Graphics&) override;

    explicit HouseKeysEditor (HouseKeysProcessor&);
    ~HouseKeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    HouseKeysProcessor& getProcessor() { return proc; }

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&);

    HouseKeysProcessor& proc;
    hkui::Look look;

    hkui::SoundSelector selector;
    hkui::SoundBrowser browser;
    hkui::AiPanel aiPanel;
    std::vector<std::unique_ptr<hkui::Knob>> mainKnobs;
    hkui::Knob* airKnob = nullptr;
    std::unique_ptr<hkui::Choice> chord;
    std::unique_ptr<hkui::Choice> smpMode;   // only shown for 'Ditt sample'
    std::vector<std::unique_ptr<hkui::FxCard>> cards;

    juce::Rectangle<int> soundArea, footer, meterArea;
    juce::String status;
    int lastSound = -1;
    float levL = 0.0f, levR = 0.0f;
    bool dragging = false;
    juce::String dropMessage;
    int dropMessageTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysEditor)
};
