#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int maskOf (std::initializer_list<int> notes)
    {
        int m = 0;
        for (int n : notes) m |= 1 << n;
        return m;
    }

    // Relative to the root (bit 0 = root). Index 7 = Custom.
    constexpr int presetMasks[7] = {
        0xFFF,                              // Chromatic
        maskOf ({ 0, 2, 4, 5, 7, 9, 11 }),  // Major
        maskOf ({ 0, 2, 3, 5, 7, 8, 10 }),  // Minor
        maskOf ({ 0, 2, 3, 5, 7, 8, 11 }),  // Harmonic Minor
        maskOf ({ 0, 2, 4, 7, 9 }),         // Pentatonic Major
        maskOf ({ 0, 3, 5, 7, 10 }),        // Pentatonic Minor
        maskOf ({ 0, 3, 5, 6, 7, 10 })      // Blues
    };
}

DepnoteTuneAudioProcessor::DepnoteTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    speedP  = apvts.getRawParameterValue ("speed");
    amountP = apvts.getRawParameterValue ("amount");
    tolP    = apvts.getRawParameterValue ("tolerance");
    mixP    = apvts.getRawParameterValue ("mix");
    outP    = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout DepnoteTuneAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
        StringArray { "Chromatic", "Major", "Minor", "Harmonic Minor",
                      "Pentatonic Major", "Pentatonic Minor", "Blues", "Custom" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Retune Speed",
        NormalisableRange<float> (0.0f, 400.0f, 1.0f, 0.5f), 25.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tolerance", 1 }, "Tolerance",
        NormalisableRange<float> (0.0f, 50.0f, 1.0f), 5.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { p.begin(), p.end() };
}

int DepnoteTuneAudioProcessor::getEffectiveMask() const
{
    const int scale = (int) apvts.getRawParameterValue ("scale")->load();
    if (scale < 0 || scale >= customScaleIndex)
        return customMask.load();

    const int key = (int) apvts.getRawParameterValue ("key")->load();
    int m = 0;
    for (int bit = 0; bit < 12; ++bit)
        if ((presetMasks[scale] >> bit) & 1)
            m |= 1 << ((bit + key) % 12);
    return m;
}

bool DepnoteTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void DepnoteTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    detector.prepare (sr);
    shifter.prepare (sr, 2);

    hop         = std::max (64, (int) (sr * 0.0058));
    hopCounter  = 0;
    envCoeff    = 1.0f - std::exp (-1.0f / (float) (0.02 * sr));
    smoothCoef  = 1.0f - std::exp (-1.0f / (float) (0.003 * sr));
    wCoef       = 1.0f - std::exp (-1.0f / (float) (0.02 * sr));
    blendCoef   = 1.0f - std::exp (-1.0f / (float) (0.01 * sr));

    env = 0.0f; corrSm = 0.0f; ratioTarget = ratioSm = 1.0f;
    wTarget = wSm = (float) (0.012 * sr);
    voicedTarget = blendSm = 0.0f;
    wasVoiced = false; currentNote = -1; unvoicedHops = 0;

    setLatencySamples ((int) (0.008 * sr));   // approximate: ~half the correction window
}

int DepnoteTuneAudioProcessor::pickTarget (float midi, int mask)
{
    if ((mask & 0xFFF) == 0) return -1;

    int best = -1;
    float bestDist = 1e9f;
    const int lo = (int) std::floor (midi) - 6, hi = (int) std::ceil (midi) + 6;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        if (! ((mask >> pc) & 1)) continue;
        const float d = std::abs (midi - (float) n);
        if (d < bestDist) { bestDist = d; best = n; }
    }

    // hysteresis: stick to the current note unless the new one is clearly closer
    if (currentNote >= 0 && best != currentNote
        && ((mask >> (((currentNote % 12) + 12) % 12)) & 1)
        && std::abs (midi - (float) currentNote) <= bestDist + 0.25f)
        best = currentNote;

    currentNote = best;
    return best;
}

void DepnoteTuneAudioProcessor::analyse (float speedMs, float amount, float tolCents, int mask)
{
    float conf = 0.0f;
    const float f = env > 1e-6f ? detector.detect (conf) : 0.0f;
    const bool voiced = f > 0.0f && conf > 0.70f;

    if (voiced)
    {
        unvoicedHops = 0;
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);

        if (! wasVoiced) { corrSm = 0.0f; currentNote = -1; }

        const int target = pickTarget (midi, mask);
        float corr = target < 0 ? 0.0f : ((float) target - midi);

        const float tol = tolCents * 0.01f;                 // soft dead-zone
        if (std::abs (corr) <= tol) corr = 0.0f;
        else corr -= std::copysign (tol, corr);
        corr *= amount;

        const float hopSec = (float) hop / (float) sr;
        const float alpha  = speedMs <= 1.0f ? 1.0f : 1.0f - std::exp (-hopSec / (speedMs * 0.001f));
        corrSm += (corr - corrSm) * alpha;

        ratioTarget  = std::exp2 (corrSm / 12.0f);
        voicedTarget = 1.0f;
        wTarget = juce::jlimit ((float) (0.006 * sr), (float) (0.040 * sr), 2.0f * (float) sr / f);
        wasVoiced = true;

        uiDetectedMidi = midi;
        uiTargetMidi   = midi + corrSm;
        uiVoiced       = true;
    }
    else if (++unvoicedHops > 3)
    {
        voicedTarget = 0.0f;
        wasVoiced = false;
        uiVoiced = false;
    }
}

void DepnoteTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const float speedMs = speedP->load();
    const float amount  = amountP->load() * 0.01f;
    const float tol     = tolP->load();
    const float mix     = mixP->load() * 0.01f;
    const float outGain = juce::Decibels::decibelsToGain (outP->load());
    const int   mask    = getEffectiveMask();

    float* ch[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const float in[2] = { ch[0][i], numCh > 1 ? ch[1][i] : 0.0f };
        const float mono  = numCh > 1 ? 0.5f * (in[0] + in[1]) : in[0];

        detector.push (mono);
        env += (mono * mono - env) * envCoeff;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            analyse (speedMs, amount, tol, mask);
        }

        ratioSm += (ratioTarget  - ratioSm) * smoothCoef;
        wSm     += (wTarget      - wSm)     * wCoef;
        blendSm += (voicedTarget - blendSm) * blendCoef;

        float wet[2] = { 0.0f, 0.0f }, dry[2] = { 0.0f, 0.0f };
        shifter.process (in, wet, dry, numCh, ratioSm, wSm);

        for (int c = 0; c < numCh; ++c)
        {
            const float corrected = dry[c] + (wet[c] - dry[c]) * blendSm;
            ch[c][i] = (dry[c] + (corrected - dry[c]) * mix) * outGain;
        }
    }
}

juce::AudioProcessorEditor* DepnoteTuneAudioProcessor::createEditor()
{
    return new DepnoteTuneAudioProcessorEditor (*this);
}

void DepnoteTuneAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("customMask", customMask.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DepnoteTuneAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            customMask = (int) vt.getProperty ("customMask", 0xFFF);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DepnoteTuneAudioProcessor();
}
