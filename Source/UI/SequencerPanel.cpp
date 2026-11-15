// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SequencerPanel.h"

using namespace Theme;

namespace
{
    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };

    juce::String trigKeyName (int slot, int trigBase)
    {
        return juce::MidiMessage::getMidiNoteName (trigBase + slot, true, true, 3);
    }

    // row colours: rest dim, slide cyan, accent pink, octave orange
    juce::Colour rowColour (int kind)
    {
        switch (kind)
        {
            case SeqSymbols::Rest:   return text;
            case SeqSymbols::Slide:  return cyan;
            case SeqSymbols::Accent: return pink;
            default:                 return orange;
        }
    }
}

//==============================================================================
void SeqSymbols::draw (juce::Graphics& g, Kind kind, juce::Rectangle<float> a, juce::Colour c)
{
    const float s = juce::jmin (a.getWidth(), a.getHeight());
    auto r = a.withSizeKeepingCentre (s, s);
    g.setColour (c);
    juce::Path p;
    switch (kind)
    {
        case Rest:   // "nothing": a ring with a slash
        {
            const auto ring = r.reduced (s * 0.18f);
            g.drawEllipse (ring, s * 0.09f);
            g.drawLine (ring.getX() + s * 0.06f, ring.getBottom() - s * 0.06f, ring.getRight() - s * 0.06f, ring.getY() + s * 0.06f, s * 0.09f);
            return;
        }
        case Slide:  // a tie arc between two dots
        {
            const float y = r.getCentreY() + s * 0.12f;
            p.startNewSubPath (r.getX() + s * 0.12f, y);
            p.quadraticTo (r.getCentreX(), r.getY() + s * 0.05f, r.getRight() - s * 0.12f, y);
            g.strokePath (p, juce::PathStrokeType (s * 0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (r.getX() + s * 0.05f, y - s * 0.07f, s * 0.14f, s * 0.14f);
            g.fillEllipse (r.getRight() - s * 0.19f, y - s * 0.07f, s * 0.14f, s * 0.14f);
            return;
        }
        case Accent: // ">" chevron
            p.startNewSubPath (r.getX() + s * 0.28f, r.getY() + s * 0.22f);
            p.lineTo (r.getRight() - s * 0.22f, r.getCentreY());
            p.lineTo (r.getX() + s * 0.28f, r.getBottom() - s * 0.22f);
            g.strokePath (p, juce::PathStrokeType (s * 0.12f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
            return;
        case OctUp:
            p.addTriangle (r.getCentreX(), r.getY() + s * 0.2f, r.getRight() - s * 0.2f, r.getBottom() - s * 0.25f,
                           r.getX() + s * 0.2f, r.getBottom() - s * 0.25f);
            g.fillPath (p);
            return;
        case OctDown:
            p.addTriangle (r.getX() + s * 0.2f, r.getY() + s * 0.25f, r.getRight() - s * 0.2f, r.getY() + s * 0.25f,
                           r.getCentreX(), r.getBottom() - s * 0.2f);
            g.fillPath (p);
            return;
    }
}

//==============================================================================
StepGrid::StepGrid (UiContext& c) : ctx (c)
{
    setTooltip ("Click the modifier rows to toggle (drag to paint). Drag a note up/down or scroll to change it. "
                "Click a step number to move the step-record cursor.");
}

juce::Rectangle<int> StepGrid::rowArea (Row row) const
{
    int y = 0, h = ledH;
    if (row >= NoteRow) { y = ledH + 4; h = noteH; }
    if (row >= RestRow) { y = ledH + 4 + noteH + 6 + (row - RestRow) * modH; h = modH; }
    return { 0, y, getWidth(), h };
}

StepGrid::Row StepGrid::rowAt (int y) const
{
    for (int r = LedRow; r <= DownRow; ++r)
        if (rowArea (static_cast<Row> (r)).contains (0, y))
            return static_cast<Row> (r);
    return None;
}

int StepGrid::columnAt (int x) const
{
    if (x < headerW) return -1;
    const float cw = static_cast<float> (getWidth() - headerW) / Pat::numSteps;
    return juce::jlimit (0, Pat::numSteps - 1, static_cast<int> ((x - headerW) / cw));
}

juce::Rectangle<int> StepGrid::cell (int col, Row row) const
{
    const float cw = static_cast<float> (getWidth() - headerW) / Pat::numSteps;
    const int x0 = headerW + juce::roundToInt (col * cw), x1 = headerW + juce::roundToInt ((col + 1) * cw);
    return rowArea (row).withX (x0).withWidth (x1 - x0).reduced (2, 1);
}

void StepGrid::paint (juce::Graphics& g)
{
    auto& proc = ctx.proc;
    const int slot = proc.getEditSlot();
    const int len = proc.bank.length (slot);
    const int playing = proc.getPlayingSlot() == slot ? proc.getPlayingStep() : -1;
    const bool stepRec = proc.isRecordArmed() && ! proc.isSeqRunning();
    const int edit = proc.getEditStep();

    // row headers: symbol + name
    struct H { Row row; const char* name; int kind; };
    const H heads[] = { { RestRow, "REST", SeqSymbols::Rest }, { SlideRow, "SLIDE", SeqSymbols::Slide },
                        { AccentRow, "ACCENT", SeqSymbols::Accent }, { UpRow, "OCT UP", SeqSymbols::OctUp },
                        { DownRow, "OCT DN", SeqSymbols::OctDown } };
    g.setFont (mono (11.0f));
    g.setColour (dim);
    g.drawText ("STEP", rowArea (LedRow).withWidth (headerW - 8), juce::Justification::centredLeft);
    g.drawText ("NOTE", rowArea (NoteRow).withWidth (headerW - 8), juce::Justification::centredLeft);
    for (const auto& h : heads)
    {
        auto r = rowArea (h.row).withWidth (headerW - 6);
        SeqSymbols::draw (g, static_cast<SeqSymbols::Kind> (h.kind), r.removeFromLeft (24).toFloat().reduced (2.0f), rowColour (h.kind));
        g.setColour (text);
        g.drawText (h.name, r.withTrimmedLeft (4), juce::Justification::centredLeft);
    }

    for (int col = 0; col < Pat::numSteps; ++col)
    {
        const auto st = proc.bank.get (slot, col);
        const bool inside = col < len;
        const float alpha = inside ? 1.0f : 0.28f;
        const bool beatStart = col % 4 == 0;

        // column backing (every other beat slightly lighter, like the 303's groups of 4)
        auto colArea = cell (col, NoteRow).getUnion (cell (col, DownRow)).expanded (1, 3).toFloat();
        g.setColour (((col / 4) % 2 == 0 ? panelLo : panel.darker (0.05f)).withAlpha (0.9f));
        g.fillRoundedRectangle (colArea, 3.0f);
        if (col == playing)
        {
            g.setColour (cyan.withAlpha (0.16f));
            g.fillRoundedRectangle (colArea, 3.0f);
            g.setColour (cyan.withAlpha (0.8f));
            g.drawRoundedRectangle (colArea, 3.0f, 1.5f);
        }
        if (stepRec && col == edit)
        {
            g.setColour (learn);
            g.drawRoundedRectangle (colArea.expanded (1.0f), 3.0f, 2.0f);
        }

        // LED + number
        auto led = cell (col, LedRow);
        const bool lit = col == playing;
        g.setColour (lit ? cyan : (beatStart ? orange.withAlpha (0.35f * alpha) : panel));
        g.fillRoundedRectangle (led.toFloat().withSizeKeepingCentre (led.getWidth() - 10.0f, 6.0f).withY (led.getY() + 2.0f), 2.0f);
        g.setColour ((lit ? cyan : text).withAlpha (alpha));
        g.setFont (mono (10.0f));
        g.drawText (juce::String (col + 1), led.withTrimmedTop (8), juce::Justification::centred);

        // note
        auto nc = cell (col, NoteRow);
        if (st.gate)
        {
            g.setColour (orange.withAlpha ((st.accent ? 0.35f : 0.18f) * alpha));
            g.fillRoundedRectangle (nc.toFloat(), 3.0f);
            g.setColour (orange.withAlpha (alpha));
            g.drawRoundedRectangle (nc.toFloat(), 3.0f, 1.0f);
            g.setColour ((st.accent ? juce::Colours::white : text).withAlpha (alpha));
            g.setFont (display (17.0f));
            juce::String name = noteNames[st.note];
            if (st.note == 12) name << "'";
            g.drawText (name, nc.withTrimmedBottom (10), juce::Justification::centred);
            // pitch bar: where the note sits in the octave
            const float frac = st.note / 12.0f;
            auto bar = nc.toFloat().reduced (6.0f, 0.0f).withTrimmedTop (nc.getHeight() - 8.0f).withHeight (3.0f);
            g.setColour (orange.withAlpha (0.25f * alpha));
            g.fillRect (bar);
            g.setColour (orange.withAlpha (alpha));
            g.fillRect (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * frac)));
            if (st.up || st.down)
                SeqSymbols::draw (g, st.up ? SeqSymbols::OctUp : SeqSymbols::OctDown,
                                  juce::Rectangle<float> (nc.getRight() - 15.0f, nc.getY() + 3.0f, 12.0f, 12.0f), orange.withAlpha (alpha));
        }
        else
        {
            g.setColour (dim.withAlpha (0.5f * alpha));
            g.drawRoundedRectangle (nc.toFloat(), 3.0f, 1.0f);
            SeqSymbols::draw (g, SeqSymbols::Rest, nc.toFloat().withSizeKeepingCentre (22.0f, 22.0f), dim.withAlpha (0.6f * alpha));
        }

        // modifier cells
        const bool on[] = { ! st.gate, st.slide, st.accent, st.up, st.down };
        for (int m = 0; m < 5; ++m)
        {
            auto mc = cell (col, static_cast<Row> (RestRow + m)).toFloat();
            const auto c = rowColour (m);
            if (on[m])
            {
                g.setColour (c.withAlpha (0.18f * alpha));
                g.fillRoundedRectangle (mc, 3.0f);
                g.setColour (c.withAlpha (0.85f * alpha));
                g.drawRoundedRectangle (mc, 3.0f, 1.0f);
                SeqSymbols::draw (g, static_cast<SeqSymbols::Kind> (m), mc.reduced (2.0f), c.withAlpha (alpha));
            }
            else
            {
                g.setColour (bg.withAlpha (0.55f));
                g.fillRoundedRectangle (mc, 3.0f);
                g.setColour (c.withAlpha (0.06f));
                g.fillEllipse (mc.withSizeKeepingCentre (5.0f, 5.0f));
            }
        }
    }
}

void StepGrid::tick()
{
    auto& proc = ctx.proc;
    const auto change = proc.bank.getChangeCount();
    const int step = proc.getPlayingSlot() == proc.getEditSlot() ? proc.getPlayingStep() : -1;
    const bool recCursor = proc.isRecordArmed() && ! proc.isSeqRunning();
    if (change != shownChange || step != shownStep || proc.getEditStep() != shownEdit
        || proc.getEditSlot() != shownSlot || recCursor != shownRecCursor)
    {
        shownChange = change; shownStep = step; shownEdit = proc.getEditStep();
        shownSlot = proc.getEditSlot(); shownRecCursor = recCursor;
        repaint();
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    auto& proc = ctx.proc;
    const int col = columnAt (e.x);
    const Row row = rowAt (e.y);
    dragRow = None;
    if (col < 0 || row == None) return;
    const int slot = proc.getEditSlot();
    auto st = proc.bank.get (slot, col);
    proc.setEditStep (col);

    if (row == LedRow) { repaint(); return; }

    if (row == NoteRow)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            for (int n = 12; n >= 0; --n)
                m.addItem (n + 1, juce::String (noteNames[n]) + (n == 12 ? "'" : ""), true, st.gate && st.note == n);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                             [this, slot, col, safe = juce::Component::SafePointer<StepGrid> (this)] (int r)
                             {
                                 if (safe == nullptr || r <= 0) return;
                                 auto s = ctx.proc.bank.get (slot, col);
                                 s.note = r - 1; s.gate = true;
                                 ctx.proc.bank.set (slot, col, s);
                             });
            return;
        }
        if (! st.gate) { st.gate = true; proc.bank.set (slot, col, st); }
        dragRow = NoteRow;
        dragCol = col;
        dragStartNote = st.note;
        if (! proc.isSeqRunning())
        {
            auditionNote = st.midiNote();
            proc.uiNote (auditionNote, st.accent ? 1.0f : 0.7f);
        }
        return;
    }

    // modifier rows: toggle, then paint the same value while dragging
    dragRow = row;
    switch (row)
    {
        case RestRow:   paintValue = st.gate; break;      // gate on -> make it a rest
        case SlideRow:  paintValue = ! st.slide; break;
        case AccentRow: paintValue = ! st.accent; break;
        case UpRow:     paintValue = ! st.up; break;
        case DownRow:   paintValue = ! st.down; break;
        case LedRow: case NoteRow: case None: default: break;
    }
    applyPaint (col);
}

