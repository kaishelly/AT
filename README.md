# Depnote Tune
Real-time pitch correction plugin (VST3 / AU / Standalone) built with JUCE 8.

## Build
Requires: CMake 3.22+, a C++17 compiler (Visual Studio 2022 / Xcode / GCC), git, internet (JUCE is fetched automatically).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The VST3 is copied to your system VST3 folder after the build (COPY_PLUGIN_AFTER_BUILD).
Use the Standalone target to test quickly with a microphone (use headphones!).

## Controls
- Key / Scale: choose notes to snap to. Click piano keys to build a custom scale.
- Retune Speed: 0 ms = hard "T-Pain" effect; 100+ ms = natural.
- Amount: how much of the correction is applied.
- Tolerance: cents of pitch error left untouched (keeps natural vibrato/drift).
- Mix / Output.

## DSP
- Detection: YIN (70 Hz - 1 kHz), refined with parabolic interpolation, hop ~5.8 ms.
- Correction: nearest allowed note with hysteresis, smoothed by Retune Speed.
- Shifting: pitch-synchronous two-tap overlap-add (window = 2 periods), Hermite interpolation.

## Known limits / next steps
- No formant preservation (large shifts sound "chipmunk"/"giant").
- Monophonic only; pitch-detector octave errors not yet filtered.
- Reported latency is approximate (~8 ms).
- Ideas: formant-preserving PSOLA/vocoder, MIDI-controlled target notes, vibrato control, per-note on/off from host, preset system.
