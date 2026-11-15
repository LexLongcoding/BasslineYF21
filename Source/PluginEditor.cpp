// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

using namespace Theme;

namespace
{
    constexpr int row1Y = 105, row1H = 300;
    constexpr int row2Y = row1Y + row1H + 10, row2H = 170;
    constexpr int seqY = row2Y + row2H + 10;
}

//==============================================================================
void BasslineAudioProcessorEditor::Canvas::paint (juce::Graphics& g)
{
    if (background.isNull())
    {
        background = juce::Image (juce::Image::RGB, designWidth, designHeight, true);
        juce::Graphics bgG (background);
        bgG.fillAll (bg);
        bgG.setColour (cyan.withAlpha (0.035f));   // scanlines
        for (int y = 0; y < designHeight; y += 3)
            bgG.fillRect (0, y, designWidth, 1);

        // logo
        auto logo = juce::Rectangle<int> (20, 20, 520, 40);
        bgG.setFont (display (32.0f, true));
        const juce::String word ("BASSLINE ");
        const int w = juce::GlyphArrangement::getStringWidthInt (bgG.getCurrentFont(), word);
        bgG.setColour (pink.withAlpha (0.35f));
        bgG.drawText (word, logo.translated (0, 1), juce::Justification::centredLeft, false);
        bgG.setColour (pink);
        bgG.drawText (word, logo, juce::Justification::centredLeft, false);
        bgG.setColour (orange);
        bgG.drawText ("YF-21", logo.withTrimmedLeft (w), juce::Justification::centredLeft, false);
        bgG.setColour (text);
        bgG.setFont (mono (11.0f));
        bgG.drawText ("ACID BASS SYNTHESIZER // 3 OSC + 16 STEP SEQUENCER", 21, 60, 520, 14, juce::Justification::centredLeft, false);

        // tri-colour bar
        const int barY = 82, x0 = 20, bw = designWidth - 40;
        bgG.setColour (orange); bgG.fillRect (x0, barY, bw * 3 / 6, 3);
        bgG.setColour (pink);   bgG.fillRect (x0 + bw * 3 / 6, barY, bw * 2 / 6, 3);
        bgG.setColour (cyan);   bgG.fillRect (x0 + bw * 5 / 6, barY, bw - bw * 5 / 6, 3);
    }
    g.drawImageAt (background, 0, 0);
}

//==============================================================================
BasslineAudioProcessorEditor::BasslineAudioProcessorEditor (BasslineAudioProcessor& p)
    : AudioProcessorEditor (p), synth (p), ctx (p),
      bpm (ctx), syncButton (ctx, "seq_sync", "SYNC", cyan),
      osc1 (ctx, 0), osc2 (ctx, 1), osc3 (ctx, 2), lfo (ctx), filter (ctx), amp (ctx), fold (ctx), dist (ctx),
      scope (ctx), master (ctx), sequencer (ctx), overlay (ctx, canvas)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    ctx.setLcd       = [this] (const juce::String& t, juce::Colour c) { setLcd (t, c); };
    ctx.armKnobLearn = [this] (int param) { armKnobLearn (param); };

    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, designWidth, designHeight);

    syncButton.setTooltip ("Follow the DAW's tempo and position");
    learnButton.setClickingTogglesState (true);
    learnButton.setColour (juce::TextButton::buttonOnColourId, learn);
    learnButton.onClick = [this] { setLearnMode (learnButton.getToggleState()); };
    panicButton.setColour (juce::TextButton::buttonOnColourId, pink);
    panicButton.setColour (juce::TextButton::buttonColourId, pink.withAlpha (0.25f));
    panicButton.setTooltip ("All notes off: stops the sequencer and silences the synth immediately");
    panicButton.onClick = [this] { synth.panic(); setLcd ("PANIC // ALL NOTES OFF, SEQUENCER STOPPED", pink); };
    lcd.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    lcd.onClick = [this] { showPatchMenu(); };
    prevPatch.onClick = [this] { synth.patches.loadNext (-1); updatePatchText(); lcd.clearMessage(); };
    nextPatch.onClick = [this] { synth.patches.loadNext (+1); updatePatchText(); lcd.clearMessage(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &bpm, &syncButton, &lcd, &prevPatch, &nextPatch, &learnButton, &panicButton,
             &osc1, &osc2, &osc3, &lfo, &filter, &amp, &fold, &dist, &scope, &master, &sequencer })
        canvas.addAndMakeVisible (c);
    canvas.addChildComponent (overlay);
    overlay.onArmed = [this] (int param) { setLcd ("LEARN: MOVE A KNOB OR FADER FOR " + paramTitle (param), learn); };
    overlay.onCancelled = [this] { setLcd ("LEARN: CLICK A CONTROL  //  ESC TO EXIT", learn); };

    updatePatchText();
    seenCcLearnCount = synth.getCcLearnCount();
    sendLookAndFeelChange();

    const float savedScale = juce::jlimit (0.5f, 1.25f, synth.uiScale);
    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 5 / 4, designHeight * 5 / 4);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    setSize (juce::roundToInt (designWidth * savedScale), juce::roundToInt (designHeight * savedScale));
    startTimerHz (30);
}

