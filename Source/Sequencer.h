// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Pattern.h"
#include <cmath>

// The step clock. Position is counted in 16th-note steps (a host's ppq * 4), so it
// can follow the DAW exactly. tick() runs once per sample and reports note events.
//
// 303 rules: a step with SLIDE holds its gate into the next step (if that step is a
// note) and the next note glides in without retriggering the envelopes. A REST step
// closes the gate. GATE sets how long non-slide notes are held (fraction of a step).
class Sequencer
{
public:
    struct Event
    {
        enum Type { NoteOn, NoteOff } type = NoteOff;
        int note = 0;
        bool accent = false, legato = false;
    };

    // stepOffset: absolute step that plays pattern step 1 (0 = locked to the bar)
    void start (int newSlot, double position, long long stepOffset)
    {
        slot = newSlot;
        offset = stepOffset;
        running = true;
        gateOpen = slideHold = false;
        suppress = -1;
        jumpTo (position);
    }

    void stop() { running = false; gateOpen = slideHold = false; playingStep = -1; }
    void setSlot (int s) { slot = s; }       // seamless switch (keeps position)

    bool isRunning() const { return running; }
    int getSlot() const { return slot; }
    int getPlayingStep() const { return playingStep; }
    double getPosition() const { return pos; }
    long long getOffset() const { return offset; }

    void setTiming (double stepsPerSampleIn, float swingIn, float gateIn)
    {
        stepsPerSample = stepsPerSampleIn;
        swing = swingIn;
        gate = gateIn;
    }
    void setTranspose (int semis) { transpose = semis; }

    // Follow the host: jump only when the position really moved (loop / locate).
    void follow (double hostPosition)
    {
        if (std::fabs (hostPosition - pos) > 0.05)
            jumpTo (hostPosition);
        else
            pos = hostPosition;
    }

    // First absolute step that starts at or after `position`.
    long long firstStepFrom (double position) const
    {
        long long k = static_cast<long long> (std::floor (position));
        while (stepTime (k) < position - 1.0e-9) ++k;
        while (k > 0 && stepTime (k - 1) >= position - 1.0e-9) --k;
        return k;
    }

    // Live recording: the absolute step nearest to now.
    long long nearestStep() const
    {
        const long long next = nextIndex, prev = nextIndex - 1;
        return (pos - stepTime (prev)) <= (stepTime (next) - pos) ? prev : next;
    }
    int patternIndex (long long absStep, int length) const
    {
        long long i = (absStep - offset) % length;
        if (i < 0) i += length;
        return static_cast<int> (i);
    }
    void suppressStep (long long absStep) { suppress = absStep; }   // already played live

    int tick (const Pat::Bank& bank, Event* out)
    {
        if (! running) return 0;
        int n = 0;

        if (gateOpen && ! slideHold && pos >= gateOffAt)
        {
            out[n++] = { Event::NoteOff, 0, false, false };
            gateOpen = false;
        }

        if (pos >= nextTime)
        {
            const long long k = nextIndex;
            const int len = bank.length (slot);
            const int idx = patternIndex (k, len);
            const auto s = bank.get (slot, idx);
            playingStep = idx;

            if (s.gate && k != suppress)
            {
                out[n++] = { Event::NoteOn, s.midiNote() + transpose, s.accent, slideHold && gateOpen };
                gateOpen = true;
                const auto next = bank.get (slot, (idx + 1) % len);
                slideHold = s.slide && next.gate;
                gateOffAt = stepTime (k) + gate;
            }
            else if (! s.gate)
            {
                if (gateOpen) out[n++] = { Event::NoteOff, 0, false, false };
                gateOpen = slideHold = false;
            }
            nextIndex = k + 1;
            nextTime = stepTime (nextIndex);
        }

        pos += stepsPerSample;
        return n;
    }

    // Swing delays every second 16th by up to half a step (50% .. 75% shuffle).
    double stepTime (long long k) const { return static_cast<double> (k) + ((k & 1) ? swing * 0.5 : 0.0); }

private:
    void jumpTo (double position)
    {
        if (gateOpen)          // a held note doesn't survive a jump: close it on the next tick
        {
            slideHold = false;
            gateOffAt = position;
        }
        pos = position;
        nextIndex = firstStepFrom (position);
        nextTime = stepTime (nextIndex);
    }

    bool running = false, gateOpen = false, slideHold = false;
    int slot = 0, playingStep = -1, transpose = 0;
    long long offset = 0, nextIndex = 0, suppress = -1;
    double pos = 0.0, nextTime = 0.0, gateOffAt = 0.0, stepsPerSample = 0.0;
    float swing = 0.0f, gate = 0.55f;
};
