// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Widgets.h"
#include <optional>

namespace SeqSymbols
{
    enum Kind { Rest, Slide, Accent, OctUp, OctDown };
    void draw (juce::Graphics&, Kind, juce::Rectangle<float> area, juce::Colour);
}

// The 16-step grid: step LEDs, the note row and one row per modifier, each with its symbol.
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (UiContext&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick();

    static constexpr int headerW = 92, ledH = 22, noteH = 44, modH = 27;

private:
    enum Row { LedRow, NoteRow, RestRow, SlideRow, AccentRow, UpRow, DownRow, None };
    Row rowAt (int y) const;
    int columnAt (int x) const;
    juce::Rectangle<int> cell (int col, Row row) const;
    juce::Rectangle<int> rowArea (Row row) const;
    void applyPaint (int col);

    UiContext& ctx;
    Row dragRow = None;
    bool paintValue = false;
    int dragCol = -1, dragStartNote = 0, auditionNote = -1;
    juce::uint32 shownChange = 0;
    int shownStep = -2, shownEdit = -2, shownSlot = -2;
    bool shownRecCursor = false;
};

// 16 slot buttons. Click to edit a slot; right-click to copy / paste / clear.
class SlotStrip : public juce::Component
{
public:
    explicit SlotStrip (UiContext&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void tick();
    std::function<void()> onSlotChanged;

private:
    int slotAt (int x) const;
    UiContext& ctx;
    std::optional<Pat::Pattern> clipboard;
    juce::String clipboardName;
    int shownPlaying = -2, shownEdit = -2, shownKeys = 0;
    bool shownTrig = false;
};

// 13-key keyboard (C to C, like the 303's) for auditioning and step entry.
// Drag across keys to play legato (= slide). Never triggers pattern slots.
class MiniKeyboard : public juce::Component
{
public:
    explicit MiniKeyboard (UiContext&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();

    int octave = 0;          // -1 .. +1
    bool accent = false;

private:
    int keyAt (juce::Point<int>) const;
    juce::Rectangle<float> keyRect (int key) const;
    static bool isBlack (int key) { const int k = key % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
    void press (int key);
    void release();
    UiContext& ctx;
    int held = -1, heldNote = -1, shownPlaying = -2;
};

//==============================================================================
class SequencerPanel : public juce::Component
{
public:
    explicit SequencerPanel (UiContext&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void markLoaded();       // the edit slot was just loaded / saved: not modified

private:
    void showPatternMenu();
    void saveAsDialog();
    void saveTo (const juce::String& name);
    void updatePatternText();
    juce::String statusText() const;

    UiContext& ctx;
    juce::TextButton playButton { "PLAY" }, recButton { "REC" };
    ParamCombo mode, start;
    juce::ComboBox slotKey;                   // the selected slot's trigger key
    juce::TextButton keyLearn { "LEARN" };
    int shownKey = -1, shownKeySlot = -1, seenKeyLearns = 0;
    Knob swing, gate;
    MiniKeyboard keyboard;
    juce::TextButton kbDown { "-" }, kbUp { "+" }, accButton { "ACC" };
    SlotStrip slots;
    LcdDisplay patternLcd { Theme::orange };
    juce::TextButton lenDown { "-" }, lenUp { "+" }, shiftLeft { "<" }, shiftRight { ">" },
                     randButton { "RANDOM" }, clearButton { "CLEAR" }, restButton { "REST" };
    StepGrid grid;

    juce::Rectangle<int> leftArea, lenArea, statusArea, kbLabelArea;
    juce::uint32 lastChange = 0;
    juce::String shownStatus;
    int shownLen = -1;
};
