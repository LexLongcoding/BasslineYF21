// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <cstdio>
#include <cstdlib>

// A 16-step pattern. Each step: a note in the octave C..C (0..12) plus the
// modifiers REST (gate off), SLIDE, ACCENT, OCTAVE UP, OCTAVE DOWN - the same
// information a 303 step holds. JUCE-free.
namespace Pat
{
    constexpr int numSteps = 16;
    constexpr int numSlots = 16;
    constexpr int baseNote = 36;        // step note 0 (C) with no octave flag = MIDI 36

    enum Flag : uint32_t { Gate = 1u << 4, Slide = 1u << 5, Accent = 1u << 6, Up = 1u << 7, Down = 1u << 8 };

    struct Step
    {
        int note = 0;                    // 0..12
        bool gate = true, slide = false, accent = false, up = false, down = false;

        uint32_t pack() const
        {
            return static_cast<uint32_t> (note & 15) | (gate ? Gate : 0u) | (slide ? Slide : 0u)
                 | (accent ? Accent : 0u) | (up ? Up : 0u) | (down ? Down : 0u);
        }
        static Step unpack (uint32_t v)
        {
            Step s;
            s.note = static_cast<int> (v & 15u);
            if (s.note > 12) s.note = 12;
            s.gate = v & Gate; s.slide = v & Slide; s.accent = v & Accent; s.up = v & Up; s.down = v & Down;
            return s;
        }
        int midiNote() const { return baseNote + note + (up ? 12 : 0) - (down ? 12 : 0); }

        // From a played MIDI note: nearest representation in C..C plus octave flags.
        static Step fromMidi (int midi, bool accent)
        {
            Step s;
            s.accent = accent;
            int rel = midi - baseNote;
            if (rel < 0)       { s.down = true; rel += 12; }
            else if (rel > 12) { s.up = true;   rel -= 12; }
            while (rel < 0)  rel += 12;      // further out than one octave: fold in
            while (rel > 12) rel -= 12;
            s.note = rel;
            return s;
        }
    };

    struct Pattern
    {
        std::array<Step, numSteps> steps {};
        int length = 16;
        std::string name = "EMPTY";

        static Pattern empty()
        {
            Pattern p;
            for (auto& s : p.steps) s.gate = false;
            return p;
        }

        // "16:hex,hex,..." - compact text used in session state and pattern files
        std::string stepsToString() const
        {
            std::string t = std::to_string (length) + ":";
            char buf[8];
            for (int i = 0; i < numSteps; ++i)
            {
                std::snprintf (buf, sizeof buf, "%03x", steps[static_cast<size_t> (i)].pack());
                t += buf;
                if (i < numSteps - 1) t += ",";
            }
            return t;
        }
        bool stepsFromString (const std::string& t)
        {
            const auto colon = t.find (':');
            if (colon == std::string::npos) return false;
            const int len = std::atoi (t.substr (0, colon).c_str());
            if (len < 1 || len > numSteps) return false;
            Pattern p;
            size_t pos = colon + 1;
            for (int i = 0; i < numSteps; ++i)
            {
                const auto comma = t.find (',', pos);
                const auto item = t.substr (pos, comma == std::string::npos ? std::string::npos : comma - pos);
                if (item.empty()) return false;
                p.steps[static_cast<size_t> (i)] = Step::unpack (static_cast<uint32_t> (std::strtoul (item.c_str(), nullptr, 16)));
                if (comma == std::string::npos) { if (i != numSteps - 1) return false; break; }
                pos = comma + 1;
            }
            steps = p.steps;
            length = len;
            return true;
        }

        // An "acid" line: a minor-ish scale, mostly roots, some accents, slides and octave jumps.
        static Pattern random (uint32_t seed)
        {
            auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return static_cast<float> (seed >> 8) / 16777216.0f; };
            static const int scales[3][8] = { { 0, 3, 5, 7, 10, 12, 0, 7 },      // minor pentatonic
                                              { 0, 2, 3, 5, 7, 8, 10, 12 },     // aeolian
                                              { 0, 1, 3, 5, 7, 8, 10, 12 } };   // phrygian
            const int sc = static_cast<int> (rnd() * 3.0f) % 3;
            Pattern p;
            p.name = "RANDOM";
            for (int i = 0; i < numSteps; ++i)
            {
                auto& s = p.steps[static_cast<size_t> (i)];
                s.gate = rnd() < (i % 4 == 0 ? 0.92f : 0.68f);
                s.note = rnd() < 0.45f ? 0 : scales[sc][static_cast<int> (rnd() * 8.0f) % 8];
                s.accent = rnd() < (i % 4 == 0 ? 0.45f : 0.22f);
                s.slide = rnd() < 0.22f;
                const float o = rnd();
                s.up = o < 0.16f;
                s.down = ! s.up && o > 0.9f;
            }
            return p;
        }
    };

    // The 16 pattern slots, shared between the audio thread (reads, and writes while
    // recording) and the editor. Every step is one atomic word, so no locks.
    class Bank
    {
    public:
        Bank()
        {
            for (int s = 0; s < numSlots; ++s)
                store (s, Pattern::empty());
        }

        Step get (int slot, int step) const
        {
            return Step::unpack (words[idx (slot)][static_cast<size_t> (step & 15)].load (std::memory_order_relaxed));
        }
        void set (int slot, int step, const Step& s)
        {
            words[idx (slot)][static_cast<size_t> (step & 15)].store (s.pack(), std::memory_order_relaxed);
            changes.fetch_add (1, std::memory_order_relaxed);
        }
        int length (int slot) const { return lengths[idx (slot)].load (std::memory_order_relaxed); }
        void setLength (int slot, int len)
        {
            lengths[idx (slot)].store (len < 1 ? 1 : (len > numSteps ? numSteps : len));
            changes.fetch_add (1, std::memory_order_relaxed);
        }

        Pattern load (int slot) const
        {
            Pattern p;
            for (int i = 0; i < numSteps; ++i) p.steps[static_cast<size_t> (i)] = get (slot, i);
            p.length = length (slot);
            return p;
        }
        void store (int slot, const Pattern& p)
        {
            for (int i = 0; i < numSteps; ++i)
                words[idx (slot)][static_cast<size_t> (i)].store (p.steps[static_cast<size_t> (i)].pack(), std::memory_order_relaxed);
            setLength (slot, p.length);
        }

        // bumps on every edit; the editor repaints and marks patterns modified when it changes
        uint32_t getChangeCount() const { return changes.load (std::memory_order_relaxed); }

    private:
        static size_t idx (int slot) { return static_cast<size_t> (slot < 0 ? 0 : (slot >= numSlots ? numSlots - 1 : slot)); }
        std::array<std::array<std::atomic<uint32_t>, numSteps>, numSlots> words {};
        std::array<std::atomic<int>, numSlots> lengths {};
        std::atomic<uint32_t> changes { 0 };
    };
}