void StepGrid::applyPaint (int col)
{
    auto& proc = ctx.proc;
    const int slot = proc.getEditSlot();
    auto st = proc.bank.get (slot, col);
    switch (dragRow)
    {
        case RestRow:   st.gate = ! paintValue; break;
        case SlideRow:  st.slide = paintValue; break;
        case AccentRow: st.accent = paintValue; break;
        case UpRow:     st.up = paintValue; if (paintValue) st.down = false; break;
        case DownRow:   st.down = paintValue; if (paintValue) st.up = false; break;
        case LedRow: case NoteRow: case None: default: return;
    }
    proc.bank.set (slot, col, st);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow == NoteRow)
    {
        auto& proc = ctx.proc;
        const int slot = proc.getEditSlot();
        auto st = proc.bank.get (slot, dragCol);
        const int n = juce::jlimit (0, 12, dragStartNote - e.getDistanceFromDragStartY() / 7);
        if (n != st.note)
        {
            st.note = n;
            proc.bank.set (slot, dragCol, st);
            if (auditionNote >= 0)
            {
                proc.uiNote (st.midiNote(), 0.7f);   // legato audition
                proc.uiNote (auditionNote, 0.0f);
                auditionNote = st.midiNote();
            }
        }
        return;
    }
    if (dragRow >= RestRow && dragRow <= DownRow)
    {
        const int col = columnAt (e.x);
        if (col >= 0) applyPaint (col);
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (auditionNote >= 0) { ctx.proc.uiNote (auditionNote, 0.0f); auditionNote = -1; }
    dragRow = None;
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int col = columnAt (e.x);
    if (col < 0 || rowAt (e.y) != NoteRow) return;
    auto& proc = ctx.proc;
    const int slot = proc.getEditSlot();
    auto st = proc.bank.get (slot, col);
    st.gate = true;
    st.note = juce::jlimit (0, 12, st.note + (w.deltaY > 0 ? 1 : -1));
    proc.bank.set (slot, col, st);
}

