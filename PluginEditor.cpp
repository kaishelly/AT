#include "PluginEditor.h"

namespace
{
    const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String noteName (float midi)
    {
        const int n = (int) std::lround (midi);
        return juce::String (noteNames[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    }

    bool isBlackKey (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
}

DepnoteTuneAudioProcessorEditor::DepnoteTuneAudioProcessorEditor (DepnoteTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (860, 560);

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList ({ "Chromatic", "Major", "Minor", "Harmonic Minor",
                            "Pentatonic Major", "Pentatonic Minor", "Blues", "Custom" }, 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAtt   = std::make_unique<ComboAtt> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt> (proc.apvts, "scale", scaleBox);

    const char* ids[5]    = { "speed", "amount", "tolerance", "mix", "output" };
    const char* titles[5] = { "RETUNE SPEED", "AMOUNT", "TOLERANCE", "MIX", "OUTPUT" };
    const char* suffix[5] = { " ms", " %", " ct", " %", " dB" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.title = titles[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);
        k.slider.setTextValueSuffix (suffix[i]);
        k.slider.setNumDecimalPlacesToDisplay (i == 4 ? 1 : 0);
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<SliderAtt> (proc.apvts, ids[i], k.slider);
    }

    startTimerHz (30);
}

DepnoteTuneAudioProcessorEditor::~DepnoteTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DepnoteTuneAudioProcessorEditor::resized()
{
    const int W = getWidth();
    scaleBox.setBounds (W - 24 - 170, 28, 170, 30);
    keyBox.setBounds (W - 24 - 170 - 10 - 80, 28, 80, 30);

    graphArea   = { 24.0f, 88.0f, 556.0f, 216.0f };
    pianoArea   = { 24.0f, 316.0f, 556.0f, 76.0f };
    readoutArea = { 596.0f, 88.0f, 240.0f, 304.0f };

    const int cellW = (W - 48) / 5;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].slider.setBounds (24 + (int) i * cellW, 404, cellW, 112);
}

void DepnoteTuneAudioProcessorEditor::timerCallback()
{
    const bool  voiced = proc.uiVoiced.load();
    const float det    = proc.uiDetectedMidi.load();
    const float tgt    = proc.uiTargetMidi.load();

    history.push_back ({ det, tgt, voiced });
    while (history.size() > maxPoints) history.pop_front();

    if (voiced)
    {
        if (! viewInit) { viewCentre = det; viewInit = true; }
        viewCentre += (det - viewCentre) * 0.05f;
    }
    repaint();
}

void DepnoteTuneAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setColour (juce::Colour (0x10ffffff));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colour (0x1affffff));
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
}

void DepnoteTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();

    g.setGradientFill (juce::ColourGradient (Theme::bg1, 0, 0, Theme::bg2, W, H, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (Theme::violet.withAlpha (0.20f), W * 0.85f, 0,
                                             juce::Colours::transparentBlack, W * 0.85f, H * 0.6f, true));
    g.fillRect (getLocalBounds());
    g.setGradientFill (juce::ColourGradient (Theme::magenta.withAlpha (0.10f), 0, H,
                                             juce::Colours::transparentBlack, W * 0.4f, H, true));
    g.fillRect (getLocalBounds());

    paintHeader (g);
    paintGraph (g);
    paintReadout (g);
    paintPiano (g);

    // knob titles
    const int cellW = (getWidth() - 48) / 5;
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colour (0x99ffffff));
    for (size_t i = 0; i < knobs.size(); ++i)
        g.drawText (knobs[i].title, 24 + (int) i * cellW, 520, cellW, 20, juce::Justification::centred);
}

