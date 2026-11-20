#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// YIN pitch detector (de Cheveigne & Kawahara, 2002) with parabolic refinement.
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr     = sampleRate;
        tauMin = std::max (2, (int) (sr / 1000.0));   // up to ~1 kHz
        tauMax = (int) (sr / 70.0) + 1;               // down to ~70 Hz
        window = (int) (sr * 0.025);

        const int n = window + tauMax + 1;
        ringSize = 1;
        while (ringSize < n) ringSize <<= 1;
        mask = ringSize - 1;

        ring.assign ((size_t) ringSize, 0.0f);
        frame.assign ((size_t) n, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 1.0f);
        writePos = 0;
    }

    void push (float x)
    {
        ring[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Returns frequency in Hz, or 0 if no reliable pitch. confidence in [0..1].
    float detect (float& confidence)
    {
        confidence = 0.0f;
        const int n = window + tauMax;
        float* fr = frame.data();
        float* cm = cmnd.data();

        for (int i = 0; i < n; ++i)
            fr[i] = ring[(size_t) ((writePos - n + i + ringSize) & mask)];

        cm[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            const float* b = fr + tau;
            for (int j = 0; j < window; ++j)
            {
                const float d = fr[j] - b[j];
                sum += d * d;
            }
            running += sum;
            cm[tau] = running > 1e-12f ? sum * (float) tau / running : 1.0f;
        }

        int tau = -1;
        for (int t = tauMin; t < tauMax; ++t)
        {
            if (cm[t] < threshold)
            {
                while (t + 1 < tauMax && cm[t + 1] < cm[t]) ++t;
                tau = t;
                break;
            }
        }

        if (tau < 0)
        {
            int m = tauMin;
            for (int t = tauMin; t < tauMax; ++t)
                if (cm[t] < cm[m]) m = t;
            if (cm[m] < 0.30f) tau = m; else return 0.0f;
        }

        float better = (float) tau;
        if (tau > 1 && tau < tauMax)
        {
            const float s0 = cm[tau - 1], s1 = cm[tau], s2 = cm[tau + 1];
            const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
            if (std::abs (denom) > 1e-9f) better += (s0 - s2) / denom;
        }

        confidence = std::max (0.0f, 1.0f - cm[tau]);
        return (float) sr / better;
    }

private:
    double sr = 44100.0;
    int tauMin = 44, tauMax = 630, window = 1102;
    int ringSize = 2048, mask = 2047, writePos = 0;
    float threshold = 0.15f;
    std::vector<float> ring, frame, cmnd;
};