//==============================================================================
SlotStrip::SlotStrip (UiContext& c) : ctx (c) {}

int SlotStrip::slotAt (int x) const
{
    return juce::jlimit (0, Pat::numSlots - 1, x * Pat::numSlots / juce::jmax (1, getWidth()));
}

void SlotStrip::paint (juce::Graphics& g)
{
    const int edit = ctx.proc.getEditSlot();
    const int playing = ctx.proc.getPlayingSlot();
    const float cw = static_cast<float> (getWidth()) / Pat::numSlots;
    for (int s = 0; s < Pat::numSlots; ++s)
    {
        auto r = juce::Rectangle<float> (s * cw, 0.0f, cw, static_cast<float> (getHeight())).reduced (2.0f, 1.0f);
        const bool isEdit = s == edit, isPlaying = s == playing;
        const bool empty = ctx.proc.slotNames[static_cast<size_t> (s)] == "EMPTY" && ! ctx.proc.slotModified[static_cast<size_t> (s)];
        g.setColour (isEdit ? orange : panelLo);
        g.fillRoundedRectangle (r, 3.0f);
        if (isPlaying)
        {
            g.setColour (cyan.withAlpha (isEdit ? 0.5f : 0.3f));
            g.fillRoundedRectangle (r, 3.0f);
        }
        g.setColour (isPlaying ? cyan : orange.withAlpha (empty ? 0.35f : 0.8f));
        g.drawRoundedRectangle (r, 3.0f, isPlaying ? 2.0f : 1.0f);
        g.setColour (isEdit ? bg : (empty ? dim : text));
        g.setFont (display (11.0f));
        g.drawText (juce::String (s + 1), r.toNearestInt(), juce::Justification::centred);
    }
}

