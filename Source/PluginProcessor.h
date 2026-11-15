// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AcidVoice.h"
#include "Sequencer.h"
#include "Pattern.h"
#include "Parameters.h"
#include "PatchManager.h"
#include "PatternManager.h"
#include <array>
#include <atomic>

class BasslineAudioProcessor : public juce::AudioProcessor
{
public:
    BasslineAudioProcessor();
    ~BasslineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Bassline YF-21"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Patches are not exposed as host programs (some hosts re-send program 0 on stop).
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Bassline YF-21"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PatchManager patches { apvts };
    PatternManager patterns;
    Pat::Bank bank;

    static int paramIndexForId (const juce::String& id);

    //==============================================================================
    // Sequencer controls (editor, message thread). Lock-free.
    void setPlay (bool on)          { playRequested.store (on); }
    bool isPlayRequested() const    { return playRequested.load(); }
    void setRecord (bool on)        { recordArmed.store (on); }
    bool isRecordArmed() const      { return recordArmed.load(); }
    int  getEditSlot() const        { return editSlot.load(); }
    void setEditSlot (int s)        { editSlot.store (juce::jlimit (0, Pat::numSlots - 1, s)); editStep.store (0); }
    int  getEditStep() const        { return editStep.load(); }
    void setEditStep (int s)        { editStep.store (juce::jlimit (0, Pat::numSteps - 1, s)); }

    bool isSeqRunning() const       { return seqRunning.load (std::memory_order_relaxed); }
    int  getPlayingSlot() const     { return playingSlot.load (std::memory_order_relaxed); }   // -1 = stopped
    int  getPlayingStep() const     { return playingStep.load (std::memory_order_relaxed); }
    bool isTriggeredByMidi() const  { return triggeredByMidi.load (std::memory_order_relaxed); }
    int  getTranspose() const       { return transposeNow.load (std::memory_order_relaxed); }
    float getDisplayBpm() const     { return displayBpm.load (std::memory_order_relaxed); }
    bool isHostSynced() const       { return hostSynced.load (std::memory_order_relaxed); }

    // PANIC: stops the sequencer and silences the voice at once, forgetting every held note.
    void panic()                    { playRequested.store (false); panicRequested.store (true); }
    int  getTriggerBase() const     { return juce::roundToInt (raw[static_cast<size_t> (ix.trigBase)]->load()); }

    // The on-screen keyboard: always plays the synth (and step/live records), never triggers slots.
    void uiNote (int note, float velocity);   // velocity 0 = note off

    // Slot names and "edited since loaded" flags (message thread only).
    std::array<juce::String, Pat::numSlots> slotNames;
    std::array<bool, Pat::numSlots> slotModified {};
    bool loadPatternIntoEditSlot (int libraryIndex);
    void clearEditSlot();
    void randomiseEditSlot();
    void shiftEditSlot (int direction);

    //==============================================================================
    // Knob (CC) MIDI learn. Params are addressed by their index in Params::specs.
    void armCcLearn (int paramIndex)    { learnCcParam.store (paramIndex); }   // -1 cancels
    int  getArmedCcParam() const        { return learnCcParam.load(); }
    int  getCcLearnCount() const        { return ccLearnCount.load(); }
    int  getLastLearnedCc() const       { return lastLearnedCc.load(); }
    int  ccForParam (int paramIndex) const;
    void clearCcForParam (int paramIndex);
    void clearAllCcs();

    //==============================================================================
    float getMeter() const { return meter.load (std::memory_order_relaxed); }
    float getLfoValue() const { return lfoNow.load (std::memory_order_relaxed); }
    static constexpr int scopeSize = 2048;
    // Copies the newest `n` output samples (oldest first) for the scope display.
    void copyScope (float* dest, int n) const;

    // Editor state saved with the session (message thread only).
    float uiScale = 0.75f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readVoiceSettings (VoiceSettings&);
    float pv (int index) const { return raw[static_cast<size_t> (index)]->load(); }

