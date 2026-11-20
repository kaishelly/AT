#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Pitch-synchronous two-tap overlap-add shifter.
// Window length W is set to ~2 periods of the detected pitch, so the two taps
// (offset by W/2 = one period) stay phase-aligned on voiced material.
class PitchShifter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        int size = 1;
        const int needed = (int) (0.1 * sampleRate) + 8;
        while (size < needed) size <<= 1;
        mask = size - 1;
        buffers.assign ((size_t) numChannels, std::vector<float> ((size_t) size, 0.0f));
        writePos = 0;
        phase = 0.0f;
    }

    void reset()
    {
        for (auto& b : buffers) std::fill (b.begin(), b.end(), 0.0f);
        phase = 0.0f;
    }

    // in -> wetOut (pitch shifted by `ratio`), dryOut (input delayed to align with wet)
    void process (const float* in, float* wetOut, float* dryOut, int numCh, float ratio, float W)
    {
        for (int c = 0; c < numCh; ++c)
            buffers[(size_t) c][(size_t) writePos] = in[c];

        phase += (1.0f - ratio) / W;
        phase -= std::floor (phase);

        const float p1 = phase;
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float g1 = 0.5f - 0.5f * std::cos (6.2831853f * p1);
        const float g2 = 1.0f - g1;
        const float d1 = p1 * W + 2.0f;
        const float d2 = p2 * W + 2.0f;
        const float dd = 0.5f * W + 2.0f;

        for (int c = 0; c < numCh; ++c)
        {
            wetOut[c] = g1 * read (c, d1) + g2 * read (c, d2);
            dryOut[c] = read (c, dd);
        }
        writePos = (writePos + 1) & mask;
    }

private:
    float read (int c, float delay) const
    {
        const int   di   = (int) delay;
        const float frac = delay - (float) di;
        const auto& b    = buffers[(size_t) c];
        const float y0 = b[(size_t) ((writePos - di - 2) & mask)];
        const float y1 = b[(size_t) ((writePos - di - 1) & mask)];
        const float y2 = b[(size_t) ((writePos - di)     & mask)];
        const float y3 = b[(size_t) ((writePos - di + 1) & mask)];
        const float x  = 1.0f - frac;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * x + c2) * x + c1) * x + y1;
    }

    std::vector<std::vector<float>> buffers;
    int mask = 0, writePos = 0;
    float phase = 0.0f;
};
