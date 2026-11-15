// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Widgets.h"
#include "UI/SynthPanels.h"
#include "UI/SequencerPanel.h"

// One page: header (logo, tempo, patch, MIDI learn), the synth panels in two rows,
// and the sequencer underneath. Laid out on a fixed 1440x930 canvas, scaled to the window.
class BasslineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit BasslineAudioProcessorEditor (BasslineAudioProcessor&);
    ~BasslineAudioProcessorEditor() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void setLearnMode (bool on);

private:
    class Canvas : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    private:
        juce::Image background;
    };

    void timerCallback() override;
    void armKnobLearn (int param);
    void setLcd (const juce::String& t, juce::Colour c = Theme::cyan)
    {
        lcd.showMessage (t, c, ctx.learnMode && c == Theme::learn ? -1 : 2500);
    }
    void showPatchMenu();
    void saveAsDialog();
    void saveTo (const juce::String& name);
    void confirmDelete();
    void updatePatchText();

    BasslineAudioProcessor& synth;
    Theme::FontKeeper fonts;
    Theme::LookAndFeel lnf;
    UiContext ctx;

    Canvas canvas;
    BpmDisplay bpm;
    ParamToggle syncButton;
    LcdDisplay lcd;
    juce::ArrowButton prevPatch { "prev", 0.5f, Theme::orange }, nextPatch { "next", 0.0f, Theme::orange };
    juce::TextButton learnButton { "MIDI LEARN" }, panicButton { "PANIC" };

    OscPanel osc1, osc2, osc3;
    LfoPanel lfo;
    FilterPanel filter;
    AmpPanel amp;
    FoldPanel fold;
    DistPanel dist;
    ScopePanel scope;
    MasterPanel master;
    SequencerPanel sequencer;
    LearnOverlay overlay;
    juce::TooltipWindow tooltips { this, 700 };

    int seenCcLearnCount = 0, tickCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasslineAudioProcessorEditor)
};
