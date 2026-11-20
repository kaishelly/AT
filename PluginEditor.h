#pragma once
#include <JuceHeader.h>
#include <deque>
#include <array>
#include "PluginProcessor.h"

namespace Theme
{
    inline const juce::Colour bg1     { 0xff0a0e1f };
    inline const juce::Colour bg2     { 0xff1c1040 };
    inline const juce::Colour cyan    { 0xff35e0ff };
    inline const juce::Colour magenta { 0xffff3d9a };
    inline const juce::Colour violet  { 0xff8a5cff };
}

struct DepnoteLookAndFeel : public juce::LookAndFeel_V4
{
    DepnoteLookAndFeel()
    {
        using namespace juce;
        setColour (ComboBox::backgroundColourId, Colour (0x22ffffff));
        setColour (ComboBox::textColourId, Colours::white);
        setColour (ComboBox::outlineColourId, Colour (0x33ffffff));
        setColour (ComboBox::arrowColourId, Theme::cyan);
        setColour (PopupMenu::backgroundColourId, Colour (0xff1c1040));
        setColour (PopupMenu::textColourId, Colours::white);
        setColour (PopupMenu::highlightedBackgroundColourId, Theme::violet);
        setColour (Slider::textBoxTextColourId, Colours::white);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider&) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto  c = b.getCentre();
        const float a = startA + pos * (endA - startA);
        const float arcR = r - 2.0f;

        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
        g.setColour (Colour (0x22ffffff));
        g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));

        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, a, true);
        g.setColour (Theme::magenta.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (11.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (ColourGradient (Theme::cyan, b.getX(), b.getBottom(),
                                           Theme::magenta, b.getRight(), b.getY(), false));
        g.strokePath (val, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));

        const float br = arcR - 10.0f;
        g.setGradientFill (ColourGradient (Colour (0xff2e3354), c.x, c.y - br,
                                           Colour (0xff10132a), c.x, c.y + br, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (Colour (0x33ffffff));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

        const float s = std::sin (a), co = std::cos (a);
        g.setColour (Colours::white);
        g.drawLine (c.x + s * br * 0.35f, c.y - co * br * 0.35f,
                    c.x + s * br * 0.85f, c.y - co * br * 0.85f, 3.0f);
    }
};

class DepnoteTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit DepnoteTuneAudioProcessorEditor (DepnoteTuneAudioProcessor&);
    ~DepnoteTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct HistoryPoint { float detected, target; bool voiced; };

    void timerCallback() override;
    void drawPanel (juce::Graphics&, juce::Rectangle<float>) const;
    void paintHeader (juce::Graphics&);
    void paintGraph (juce::Graphics&);
    void paintReadout (juce::Graphics&);
    void paintPiano (juce::Graphics&);
    juce::Rectangle<float> keyRect (int pc) const;
    void toggleNote (int pc);

    DepnoteLookAndFeel laf;   // must outlive the components below
    DepnoteTuneAudioProcessor& proc;

    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;

    struct Knob { juce::Slider slider; juce::String title; std::unique_ptr<SliderAtt> att; };
    std::array<Knob, 5> knobs;

    juce::Rectangle<float> graphArea, pianoArea, readoutArea;
    std::deque<HistoryPoint> history;
    static constexpr size_t maxPoints = 150;
    float viewCentre = 60.0f;
    bool  viewInit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DepnoteTuneAudioProcessorEditor)
};
