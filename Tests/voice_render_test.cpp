// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// DSP-only test (no JUCE): renders the acid voice through a few settings to WAV files
// and checks for NaN / runaway levels. Build:
//   g++ -std=c++17 -O2 -I../Source voice_render_test.cpp -o voice_render_test
// Run with an output folder:  ./voice_render_test out/

#include "AcidVoice.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace
{
    constexpr double fs = 48000.0;
    constexpr int os = 4;

    void writeWav (const std::string& path, const std::vector<float>& x)
    {
        FILE* f = std::fopen (path.c_str(), "wb");
        if (! f) return;
        const uint32_t n = static_cast<uint32_t> (x.size()), bytes = n * 2;
        auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
        auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
        std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
        w32 (16); w16 (1); w16 (1); w32 (48000); w32 (96000); w16 (2); w16 (16);
        std::fwrite ("data", 1, 4, f); w32 (bytes);
        for (float v : x) { const float c = v > 1 ? 1 : (v < -1 ? -1 : v); w16 (static_cast<uint16_t> (static_cast<int16_t> (c * 32767))); }
        std::fclose (f);
    }

    // simple windowed-sinc decimator (test only; the plugin uses JUCE's oversampler)
    struct Decimator
    {
        std::vector<float> h, hist;
        size_t pos = 0;
        Decimator()
        {
            const int taps = 63;
            for (int i = 0; i < taps; ++i)
            {
                const double t = i - (taps - 1) / 2.0, fc = 0.45 / os;
                const double sinc = t == 0 ? 2 * fc : std::sin (2 * M_PI * fc * t) / (M_PI * t);
                h.push_back (static_cast<float> (sinc * (0.42 - 0.5 * std::cos (2 * M_PI * i / (taps - 1)) + 0.08 * std::cos (4 * M_PI * i / (taps - 1)))));
            }
            hist.assign (h.size(), 0.0f);
        }
        float process (const float* in)
        {
            for (int k = 0; k < os; ++k) { hist[pos] = in[k]; pos = (pos + 1) % hist.size(); }
            float y = 0;
            for (size_t i = 0; i < h.size(); ++i) y += h[i] * hist[(pos + i) % hist.size()];
            return y;
        }
    };

    struct Step { int note; bool gate, slide, accent; };

    // A classic-style 16-step line at 130 BPM, played 4 times.
    std::vector<float> renderPattern (const VoiceSettings& s, float seconds = 7.4f)
    {
        const Step pat[16] = { {36,1,0,1},{36,1,0,0},{48,1,1,0},{36,1,0,0},{39,1,0,1},{36,0,0,0},{43,1,1,0},{46,1,0,0},
                               {36,1,0,1},{48,1,0,0},{36,1,1,0},{41,1,0,1},{36,1,0,0},{34,1,1,0},{36,1,0,1},{38,1,0,0} };
        AcidVoice v;
        v.prepare (fs, os);
        v.setSettings (s);
        Decimator dec;
        const double stepLen = 60.0 / 130.0 / 4.0 * fs;
        const int total = static_cast<int> (seconds * fs);
        std::vector<float> out;
        float buf[os];
        int prevStep = -1;
        bool prevSlide = false;
        for (int n = 0; n < total; ++n)
        {
            const int step = static_cast<int> (n / stepLen);
            const double inStep = n - step * stepLen;
            const auto& st = pat[step % 16];
            if (step != prevStep)
            {
                prevStep = step;
                if (st.gate) v.noteOn (st.note, st.accent, prevSlide);
                else if (v.isGateOn()) v.noteOff();
                prevSlide = st.gate && st.slide && pat[(step + 1) % 16].gate;
            }
            if (v.isGateOn() && ! prevSlide && inStep > stepLen * 0.55) v.noteOff();
            v.render (buf);
            out.push_back (dec.process (buf));
        }
        return out;
    }

    bool check (const char* name, const std::vector<float>& x, float minPeak = 0.02f)
    {
        float peak = 0; double sum = 0; bool bad = false;
        for (float v : x) { if (! std::isfinite (v)) bad = true; peak = std::fmax (peak, std::fabs (v)); sum += v * v; }
        const double rms = std::sqrt (sum / x.size());
        const bool ok = ! bad && peak < 1.6f && peak > minPeak;
        std::printf ("%-24s peak %.3f  rms %.3f  %s\n", name, peak, rms, ok ? "ok" : "FAIL");
        return ok;
    }
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "./";
    int failures = 0;
    auto run = [&] (const char* name, std::function<void (VoiceSettings&)> tweak)
    {
        VoiceSettings s;
        s.osc[0].on = true;
        tweak (s);
        auto x = renderPattern (s);
        if (! check (name, x)) ++failures;
        writeWav (dir + name + ".wav", x);
    };

    run ("01_default_303", [] (VoiceSettings&) {});
    run ("02_square", [] (VoiceSettings& s) { s.osc[0].wave = 1; });
    run ("03_reso_max", [] (VoiceSettings& s) { s.reso = 1.0f; s.envMod = 0.8f; });
    run ("04_ladder24", [] (VoiceSettings& s) { s.filterMode = 1; s.reso = 0.8f; });
    run ("05_svf12", [] (VoiceSettings& s) { s.filterMode = 2; s.reso = 0.9f; });
    run ("06_three_osc", [] (VoiceSettings& s) {
        s.osc[1] = { true, 1, 0, 0, 7, 0.3f, 0.6f, false };
        s.osc[2] = { true, 3, -1, 0, 0, 0, 0.7f, false }; });
    run ("07_wt_formant_lfo", [] (VoiceSettings& s) {
        s.osc[0].wave = 4; s.osc[0].lfo = true; s.lfoShape = 1.0f; s.lfoRateHz = 1.5f; s.cutoff = 1500; });
    run ("08_wt_all_tables", [] (VoiceSettings& s) { s.osc[0].wave = 9; s.osc[0].shape = 0.6f; s.cutoff = 3000; });
    run ("09_fold", [] (VoiceSettings& s) { s.foldAmount = 0.6f; s.foldSym = 0.3f; });
    run ("10_dist_fuzz", [] (VoiceSettings& s) { s.distType = 2; s.distDrive = 0.8f; });
    run ("11_dist_crush", [] (VoiceSettings& s) { s.distType = 4; s.distDrive = 0.7f; });
    run ("12_dist_soft_full", [] (VoiceSettings& s) { s.distType = 0; s.distDrive = 1.0f; s.reso = 0.9f; });
    run ("13_cutoff_extremes", [] (VoiceSettings& s) { s.cutoff = 12000; s.reso = 1.0f; s.envMod = 1.0f; s.drive = 1.0f; });
    run ("14_lfo_pitch_sh", [] (VoiceSettings& s) { s.osc[0].lfo = true; s.lfoWave = 5; s.lfoPitch = 12; s.lfoRateHz = 8; });

    // every wave at a high note: no NaNs, nothing silent
    for (int w = 0; w < 12; ++w)
    {
        AcidVoice v; v.prepare (fs, os);
        VoiceSettings s; s.osc[0].on = true; s.osc[0].wave = w; s.osc[0].shape = 0.5f; s.cutoff = 12000; s.envMod = 0;
        v.setSettings (s);
        v.noteOn (96, false, false);
        std::vector<float> x; float buf[os]; Decimator d;
        for (int n = 0; n < 4800; ++n) { v.render (buf); x.push_back (d.process (buf)); }
        char name[32]; std::snprintf (name, sizeof name, "wave_%02d_high_note", w);
        if (! check (name, x)) ++failures;
    }

    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
