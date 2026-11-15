// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"
#include <functional>
#include <memory>

// Shared by every widget: the processor plus callbacks into the editor.
struct UiContext
{
    explicit UiContext (BasslineAudioProcessor& p) : proc (p) {}

    BasslineAudioProcessor& proc;
    bool learnMode = false;
    std::function<void (const juce::String&, juce::Colour)> setLcd;
    std::function<void (int param)> armKnobLearn;    // from a control's right-click menu
};

// Any control bound to a parameter carries this property; MIDI learn finds them by it.
constexpr const char* learnProperty = "yfParam";

juce::String groupTitleFor (const juce::String& paramId);
juce::String paramTitle (int paramIndex);           // "FILTER CUTOFF"
void showParamMenu (UiContext&, juce::Component& target, int paramIndex, std::function<void()> reset);

//==============================================================================
// A parameter knob with its label underneath; right-click for MIDI CC learn.
class Knob : public juce::Component
{
public:
    Knob (UiContext&, const char* paramId, juce::Colour accent, bool large = false);
    ~Knob() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // via listener on the slider

private:
    void reportValue();

    UiContext& ctx;
    const int paramIndex;
    const bool isLarge;
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Latching button bound to a bool parameter (OSC ON, LFO patch, SYNC ...).
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (UiContext&, const char* paramId, const juce::String& text, juce::Colour accent);
    void mouseDown (const juce::MouseEvent&) override;

private:
    UiContext& ctx;
    const int paramIndex;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Drop-down bound to a choice parameter.
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (UiContext&, const char* paramId, juce::Colour accent);
    void mouseDown (const juce::MouseEvent&) override;

private:
    UiContext& ctx;
    const int paramIndex;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
// LCD showing a name ("PATCH 03 // CLASSIC ACID *"), temporarily replaced by
// messages (knob values, MIDI learn). Click for its menu.
class LcdDisplay : public juce::Component
{
public:
    explicit LcdDisplay (juce::Colour c = Theme::cyan) : colour (c) {}
    void setText (const juce::String& t);
    void showMessage (const juce::String& t, juce::Colour c, int holdMs = 2500);   // holdMs < 0: until cleared
    void clearMessage();
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

    std::function<void()> onClick;

private:
    juce::Colour colour;
    juce::String text, message;
    juce::Colour messageColour { Theme::cyan };
    juce::uint32 messageUntil = 0;
    bool persistent = false;
    bool showingMessage() const;
};

// Tempo LCD: "125.0 BPM". Drag up/down (shift = fine) or use the wheel to change the
// tempo; when following the host it shows the host's tempo and "HOST".
class BpmDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BpmDisplay (UiContext&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick();

private:
    void nudge (float bpm);
    UiContext& ctx;
    float dragStartBpm = 125.0f, shown = -1.0f;
    bool shownSync = false, beat = false;
};

class LevelMeter : public juce::Component
{
public:
    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
};

// Oscilloscope of the output, triggered on a rising zero crossing.
class Scope : public juce::Component
{
public:
    explicit Scope (UiContext& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void tick() { repaint(); }

private:
    UiContext& ctx;
    std::array<float, BasslineAudioProcessor::scopeSize> buffer {};
};

// MIDI learn mode: dims the editor, outlines every learnable control with its CC.
// Click one, then move a knob / fader on your controller. Right-click clears.
class LearnOverlay : public juce::Component
{
public:
    LearnOverlay (UiContext&, juce::Component& root);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int param)> onArmed;
    std::function<void()> onCancelled;

private:
    std::vector<std::pair<int, juce::Rectangle<int>>> targets();
    UiContext& ctx;
    juce::Component& root;
};
