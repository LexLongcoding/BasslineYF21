// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SynthPanels.h"
#include "../Oscillator.h"

using namespace Theme;

namespace
{
    constexpr int titleH = 34;   // top bar + title row
}

void SynthPanel::grid (juce::Rectangle<int> area, int cols, std::initializer_list<juce::Component*> items)
{
    const int n = static_cast<int> (items.size());
    const int rows = (n + cols - 1) / cols;
    const int cw = area.getWidth() / cols, ch = area.getHeight() / juce::jmax (1, rows);
    int i = 0;
    for (auto* c : items)
    {
        if (c != nullptr)
            c->setBounds (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * ch, cw, ch);
        ++i;
    }
}

//==============================================================================
OscPanel::OscPanel (UiContext& c, int index)
    : SynthPanel (c, "OSC " + juce::String (index + 1), orange, orange),
      prefix ("osc" + juce::String (index + 1) + "_")
{
    auto id = [this] (const char* s) { return juce::String (prefix + s); };
    // the IDs must outlive the call only for the constructor (APVTS copies them)
    onButton = &add<ParamToggle> (ctx, id ("on").toRawUTF8(), "ON", orange);
    wave     = &add<ParamCombo> (ctx, id ("wave").toRawUTF8(), orange);
    oct      = &add<Knob> (ctx, id ("oct").toRawUTF8(), orange);
    semi     = &add<Knob> (ctx, id ("semi").toRawUTF8(), orange);
    fine     = &add<Knob> (ctx, id ("fine").toRawUTF8(), orange);
    shape    = &add<Knob> (ctx, id ("shape").toRawUTF8(), orange);
    level    = &add<Knob> (ctx, id ("level").toRawUTF8(), orange);
    lfo      = &add<ParamToggle> (ctx, id ("lfo").toRawUTF8(), "LFO", pink);
    lfo->setTooltip ("Patch the LFO to this oscillator (pitch and shape depth are set on the LFO panel)");
}

void OscPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    onButton->setBounds (r.getRight() - 46, 11, 46, 20);
    r.removeFromTop (titleH + 4);
    wave->setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    previewArea = r.removeFromBottom (44);
    r.removeFromBottom (4);
    auto knobs = r;
    const int cw = knobs.getWidth() / 3, ch = knobs.getHeight() / 2;
    grid (knobs, 3, { oct, semi, fine, shape, level, nullptr });
    lfo->setBounds (juce::Rectangle<int> (knobs.getX() + 2 * cw, knobs.getY() + ch, cw, ch).withSizeKeepingCentre (cw - 12, 24));
}

void OscPanel::tick()
{
    auto& apvts = ctx.proc.apvts;
    const int w = static_cast<int> (apvts.getRawParameterValue (prefix + "wave")->load());
    const float s = apvts.getRawParameterValue (prefix + "shape")->load();
    const bool on = apvts.getRawParameterValue (prefix + "on")->load() >= 0.5f;
    if (w == shownWave && std::abs (s - shownShape) < 0.002f && on == shownOn)
        return;
    shownWave = w; shownShape = s; shownOn = on;

    // run a real oscillator for one cycle to draw what it makes
    Oscillator o;
    constexpr int points = 160;
    o.setSampleRate (points * 50.0);
    o.set (w, 50.0f, s);
    std::array<float, points> v {};
    float peak = 0.01f;
    for (auto& x : v) { x = o.next(); peak = juce::jmax (peak, std::abs (x)); }
    preview.clear();
    auto a = previewArea.toFloat().reduced (8.0f, 7.0f);
    for (int i = 0; i < points; ++i)
    {
        const float x = a.getX() + a.getWidth() * i / (points - 1.0f);
        const float y = a.getCentreY() - v[static_cast<size_t> (i)] / peak * a.getHeight() * 0.5f;
        if (i == 0) preview.startNewSubPath (x, y); else preview.lineTo (x, y);
    }
    repaint (previewArea);
}

void OscPanel::paint (juce::Graphics& g)
{
    SynthPanel::paint (g);
    drawLcd (g, previewArea.toFloat(), orange);
    g.setColour (orange.withAlpha (shownOn ? 0.25f : 0.08f));
    g.strokePath (preview, juce::PathStrokeType (4.0f));
    g.setColour (orange.withAlpha (shownOn ? 1.0f : 0.3f));
    g.strokePath (preview, juce::PathStrokeType (1.5f));
}

//==============================================================================
LfoPanel::LfoPanel (UiContext& c) : SynthPanel (c, "LFO", pink, pink)
{
    wave   = &add<ParamCombo> (ctx, "lfo_wave", pink);
    sync   = &add<ParamToggle> (ctx, "lfo_sync", "SYNC", pink);
    div    = &add<ParamCombo> (ctx, "lfo_div", pink);
    rate   = &add<Knob> (ctx, "lfo_rate", pink);
    pitch  = &add<Knob> (ctx, "lfo_pitch", pink);
    shape  = &add<Knob> (ctx, "lfo_shape", pink);
    cutoff = &add<Knob> (ctx, "lfo_cutoff", pink);
}

void LfoPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    ledArea = juce::Rectangle<int> (r.getRight() - 16, 15, 12, 12);
    r.removeFromTop (titleH + 4);
    wave->setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    auto row = r.removeFromTop (26);
    sync->setBounds (row.removeFromLeft (64));
    row.removeFromLeft (6);
    div->setBounds (row);
    r.removeFromTop (4);
    patchArea = r.removeFromBottom (16);
    grid (r, 2, { rate, pitch, shape, cutoff });
}

void LfoPanel::tick()
{
    const bool synced = ctx.proc.apvts.getRawParameterValue ("lfo_sync")->load() >= 0.5f;
    rate->setEnabled (! synced);
    div->setEnabled (synced);
    const float v = ctx.proc.getLfoValue();
    if (std::abs (v - shownLfo) > 0.02f) { shownLfo = v; repaint (ledArea.expanded (4)); }
}

void LfoPanel::paint (juce::Graphics& g)
{
    SynthPanel::paint (g);
    const float b = 0.5f + 0.5f * shownLfo;
    g.setColour (pink.withAlpha (0.15f + 0.35f * b));
    g.fillEllipse (ledArea.toFloat().expanded (3.0f));
    g.setColour (pink.withAlpha (0.25f + 0.75f * b));
    g.fillEllipse (ledArea.toFloat());

    g.setColour (dim);
    g.setFont (mono (10.0f));
    g.drawFittedText ("PATCH: LFO BUTTON ON EACH OSC", patchArea, juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
FilterPanel::FilterPanel (UiContext& c) : SynthPanel (c, "FILTER // VCF", cyan, pink)
{
    mode    = &add<ParamCombo> (ctx, "flt_mode", cyan);
    cutoff  = &add<Knob> (ctx, "flt_cutoff", cyan, true);
    reso    = &add<Knob> (ctx, "flt_reso", pink, true);
    envmod  = &add<Knob> (ctx, "flt_envmod", cyan);
    decay   = &add<Knob> (ctx, "flt_decay", cyan);
    accentK = &add<Knob> (ctx, "flt_accent", pink);
    drive   = &add<Knob> (ctx, "flt_drive", cyan);
    keytrk  = &add<Knob> (ctx, "flt_keytrk", cyan);
    slide   = &add<Knob> (ctx, "voice_slide", pink);
}

void FilterPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 0);
    mode->setBounds (r.getRight() - 150, 9, 150, 24);
    r.removeFromTop (titleH + 8);
    r.removeFromBottom (10);
    auto big = r.removeFromLeft (r.getWidth() * 46 / 100);
    grid (big.withSizeKeepingCentre (big.getWidth(), juce::jmin (big.getHeight(), 170)), 2, { cutoff, reso });
    r.removeFromLeft (8);
    grid (r, 3, { envmod, decay, accentK, drive, keytrk, slide });
}

//==============================================================================
AmpPanel::AmpPanel (UiContext& c) : SynthPanel (c, "AMP ENV // VCA", orange, orange)
{
    for (auto* id : { "amp_attack", "amp_decay", "amp_sustain", "amp_release" })
        add<Knob> (ctx, id, orange);
}

void AmpPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    r.removeFromTop (titleH + 6);
    r.removeFromBottom (12);
    grid (r, 4, { owned[0].get(), owned[1].get(), owned[2].get(), owned[3].get() });
}

FoldPanel::FoldPanel (UiContext& c) : SynthPanel (c, "WAVE FOLDER", pink, pink)
{
    for (auto* id : { "fold_amount", "fold_sym", "fold_mix" })
        add<Knob> (ctx, id, pink);
}

void FoldPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    r.removeFromTop (titleH + 6);
    r.removeFromBottom (12);
    grid (r, 3, { owned[0].get(), owned[1].get(), owned[2].get() });
}

DistPanel::DistPanel (UiContext& c) : SynthPanel (c, "DISTORTION", pink, orange)
{
    type = &add<ParamCombo> (ctx, "dist_type", pink);
    for (auto* id : { "dist_drive", "dist_tone", "dist_mix" })
        add<Knob> (ctx, id, pink);
}

void DistPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    type->setBounds (r.getRight() - 110, 9, 110, 24);
    r.removeFromTop (titleH + 6);
    r.removeFromBottom (12);
    grid (r, 3, { owned[1].get(), owned[2].get(), owned[3].get() });
}

ScopePanel::ScopePanel (UiContext& c) : SynthPanel (c, "SCOPE // OUTPUT", cyan, cyan), scope (c)
{
    addAndMakeVisible (scope);
}

void ScopePanel::resized()
{
    auto r = getLocalBounds().reduced (12, 0);
    r.removeFromTop (titleH + 4);
    r.removeFromBottom (12);
    scope.setBounds (r);
}

MasterPanel::MasterPanel (UiContext& c) : SynthPanel (c, "MASTER", pink, pink)
{
    add<Knob> (ctx, "master_volume", pink);
    add<Knob> (ctx, "master_tune", pink);
    addAndMakeVisible (meter);
}

void MasterPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 0);
    r.removeFromTop (titleH + 6);
    r.removeFromBottom (12);
    meter.setBounds (r.removeFromRight (14));
    r.removeFromRight (10);
    grid (r, 2, { owned[0].get(), owned[1].get() });
}
