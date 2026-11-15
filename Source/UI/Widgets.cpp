// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Widgets.h"

using namespace Theme;

juce::String groupTitleFor (const juce::String& paramId)
{
    const auto prefix = paramId.upToFirstOccurrenceOf ("_", false, false);
    for (const auto& g : Params::groups)
        if (prefix == g.prefix)
            return g.title;
    return prefix.toUpperCase();
}

juce::String paramTitle (int paramIndex)
{
    if (paramIndex < 0 || paramIndex >= Params::numParams) return {};
    const auto& s = Params::specs[paramIndex];
    return groupTitleFor (s.id) + " " + s.knob;
}

void showParamMenu (UiContext& ctx, juce::Component& target, int paramIndex, std::function<void()> reset)
{
    const int cc = ctx.proc.ccForParam (paramIndex);
    juce::PopupMenu m;
    m.addSectionHeader (Params::specs[paramIndex].name);
    m.addItem (1, "Learn MIDI CC...");
    m.addItem (2, cc >= 0 ? "Clear MIDI CC (CC " + juce::String (cc) + ")" : juce::String ("Clear MIDI CC"), cc >= 0);
    if (reset) m.addItem (3, "Reset to default");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [&ctx, paramIndex, reset, safe = juce::Component::SafePointer<juce::Component> (&target)] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == 1 && ctx.armKnobLearn) ctx.armKnobLearn (paramIndex);
                         if (r == 2) ctx.proc.clearCcForParam (paramIndex);
                         if (r == 3 && reset) reset();
                     });
}

//==============================================================================
Knob::Knob (UiContext& c, const char* paramId, juce::Colour accent, bool large)
    : ctx (c), paramIndex (BasslineAudioProcessor::paramIndexForId (paramId)), isLarge (large)
{
    jassert (paramIndex >= 0);
    const auto& spec = Params::specs[paramIndex];
    label = spec.knob;
    getProperties().set (learnProperty, paramIndex);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setTooltip (spec.name);
    slider.setMouseDragSensitivity (large ? 320 : 220);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ctx.proc.apvts, spec.id, slider);
    slider.setDoubleClickReturnValue (true, spec.def);
    slider.onDragStart   = [this] { reportValue(); };
    slider.onValueChange = [this] { if (slider.isMouseButtonDown()) reportValue(); };
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
}

Knob::~Knob() { slider.removeMouseListener (this); }

void Knob::reportValue()
{
    if (ctx.setLcd == nullptr) return;
    if (auto* p = ctx.proc.apvts.getParameter (Params::specs[paramIndex].id))
        ctx.setLcd (paramTitle (paramIndex) + "  " + p->getCurrentValueAsText(), cyan);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showParamMenu (ctx, slider, paramIndex, [safe = juce::Component::SafePointer<Knob> (this)]
                       { if (safe) safe->slider.setValue (Params::specs[safe->paramIndex].def, juce::sendNotificationSync); });
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (isLarge ? 22 : 15);
    const int size = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (size, size));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (text);
    g.setFont (isLarge ? display (12.0f) : mono (10.0f));
    g.drawFittedText (label, getLocalBounds().removeFromBottom (isLarge ? 20 : 14), juce::Justification::centred, 1, 0.55f);
}

//==============================================================================
ParamToggle::ParamToggle (UiContext& c, const char* paramId, const juce::String& t, juce::Colour accent)
    : juce::TextButton (t), ctx (c), paramIndex (BasslineAudioProcessor::paramIndexForId (paramId))
{
    jassert (paramIndex >= 0);
    getProperties().set (learnProperty, paramIndex);
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setTooltip (Params::specs[paramIndex].name);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (ctx.proc.apvts, paramId, *this);
}

void ParamToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showParamMenu (ctx, *this, paramIndex, {}); return; }
    juce::TextButton::mouseDown (e);
}

ParamCombo::ParamCombo (UiContext& c, const char* paramId, juce::Colour accent)
    : ctx (c), paramIndex (BasslineAudioProcessor::paramIndexForId (paramId))
{
    jassert (paramIndex >= 0);
    getProperties().set (learnProperty, paramIndex);
    const auto& spec = Params::specs[paramIndex];
    if (spec.unit == Params::Unit::Note)
        for (int n = static_cast<int> (spec.min); n <= static_cast<int> (spec.max); ++n)
            addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 3) + "  (" + juce::String (n) + ")", n - static_cast<int> (spec.min) + 1);
    else
        addItemList (juce::StringArray::fromTokens (spec.choices, "|", ""), 1);
    setColour (juce::ComboBox::outlineColourId, accent);
    setColour (juce::ComboBox::arrowColourId, accent);
    setTooltip (Params::specs[paramIndex].name);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (ctx.proc.apvts, paramId, *this);
}

