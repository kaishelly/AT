#pragma once
#include <JuceHeader.h>
#include "PitchDetector.h"
#include "PitchShifter.h"

class DepnoteTuneAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int customScaleIndex = 7;

    DepnoteTuneAudioProcessor();
    ~DepnoteTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Depnote Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Notes allowed for correction (bit n = pitch class n, C = 0)
    int getEffectiveMask() const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> customMask { 0xFFF };

    // telemetry for the UI
    std::atomic<float> uiDetectedMidi { 60.0f }, uiTargetMidi { 60.0f };
    std::atomic<bool>  uiVoiced { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void analyse (float speedMs, float amount, float tolCents, int mask);
    int  pickTarget (float midi, int mask);

    PitchDetector detector;
    PitchShifter  shifter;

    double sr = 44100.0;
    int hop = 256, hopCounter = 0, unvoicedHops = 0, currentNote = -1;
    float env = 0.0f, envCoeff = 0.001f;
    float corrSm = 0.0f;
    float ratioTarget = 1.0f, ratioSm = 1.0f;
    float wTarget = 500.0f, wSm = 500.0f;
    float voicedTarget = 0.0f, blendSm = 0.0f;
    float smoothCoef = 0.01f, wCoef = 0.001f, blendCoef = 0.005f;
    bool wasVoiced = false;

    std::atomic<float> *speedP = nullptr, *amountP = nullptr, *tolP = nullptr,
                       *mixP = nullptr, *outP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DepnoteTuneAudioProcessor)
};