void SlotStrip::mouseMove (const juce::MouseEvent& e)
{
    const int s = slotAt (e.x);
    const int trigOct = ctx.proc.getTriggerBase();
    if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (getParentComponent()))
        juce::ignoreUnused (tip);
    if (ctx.setLcd)
        ctx.setLcd ("SLOT " + juce::String (s + 1) + "  //  " + ctx.proc.slotNames[static_cast<size_t> (s)]
                    + "  //  TRIGGER KEY " + trigKeyName (s, trigOct), orange);
}

void SlotStrip::mouseDown (const juce::MouseEvent& e)
{
    const int s = slotAt (e.x);
    auto& proc = ctx.proc;
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addSectionHeader ("SLOT " + juce::String (s + 1));
        m.addItem (1, "Copy");
        m.addItem (2, "Paste" + (clipboard ? " \"" + clipboardName + "\"" : juce::String()), clipboard.has_value());
        m.addItem (3, "Clear");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this, s, safe = juce::Component::SafePointer<SlotStrip> (this)] (int r)
                         {
                             if (safe == nullptr) return;
                             auto& p = ctx.proc;
                             if (r == 1) { clipboard = p.bank.load (s); clipboardName = p.slotNames[static_cast<size_t> (s)]; }
                             if (r == 2 && clipboard)
                             {
                                 p.bank.store (s, *clipboard);
                                 p.slotNames[static_cast<size_t> (s)] = clipboardName;
                             }
                             if (r == 3)
                             {
                                 p.bank.store (s, Pat::Pattern::empty());
                                 p.slotNames[static_cast<size_t> (s)] = "EMPTY";
                                 p.slotModified[static_cast<size_t> (s)] = false;
                             }
                             if (onSlotChanged) onSlotChanged();
                             repaint();
                         });
        return;
    }
    proc.setEditSlot (s);
    if (onSlotChanged) onSlotChanged();
    repaint();
}

void SlotStrip::tick()
{
    const int p = ctx.proc.getPlayingSlot(), ed = ctx.proc.getEditSlot();
    const bool trig = ctx.proc.isTriggeredByMidi();
    if (p != shownPlaying || ed != shownEdit || trig != shownTrig)
    {
        shownPlaying = p; shownEdit = ed; shownTrig = trig;
        repaint();
    }
}

