// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Pattern.h"
#include <vector>

// Built-in patterns, written as text: one token per step.
//   note name C C# D D# E F F# G G# A A# B C'   (C' = the top C)
//   then any modifiers: ^ octave up, v octave down, > accent, ~ slide
//   "-" = rest
namespace FactoryPatterns
{
    struct Def { const char* name; int length; const char* steps; float swing; float gate; };

    inline Pat::Pattern parse (const Def& d)
    {
        Pat::Pattern p;
        p.name = d.name;
        p.length = d.length;
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };
        std::string text (d.steps);
        size_t pos = 0;
        for (int i = 0; i < Pat::numSteps; ++i)
        {
            while (pos < text.size() && text[pos] == ' ') ++pos;
            const auto end = text.find (' ', pos);
            const auto tok = text.substr (pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? text.size() : end;
            auto& s = p.steps[static_cast<size_t> (i)];
            if (tok.empty() || tok[0] == '-') { s.gate = false; continue; }
            int best = 0; size_t bestLen = 0;
            for (int n = 0; n < 13; ++n)
            {
                const std::string nm (names[n]);
                if (tok.compare (0, nm.size(), nm) == 0 && nm.size() >= bestLen) { best = n; bestLen = nm.size(); }
            }
            s.note = best;
            for (size_t c = bestLen; c < tok.size(); ++c)
            {
                if (tok[c] == '^') s.up = true;
                if (tok[c] == 'v') s.down = true;
                if (tok[c] == '>') s.accent = true;
                if (tok[c] == '~') s.slide = true;
            }
        }
        return p;
    }

    inline const std::vector<Def>& all()
    {
        static const std::vector<Def> defs {
            { "EMPTY",          16, "- - - - - - - - - - - - - - - -", 0.0f, 0.55f },
            { "FIRST CONTACT",  16, "C> C C^~ C D#> - G~ A# C> C^ C~ F> C A#v~ C> D", 0.0f, 0.55f },
            { "SUPERCRUISE",    16, "C> C^ C C^> C C^ D# C^ C> C^ C G C^> F C^ C", 0.0f, 0.5f },
            { "BRAIN DIRECT",   16, "C>~ C^ - C G>~ F D#~ C - C C^>~ C' - A#v> C~ D#", 0.1f, 0.55f },
            { "DOGFIGHT",       16, "C> - C C^> - C D# C> - F~ G A#> C^ - C", 0.0f, 0.45f },
            { "MOONLIT EDEN",   16, "C~ D# G~ A# C^> A# G~ D# C~ D#^ C' A#~ G> F D#~ D", 0.15f, 0.6f },
            { "PHRYGIAN NIGHT", 16, "C> C# C C^>~ C# C - C> C# D# C~ C#^ C> - G# C", 0.0f, 0.5f },
            { "OVERDRIVE 7",    7,  "C> C^ C G~ A#> C^ D#", 0.0f, 0.5f },
            { "SLOW SLIDE",     16, "C~ C~ G~ G~ D#~ D#~ F~ F> C~ C~ A#v~ A#v~ C~ C~ D#~ G", 0.0f, 0.9f },
        };
        return defs;
    }
}