BasslineAudioProcessorEditor::~BasslineAudioProcessorEditor()
{
    synth.armCcLearn (-1);
    setLookAndFeel (nullptr);
}

void BasslineAudioProcessorEditor::setLearnMode (bool on)
{
    ctx.learnMode = on;
    learnButton.setToggleState (on, juce::dontSendNotification);
    synth.armCcLearn (-1);
    overlay.setVisible (on);
    overlay.toFront (false);
    if (on) setLcd ("LEARN: CLICK A CONTROL  //  ESC TO EXIT", learn);
    else    lcd.clearMessage();
}

void BasslineAudioProcessorEditor::armKnobLearn (int param)
{
    synth.armCcLearn (param);
    setLcd ("LEARN: MOVE A KNOB OR FADER FOR " + paramTitle (param) + "  //  ESC CANCELS", learn);
}

bool BasslineAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
        return false;
    if (synth.getArmedCcParam() >= 0)
    {
        synth.armCcLearn (-1);
        setLcd (ctx.learnMode ? "LEARN: CANCELLED  //  CLICK A CONTROL" : "LEARN: CANCELLED", learn);
        overlay.repaint();
        return true;
    }
    if (ctx.learnMode) { setLearnMode (false); return true; }
    return false;
}

void BasslineAudioProcessorEditor::timerCallback()
{
    updatePatchText();
    lcd.tick();
    bpm.tick();
    for (SynthPanel* p : std::initializer_list<SynthPanel*> { &osc1, &osc2, &osc3, &lfo, &filter, &amp, &fold, &dist, &scope, &master })
        p->tick();
    sequencer.tick();

    bool learnChanged = false;
    if (const int n = synth.getCcLearnCount(); n != seenCcLearnCount)
    {
        seenCcLearnCount = n;
        learnChanged = true;
        const int cc = synth.getLastLearnedCc();
        for (int i = 0; i < Params::numParams; ++i)
            if (synth.ccForParam (i) == cc)
                setLcd (paramTitle (i) + " <- CC " + juce::String (cc), learn);
    }
    if (overlay.isVisible() && (learnChanged || ++tickCount % 3 == 0))
        overlay.repaint();
}

void BasslineAudioProcessorEditor::resized()
{
    if (getWidth() <= 0) return;
    const float scale = static_cast<float> (getWidth()) / static_cast<float> (designWidth);
    canvas.setTransform (juce::AffineTransform::scale (scale));
    synth.uiScale = scale;

    // header, right-aligned
    auto header = juce::Rectangle<int> (20, 26, designWidth - 40, 38);
    learnButton.setBounds (header.removeFromRight (130).reduced (0, 2));
    header.removeFromRight (10);
    panicButton.setBounds (header.removeFromRight (90).reduced (0, 2));
    header.removeFromRight (16);
    nextPatch.setBounds (header.removeFromRight (26).reduced (4, 10));
    header.removeFromRight (2);
    lcd.setBounds (header.removeFromRight (300).reduced (0, 2));
    header.removeFromRight (2);
    prevPatch.setBounds (header.removeFromRight (26).reduced (4, 10));
    header.removeFromRight (24);
    syncButton.setBounds (header.removeFromRight (70).reduced (0, 4));
    header.removeFromRight (8);
    bpm.setBounds (header.removeFromRight (150));

    // row 1: OSC 1-3, LFO, FILTER
    int x = 20;
    auto place = [&x] (juce::Component& c, int y, int w, int h) { c.setBounds (x, y, w, h); x += w + 10; };
    place (osc1, row1Y, 215, row1H);
    place (osc2, row1Y, 215, row1H);
    place (osc3, row1Y, 215, row1H);
    place (lfo, row1Y, 230, row1H);
    place (filter, row1Y, designWidth - 20 - x, row1H);

    // row 2: AMP, FOLD, DIST, SCOPE, MASTER
    x = 20;
    place (amp, row2Y, 290, row2H);
    place (fold, row2Y, 230, row2H);
    place (dist, row2Y, 300, row2H);
    place (scope, row2Y, 300, row2H);
    place (master, row2Y, designWidth - 20 - x, row2H);

    sequencer.setBounds (20, seqY, designWidth - 40, designHeight - seqY - 10);
    overlay.setBounds (0, row1Y - 8, designWidth, designHeight - row1Y + 8);
}