//==============================================================================
MiniKeyboard::MiniKeyboard (UiContext& c) : ctx (c) {}

juce::Rectangle<float> MiniKeyboard::keyRect (int key) const
{
    const float ww = static_cast<float> (getWidth()) / 8.0f, h = static_cast<float> (getHeight());
    static const int whiteIndex[] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6, 7 };
    if (! isBlack (key))
        return { whiteIndex[key] * ww, 0.0f, ww, h };
    const int leftWhite = whiteIndex[key - 1];
    return { (leftWhite + 1) * ww - ww * 0.32f, 0.0f, ww * 0.64f, h * 0.6f };
}

int MiniKeyboard::keyAt (juce::Point<int> p) const
{
    const auto pf = p.toFloat();
    for (int k = 0; k <= 12; ++k)
        if (isBlack (k) && keyRect (k).contains (pf)) return k;
    for (int k = 0; k <= 12; ++k)
        if (! isBlack (k) && keyRect (k).contains (pf)) return k;
    return -1;
}

void MiniKeyboard::paint (juce::Graphics& g)
{
    // the key the pattern is playing right now (in the edit slot)
    int playingKey = -1;
    const int slot = ctx.proc.getEditSlot();
    if (ctx.proc.getPlayingSlot() == slot && ctx.proc.getPlayingStep() >= 0)
    {
        const auto st = ctx.proc.bank.get (slot, ctx.proc.getPlayingStep());
        if (st.gate) playingKey = st.note;
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k <= 12; ++k)
        {
            if (isBlack (k) != (pass == 1)) continue;
            auto r = keyRect (k).reduced (1.0f, 0.0f);
            const bool down = k == held;
            juce::Colour face = isBlack (k) ? juce::Colour (0xff17133a) : text.withAlpha (0.92f);
            if (k == playingKey) face = face.interpolatedWith (cyan, 0.55f);
            if (down) face = accent ? pink : orange;
            g.setColour (face);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (bg);
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
        }
    g.setColour (bg.withAlpha (0.8f));
    g.setFont (mono (9.0f));
    g.drawText ("C", keyRect (0).withTrimmedTop (getHeight() - 14.0f).toNearestInt(), juce::Justification::centred);
    g.drawText ("C'", keyRect (12).withTrimmedTop (getHeight() - 14.0f).toNearestInt(), juce::Justification::centred);
}

void MiniKeyboard::press (int key)
{
    if (key == held || key < 0) return;
    const int note = Pat::baseNote + key + 12 * octave;
    ctx.proc.uiNote (note, accent ? 1.0f : 0.7f);   // new note first: dragging across keys = legato / slide
    if (heldNote >= 0) ctx.proc.uiNote (heldNote, 0.0f);
    held = key;
    heldNote = note;
    repaint();
}

void MiniKeyboard::release()
{
    if (heldNote >= 0) ctx.proc.uiNote (heldNote, 0.0f);
    held = heldNote = -1;
    repaint();
}

void MiniKeyboard::mouseDown (const juce::MouseEvent& e) { press (keyAt (e.getPosition())); }
void MiniKeyboard::mouseDrag (const juce::MouseEvent& e) { const int k = keyAt (e.getPosition()); if (k >= 0) press (k); }
void MiniKeyboard::mouseUp (const juce::MouseEvent&)     { release(); }

void MiniKeyboard::tick()
{
    int p = -1;
    const int slot = ctx.proc.getEditSlot();
    if (ctx.proc.getPlayingSlot() == slot && ctx.proc.getPlayingStep() >= 0)
        p = ctx.proc.getPlayingStep();
    if (p != shownPlaying) { shownPlaying = p; repaint(); }
}

