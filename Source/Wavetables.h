// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>

// The 8 digital wavetables, generated in code at startup (no sample files).
// Each table has 8 frames that SHAPE morphs through, and each frame is stored
// band-limited at 10 levels (1024, 512 ... 2 harmonics) so high notes don't alias.
// JUCE-free so it can be unit tested.
class Wavetables
{
public:
    static constexpr int numTables = 8;
    static constexpr int numFrames = 8;
    static constexpr int numLevels = 10;     // level L keeps harmonics 1 .. (1024 >> L)
    static constexpr int size = 2048;        // samples per cycle
    static constexpr int stride = size + 1;  // + guard sample for interpolation

    enum Table { Formant, Organ, Sync, Fold, Bits, Phase, Glass, Harmonic };

    static const Wavetables& get()
    {
        static const Wavetables instance;    // built once, on first use (thread-safe)
        return instance;
    }

    const float* row (int table, int frame, int level) const
    {
        return data.data() + static_cast<size_t> (((table * numFrames + frame) * numLevels + level) * stride);
    }

    // Pick the level whose highest harmonic stays under maxHarmonics.
    static int levelFor (float maxHarmonics)
    {
        int level = 0, harmonics = size / 2;
        while (level < numLevels - 1 && static_cast<float> (harmonics) > maxHarmonics)
        {
            harmonics >>= 1;
            ++level;
        }
        return level;
    }

private:
    std::vector<float> data;
    static constexpr double twoPi = 6.283185307179586;

    Wavetables()
    {
        data.assign (static_cast<size_t> (numTables * numFrames * numLevels * stride), 0.0f);
        std::vector<double> cycle (size);
        std::vector<std::complex<double>> spectrum (size), work (size);

        for (int t = 0; t < numTables; ++t)
            for (int f = 0; f < numFrames; ++f)
            {
                const double morph = static_cast<double> (f) / (numFrames - 1);
                for (int n = 0; n < size; ++n)
                    cycle[static_cast<size_t> (n)] = generate (t, morph, static_cast<double> (n) / size);

                for (int n = 0; n < size; ++n)
                    spectrum[static_cast<size_t> (n)] = { cycle[static_cast<size_t> (n)], 0.0 };
                fft (spectrum, false);

                float peak = 0.0f;
                for (int level = 0; level < numLevels; ++level)
                {
                    const int maxH = (size / 2) >> level;
                    for (int k = 0; k < size; ++k)
                    {
                        const int h = k <= size / 2 ? k : size - k;
                        work[static_cast<size_t> (k)] = (h == 0 || h > maxH) ? std::complex<double> (0.0, 0.0)
                                                                             : spectrum[static_cast<size_t> (k)];
                    }
                    fft (work, true);
                    float* out = const_cast<float*> (row (t, f, level));
                    for (int n = 0; n < size; ++n)
                        out[n] = static_cast<float> (work[static_cast<size_t> (n)].real() / size);
                    out[size] = out[0];
                    if (level == 0)
                        for (int n = 0; n < size; ++n)
                            peak = std::max (peak, std::abs (out[n]));
                }

                // normalise every frame (all its levels by the same factor) to peak 1
                const float g = peak > 1.0e-6f ? 1.0f / peak : 1.0f;
                for (int level = 0; level < numLevels; ++level)
                {
                    float* out = const_cast<float*> (row (t, f, level));
                    for (int n = 0; n < stride; ++n)
                        out[n] *= g;
                }
            }
    }

    static double gauss (double x, double centre, double width)
    {
        const double d = (x - centre) / width;
        return std::exp (-d * d);
    }

    // One cycle of table t at morph position m (0..1), phase p (0..1).
    static double generate (int t, double m, double p)
    {
        switch (t)
        {
            case Formant:
            {
                // vowel formants walking a -> e -> i -> o -> u (positions as harmonic numbers
                // of a ~110 Hz fundamental)
                const double f1s[] = { 7.0, 4.5, 2.6, 4.5, 2.8 };
                const double f2s[] = { 11.0, 17.0, 21.0, 8.0, 6.5 };
                const double pos = m * 4.0;
                const int i = std::min (3, static_cast<int> (pos));
                const double fr = pos - i;
                const double f1 = f1s[i] + (f1s[i + 1] - f1s[i]) * fr;
                const double f2 = f2s[i] + (f2s[i + 1] - f2s[i]) * fr;
                double y = 0.0;
                for (int h = 1; h <= 48; ++h)
                {
                    const double a = gauss (h, f1, 1.6) + 0.6 * gauss (h, f2, 2.2) + 0.25 / h;
                    y += a * std::sin (twoPi * h * p);
                }
                return y;
            }
            case Organ:
            {
                // drawbar registrations: 16' 8' 5 1/3' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1'
                static const int harm[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                static const double regs[5][9] = {
                    { 8, 8, 8, 0, 0, 0, 0, 0, 0 },
                    { 8, 8, 6, 8, 0, 0, 0, 0, 0 },
                    { 8, 4, 8, 6, 4, 0, 3, 0, 0 },
                    { 6, 8, 8, 8, 6, 8, 0, 4, 6 },
                    { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
                const double pos = m * 4.0;
                const int i = std::min (3, static_cast<int> (pos));
                const double fr = pos - i;
                double y = 0.0;
                for (int d = 0; d < 9; ++d)
                {
                    const double a = (regs[i][d] + (regs[i + 1][d] - regs[i][d]) * fr) / 8.0;
                    y += a * std::sin (twoPi * harm[d] * p);
                }
                return y;
            }
            case Sync:
            {
                // hard-synced saw, slave ratio 1 -> 8
                const double r = 1.0 + m * 7.0;
                const double s = p * r;
                return 2.0 * (s - std::floor (s)) - 1.0;
            }
            case Fold:
            {
                const double drive = 1.0 + m * 6.0;
                return std::sin (drive * std::sin (twoPi * p) * 1.5707963267948966);
            }
            case Bits:
            {
                // sine crushed from 6 bits down to 1 bit, with sample-and-hold steps
                const double bits = 6.0 - m * 5.0;
                const double q = std::pow (2.0, bits - 1.0);
                const double steps = 64.0 - m * 48.0;
                const double held = std::floor (p * steps) / steps;
                return std::round (std::sin (twoPi * held) * q) / q;
            }
            case Phase:
            {
                // phase-distortion "resonance": a windowed cosine burst that rises in pitch
                const double k = 1.0 + m * 14.0;
                return (1.0 - p) * std::cos (twoPi * p * k);
            }
            case Glass:
            {
                // 2-operator FM, ratio 3, index 0 -> 6
                const double index = m * 6.0;
                return std::sin (twoPi * p + index * std::sin (twoPi * 3.0 * p));
            }
            case Harmonic:
            default:
            {
                // a band of harmonics sweeping upward over a quiet fundamental
                const double centre = 1.0 + m * 30.0;
                double y = 0.5 * std::sin (twoPi * p);
                for (int h = 1; h <= 48; ++h)
                    y += gauss (h, centre, 2.0) * std::sin (twoPi * h * p);
                return y;
            }
        }
    }

    // In-place radix-2 FFT (size is a power of two). inverse = no 1/N scaling.
    static void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = twoPi / static_cast<double> (len) * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }
};
