// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_core/juce_core.h>
#include "Pattern.h"
#include <vector>

// Pattern library: factory patterns (code) + user .yfpattern files (small XML) in
//   Windows  %APPDATA%\Bassline YF-21\Patterns   (macOS ~/Library/..., Linux ~/.config/...)
// A pattern file holds the 16 steps, the length, swing and gate. Loading one puts it
// into the selected slot of the 16-slot bank (the bank itself is saved with the project).
// Message thread only.
class PatternManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = false;
        int factoryIndex = -1;
        juce::File file;
    };

    struct Loaded
    {
        Pat::Pattern pattern;
        float swing = 0.0f, gate = 0.55f;
    };

    PatternManager() { refresh(); }

    static juce::File getUserFolder();
    static constexpr const char* fileExtension = ".yfpattern";
    static juce::String sanitiseName (const juce::String&);

    void refresh();
    const std::vector<Entry>& getEntries() const { return entries; }
    int getNumFactory() const;
    int indexOf (const juce::String& name) const;   // -1 if not in the list

    bool load (int index, Loaded& out) const;
    bool saveUser (const juce::String& name, const Pat::Pattern&, float swing, float gate);
    bool deleteUser (int index);
    bool userPatternExists (const juce::String& name) const;

private:
    std::vector<Entry> entries;
};