void DepnoteTuneAudioProcessorEditor::paintHeader (juce::Graphics& g)
{
    juce::Font f (juce::FontOptions (32.0f, juce::Font::bold));
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, "DEPNOTE", 0.0f, 0.0f);
    const float tw = ga.getBoundingBox (0, -1, true).getWidth();

    g.setFont (f);
    g.setColour (juce::Colours::white);
    g.drawText ("DEPNOTE", 24, 18, (int) tw + 4, 40, juce::Justification::centredLeft);

    g.setGradientFill (juce::ColourGradient (Theme::cyan, 24 + tw + 10, 18, Theme::magenta, 24 + tw + 110, 58, false));
    g.drawText ("TUNE", 24 + (int) tw + 10, 18, 120, 40, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText ("REAL-TIME PITCH CORRECTION", 26, 54, 300, 16, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("KEY", keyBox.getX(), 12, 80, 14, juce::Justification::centredLeft);
    g.drawText ("SCALE", scaleBox.getX(), 12, 100, 14, juce::Justification::centredLeft);
}

void DepnoteTuneAudioProcessorEditor::paintGraph (juce::Graphics& g)
{
    drawPanel (g, graphArea);

    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip;
    clip.addRoundedRectangle (graphArea, 14.0f);
    g.reduceClipRegion (clip);

    const float visible = 16.0f;
    const float gh = graphArea.getHeight();
    auto yFor = [&] (float m) { return graphArea.getCentreY() - (m - viewCentre) / visible * gh; };
    const int mask = proc.getEffectiveMask();

    g.setFont (juce::FontOptions (11.0f));
    for (int m = (int) std::floor (viewCentre - 9.0f); m <= (int) std::ceil (viewCentre + 9.0f); ++m)
    {
        const int pc = ((m % 12) + 12) % 12;
        const float y = yFor ((float) m);
        if ((mask >> pc) & 1)
        {
            g.setColour (Theme::violet.withAlpha (0.10f));
            g.fillRect (graphArea.getX(), y - gh / visible * 0.5f, graphArea.getWidth(), gh / visible);
            g.setColour (Theme::violet.withAlpha (0.45f));
            g.drawHorizontalLine ((int) y, graphArea.getX(), graphArea.getRight());
            g.setColour (juce::Colour (0x99ffffff));
            g.drawText (noteName ((float) m), (int) graphArea.getX() + 8, (int) y - 8, 40, 16,
                        juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (juce::Colour (0x0affffff));
            g.drawHorizontalLine ((int) y, graphArea.getX(), graphArea.getRight());
        }
    }

    if (history.size() < 2) return;

    juce::Path detPath, tgtPath;
    const float step = graphArea.getWidth() / (float) (maxPoints - 1);
    bool prev = false;
    for (size_t i = 0; i < history.size(); ++i)
    {
        const auto& h = history[i];
        const float x = graphArea.getRight() - (float) (history.size() - 1 - i) * step;
        if (h.voiced)
        {
            if (! prev) { detPath.startNewSubPath (x, yFor (h.detected)); tgtPath.startNewSubPath (x, yFor (h.target)); }
            else        { detPath.lineTo (x, yFor (h.detected));          tgtPath.lineTo (x, yFor (h.target)); }
        }
        prev = h.voiced;
    }

    const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    g.setColour (Theme::cyan.withAlpha (0.55f));
    g.strokePath (detPath, stroke (1.5f));
    g.setColour (Theme::magenta.withAlpha (0.18f));
    g.strokePath (tgtPath, stroke (9.0f));
    g.setColour (Theme::magenta);
    g.strokePath (tgtPath, stroke (2.5f));
}

void DepnoteTuneAudioProcessorEditor::paintReadout (juce::Graphics& g)
{
    drawPanel (g, readoutArea);
    const auto r = readoutArea.reduced (18.0f);
    const bool voiced = proc.uiVoiced.load();
    const float det = proc.uiDetectedMidi.load();
    const float tgt = proc.uiTargetMidi.load();

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText ("DETECTED", r.getX(), r.getY(), r.getWidth(), 16, juce::Justification::centred);

    g.setFont (juce::FontOptions (64.0f, juce::Font::bold));
    g.setColour (voiced ? Theme::cyan : juce::Colour (0x33ffffff));
    g.drawText (voiced ? noteName (det) : "--", (int) r.getX(), (int) r.getY() + 18, (int) r.getWidth(), 76,
                juce::Justification::centred);

    const float cents = voiced ? (det - std::round (det)) * 100.0f : 0.0f;
    g.setFont (juce::FontOptions (14.0f));
    g.setColour (juce::Colours::white);
    g.drawText (voiced ? juce::String::formatted ("%+.0f cents", cents) : juce::String ("no pitch"),
                (int) r.getX(), (int) r.getY() + 96, (int) r.getWidth(), 20, juce::Justification::centred);

    // cents meter
    auto meter = juce::Rectangle<float> (r.getX() + 6, r.getY() + 128, r.getWidth() - 12, 8.0f);
    g.setColour (juce::Colour (0x22ffffff));
    g.fillRoundedRectangle (meter, 4.0f);
    g.setColour (juce::Colour (0x66ffffff));
    g.fillRect (meter.getCentreX() - 1.0f, meter.getY() - 4.0f, 2.0f, meter.getHeight() + 8.0f);
    if (voiced)
    {
        const float mx = meter.getCentreX() + juce::jlimit (-1.0f, 1.0f, cents / 50.0f) * meter.getWidth() * 0.5f;
        g.setColour (Theme::cyan.withAlpha (0.3f));
        g.fillEllipse (mx - 11.0f, meter.getCentreY() - 11.0f, 22.0f, 22.0f);
        g.setColour (Theme::cyan);
        g.fillEllipse (mx - 6.0f, meter.getCentreY() - 6.0f, 12.0f, 12.0f);
    }

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText ("OUTPUT", r.getX(), r.getY() + 170, r.getWidth(), 16, juce::Justification::centred);

    g.setFont (juce::FontOptions (44.0f, juce::Font::bold));
    g.setColour (voiced ? Theme::magenta : juce::Colour (0x33ffffff));
    g.drawText (voiced ? noteName (tgt) : "--", (int) r.getX(), (int) r.getY() + 188, (int) r.getWidth(), 56,
                juce::Justification::centred);
}

juce::Rectangle<float> DepnoteTuneAudioProcessorEditor::keyRect (int pc) const
{
    const float ww = pianoArea.getWidth() / 7.0f;
    const float x0 = pianoArea.getX(), y0 = pianoArea.getY();
    if (isBlackKey (pc))
    {
        const int boundary = pc == 1 ? 1 : pc == 3 ? 2 : pc == 6 ? 4 : pc == 8 ? 5 : 6;
        const float bw = ww * 0.6f;
        return { x0 + (float) boundary * ww - bw * 0.5f, y0, bw, pianoArea.getHeight() * 0.6f };
    }
    const int idx = pc == 0 ? 0 : pc == 2 ? 1 : pc == 4 ? 2 : pc == 5 ? 3 : pc == 7 ? 4 : pc == 9 ? 5 : 6;
    return { x0 + (float) idx * ww, y0, ww, pianoArea.getHeight() };
}

void DepnoteTuneAudioProcessorEditor::paintPiano (juce::Graphics& g)
{
    drawPanel (g, pianoArea.expanded (4.0f));
    const int mask = proc.getEffectiveMask();
    const int detPc = proc.uiVoiced.load() ? (((int) std::lround (proc.uiDetectedMidi.load()) % 12) + 12) % 12 : -1;

    for (int pass = 0; pass < 2; ++pass)           // whites first, then blacks
        for (int pc = 0; pc < 12; ++pc)
        {
            if (isBlackKey (pc) != (pass == 1)) continue;
            const bool allowed = (mask >> pc) & 1;
            auto k = keyRect (pc).reduced (1.5f);

            if (pass == 0)
                g.setGradientFill (allowed
                    ? juce::ColourGradient (Theme::violet.brighter (0.6f), k.getX(), k.getY(), Theme::violet, k.getX(), k.getBottom(), false)
                    : juce::ColourGradient (juce::Colour (0xff2a2d48), k.getX(), k.getY(), juce::Colour (0xff1a1c33), k.getX(), k.getBottom(), false));
            else
                g.setGradientFill (allowed
                    ? juce::ColourGradient (Theme::magenta, k.getX(), k.getY(), Theme::magenta.darker (0.5f), k.getX(), k.getBottom(), false)
                    : juce::ColourGradient (juce::Colour (0xff0c0d1a), k.getX(), k.getY(), juce::Colour (0xff15172b), k.getX(), k.getBottom(), false));
            g.fillRoundedRectangle (k, 5.0f);

            if (pc == detPc)
            {
                g.setColour (Theme::cyan);
                g.fillEllipse (k.getCentreX() - 5.0f, k.getBottom() - 16.0f, 10.0f, 10.0f);
            }
            if (pass == 0)
            {
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
                g.setColour (allowed ? juce::Colours::white.withAlpha (0.9f) : juce::Colour (0x55ffffff));
                g.drawText (noteNames[pc], k.withTrimmedTop (k.getHeight() * 0.55f).withTrimmedBottom (18.0f),
                            juce::Justification::centred);
            }
        }
}

void DepnoteTuneAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (! pianoArea.contains (p)) return;

    for (int pc : { 1, 3, 6, 8, 10 })
        if (keyRect (pc).contains (p)) { toggleNote (pc); return; }
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 })
        if (keyRect (pc).contains (p)) { toggleNote (pc); return; }
}

void DepnoteTuneAudioProcessorEditor::toggleNote (int pc)
{
    proc.customMask = proc.getEffectiveMask() ^ (1 << pc);
    if (auto* sp = proc.apvts.getParameter ("scale"))
    {
        sp->beginChangeGesture();
        sp->setValueNotifyingHost (sp->convertTo0to1 ((float) DepnoteTuneAudioProcessor::customScaleIndex));
        sp->endChangeGesture();
    }
}
