// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Filter.h"
#include <cmath>

namespace dsp
{
    struct DcBlocker
    {
        float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
        void set (double fs) { r = static_cast<float> (1.0 - 6.283185307179586 * 12.0 / fs); }
        float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0.0f; }
    };

    struct OnePoleLP
    {
        float a = 1.0f, z = 0.0f;
        void set (float fc, double fs) { a = 1.0f - static_cast<float> (std::exp (-6.283185307179586 * fc / fs)); }
        float process (float x) { z += a * (x - z); return z; }
        void reset() { z = 0.0f; }
    };

    // Transparent below 0.8, then a smooth knee to 1.0.
    inline float softLimit (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.8f) return x;
        const float over = (a - 0.8f) / 0.2f;
        const float y = 0.8f + 0.2f * (over / (1.0f + over));
        return x < 0.0f ? -y : y;
    }
}

// Sine wave folder: FOLD drives the signal into a sine transfer curve that folds
// it back on itself; SYM shifts it for even harmonics. FOLD 0 = bypass.
class WaveFolder
{
public:
    void setSampleRate (double fs) { dc.set (fs); }
    void set (float amount, float symmetry, float mixAmount)
    {
        gain = 1.0f + amount * 7.0f;
        offset = symmetry * 0.9f;
        mix = mixAmount * (amount * 12.0f < 1.0f ? amount * 12.0f : 1.0f);   // fades in from 0
        offsetOut = std::sin (offset * 1.5707963f);
    }
    float process (float x)
    {
        if (mix <= 0.0f) return x;
        const float wet = (std::sin ((x * gain + offset) * 1.5707963f) - offsetOut) * 0.8f;
        return x + (dc.process (wet) - x) * mix;
    }
    void reset() { dc.reset(); }

private:
    float gain = 1.0f, offset = 0.0f, offsetOut = 0.0f, mix = 0.0f;
    dsp::DcBlocker dc;
};

// Distortion before the output: SOFT (tanh), HARD (clip), FUZZ (asymmetric, gated
// edge), TUBE (asymmetric soft), CRUSH (bits + sample rate). TONE is a low-pass.
class Distortion
{
public:
    enum Type { Soft, Hard, Fuzz, Tube, Crush };

    void setSampleRate (double fs) { sampleRate = fs; dc.set (fs); }
    void set (int newType, float drive, float tone, float mixAmount)
    {
        type = newType;
        pre = std::pow (10.0f, drive * 34.0f / 20.0f);           // 0 .. +34 dB
        makeup = 1.0f / (1.0f + drive * 1.3f);
        mix = mixAmount * (drive * 20.0f < 1.0f ? drive * 20.0f : 1.0f);
        lp.set (700.0f * std::pow (2.0f, tone * 4.8f), sampleRate);
        bits = std::pow (2.0f, 12.0f - drive * 9.0f);
        holdLength = 1 + static_cast<int> (drive * drive * 24.0f);
    }

    float process (float x)
    {
        if (mix <= 0.0f) return x;
        const float u = x * pre;
        float y;
        switch (type)
        {
            case Hard:  y = u > 1.0f ? 1.0f : (u < -1.0f ? -1.0f : u); break;
            case Fuzz:
            {
                const float a = dsp::fastTanh (u + 0.35f) - 0.3364f;   // tanh(0.35)
                y = a > 0.0f ? a : a * 0.45f;
                y = dsp::fastTanh (y * 2.0f);
                break;
            }
            case Tube:  y = u >= 0.0f ? 1.0f - std::exp (-u) : -(1.0f - std::exp (u * 1.4f)) / 1.4f; break;
            case Crush:
            {
                if (++holdCount >= holdLength) { holdCount = 0; held = std::round (dsp::fastTanh (u * 0.5f) * bits) / bits; }
                y = held * 0.9f;
                break;
            }
            case Soft:
            default:    y = dsp::fastTanh (u); break;
        }
        const float wet = dc.process (lp.process (y)) * makeup;
        return x + (wet - x) * mix;
    }
    void reset() { lp.reset(); dc.reset(); held = 0.0f; holdCount = 0; }

private:
    double sampleRate = 192000.0;
    int type = Soft, holdLength = 1, holdCount = 0;
    float pre = 1.0f, makeup = 1.0f, mix = 0.0f, bits = 4096.0f, held = 0.0f;
    dsp::OnePoleLP lp;
    dsp::DcBlocker dc;
};