void ParamCombo::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showParamMenu (ctx, *this, paramIndex, {}); return; }
    juce::ComboBox::mouseDown (e);
}

//==============================================================================
void LcdDisplay::setText (const juce::String& t)
{
    if (t != text) { text = t; if (! showingMessage()) repaint(); }
}

void LcdDisplay::showMessage (const juce::String& t, juce::Colour c, int holdMs)
{
    message = t;
    messageColour = c;
    persistent = holdMs < 0;
    messageUntil = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (juce::jmax (0, holdMs));
    repaint();
}

void LcdDisplay::clearMessage() { message.clear(); persistent = false; repaint(); }

bool LcdDisplay::showingMessage() const
{
    return message.isNotEmpty() && (persistent || juce::Time::getMillisecondCounter() < messageUntil);
}

void LcdDisplay::tick()
{
    if (message.isNotEmpty() && ! showingMessage())
        clearMessage();
}

void LcdDisplay::paint (juce::Graphics& g)
{
    drawLcd (g, getLocalBounds().toFloat(), colour);
    const bool msg = showingMessage();
    g.setColour (msg ? messageColour : colour);
    g.setFont (mono (13.0f));
    g.drawFittedText ((msg ? message : text).toUpperCase(), getLocalBounds().reduced (12, 0).withTrimmedRight (onClick ? 12 : 0),
                      juce::Justification::centredLeft, 1, 0.8f);
    if (! msg && onClick)
    {
        juce::Path arrow;
        const float ax = static_cast<float> (getWidth()) - 14.0f, ay = static_cast<float> (getHeight()) * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (colour.withAlpha (0.6f));
        g.fillPath (arrow);
    }
}

//==============================================================================
BpmDisplay::BpmDisplay (UiContext& c) : ctx (c)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Tempo. Drag up/down or scroll to change (Shift = fine), double-click for 125. "
                "With SYNC on, the DAW's tempo is used.");
}

void BpmDisplay::paint (juce::Graphics& g)
{
    const bool host = ctx.proc.isHostSynced();
    drawLcd (g, getLocalBounds().toFloat(), orange);
    auto r = getLocalBounds().reduced (10, 0);
    g.setColour (orange);
    g.setFont (mono (22.0f));
    g.drawText (juce::String (ctx.proc.getDisplayBpm(), 1), r.removeFromLeft (r.getWidth() - 40), juce::Justification::centredRight, false);
    g.setFont (mono (10.0f));
    g.setColour (host ? cyan : text.withAlpha (0.8f));
    auto tag = r.withTrimmedLeft (6);
    g.drawText ("BPM", tag.removeFromTop (getHeight() / 2).withTrimmedTop (3), juce::Justification::bottomLeft, false);
    g.drawText (host ? "HOST" : "INT", tag.withTrimmedBottom (3), juce::Justification::topLeft, false);

    // beat light: on for the first 16th of every beat while the sequencer runs
    if (beat)
    {
        g.setColour (pink);
        g.fillEllipse (6.0f, getHeight() * 0.5f - 3.0f, 6.0f, 6.0f);
    }
}

void BpmDisplay::tick()
{
    const float b = ctx.proc.getDisplayBpm();
    const bool s = ctx.proc.isHostSynced();
    const int step = ctx.proc.getPlayingStep();
    const bool newBeat = step >= 0 && step % 4 == 0;
    if (std::abs (b - shown) > 0.04f || s != shownSync || newBeat != beat)
    {
        shown = b; shownSync = s; beat = newBeat;
        repaint();
    }
}

void BpmDisplay::nudge (float bpm)
{
    if (auto* p = ctx.proc.apvts.getParameter ("seq_tempo"))
    {
        bpm = juce::jlimit (40.0f, 240.0f, std::round (bpm * 10.0f) / 10.0f);
        p->setValueNotifyingHost (p->convertTo0to1 (bpm));
        if (ctx.setLcd) ctx.setLcd ("TEMPO  " + juce::String (bpm, 1) + " BPM"
                                    + (ctx.proc.isHostSynced() ? "  (HOST TEMPO IN USE - TURN SYNC OFF)" : ""), orange);
    }
}

void BpmDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showParamMenu (ctx, *this, BasslineAudioProcessor::paramIndexForId ("seq_tempo"), [this] { nudge (125.0f); });
        return;
    }
    dragStartBpm = ctx.proc.apvts.getRawParameterValue ("seq_tempo")->load();
}

void BpmDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float perPixel = e.mods.isShiftDown() ? 0.05f : 0.4f;
    nudge (dragStartBpm - static_cast<float> (e.getDistanceFromDragStartY()) * perPixel);
}

void BpmDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float cur = ctx.proc.apvts.getRawParameterValue ("seq_tempo")->load();
    nudge (cur + (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 1.0f));
}

void BpmDisplay::mouseDoubleClick (const juce::MouseEvent&) { nudge (125.0f); }

//==============================================================================
void LevelMeter::setLevel (float peak)
{
    const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const float next = juce::jmax (target, level * 0.86f);
    if (std::abs (next - level) > 0.002f) { level = next; repaint(); }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient grad (cyan, 0.0f, r.getBottom(), pink, 0.0f, r.getY(), false);
    grad.addColour (0.75, orange);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (bg);
    g.fillRect (r.withHeight (r.getHeight() * (1.0f - level)));
}

//==============================================================================
void Scope::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawLcd (g, area, cyan);
    auto inner = area.reduced (6.0f, 6.0f);

    g.setColour (cyan.withAlpha (0.12f));     // graticule
    for (int i = 1; i < 8; ++i)
        g.fillRect (inner.getX() + inner.getWidth() * i / 8.0f, inner.getY(), 1.0f, inner.getHeight());
    g.fillRect (inner.getX(), inner.getCentreY(), inner.getWidth(), 1.0f);

    const int n = static_cast<int> (buffer.size());
    ctx.proc.copyScope (buffer.data(), n);
    const int window = 900;
    int startAt = n - window - 1;
    for (int i = n - window - 1; i > n / 4; --i)   // newest rising zero crossing that leaves a full window
        if (buffer[static_cast<size_t> (i - 1)] < 0.0f && buffer[static_cast<size_t> (i)] >= 0.0f) { startAt = i; break; }

    juce::Path p;
    for (int i = 0; i < window; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, buffer[static_cast<size_t> (startAt + i)]);
        const float x = inner.getX() + inner.getWidth() * i / (window - 1.0f);
        const float y = inner.getCentreY() - v * inner.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (cyan.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (cyan);
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

//==============================================================================
LearnOverlay::LearnOverlay (UiContext& c, juce::Component& r) : ctx (c), root (r) {}

std::vector<std::pair<int, juce::Rectangle<int>>> LearnOverlay::targets()
{
    std::vector<std::pair<int, juce::Rectangle<int>>> out;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (child == this || ! child->isVisible()) continue;
            if (child->getProperties().contains (learnProperty))
                out.emplace_back (static_cast<int> (child->getProperties()[learnProperty]),
                                  getLocalArea (child, child->getLocalBounds()));
            else
                walk (*child);
        }
    };
    walk (root);
    return out;
}

void LearnOverlay::paint (juce::Graphics& g)
{
    g.fillAll (bg.withAlpha (0.45f));
    const int armed = ctx.proc.getArmedCcParam();
    for (const auto& [param, r] : targets())
    {
        const bool isArmed = param == armed;
        const int cc = ctx.proc.ccForParam (param);
        const bool mapped = cc >= 0;
        const auto rf = r.toFloat().reduced (1.0f);
        if (isArmed)
        {
            g.setColour (learn.withAlpha (0.28f));
            g.fillRoundedRectangle (rf, 4.0f);
        }
        g.setColour (learn.withAlpha (isArmed ? 1.0f : (mapped ? 0.85f : 0.4f)));
        g.drawRoundedRectangle (rf, 4.0f, isArmed ? 2.5f : 1.2f);

        const juce::String badge = isArmed ? "MOVE CC" : (mapped ? "CC " + juce::String (cc) : juce::String ("--"));
        g.setFont (mono (10.0f));
        const int bw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), badge) + 8;
        auto br = juce::Rectangle<int> (r.getRight() - bw - 2, r.getY() + 2, bw, 13).toFloat();
        g.setColour (mapped || isArmed ? learn : panelLo);
        g.fillRoundedRectangle (br, 3.0f);
        g.setColour (mapped || isArmed ? bg : dim);
        g.drawText (badge, br.toNearestInt(), juce::Justification::centred, false);
    }
}

void LearnOverlay::mouseDown (const juce::MouseEvent& e)
{
    // smallest target under the mouse (a toggle inside a panel beats the panel)
    int best = -1, bestArea = 1 << 30;
    for (const auto& [param, r] : targets())
        if (r.contains (e.getPosition()) && r.getWidth() * r.getHeight() < bestArea)
        {
            best = param;
            bestArea = r.getWidth() * r.getHeight();
        }

    if (best >= 0 && e.mods.isPopupMenu())
        ctx.proc.clearCcForParam (best);
    else if (best >= 0 && ctx.proc.getArmedCcParam() != best)
    {
        ctx.proc.armCcLearn (best);
        if (onArmed) onArmed (best);
    }
    else
    {
        ctx.proc.armCcLearn (-1);
        if (onCancelled) onCancelled();
    }
    repaint();
}