    struct HostInfo
    {
        bool playing = false, hasBpm = false, looping = false;
        double ppq = 0.0, bpm = 120.0, loopStart = 0.0, loopEnd = 0.0;   // ppq = quarter notes
    };
    void handleMidi (const juce::MidiMessage&, int sample, bool fromUi);
    void handleController (int cc, int value);
    void startSequencer (int slot, int sample, bool fromTrigger);
    void stopSequencer();
    void livePlay (int note, float velocity);
    void liveRelease (int note);
    void recordNote (int note, bool accent, bool legato);
    double hostPositionAt (int sample) const;
    void allNotesOff();

    juce::String ccMapToString() const;
    void ccMapFromString (const juce::String&);

    static constexpr int numCcs = 120;
    std::array<juce::RangedAudioParameter*, static_cast<size_t> (Params::numParams)> paramList {};
    std::array<std::atomic<float>*, static_cast<size_t> (Params::numParams)> raw {};

    struct OscIx { int on, wave, oct, semi, fine, shape, level, lfo; };
    struct Ix
    {
        std::array<OscIx, 3> osc {};
        int lfoWave, lfoRate, lfoSync, lfoDiv, lfoPitch, lfoShape, lfoCutoff;
        int fltMode, cutoff, reso, envMod, decay, accent, drive, keyTrack;
        int attack, ampDecay, sustain, release, slide;
        int foldAmount, foldSym, foldMix, distType, distDrive, distTone, distMix;
        int volume, tune, tempo, sync, swing, gate, mode, start, trigBase;
    } ix {};

    // ---- audio thread state
    AcidVoice voice;
    Sequencer seq;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono;
    static constexpr int osFactor = 4, chunkSize = 256;
    double sampleRateHz = 48000.0;
    HostInfo host;
    double blockStepsPerSample = 0.0, blockBeatsPerSample = 0.0;
    double lateSteps = 0.0;          // how late a step may still play after a host jump (about one block)

    struct NoteStack
    {
        std::array<int, 16> notes {};
        int count = 0;
        void push (int n) { remove (n); if (count < 16) notes[static_cast<size_t> (count++)] = n; }
        bool remove (int n)
        {
            for (int i = 0; i < count; ++i)
                if (notes[static_cast<size_t> (i)] == n)
                {
                    for (int j = i; j < count - 1; ++j) notes[static_cast<size_t> (j)] = notes[static_cast<size_t> (j + 1)];
                    --count;
                    return true;
                }
            return false;
        }
        int top() const { return count > 0 ? notes[static_cast<size_t> (count - 1)] : -1; }
        void clear() { count = 0; }
    };
    NoteStack liveNotes, triggerNotes, transposeNotes;
    // What each incoming MIDI note did when it started, so its note-off undoes exactly that,
    // whatever the mode or the other keys are doing by then.
    enum class Role : uint8_t { None, Live, Trigger, Transpose };
    std::array<Role, 128> noteRole {};
    bool hostWasPlaying = false;
    void releaseEverything();
    bool seqFromTrigger = false, seqOwnsVoice = false;
    int lastRecordedStep = -1;

    // UI keyboard -> audio thread
    struct UiNote { int note; float velocity; };
    juce::AbstractFifo uiFifo { 64 };
    std::array<UiNote, 64> uiNotes {};

    std::atomic<bool> playRequested { false }, recordArmed { false }, panicRequested { false };
    std::atomic<int> editSlot { 0 }, editStep { 0 };
    std::atomic<bool> seqRunning { false }, triggeredByMidi { false }, hostSynced { false };
    std::atomic<int> playingSlot { -1 }, playingStep { -1 }, transposeNow { 0 };
    std::atomic<float> displayBpm { 125.0f }, meter { 0.0f }, lfoNow { 0.0f };

    std::atomic<int> learnCcParam { -1 }, ccLearnCount { 0 }, lastLearnedCc { -1 };
    std::array<std::atomic<int>, numCcs> ccToParam;

    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasslineAudioProcessor)
};