//==============================================================================
SequencerPanel::SequencerPanel (UiContext& c)
    : ctx (c),
      mode (c, "seq_mode", orange), start (c, "seq_start", orange), keys (c, "seq_trigbase", orange),
      swing (c, "seq_swing", orange), gate (c, "seq_gate", orange),
      keyboard (c), slots (c), grid (c)
{
    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonOnColourId, cyan);
    playButton.setTooltip ("Play the selected slot (follows the DAW's position when SYNC is on and the DAW is playing)");
    playButton.onClick = [this] { ctx.proc.setPlay (playButton.getToggleState()); };
    recButton.setClickingTogglesState (true);
    recButton.setColour (juce::TextButton::buttonOnColourId, pink);
    recButton.setTooltip ("While playing: live record (notes are quantised to the nearest step). "
                          "While stopped: step record at the yellow cursor. Overlapping notes = slide, velocity 100+ = accent.");
    recButton.onClick = [this]
    {
        ctx.proc.setRecord (recButton.getToggleState());
        if (recButton.getToggleState() && ! ctx.proc.isSeqRunning()) ctx.proc.setEditStep (0);
    };

    for (auto* b : { &kbDown, &kbUp, &accButton, &lenDown, &lenUp, &shiftLeft, &shiftRight, &randButton, &clearButton, &restButton })
        addAndMakeVisible (b);
    accButton.setClickingTogglesState (true);
    accButton.setColour (juce::TextButton::buttonOnColourId, pink);
    accButton.setTooltip ("Keyboard plays accented notes");
    accButton.onClick = [this] { keyboard.accent = accButton.getToggleState(); };
    kbDown.onClick = [this] { keyboard.octave = juce::jmax (-1, keyboard.octave - 1); repaint (kbLabelArea); };
    kbUp.onClick   = [this] { keyboard.octave = juce::jmin (1, keyboard.octave + 1); repaint (kbLabelArea); };

    auto& bank = ctx.proc.bank;
    lenDown.onClick = [this, &bank] { bank.setLength (ctx.proc.getEditSlot(), bank.length (ctx.proc.getEditSlot()) - 1); };
    lenUp.onClick   = [this, &bank] { bank.setLength (ctx.proc.getEditSlot(), bank.length (ctx.proc.getEditSlot()) + 1); };
    shiftLeft.onClick  = [this] { ctx.proc.shiftEditSlot (-1); };
    shiftRight.onClick = [this] { ctx.proc.shiftEditSlot (+1); };
    shiftLeft.setTooltip ("Shift the pattern one step earlier");
    shiftRight.setTooltip ("Shift the pattern one step later");
    randButton.setTooltip ("Generate a random acid line in this slot");
    randButton.onClick = [this]
    {
        ctx.proc.randomiseEditSlot();
        ctx.proc.slotNames[static_cast<size_t> (ctx.proc.getEditSlot())] = "RANDOM";
    };
    clearButton.onClick = [this] { ctx.proc.clearEditSlot(); };
    restButton.setTooltip ("Step record: enter a rest at the cursor and move on");
    restButton.onClick = [this]
    {
        auto& p = ctx.proc;
        const int slot = p.getEditSlot(), step = p.getEditStep();
        auto st = p.bank.get (slot, step);
        st.gate = false; st.slide = false;
        p.bank.set (slot, step, st);
        p.setEditStep ((step + 1) % p.bank.length (slot));
    };

    patternLcd.onClick = [this] { showPatternMenu(); };
    patternLcd.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    slots.onSlotChanged = [this] { lastChange = ctx.proc.bank.getChangeCount(); updatePatternText(); };

    for (juce::Component* comp : std::initializer_list<juce::Component*> { &playButton, &recButton, &mode, &start, &keys,
                                                                           &swing, &gate, &keyboard, &slots, &patternLcd, &grid })
        addAndMakeVisible (comp);
    lastChange = ctx.proc.bank.getChangeCount();
    updatePatternText();
}

void SequencerPanel::markLoaded()
{
    lastChange = ctx.proc.bank.getChangeCount();
    ctx.proc.slotModified[static_cast<size_t> (ctx.proc.getEditSlot())] = false;
    updatePatternText();
}

void SequencerPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    r.removeFromTop (34);
    r.removeFromBottom (10);
    statusArea = juce::Rectangle<int> (getWidth() / 2 - 100, 10, getWidth() / 2 + 86, 18);

    // ---- left column: transport, play mode, swing / gate, keyboard
    leftArea = r.removeFromLeft (250);
    auto l = leftArea;
    auto row = l.removeFromTop (32);
    playButton.setBounds (row.removeFromLeft (121));
    row.removeFromLeft (8);
    recButton.setBounds (row);
    l.removeFromTop (18);
    mode.setBounds (l.removeFromTop (24));
    l.removeFromTop (18);
    row = l.removeFromTop (24);
    start.setBounds (row.removeFromLeft (121));
    row.removeFromLeft (8);
    keys.setBounds (row);
    l.removeFromTop (4);
    auto kb = l.removeFromBottom (64);
    l.removeFromBottom (4);
    auto kbRow = l.removeFromBottom (20);
    kbDown.setBounds (kbRow.removeFromLeft (24));
    kbLabelArea = kbRow.removeFromLeft (70);
    kbUp.setBounds (kbRow.removeFromLeft (24));
    accButton.setBounds (kbRow.removeFromRight (50));
    keyboard.setBounds (kb);
    auto knobs = l;
    swing.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
    gate.setBounds (knobs);

    // ---- right: slots, toolbar, grid
    r.removeFromLeft (18);
    slots.setBounds (r.removeFromTop (24).withTrimmedLeft (StepGrid::headerW));
    r.removeFromTop (8);
    auto tools = r.removeFromTop (28);
    patternLcd.setBounds (tools.removeFromLeft (360));
    tools.removeFromLeft (16);
    lenDown.setBounds (tools.removeFromLeft (24));
    lenArea = tools.removeFromLeft (70);
    lenUp.setBounds (tools.removeFromLeft (24));
    tools.removeFromLeft (16);
    shiftLeft.setBounds (tools.removeFromLeft (28));
    tools.removeFromLeft (4);
    shiftRight.setBounds (tools.removeFromLeft (28));
    tools.removeFromLeft (16);
    randButton.setBounds (tools.removeFromLeft (84));
    tools.removeFromLeft (8);
    clearButton.setBounds (tools.removeFromLeft (76));
    tools.removeFromLeft (8);
    restButton.setBounds (tools.removeFromLeft (70));
    r.removeFromTop (8);
    grid.setBounds (r);
}

void SequencerPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), "SEQUENCER // 16 STEP", orange, cyan);

    g.setFont (mono (11.0f));
    g.setColour (shownStatus.startsWith ("REC") || shownStatus.startsWith ("STEP") ? pink : cyan);
    g.drawText (shownStatus, statusArea, juce::Justification::centredRight, false);

    g.setColour (dim);
    g.setFont (mono (10.0f));
    g.drawText ("PLAY MODE", mode.getBounds().translated (0, -15).withHeight (14), juce::Justification::bottomLeft);
    g.drawText ("PATTERN START", start.getBounds().translated (0, -15).withHeight (14), juce::Justification::bottomLeft);
    g.drawText ("TRIGGER KEYS FROM", keys.getBounds().translated (0, -15).withHeight (14), juce::Justification::bottomLeft);
    g.drawText ("KEYS OCT " + juce::String (keyboard.octave > 0 ? "+" : "") + juce::String (keyboard.octave),
                kbLabelArea, juce::Justification::centred);

    g.setColour (text);
    g.setFont (mono (12.0f));
    g.drawText ("LEN " + juce::String (shownLen), lenArea, juce::Justification::centred);

    // divider between the left column and the grid
    g.setColour (orange.withAlpha (0.25f));
    g.fillRect (leftArea.getRight() + 8, leftArea.getY(), 1, leftArea.getHeight());
}

juce::String SequencerPanel::statusText() const
{
    auto& p = ctx.proc;
    const bool rec = p.isRecordArmed(), running = p.isSeqRunning();
    if (rec && running && ! p.isTriggeredByMidi()) return "REC // LIVE - PLAY NOTES, THEY'RE QUANTISED TO THE NEAREST STEP";
    if (rec && ! running) return "STEP REC // STEP " + juce::String (p.getEditStep() + 1) + " - PLAY A KEY, REST FOR A REST";
    const bool free = p.apvts.getRawParameterValue ("seq_mode")->load() >= 0.5f;
    if (running)
    {
        juce::String t = "PLAYING SLOT " + juce::String (p.getPlayingSlot() + 1);
        if (p.isTriggeredByMidi()) t << " (MIDI KEY)";
        if (p.getTranspose() != 0) t << "  TRANSPOSE " << (p.getTranspose() > 0 ? "+" : "") << p.getTranspose();
        return t;
    }
    if (free) return "FREE MODE // EVERY KEY PLAYS THE SYNTH";
    const int trigOct = p.getTriggerBase();
    return "PATTERN MODE // " + trigKeyName (0, trigOct) + "-" + trigKeyName (15, trigOct)
           + " PLAY SLOTS 1-16, HIGHER KEYS TRANSPOSE";
}

void SequencerPanel::updatePatternText()
{
    auto& p = ctx.proc;
    const int s = p.getEditSlot();
    patternLcd.setText ("SLOT " + juce::String (s + 1).paddedLeft ('0', 2) + " // " + p.slotNames[static_cast<size_t> (s)]
                        + (p.slotModified[static_cast<size_t> (s)] ? " *" : ""));
}

void SequencerPanel::tick()
{
    auto& p = ctx.proc;
    if (const auto change = p.bank.getChangeCount(); change != lastChange)
    {
        lastChange = change;
        p.slotModified[static_cast<size_t> (p.getEditSlot())] = true;
    }
    updatePatternText();
    patternLcd.tick();

    playButton.setToggleState (p.isPlayRequested(), juce::dontSendNotification);
    recButton.setToggleState (p.isRecordArmed(), juce::dontSendNotification);
    const auto status = statusText();
    const int len = p.bank.length (p.getEditSlot());
    if (status != shownStatus || len != shownLen)
    {
        shownStatus = status;
        shownLen = len;
        repaint (statusArea);
        repaint (lenArea);
    }
    slots.tick();
    grid.tick();
    keyboard.tick();
}