//==============================================================================
// Patches (same flow as Decu6000)
void BasslineAudioProcessorEditor::updatePatchText()
{
    auto& pm = synth.patches;
    const int n = pm.getDisplayNumber();
    lcd.setText ("PATCH " + (n > 0 ? juce::String (n).paddedLeft ('0', 2) : juce::String ("--"))
                 + " // " + pm.getCurrentName() + (pm.isModified() ? " *" : ""));
}

void BasslineAudioProcessorEditor::showPatchMenu()
{
    auto& pm = synth.patches;
    pm.refresh();
    const auto& entries = pm.getEntries();

    enum { saveId = 1, saveAsId, deleteId, folderId, loadBase = 1000 };
    juce::PopupMenu m;
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < static_cast<int> (entries.size()); ++i)
        if (entries[static_cast<size_t> (i)].factory)
            m.addItem (loadBase + i, entries[static_cast<size_t> (i)].name, true, i == pm.getCurrentIndex());
    m.addSectionHeader ("USER");
    if (static_cast<int> (entries.size()) == pm.getNumFactory())
        m.addItem (-1, "(no user patches yet - use Save As)", false);
    for (int i = 0; i < static_cast<int> (entries.size()); ++i)
        if (! entries[static_cast<size_t> (i)].factory)
            m.addItem (loadBase + i, entries[static_cast<size_t> (i)].name, true, i == pm.getCurrentIndex());
    m.addSeparator();
    m.addItem (saveId, "Save \"" + pm.getCurrentName() + "\"", pm.isCurrentUserPatch());
    m.addItem (saveAsId, "Save As...");
    m.addItem (deleteId, "Delete \"" + pm.getCurrentName() + "\"...", pm.isCurrentUserPatch());
    m.addSeparator();
    m.addItem (folderId, "Open patch folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lcd),
                     [safe = juce::Component::SafePointer<BasslineAudioProcessorEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& p = safe->synth.patches;
                         if (r >= loadBase)
                         {
                             if (! p.load (r - loadBase)) safe->setLcd ("COULDN'T READ THAT PATCH FILE", pink);
                             else safe->lcd.clearMessage();
                         }
                         else if (r == saveId)   safe->saveTo (p.getCurrentName());
                         else if (r == saveAsId) safe->saveAsDialog();
                         else if (r == deleteId) safe->confirmDelete();
                         else if (r == folderId)
                         {
                             const auto folder = PatchManager::getUserFolder();
                             folder.createDirectory();
                             folder.startAsProcess();
                         }
                         safe->updatePatchText();
                     });
}

void BasslineAudioProcessorEditor::saveAsDialog()
{
    auto* w = new juce::AlertWindow ("SAVE PATCH", "Name for this patch:", juce::MessageBoxIconType::NoIcon, this);
    const auto& pm = synth.patches;
    w->addTextEditor ("name", pm.isCurrentUserPatch() || pm.getCurrentName() != "INIT" ? pm.getCurrentName() : "MY PATCH");
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<BasslineAudioProcessorEditor> (this), w] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const auto name = PatchManager::sanitiseName (w->getTextEditorContents ("name"));
            if (name.isEmpty()) { safe->setLcd ("PATCH NAME CAN'T BE EMPTY", pink); return; }
            if (safe->synth.patches.userPatchExists (name) && name != safe->synth.patches.getCurrentName())
            {
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                  .withTitle ("REPLACE PATCH?")
                                                  .withMessage ("A patch called \"" + name + "\" already exists. Replace it?")
                                                  .withButton ("REPLACE").withButton ("CANCEL")
                                                  .withAssociatedComponent (safe.getComponent()),
                                              [safe, name] (int r) { if (safe != nullptr && r == 1) safe->saveTo (name); });
                return;
            }
            safe->saveTo (name);
        }), true);
}

void BasslineAudioProcessorEditor::saveTo (const juce::String& name)
{
    if (synth.patches.saveUser (name)) setLcd ("SAVED: " + PatchManager::sanitiseName (name), orange);
    else setLcd ("COULDN'T SAVE - CHECK THE PATCH FOLDER", pink);
    updatePatchText();
}

void BasslineAudioProcessorEditor::confirmDelete()
{
    const auto name = synth.patches.getCurrentName();
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("DELETE PATCH?")
                                      .withMessage ("Delete the patch \"" + name + "\"? The current sound stays loaded.")
                                      .withButton ("DELETE").withButton ("CANCEL")
                                      .withAssociatedComponent (this),
                                  [safe = juce::Component::SafePointer<BasslineAudioProcessorEditor> (this), name] (int r)
                                  {
                                      if (safe == nullptr || r != 1) return;
                                      auto& pm = safe->synth.patches;
                                      if (pm.deleteUser (pm.getCurrentIndex())) safe->setLcd ("DELETED: " + name, orange);
                                      safe->updatePatchText();
                                  });
}
