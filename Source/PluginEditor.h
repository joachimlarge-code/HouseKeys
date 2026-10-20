#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace hkui
{
namespace col
{
    const juce::Colour bg      { 0xff141218 };
    const juce::Colour bg2     { 0xff1c1922 };
    const juce::Colour panel   { 0xff221f2a };
    const juce::Colour border  { 0xff34303f };
    const juce::Colour track   { 0xff3a3546 };
    const juce::Colour knob    { 0xff2d2936 };
    const juce::Colour accent  { 0xffffb43c };
    const juce::Colour accent2 { 0xffff6f61 };
    const juce::Colour text    { 0xffeee8f2 };
    const juce::Colour dim     { 0xff8c849a };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title);
    void resized() override;

    juce::Slider slider;
    juce::Label  label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title);
    void resized() override;

    juce::ComboBox box;
    juce::Label    label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
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

private:
    void timerCallback() override;

    struct Section { juce::String title; juce::Rectangle<int> area; };

    HouseKeysProcessor& proc;
    hkui::Look look;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    std::unique_ptr<hkui::Choice> model, chord, pumpRate;
    std::vector<std::unique_ptr<hkui::Knob>> knobs;
    std::vector<Section> sections;

    hkui::Knob* knob (const juce::String& id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseKeysEditor)
};