//==============================================================================
void SequencerPanel::showPatternMenu()
{
    auto& p = ctx.proc;
    auto& pm = p.patterns;
    pm.refresh();
    const auto& entries = pm.getEntries();
    const int slot = p.getEditSlot();
    const auto current = p.slotNames[static_cast<size_t> (slot)];
    const int currentIndex = pm.indexOf (current);
    const bool isUser = currentIndex >= pm.getNumFactory();

    enum { saveId = 1, saveAsId, deleteId, folderId, loadBase = 1000 };
    juce::PopupMenu m;
    m.addSectionHeader ("LOAD INTO SLOT " + juce::String (slot + 1) + " - FACTORY");
    for (int i = 0; i < static_cast<int> (entries.size()); ++i)
        if (entries[static_cast<size_t> (i)].factory)
            m.addItem (loadBase + i, entries[static_cast<size_t> (i)].name, true, i == currentIndex);
    m.addSectionHeader ("USER");
    if (static_cast<int> (entries.size()) == pm.getNumFactory())
        m.addItem (-1, "(no user patterns yet - use Save As)", false);
    for (int i = 0; i < static_cast<int> (entries.size()); ++i)
        if (! entries[static_cast<size_t> (i)].factory)
            m.addItem (loadBase + i, entries[static_cast<size_t> (i)].name, true, i == currentIndex);
    m.addSeparator();
    m.addItem (saveId, "Save \"" + current + "\"", isUser);
    m.addItem (saveAsId, "Save As...");
    m.addItem (deleteId, "Delete \"" + current + "\" from the library...", isUser);
    m.addSeparator();
    m.addItem (folderId, "Open pattern folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternLcd),
                     [this, currentIndex, current, safe = juce::Component::SafePointer<SequencerPanel> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& proc = ctx.proc;
                         if (r >= loadBase)
                         {
                             if (proc.loadPatternIntoEditSlot (r - loadBase)) markLoaded();
                             else patternLcd.showMessage ("COULDN'T READ THAT PATTERN FILE", pink);
                         }
                         else if (r == saveId)   saveTo (current);
                         else if (r == saveAsId) saveAsDialog();
                         else if (r == deleteId)
                         {
                             juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                 .withIconType (juce::MessageBoxIconType::WarningIcon)
                                 .withTitle ("DELETE PATTERN?")
                                 .withMessage ("Delete \"" + current + "\" from the pattern library? The slot keeps its notes.")
                                 .withButton ("DELETE").withButton ("CANCEL").withAssociatedComponent (this),
                                 [this, currentIndex, current, safe] (int b)
                                 {
                                     if (safe == nullptr || b != 1) return;
                                     if (ctx.proc.patterns.deleteUser (currentIndex))
                                         patternLcd.showMessage ("DELETED: " + current, orange);
                                 });
                         }
                         else if (r == folderId)
                         {
                             const auto folder = PatternManager::getUserFolder();
                             folder.createDirectory();
                             folder.startAsProcess();
                         }
                         updatePatternText();
                     });
}

void SequencerPanel::saveAsDialog()
{
    auto* w = new juce::AlertWindow ("SAVE PATTERN", "Name for this pattern:", juce::MessageBoxIconType::NoIcon, this);
    const auto current = ctx.proc.slotNames[static_cast<size_t> (ctx.proc.getEditSlot())];
    w->addTextEditor ("name", current == "EMPTY" || current == "RANDOM" ? "MY PATTERN" : current);
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<SequencerPanel> (this), w] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const auto name = PatternManager::sanitiseName (w->getTextEditorContents ("name"));
            if (name.isEmpty()) { safe->patternLcd.showMessage ("PATTERN NAME CAN'T BE EMPTY", Theme::pink); return; }
            if (safe->ctx.proc.patterns.userPatternExists (name)
                && name != safe->ctx.proc.slotNames[static_cast<size_t> (safe->ctx.proc.getEditSlot())])
            {
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle ("REPLACE PATTERN?")
                    .withMessage ("A pattern called \"" + name + "\" already exists. Replace it?")
                    .withButton ("REPLACE").withButton ("CANCEL").withAssociatedComponent (safe.getComponent()),
                    [safe, name] (int r) { if (safe != nullptr && r == 1) safe->saveTo (name); });
                return;
            }
            safe->saveTo (name);
        }), true);
}

void SequencerPanel::saveTo (const juce::String& rawName)
{
    auto& p = ctx.proc;
    const auto name = PatternManager::sanitiseName (rawName);
    auto pat = p.bank.load (p.getEditSlot());
    const float sw = p.apvts.getRawParameterValue ("seq_swing")->load();
    const float gt = p.apvts.getRawParameterValue ("seq_gate")->load();
    if (p.patterns.saveUser (name, pat, sw, gt))
    {
        p.slotNames[static_cast<size_t> (p.getEditSlot())] = name;
        markLoaded();
        patternLcd.showMessage ("SAVED: " + name, orange);
    }
    else
        patternLcd.showMessage ("COULDN'T SAVE - CHECK THE PATTERN FOLDER", pink);
}
