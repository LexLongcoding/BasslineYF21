// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Widgets.h"
#include <vector>

// A panel in the Decu6000 style: coloured top bar, title, controls.
class SynthPanel : public juce::Component
{
public:
    SynthPanel (UiContext& c, juce::String t, juce::Colour a, juce::Colour a2) : ctx (c), title (std::move (t)), accent (a), accent2 (a2) {}
    void paint (juce::Graphics& g) override { Theme::drawPanel (g, getLocalBounds(), title, accent, accent2); }
    virtual void tick() {}

protected:
    template <typename T, typename... Args>
    T& add (Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }
    // lays components out in a grid of equal cells
    static void grid (juce::Rectangle<int> area, int cols, std::initializer_list<juce::Component*> items);

    UiContext& ctx;
    juce::String title;
    juce::Colour accent, accent2;
    std::vector<std::unique_ptr<juce::Component>> owned;
};

//==============================================================================
class OscPanel : public SynthPanel
{
public:
    OscPanel (UiContext&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() override;

private:
    juce::String prefix;
    ParamToggle* onButton = nullptr;
    ParamCombo* wave = nullptr;
    Knob *oct = nullptr, *semi = nullptr, *fine = nullptr, *shape = nullptr, *level = nullptr;
    ParamToggle* lfo = nullptr;
    juce::Rectangle<int> previewArea;
    juce::Path preview;
    int shownWave = -1;
    float shownShape = -1.0f;
    bool shownOn = true;
};

class LfoPanel : public SynthPanel
{
public:
    explicit LfoPanel (UiContext&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() override;

private:
    ParamCombo* wave = nullptr;
    ParamToggle* sync = nullptr;
    ParamCombo* div = nullptr;
    Knob *rate = nullptr, *pitch = nullptr, *shape = nullptr, *cutoff = nullptr;
    juce::Rectangle<int> ledArea, patchArea;
    float shownLfo = 0.0f;
};

class FilterPanel : public SynthPanel
{
public:
    explicit FilterPanel (UiContext&);
    void resized() override;

private:
    ParamCombo* mode = nullptr;
    Knob *cutoff = nullptr, *reso = nullptr, *envmod = nullptr, *decay = nullptr, *accentK = nullptr,
         *drive = nullptr, *keytrk = nullptr, *slide = nullptr;
};

class AmpPanel : public SynthPanel
{
public:
    explicit AmpPanel (UiContext&);
    void resized() override;
};

class FoldPanel : public SynthPanel
{
public:
    explicit FoldPanel (UiContext&);
    void resized() override;
};

class DistPanel : public SynthPanel
{
public:
    explicit DistPanel (UiContext&);
    void resized() override;

private:
    ParamCombo* type = nullptr;
};

class ScopePanel : public SynthPanel
{
public:
    explicit ScopePanel (UiContext&);
    void resized() override;
    void tick() override { scope.tick(); }

private:
    Scope scope;
};

class MasterPanel : public SynthPanel
{
public:
    explicit MasterPanel (UiContext&);
    void resized() override;
    void tick() override { meter.setLevel (ctx.proc.getMeter()); }

private:
    LevelMeter meter;
};
