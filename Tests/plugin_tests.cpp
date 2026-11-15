// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Plugin-level tests: MIDI modes (PATTERN / FREE), slot triggers, transpose, RETRIG/SYNC
// starts, live + step recording, CC learn, state round trip, every factory patch,
// and an editor screenshot. Usage: YF21Tests <output folder>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

namespace
{
    int failures = 0, checks = 0;
    void check (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) ++failures;
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    }

    struct TestPlayHead : juce::AudioPlayHead
    {
        bool playing = false, looping = false, reportLoop = true;
        double bpm = 120.0, ppq = 0.0, loopStart = 0.0, loopEnd = 4.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (playing);
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            if (looping && reportLoop)
            {
                p.setIsLooping (true);
                p.setLoopPoints (LoopPoints { loopStart, loopEnd });
            }
            return p;
        }
    };

    struct Ev { juce::int64 sample; juce::MidiMessage msg; };

    constexpr double fs = 48000.0;
    constexpr int block = 512;

    // Renders `seconds` of audio (mono, left channel). If a playhead is given and playing,
    // its ppq advances with the audio.
    std::vector<float> render (BasslineAudioProcessor& p, double seconds, std::vector<Ev> events = {},
                               TestPlayHead* ph = nullptr, std::function<void (juce::int64)> perBlock = {})
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, block);
        const auto total = static_cast<juce::int64> (seconds * fs);
        for (juce::int64 t = 0; t < total; t += block)
        {
            if (perBlock) perBlock (t);
            juce::MidiBuffer midi;
            for (auto& e : events)
                if (e.sample >= t && e.sample < t + block)
                    midi.addEvent (e.msg, static_cast<int> (e.sample - t));
            buf.clear();
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
            if (ph != nullptr && ph->playing)
            {
                ph->ppq += block / fs * ph->bpm / 60.0;
                if (ph->looping && ph->ppq >= ph->loopEnd) ph->ppq -= ph->loopEnd - ph->loopStart;   // wraps mid-block
            }
        }
        return out;
    }

    float rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
    {
        if (to == 0 || to > x.size()) to = x.size();
        double s = 0; for (size_t i = from; i < to; ++i) s += x[i] * x[i];
        return to > from ? static_cast<float> (std::sqrt (s / (to - from))) : 0.0f;
    }
    bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }
    float peak (const std::vector<float>& x) { float m = 0; for (float v : x) m = std::max (m, std::abs (v)); return m; }

    // fundamental by autocorrelation (30..1000 Hz)
    double pitchOf (const std::vector<float>& x, size_t from, size_t len)
    {
        // first lag whose correlation reaches 90% of the best (avoids picking 2 or 3 periods)
        const int lo = static_cast<int> (fs / 1000), hi = static_cast<int> (fs / 30);
        std::vector<double> c (static_cast<size_t> (hi), 0.0);
        double best = 0;
        for (int lag = lo; lag < hi; ++lag)
        {
            double s = 0;
            for (size_t i = 0; i < len; ++i) s += x[from + i] * x[from + i + static_cast<size_t> (lag)];
            c[static_cast<size_t> (lag)] = s;
            best = std::max (best, s);
        }
        for (int lag = lo + 1; lag < hi - 1; ++lag)
            if (c[static_cast<size_t> (lag)] >= 0.9 * best && c[static_cast<size_t> (lag)] >= c[static_cast<size_t> (lag - 1)]
                && c[static_cast<size_t> (lag)] >= c[static_cast<size_t> (lag + 1)])
                return fs / lag;
        return 0.0;
    }

    void setParam (BasslineAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void writeWav (const juce::File& f, const std::vector<float>& x)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (auto* w = wav.createWriterFor (os.get(), fs, 1, 16, {}, 0))
        {
            os.release();
            std::unique_ptr<juce::AudioFormatWriter> writer (w);
            const float* ch = x.data();
            writer->writeFromFloatArrays (&ch, 1, static_cast<int> (x.size()));
        }
    }

    std::unique_ptr<BasslineAudioProcessor> fresh()
    {
        auto p = std::make_unique<BasslineAudioProcessor>();
        p->setPlayConfigDetails (0, 2, fs, block);
        p->prepareToPlay (fs, block);
        return p;
    }

    // all steps C, gate long, no slide/accent: easy to measure pitch
    void plainPattern (BasslineAudioProcessor& p, int slot)
    {
        Pat::Pattern pat;
        for (auto& s : pat.steps) s = {};
        p.bank.store (slot, pat);
        setParam (p, "seq_gate", 1.0f);
        setParam (p, "flt_cutoff", 3000.0f);
        setParam (p, "flt_reso", 0.0f);
        setParam (p, "flt_envmod", 0.0f);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    // ---------------------------------------------------------------- silence + defaults
    {
        auto p = fresh();
        auto x = render (*p, 0.5);
        check (finite (x) && peak (x) < 1e-6f, "silent with no input");
        check (p->slotNames[1] == "SUPERCRUISE", "bank starts with factory patterns (slot 2 = SUPERCRUISE)");
    }

    // ---------------------------------------------------------------- PATTERN mode: trigger key plays a slot
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);   // internal clock
        setParam (*p, "seq_tempo", 125.0f);
        const auto x = render (*p, 4.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) },            // C0 = slot 1
                                          { static_cast<juce::int64> (3.0 * fs), juce::MidiMessage::noteOff (1, 24) } });
        check (finite (x), "pattern render finite");
        check (rms (x, 0, static_cast<size_t> (3.0 * fs)) > 0.05f, "holding C0 plays slot 1");
        check (rms (x, static_cast<size_t> (3.3 * fs)) < 0.002f, "releasing C0 stops it");
        writeWav (outDir.getChildFile ("pattern_slot1.wav"), x);
    }

    // ---------------------------------------------------------------- trigger octave option
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        p->setSlotKeysFrom (0);   // C-2 .. D#-1
        auto x = render (*p, 1.0, { { 0, juce::MidiMessage::noteOn (1, 1, 0.8f) } });   // C#-2 = slot 2
        check (p->getPlayingSlot() == 1, "trigger keys from C-2: note 1 plays slot 2");
        check (p->isTriggeredByMidi(), "...reported as MIDI-triggered");
        auto q = fresh();
        setParam (*q, "seq_sync", 0.0f);
        q->setSlotKeysFrom (17);   // F-1: any note, not just octaves
        render (*q, 0.5, { { 0, juce::MidiMessage::noteOn (1, 17 + 15, 0.8f) } });
        check (q->getPlayingSlot() == 15, "trigger keys from F-1: note 32 plays slot 16");
        render (*q, 0.2, { { 0, juce::MidiMessage::noteOff (1, 32) }, { 10, juce::MidiMessage::noteOn (1, 16, 0.8f) } });
        check (q->getPlayingSlot() < 0, "...and E-1 (just below) is an ordinary note");
    }

    // ---------------------------------------------------------------- transpose while a slot is held
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        plainPattern (*p, 0);
        auto a = render (*p, 1.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        const double f0 = pitchOf (a, static_cast<size_t> (0.5 * fs), 4096);
        auto p2 = fresh();
        setParam (*p2, "seq_sync", 0.0f);
        plainPattern (*p2, 0);
        auto b = render (*p2, 1.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) },
                                     { 0, juce::MidiMessage::noteOn (1, 67, 0.8f) } });   // G3 = +7
        const double f1 = pitchOf (b, static_cast<size_t> (0.5 * fs), 4096);
        const double semis = 12.0 * std::log2 (f1 / f0);
        check (std::abs (f0 - 65.4) < 2.0, "slot C plays C1 (65.4 Hz), measured " + juce::String (f0, 1));
        check (std::abs (semis - 7.0) < 0.3, "holding G3 transposes +7 st, measured " + juce::String (semis, 2));
        check (p2->getTranspose() == 7, "transpose reported to the UI");
    }

    // ---------------------------------------------------------------- FREE mode: keys play, overlap = slide
    {
        auto p = fresh();
        setParam (*p, "seq_mode", 1.0f);
        setParam (*p, "flt_cutoff", 3000.0f); setParam (*p, "flt_envmod", 0.0f); setParam (*p, "flt_reso", 0.0f);
        setParam (*p, "amp_sustain", 1.0f);
        // C0 (24) would trigger a slot in PATTERN mode; in FREE mode it's just a note
        const auto x = render (*p, 2.0, { { 0, juce::MidiMessage::noteOn (1, 36, 0.6f) },
                                          { static_cast<juce::int64> (0.5 * fs), juce::MidiMessage::noteOn (1, 48, 0.6f) },
                                          { static_cast<juce::int64> (0.6 * fs), juce::MidiMessage::noteOff (1, 36) },
                                          { static_cast<juce::int64> (1.2 * fs), juce::MidiMessage::noteOff (1, 48) },
                                          { static_cast<juce::int64> (1.4 * fs), juce::MidiMessage::noteOn (1, 24, 0.6f) } });
        check (p->getPlayingSlot() < 0, "FREE mode: C0 doesn't trigger a slot");
        const double fa = pitchOf (x, static_cast<size_t> (0.3 * fs), 4096);
        const double fb = pitchOf (x, static_cast<size_t> (0.9 * fs), 4096);
        check (std::abs (12.0 * std::log2 (fb / fa) - 12.0) < 0.3, "FREE mode: overlapping notes glide to the new pitch");
        // the slide: right after the second note, pitch is between the two
        const double mid = pitchOf (x, static_cast<size_t> (0.5 * fs) + 300, 1200);
        check (mid > fa * 1.05 && mid < fb * 0.97, "...and it glides rather than jumps (" + juce::String (mid, 1) + " Hz mid-slide)");
        check (rms (x, static_cast<size_t> (1.45 * fs)) > 0.02f, "FREE mode: C0 plays a note");
    }

    // ---------------------------------------------------------------- host sync: SYNC vs RETRIG start
    for (int startMode = 0; startMode < 2; ++startMode)
    {
        auto p = fresh();
        TestPlayHead ph;
        ph.playing = true; ph.bpm = 120.0; ph.ppq = 1.25;   // beat 2, second 16th -> absolute step 5
        p->setPlayHead (&ph);
        setParam (*p, "seq_start", static_cast<float> (startMode));
        int firstStep = -1;
        render (*p, 0.2, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } }, &ph,
                [&] (juce::int64) { if (firstStep < 0 && p->getPlayingStep() >= 0) firstStep = p->getPlayingStep(); });
        render (*p, 0.01, {}, &ph, [&] (juce::int64) { if (firstStep < 0) firstStep = p->getPlayingStep(); });
        if (startMode == 1) check (firstStep == 5, "SYNC start plays the step under the DAW's position (step 6), got " + juce::String (firstStep + 1));
        else                check (firstStep == 0, "RETRIG start plays step 1, got " + juce::String (firstStep + 1));
        p->setPlayHead (nullptr);
    }

    // ---------------------------------------------------------------- follows the host tempo
    {
        auto p = fresh();
        TestPlayHead ph; ph.playing = true; ph.bpm = 140.0;
        p->setPlayHead (&ph);
        render (*p, 0.05, {}, &ph);
        check (std::abs (p->getDisplayBpm() - 140.0f) < 0.01f && p->isHostSynced(), "SYNC on: shows the host's 140 BPM");
        setParam (*p, "seq_sync", 0.0f);
        render (*p, 0.05, {}, &ph);
        check (std::abs (p->getDisplayBpm() - 125.0f) < 0.01f, "SYNC off: internal tempo");
        p->setPlayHead (nullptr);
    }

    // ---------------------------------------------------------------- step record
    {
        auto p = fresh();
        p->setEditSlot (5);
        p->clearEditSlot();
        p->setRecord (true);
        const auto ev = std::vector<Ev> {
            { 100, juce::MidiMessage::noteOn (1, 39, 0.6f) },      // D#1 -> D#, step 1   (note: UI-free MIDI, FREE mode below)
            { 4000, juce::MidiMessage::noteOff (1, 39) },
            { 8000, juce::MidiMessage::noteOn (1, 55, 1.0f) },     // G2 accent -> G + octave up, step 2
            { 9000, juce::MidiMessage::noteOn (1, 58, 0.6f) },     // overlap -> step 2 gets SLIDE, A#^ step 3
            { 9500, juce::MidiMessage::noteOff (1, 55) },
            { 12000, juce::MidiMessage::noteOff (1, 58) } };
        setParam (*p, "seq_mode", 1.0f);   // FREE so D#1 isn't a trigger key
        render (*p, 0.5, ev);
        const auto s0 = p->bank.get (5, 0), s1 = p->bank.get (5, 1), s2 = p->bank.get (5, 2);
        check (s0.gate && s0.note == 3 && ! s0.up && ! s0.accent, "step record: D#1 -> step 1 = D#");
        check (s1.gate && s1.note == 7 && s1.up && s1.accent && s1.slide, "step record: G2 accented + overlapped -> step 2 = G, oct up, accent, slide");
        check (s2.gate && s2.note == 10 && s2.up && ! s2.slide, "step record: A#2 -> step 3 = A#, oct up");
        check (p->getEditStep() == 3, "step record cursor moved to step 4");
    }

    // ---------------------------------------------------------------- live record (quantised)
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        setParam (*p, "seq_tempo", 120.0f);   // 16th = 6000 samples
        setParam (*p, "seq_mode", 1.0f);
        p->setEditSlot (6);
        p->bank.setLength (6, 16);
        p->clearEditSlot();
        p->setPlay (true);
        p->setRecord (true);
        // note just after step 5 (index 4 starts at 24000) and just before step 9 (48000)
        render (*p, 1.2, { { 24900, juce::MidiMessage::noteOn (1, 43, 0.6f) }, { 27000, juce::MidiMessage::noteOff (1, 43) },
                           { 47200, juce::MidiMessage::noteOn (1, 41, 1.0f) }, { 50000, juce::MidiMessage::noteOff (1, 41) } });
        const auto s4 = p->bank.get (6, 4), s8 = p->bank.get (6, 8);
        check (s4.gate && s4.note == 7, "live record: late note lands on step 5");
        check (s8.gate && s8.note == 5 && s8.accent, "live record: early note lands on step 9 (accented)");
        int gates = 0; for (int i = 0; i < 16; ++i) gates += p->bank.get (6, i).gate ? 1 : 0;
        check (gates == 2, "live record: nothing else written");
    }

    // ---------------------------------------------------------------- 303 slide in the sequencer
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        setParam (*p, "seq_tempo", 120.0f);
        setParam (*p, "seq_gate", 0.5f);
        Pat::Pattern pat = Pat::Pattern::empty();
        pat.steps[0] = {}; pat.steps[0].slide = true;          // C with slide
        pat.steps[1] = {}; pat.steps[1].note = 12;             // C' : glides in, gate held across
        p->bank.store (0, pat);
        setParam (*p, "amp_sustain", 1.0f);
        auto x = render (*p, 0.5, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        // at the end of step 1 (gate 50%), a non-slide note would be off; with slide it keeps sounding
        check (rms (x, 4500, 6000) > 0.05f, "slide step holds its gate into the next step");
        check (rms (x, 6000 + 3500, 12000) < rms (x, 6000, 8000) * 0.5f, "step after the slide closes at its own gate length");
    }

    // ---------------------------------------------------------------- stuck notes (bug report 2026-11-14)
    {
        // a higher note starts just before the slot key and ends while the slot is held
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        setParam (*p, "amp_sustain", 1.0f);
        auto x = render (*p, 3.0, { { 0,     juce::MidiMessage::noteOn (1, 48, 0.8f) },      // plays the synth
                                    { 200,   juce::MidiMessage::noteOn (1, 24, 0.8f) },      // slot 1 starts
                                    { 24000, juce::MidiMessage::noteOff (1, 48) },           // ends while slot held
                                    { 96000, juce::MidiMessage::noteOff (1, 24) } });        // slot ends at 2 s
        check (rms (x, static_cast<size_t> (2.3 * fs)) < 0.002f, "no drone: note that began before the slot key is released");
        auto y = render (*p, 1.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        check (rms (y, 12000) > 0.05f, "...and the next slot key still plays the pattern");
    }
    {
        // a note pressed in FREE mode is released after switching to PATTERN mode
        auto p = fresh();
        setParam (*p, "seq_mode", 1.0f);
        setParam (*p, "amp_sustain", 1.0f);
        render (*p, 0.3, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        setParam (*p, "seq_mode", 0.0f);
        auto x = render (*p, 1.0, { { 0, juce::MidiMessage::noteOff (1, 24) } });
        check (rms (x, 12000) < 0.002f && p->getPlayingSlot() < 0, "no drone: note-off after a PLAY MODE change releases the note");
    }
    {
        // the DAW stops without sending a note-off for the slot key
        auto p = fresh();
        TestPlayHead ph; ph.playing = true; ph.bpm = 120.0;
        p->setPlayHead (&ph);
        setParam (*p, "amp_sustain", 1.0f);
        render (*p, 1.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } }, &ph);
        ph.playing = false;
        auto x = render (*p, 1.0, {}, &ph);
        check (p->getPlayingSlot() < 0, "DAW stop ends a slot held by a trigger key");
        check (rms (x, 12000) < 0.002f, "...and the voice is released");
        p->setPlayHead (nullptr);
    }
    {
        // the DAW loops back while a long gate is open: the gate closes instead of holding until the old position
        auto p = fresh();
        TestPlayHead ph; ph.playing = true; ph.bpm = 120.0; ph.ppq = 3.5;   // step 15 of bar 1
        p->setPlayHead (&ph);
        Pat::Pattern pat = Pat::Pattern::empty();
        pat.steps[14] = {};                                  // only step 15 plays
        p->bank.store (0, pat);
        setParam (*p, "seq_gate", 1.0f);
        setParam (*p, "amp_sustain", 1.0f);
        render (*p, 0.06, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } }, &ph);
        ph.ppq = 0.0;                                        // loop back to the start
        auto x = render (*p, 0.5, {}, &ph);
        check (rms (x, 6000) < 0.002f, "DAW loop jump closes an open gate");
        p->setPlayHead (nullptr);
    }

    // ---------------------------------------------------------------- DAW loop: step 1 must play every pass (bug report)
    for (int variant = 0; variant < 6; ++variant)
    {
        // 0: DAW reports loop points, sends C0 off+on at the loop point   1: same, C0 held through
        // 2: no loop points reported, C0 off+on                           3: no loop points, C0 held through
        // 4/5: C0 off at the loop point, on at the start of the next audio block (DAWs that don't split
        //      blocks at the loop) - with / without loop points
        auto p = fresh();
        TestPlayHead ph; ph.playing = true; ph.bpm = 120.0; ph.looping = true; ph.reportLoop = variant < 2 || variant == 4;
        p->setPlayHead (&ph);
        Pat::Pattern pat = Pat::Pattern::empty();
        pat.steps[0] = {};                  // only step 1 plays
        p->bank.store (0, pat);
        setParam (*p, "amp_sustain", 1.0f);
        setParam (*p, "seq_gate", 0.5f);
        const juce::int64 bar = 96000;      // 1 bar at 120 BPM; 96000 / 512 blocks = loop point mid-block
        std::vector<Ev> ev { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } };
        if (variant % 2 == 0 || variant >= 4)
            for (int pass = 1; pass < 3; ++pass)
            {
                const juce::int64 on = variant >= 4 ? (bar * pass / block + 1) * block : bar * pass;
                ev.push_back ({ bar * pass, juce::MidiMessage::noteOff (1, 24) });
                ev.push_back ({ on, juce::MidiMessage::noteOn (1, 24, 0.8f) });
            }
        auto x = render (*p, 6.2, ev, &ph);
        const char* names[] = { "loop points, key re-sent", "loop points, key held", "no loop points, key re-sent",
                                "no loop points, key held", "loop points, key re-sent next block", "no loop points, key re-sent next block" };
        bool ok = true;
        for (int pass = 0; pass < 3; ++pass)
        {
            const auto from = static_cast<size_t> (bar * pass), mid = from + 1800;
            ok = ok && rms (x, from, mid) > 0.05f;                         // step 1 sounds (within ~10 ms)
            ok = ok && rms (x, from + 6000, from + 12000) < 0.01f;        // and nothing else
        }
        check (ok, juce::String ("DAW loop (") + names[variant] + "): step 1 plays on every pass");
        p->setPlayHead (nullptr);
    }

    // ---------------------------------------------------------------- switching slots with trigger keys
    for (int startMode = 0; startMode < 2; ++startMode)
    {
        auto p = fresh();
        TestPlayHead ph; ph.playing = true; ph.bpm = 120.0;
        p->setPlayHead (&ph);
        setParam (*p, "seq_start", static_cast<float> (startMode));
        p->setEditSlot (5);
        std::vector<int> seen;   // slots played, in order (stopped moments left out)
        // C0 (slot 1), then D0 (slot 3) with a gap, then E0 (slot 5) overlapping D0
        render (*p, 3.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) },     { 40000, juce::MidiMessage::noteOff (1, 24) },
                           { 48000, juce::MidiMessage::noteOn (1, 26, 0.8f) }, { 90000, juce::MidiMessage::noteOn (1, 28, 0.8f) },
                           { 96000, juce::MidiMessage::noteOff (1, 26) } }, &ph,
               [&] (juce::int64) { const int s = p->getPlayingSlot(); if (s >= 0 && (seen.empty() || seen.back() != s)) seen.push_back (s); });
        juce::String order; for (int v : seen) order << v + 1 << " ";
        const bool ok = seen == std::vector<int> { 0, 2, 4 };
        check (ok, juce::String (startMode ? "SYNC" : "RETRIG") + ": trigger keys switch slots (played: " + order.trim() + ")");
        check (p->getEditSlot() == 4, juce::String (startMode ? "SYNC" : "RETRIG") + ": the editor follows the triggered slot");
        p->setPlayHead (nullptr);
    }

    // ---------------------------------------------------------------- panic
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        setParam (*p, "amp_sustain", 1.0f); setParam (*p, "amp_release", 2.0f);
        p->setPlay (true);
        render (*p, 0.5, { { 0, juce::MidiMessage::noteOn (1, 48, 0.8f) }, { 100, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        p->panic();
        auto x = render (*p, 0.3);
        check (rms (x, 600) < 1.0e-4f, "PANIC: silent at once, even with a 2 s release");
        check (! p->isSeqRunning() && ! p->isPlayRequested(), "PANIC: sequencer stopped, PLAY off");
        auto y = render (*p, 0.5, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });
        check (rms (y, 6000) > 0.05f, "after PANIC, a slot key plays again");
    }

    // ---------------------------------------------------------------- per-slot trigger keys (owner's request)
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        p->setSlotKey (0, 36);                       // slot 1 -> C1
        p->setSlotKey (1, 41);                       // slot 2 -> F1
        check (p->getSlotKey (0) == 36 && p->getSlotKey (1) == 41, "slot keys: setting slot 2's key leaves slot 1's alone");
        check (p->getSlotKey (12) == 24, "slot keys: a key another slot used is swapped (slot 13 got C0)");
        render (*p, 0.3, { { 0, juce::MidiMessage::noteOn (1, 41, 0.8f) } });
        check (p->getPlayingSlot() == 1, "slot keys: F1 plays slot 2");
        render (*p, 0.3, { { 0, juce::MidiMessage::noteOff (1, 41) }, { 10, juce::MidiMessage::noteOn (1, 36, 0.8f) } });
        check (p->getPlayingSlot() == 0, "slot keys: C1 plays slot 1");
        render (*p, 0.1, { { 0, juce::MidiMessage::noteOff (1, 36) } });

        // learn: the next note becomes slot 5's key and is not played
        p->armKeyLearn (4);
        auto x = render (*p, 0.3, { { 0, juce::MidiMessage::noteOn (1, 31, 0.8f) }, { 4000, juce::MidiMessage::noteOff (1, 31) } });
        check (p->getSlotKey (4) == 31 && p->getArmedKeySlot() < 0 && p->getPlayingSlot() < 0 && rms (x) < 0.002f,
               "slot key learn: G0 becomes slot 5's key without playing anything");
        render (*p, 0.3, { { 0, juce::MidiMessage::noteOn (1, 31, 0.8f) } });
        check (p->getPlayingSlot() == 4, "slot key learn: G0 now plays slot 5");

        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto q = fresh();
        q->setStateInformation (mb.getData(), static_cast<int> (mb.getSize()));
        check (q->getSlotKey (0) == 36 && q->getSlotKey (1) == 41 && q->getSlotKey (4) == 31, "slot keys saved with the project");
    }

    // ---------------------------------------------------------------- old trigger key settings are converted
    for (int v = 0; v < 2; ++v)
    {
        auto p = fresh();
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), static_cast<int> (mb.getSize()));
        if (auto* extra = xml->getChildByName ("YF21Extra")) extra->removeAttribute ("slotKeys");
        auto* old = xml->createNewChildElement ("PARAM");
        if (v == 0) { old->setAttribute ("id", "seq_trigoct");  old->setAttribute ("value", 1.0); }    // v0.1.0-0.1.1 "C-1"
        else        { old->setAttribute ("id", "seq_trigbase"); old->setAttribute ("value", 17.0); }   // v0.1.2-0.1.3 "F-1"
        juce::MemoryBlock mb2;
        juce::AudioProcessor::copyXmlToBinary (*xml, mb2);
        auto q = fresh();
        q->setStateInformation (mb2.getData(), static_cast<int> (mb2.getSize()));
        const int first = v == 0 ? 12 : 17;
        check (q->getSlotKey (0) == first && q->getSlotKey (15) == first + 15,
               juce::String ("old project (") + (v == 0 ? "trigger octave C-1" : "trigger start F-1") + ") converts to per-slot keys");
    }

    // ---------------------------------------------------------------- CC learn
    {
        auto p = fresh();
        const int idx = BasslineAudioProcessor::paramIndexForId ("flt_cutoff");
        p->armCcLearn (idx);
        render (*p, 0.02, { { 10, juce::MidiMessage::controllerEvent (1, 74, 127) } });
        check (p->ccForParam (idx) == 74, "CC learn: cutoff <- CC 74");
        check (p->apvts.getRawParameterValue ("flt_cutoff")->load() > 11000.0f, "CC 74 at 127 moves cutoff to max");
        render (*p, 0.02, { { 10, juce::MidiMessage::controllerEvent (1, 74, 0) } });
        check (p->apvts.getRawParameterValue ("flt_cutoff")->load() < 50.0f, "CC 74 at 0 moves cutoff to min");
    }

    // ---------------------------------------------------------------- state round trip
    {
        auto p = fresh();
        p->armCcLearn (BasslineAudioProcessor::paramIndexForId ("flt_reso"));
        render (*p, 0.02, { { 10, juce::MidiMessage::controllerEvent (1, 71, 64) } });
        p->setEditSlot (9);
        p->randomiseEditSlot();
        p->slotNames[9] = "MY LINE";
        p->bank.setLength (9, 11);
        setParam (*p, "seq_mode", 1.0f);
        const auto before = p->bank.load (9).stepsToString();
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto q = fresh();
        q->setStateInformation (mb.getData(), static_cast<int> (mb.getSize()));
        check (q->bank.load (9).stepsToString() == before, "state: slot steps restored");
        check (q->bank.length (9) == 11 && q->slotNames[9] == "MY LINE", "state: slot length and name restored");
        check (q->ccForParam (BasslineAudioProcessor::paramIndexForId ("flt_reso")) == 71, "state: CC map restored");
        check (q->apvts.getRawParameterValue ("seq_mode")->load() > 0.5f, "state: play mode restored");
        check (q->getEditSlot() == 9, "state: selected slot restored");
    }

    // ---------------------------------------------------------------- every factory patch with a pattern
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        const int n = static_cast<int> (p->patches.getEntries().size());
        juce::String worst;
        bool allOk = true;
        for (int i = 0; i < n; ++i)
        {
            p->patches.load (i);
            setParam (*p, "seq_sync", 0.0f);
            auto x = render (*p, 2.0, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) },
                                        { static_cast<juce::int64> (1.9 * fs), juce::MidiMessage::noteOff (1, 24) } });
            const float r = rms (x), pk = peak (x);
            const bool ok = finite (x) && r > 0.03f && pk <= 1.0f;
            std::printf ("      patch %-18s rms %.3f peak %.3f\n", p->patches.getEntries()[static_cast<size_t> (i)].name.toRawUTF8(), r, pk);
            allOk = allOk && ok;
            writeWav (outDir.getChildFile ("patch_" + juce::String (i).paddedLeft ('0', 2) + ".wav"), x);
            render (*p, 0.3);   // let it ring out
        }
        check (allOk, "every factory patch plays, finite, peak <= 0 dBFS");
    }

    // ---------------------------------------------------------------- every factory pattern parses
    {
        auto p = fresh();
        bool ok = true;
        for (int i = 0; i < p->patterns.getNumFactory(); ++i)
        {
            PatternManager::Loaded l;
            ok = ok && p->patterns.load (i, l) && l.pattern.length >= 1;
        }
        check (ok, "factory patterns load");
        PatternManager::Loaded l;
        p->patterns.load (1, l);   // FIRST CONTACT: "C> C C^~ ..."
        check (l.pattern.steps[0].accent && l.pattern.steps[2].up && l.pattern.steps[2].slide && ! l.pattern.steps[5].gate,
               "pattern text parses accents, octave, slide, rests");
    }

    // ---------------------------------------------------------------- editor screenshot
    {
        auto p = fresh();
        setParam (*p, "seq_sync", 0.0f);
        p->setEditSlot (0);
        render (*p, 0.6, { { 0, juce::MidiMessage::noteOn (1, 24, 0.8f) } });   // something on the scope + playhead
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        ed->setSize (Theme::designWidth, Theme::designHeight);
        for (int i = 0; i < 4; ++i)
        {
            render (*p, 0.05);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("editor.png");
        f.deleteFile();
        if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        check (f.getSize() > 10000, "editor screenshot written");

        // MIDI learn view
        if (auto* e = dynamic_cast<BasslineAudioProcessorEditor*> (ed.get()))
        {
            e->setLearnMode (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f2 = outDir.getChildFile ("editor_learn.png");
            f2.deleteFile();
            if (auto os = f2.createOutputStream()) png.writeImageToStream (img2, *os);

            // step record: stopped, REC armed, cursor on step 5
            e->setLearnMode (false);
            render (*p, 0.3, { { 0, juce::MidiMessage::noteOff (1, 24) } });
            p->setRecord (true);
            p->setEditStep (4);
            for (int i = 0; i < 3; ++i) { render (*p, 0.05); juce::MessageManager::getInstance()->runDispatchLoopUntil (40); }
            auto img3 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f3 = outDir.getChildFile ("editor_steprec.png");
            f3.deleteFile();
            if (auto os = f3.createOutputStream()) png.writeImageToStream (img3, *os);
        }
        ed.reset();
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
