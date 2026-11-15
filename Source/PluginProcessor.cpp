// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String signedInt (int v) { return (v > 0 ? "+" : "") + juce::String (v); }

    juce::AudioParameterFloatAttributes attributesFor (Params::Unit unit)
    {
        using U = Params::Unit;
        auto a = juce::AudioParameterFloatAttributes();
        auto number = [] (const juce::String& t) { return t.getFloatValue(); };
        switch (unit)
        {
            case U::Time:
                return a.withLabel ("s")
                    .withStringFromValueFunction ([] (float v, int)
                    {
                        return v < 1.0f ? juce::String (v * 1000.0f, v < 0.01f ? 1 : 0) + " ms" : juce::String (v, 2) + " s";
                    })
                    .withValueFromStringFunction ([] (const juce::String& t)
                    {
                        const float v = t.getFloatValue();
                        return t.containsIgnoreCase ("ms") ? v / 1000.0f : v;
                    });
            case U::Hz:
                return a.withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int)
                    {
                        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                        return v < 20.0f ? juce::String (v, 2) + " Hz" : juce::String (juce::roundToInt (v)) + " Hz";
                    })
                    .withValueFromStringFunction ([] (const juce::String& t)
                    {
                        const float v = t.getFloatValue();
                        return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
                    });
            case U::Semitones:
                return a.withLabel ("st")
                    .withStringFromValueFunction ([] (float v, int)
                    {
                        const bool whole = std::abs (v - std::round (v)) < 0.01f;
                        return whole ? signedInt (juce::roundToInt (v)) + " st" : juce::String (v, 2) + " st";
                    })
                    .withValueFromStringFunction (number);
            case U::Cents:
                return a.withLabel ("ct")
                    .withStringFromValueFunction ([] (float v, int) { return signedInt (juce::roundToInt (v)) + " ct"; })
                    .withValueFromStringFunction (number);
            case U::Octave:
                return a.withLabel ("oct")
                    .withStringFromValueFunction ([] (float v, int) { return signedInt (juce::roundToInt (v)) + " oct"; })
                    .withValueFromStringFunction (number);
            case U::Bipolar:
                return a.withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return signedInt (juce::roundToInt (v * 100.0f)) + "%"; })
                    .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });
            case U::Bpm:
                return a.withLabel ("BPM")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " BPM"; })
                    .withValueFromStringFunction (number);
            case U::Swing:   // 0..1 shown as 50% .. 75% (the usual swing amounts)
                return a.withLabel ("%")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (50.0f + v * 25.0f)) + "%"; })
                    .withValueFromStringFunction ([] (const juce::String& t) { return (t.getFloatValue() - 50.0f) / 25.0f; });
            case U::Integer:
                return a.withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); })
                    .withValueFromStringFunction (number);
            case U::Percent:
            case U::Toggle:
            case U::Choice:
            default:
                break;
        }
        return a.withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });
    }

    int indexOf (const char* id)
    {
        for (int i = 0; i < Params::numParams; ++i)
            if (juce::String (Params::specs[i].id) == id)
                return i;
        jassertfalse;
        return 0;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout BasslineAudioProcessor::createLayout()
{
    using U = Params::Unit;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : Params::specs)
    {
        const juce::ParameterID pid { s.id, 1 };
        if (s.unit == U::Toggle)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def >= 0.5f));
        else if (s.unit == U::Choice)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name,
                            juce::StringArray::fromTokens (s.choices, "|", ""), static_cast<int> (s.def)));
        else
        {
            juce::NormalisableRange<float> range (s.min, s.max);
            if (s.centre > 0.0f)
                range.setSkewForCentre (s.centre);
            if (s.unit == U::Integer || s.unit == U::Octave || (s.unit == U::Semitones && s.min < 0.0f))
                range.interval = 1.0f;
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, range, s.def, attributesFor (s.unit)));
        }
    }
    return layout;
}

int BasslineAudioProcessor::paramIndexForId (const juce::String& id)
{
    for (int i = 0; i < Params::numParams; ++i)
        if (id == Params::specs[i].id)
            return i;
    return -1;
}

BasslineAudioProcessor::BasslineAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BASSLINE_YF21", createLayout())
{
    for (int i = 0; i < Params::numParams; ++i)
    {
        paramList[static_cast<size_t> (i)] = apvts.getParameter (Params::specs[i].id);
        raw[static_cast<size_t> (i)] = apvts.getRawParameterValue (Params::specs[i].id);
    }
    for (auto& c : ccToParam)
        c.store (-1);

    for (int o = 0; o < 3; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        auto f = [&] (const char* s) { return paramIndexForId (p + s); };
        ix.osc[static_cast<size_t> (o)] = { f ("on"), f ("wave"), f ("oct"), f ("semi"), f ("fine"), f ("shape"), f ("level"), f ("lfo") };
    }
    ix.lfoWave = indexOf ("lfo_wave"); ix.lfoRate = indexOf ("lfo_rate"); ix.lfoSync = indexOf ("lfo_sync");
    ix.lfoDiv = indexOf ("lfo_div"); ix.lfoPitch = indexOf ("lfo_pitch"); ix.lfoShape = indexOf ("lfo_shape");
    ix.lfoCutoff = indexOf ("lfo_cutoff");
    ix.fltMode = indexOf ("flt_mode"); ix.cutoff = indexOf ("flt_cutoff"); ix.reso = indexOf ("flt_reso");
    ix.envMod = indexOf ("flt_envmod"); ix.decay = indexOf ("flt_decay"); ix.accent = indexOf ("flt_accent");
    ix.drive = indexOf ("flt_drive"); ix.keyTrack = indexOf ("flt_keytrk");
    ix.attack = indexOf ("amp_attack"); ix.ampDecay = indexOf ("amp_decay"); ix.sustain = indexOf ("amp_sustain");
    ix.release = indexOf ("amp_release"); ix.slide = indexOf ("voice_slide");
    ix.foldAmount = indexOf ("fold_amount"); ix.foldSym = indexOf ("fold_sym"); ix.foldMix = indexOf ("fold_mix");
    ix.distType = indexOf ("dist_type"); ix.distDrive = indexOf ("dist_drive"); ix.distTone = indexOf ("dist_tone");
    ix.distMix = indexOf ("dist_mix");
    ix.volume = indexOf ("master_volume"); ix.tune = indexOf ("master_tune");
    ix.tempo = indexOf ("seq_tempo"); ix.sync = indexOf ("seq_sync"); ix.swing = indexOf ("seq_swing");
    ix.gate = indexOf ("seq_gate"); ix.mode = indexOf ("seq_mode"); ix.start = indexOf ("seq_start");
    ix.trigOct = indexOf ("seq_trigoct");

    // A fresh instance starts with a few factory patterns in the bank so there's something to play.
    for (int s = 0; s < Pat::numSlots; ++s)
        slotNames[static_cast<size_t> (s)] = "EMPTY";
    for (int s = 0; s < 8; ++s)
    {
        PatternManager::Loaded l;
        if (patterns.load (s + 1, l))   // skip "EMPTY"
        {
            bank.store (s, l.pattern);
            slotNames[static_cast<size_t> (s)] = juce::String (l.pattern.name);
        }
    }
}

bool BasslineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BasslineAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, 2,
                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing (chunkSize);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));
    mono.setSize (1, chunkSize);
    voice.prepare (sampleRate, osFactor);
    seq.stop();
    liveNotes.clear(); triggerNotes.clear(); transposeNotes.clear();
    seqOwnsVoice = seqFromTrigger = false;
}

//==============================================================================
void BasslineAudioProcessor::readVoiceSettings (VoiceSettings& s)
{
    for (size_t o = 0; o < 3; ++o)
    {
        const auto& i = ix.osc[o];
        auto& v = s.osc[o];
        v.on = pv (i.on) >= 0.5f;
        v.wave = juce::jlimit (0, Params::numWaves - 1, static_cast<int> (pv (i.wave)));
        v.octave = juce::roundToInt (pv (i.oct));
        v.semi = std::round (pv (i.semi));
        v.fine = pv (i.fine);
        v.shape = pv (i.shape);
        v.level = pv (i.level);
        v.lfo = pv (i.lfo) >= 0.5f;
    }
    s.lfoWave = static_cast<int> (pv (ix.lfoWave));
    s.lfoPitch = pv (ix.lfoPitch);
    s.lfoShape = pv (ix.lfoShape);
    s.lfoCutoff = pv (ix.lfoCutoff);
    s.filterMode = static_cast<int> (pv (ix.fltMode));
    s.cutoff = pv (ix.cutoff); s.reso = pv (ix.reso); s.envMod = pv (ix.envMod); s.decay = pv (ix.decay);
    s.accent = pv (ix.accent); s.drive = pv (ix.drive); s.keyTrack = pv (ix.keyTrack);
    s.attack = pv (ix.attack); s.ampDecay = pv (ix.ampDecay); s.sustain = pv (ix.sustain); s.release = pv (ix.release);
    s.slideTime = pv (ix.slide);
    s.foldAmount = pv (ix.foldAmount); s.foldSym = pv (ix.foldSym); s.foldMix = pv (ix.foldMix);
    s.distType = static_cast<int> (pv (ix.distType));
    s.distDrive = pv (ix.distDrive); s.distTone = pv (ix.distTone); s.distMix = pv (ix.distMix);
    s.volume = pv (ix.volume); s.tuneCents = pv (ix.tune);
}

double BasslineAudioProcessor::hostPositionAt (int sample) const
{
    return (host.ppq + sample * blockBeatsPerSample) * 4.0;
}

void BasslineAudioProcessor::startSequencer (int slot, int sample, bool fromTrigger)
{
    const bool followHost = host.playing;
    double pos = 0.0;
    long long offset = 0;
    if (followHost)
    {
        pos = hostPositionAt (sample);
        const bool retrig = pv (ix.start) < 0.5f;
        offset = retrig ? seq.firstStepFrom (pos) : 0;   // RETRIG: step 1 on the next 16th; SYNC: locked to the bar
    }
    if (seqOwnsVoice && voice.isGateOn() && liveNotes.count == 0)
        voice.noteOff();
    seq.start (slot, pos, offset);
    seqFromTrigger = fromTrigger;
    seqOwnsVoice = false;
}

void BasslineAudioProcessor::stopSequencer()
{
    if (! seq.isRunning()) return;
    seq.stop();
    if (seqOwnsVoice && liveNotes.count == 0)
        voice.noteOff();
    seqOwnsVoice = seqFromTrigger = false;
}

void BasslineAudioProcessor::allNotesOff()
{
    liveNotes.clear(); triggerNotes.clear(); transposeNotes.clear();
    seq.setTranspose (0);
    if (seqFromTrigger) stopSequencer();
    voice.noteOff();
}

void BasslineAudioProcessor::recordNote (int note, bool accent, bool legato)
{
    const int slot = getEditSlot();
    if (seq.isRunning() && ! seqFromTrigger)
    {
        // live: quantise to the nearest step of the running pattern
        const long long k = seq.nearestStep();
        const int idx = seq.patternIndex (k, bank.length (slot));
        auto st = Pat::Step::fromMidi (note, accent);
        bank.set (slot, idx, st);
        if (legato && lastRecordedStep >= 0 && lastRecordedStep != idx)
        {
            auto prev = bank.get (slot, lastRecordedStep);
            prev.slide = true;
            bank.set (slot, lastRecordedStep, prev);
        }
        if (seq.stepTime (k) > seq.getPosition())
            seq.suppressStep (k);   // already heard live; don't play it again this pass
        lastRecordedStep = idx;
    }
    else if (! seq.isRunning())
    {
        // step record: write at the cursor and move on
        const int len = bank.length (slot);
        const int idx = juce::jlimit (0, len - 1, getEditStep());
        bank.set (slot, idx, Pat::Step::fromMidi (note, accent));
        if (legato && lastRecordedStep >= 0 && lastRecordedStep != idx)
        {
            auto prev = bank.get (slot, lastRecordedStep);
            prev.slide = true;
            bank.set (slot, lastRecordedStep, prev);
        }
        lastRecordedStep = idx;
        editStep.store ((idx + 1) % len);
    }
}

void BasslineAudioProcessor::livePlay (int note, float velocity)
{
    const bool accent = velocity >= 100.0f / 127.0f;
    const bool legato = liveNotes.count > 0;
    if (recordArmed.load())
        recordNote (note, accent, legato);
    liveNotes.push (note);
    voice.noteOn (note, accent, legato);   // overlapping notes slide (303 style)
    seqOwnsVoice = false;
}

void BasslineAudioProcessor::liveRelease (int note)
{
    const bool wasTop = liveNotes.top() == note;
    if (! liveNotes.remove (note)) return;
    if (liveNotes.count == 0)
    {
        voice.noteOff();
        lastRecordedStep = -1;
    }
    else if (wasTop)
        voice.noteOn (liveNotes.top(), false, true);   // glide back to the note still held
}

void BasslineAudioProcessor::handleMidi (const juce::MidiMessage& m, int sample, bool fromUi)
{
    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 120 || cc == 123) { allNotesOff(); return; }
        handleController (cc, m.getControllerValue());
        return;
    }
    if (m.isAllNotesOff() || m.isAllSoundOff()) { allNotesOff(); return; }
    if (! m.isNoteOnOrOff()) return;

    const int note = m.getNoteNumber();
    const bool on = m.isNoteOn();
    const bool patternMode = pv (ix.mode) < 0.5f;
    const int trigBase = 24 - 12 * (2 - static_cast<int> (pv (ix.trigOct)));   // C0 = 24, C-1 = 12, C-2 = 0

    if (! fromUi && patternMode && note >= trigBase && note < trigBase + Pat::numSlots)
    {
        const int slot = note - trigBase;
        if (on)
        {
            triggerNotes.push (note);
            const bool retrig = pv (ix.start) < 0.5f;
            if (seq.isRunning() && seqFromTrigger && ! retrig)
                seq.setSlot (slot);                   // SYNC: switch slot without losing the bar position
            else
                startSequencer (slot, sample, true);
        }
        else if (triggerNotes.remove (note))
        {
            if (triggerNotes.count == 0)
            {
                stopSequencer();
                transposeNotes.clear();
                seq.setTranspose (0);
            }
            else if (seq.getSlot() != triggerNotes.top() - trigBase)
                seq.setSlot (triggerNotes.top() - trigBase);
        }
        return;
    }

    if (! fromUi && patternMode && triggerNotes.count > 0)
    {
        // while a slot is held, other keys transpose it (relative to C3)
        if (on) transposeNotes.push (note); else transposeNotes.remove (note);
        seq.setTranspose (transposeNotes.count > 0 ? transposeNotes.top() - 60 : 0);
        return;
    }

    if (on) livePlay (note, m.getFloatVelocity());
    else    liveRelease (note);
}

void BasslineAudioProcessor::uiNote (int note, float velocity)
{
    const auto w = uiFifo.write (1);
    if (w.blockSize1 > 0) uiNotes[static_cast<size_t> (w.startIndex1)] = { note, velocity };
}

//==============================================================================
void BasslineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- host
    host = {};
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            host.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) { host.bpm = *b; host.hasBpm = *b > 1.0; }
            if (auto q = pos->getPpqPosition()) host.ppq = *q; else host.playing = false;
        }
    const bool sync = pv (ix.sync) >= 0.5f && host.hasBpm;
    const double bpm = sync ? host.bpm : static_cast<double> (pv (ix.tempo));
    if (! sync) host.playing = false;        // not following the host: run on our own clock
    displayBpm.store (static_cast<float> (bpm));
    hostSynced.store (sync);
    blockBeatsPerSample = bpm / 60.0 / sampleRateHz;
    blockStepsPerSample = blockBeatsPerSample * 4.0;

    // ---- voice settings + LFO
    VoiceSettings vs;
    readVoiceSettings (vs);
    const bool lfoSync = pv (ix.lfoSync) >= 0.5f;
    const float divBeats = Params::lfoDivBeats[juce::jlimit (0, Params::numLfoDivs - 1, static_cast<int> (pv (ix.lfoDiv)))];
    vs.lfoRateHz = lfoSync ? static_cast<float> (bpm / 60.0 / divBeats) : pv (ix.lfoRate);
    voice.setSettings (vs);
    if (lfoSync && host.playing)
        voice.syncLfo (host.ppq / divBeats);

    // ---- sequencer
    seq.setTiming (blockStepsPerSample, pv (ix.swing), pv (ix.gate));
    if (seq.isRunning() && host.playing)
        seq.follow (hostPositionAt (0));

    // internal PLAY (only when no trigger key is holding a slot)
    if (triggerNotes.count == 0)
    {
        if (playRequested.load())
        {
            if (! seq.isRunning() || seqFromTrigger) startSequencer (getEditSlot(), 0, false);
            else if (seq.getSlot() != getEditSlot()) seq.setSlot (getEditSlot());
        }
        else if (seq.isRunning() && ! seqFromTrigger)
            stopSequencer();
    }

    // on-screen keyboard
    {
        const auto sc = uiFifo.read (uiFifo.getNumReady());
        auto take = [&] (int start, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const auto& u = uiNotes[static_cast<size_t> (start + i)];
                handleMidi (u.velocity > 0.0f ? juce::MidiMessage::noteOn (1, u.note, u.velocity)
                                              : juce::MidiMessage::noteOff (1, u.note), 0, true);
            }
        };
        take (sc.startIndex1, sc.blockSize1);
        take (sc.startIndex2, sc.blockSize2);
    }

    // ---- render in chunks: sequencer + MIDI sample-accurately, voice oversampled
    auto midiIt = midi.cbegin();
    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);
        mono.clear();
        juce::dsp::AudioBlock<float> block (mono.getArrayOfWritePointers(), 1, static_cast<size_t> (n));
        auto up = oversampler->processSamplesUp (block);
        float* os = up.getChannelPointer (0);

        for (int i = 0; i < n; ++i)
        {
            const int t = start + i;
            while (midiIt != midi.cend() && (*midiIt).samplePosition <= t)
            {
                handleMidi ((*midiIt).getMessage(), t, false);
                ++midiIt;
            }

            Sequencer::Event ev[3];
            const int ne = seq.tick (bank, ev);
            for (int e = 0; e < ne; ++e)
            {
                if (liveNotes.count > 0) continue;     // keys being played win over the pattern
                if (ev[e].type == Sequencer::Event::NoteOn)
                {
                    voice.noteOn (ev[e].note, ev[e].accent, ev[e].legato && seqOwnsVoice);
                    seqOwnsVoice = true;
                }
                else if (seqOwnsVoice)
                    voice.noteOff();
            }
            voice.render (os + static_cast<size_t> (i * osFactor));
        }
        lfoNow.store (voice.getLfoValue(), std::memory_order_relaxed);

        oversampler->processSamplesDown (block);

        const float* src = mono.getReadPointer (0);
        float peak = 0.0f;
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const float y = dsp::softLimit (src[i]);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, start + i, y);
            peak = juce::jmax (peak, std::abs (y));
            scope[static_cast<size_t> (w)].store (y, std::memory_order_relaxed);
            w = (w + 1) % scopeSize;
        }
        scopeWrite.store (w, std::memory_order_relaxed);
        meter.store (juce::jmax (peak, meter.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    }
    while (midiIt != midi.cend()) { handleMidi ((*midiIt).getMessage(), numSamples - 1, false); ++midiIt; }
    midi.clear();

    seqRunning.store (seq.isRunning(), std::memory_order_relaxed);
    playingSlot.store (seq.isRunning() ? seq.getSlot() : -1, std::memory_order_relaxed);
    playingStep.store (seq.isRunning() ? seq.getPlayingStep() : -1, std::memory_order_relaxed);
    triggeredByMidi.store (seq.isRunning() && seqFromTrigger, std::memory_order_relaxed);
    transposeNow.store (transposeNotes.count > 0 ? transposeNotes.top() - 60 : 0, std::memory_order_relaxed);
}

void BasslineAudioProcessor::copyScope (float* dest, int n) const
{
    n = juce::jmin (n, scopeSize);
    const int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        dest[i] = scope[static_cast<size_t> ((w - n + i + scopeSize) % scopeSize)].load (std::memory_order_relaxed);
}

//==============================================================================
// Pattern slot editing (message thread)
bool BasslineAudioProcessor::loadPatternIntoEditSlot (int libraryIndex)
{
    PatternManager::Loaded l;
    if (! patterns.load (libraryIndex, l))
        return false;
    const int slot = getEditSlot();
    bank.store (slot, l.pattern);
    slotNames[static_cast<size_t> (slot)] = juce::String (l.pattern.name);
    slotModified[static_cast<size_t> (slot)] = false;
    for (auto [id, v] : { std::pair<const char*, float> { "seq_swing", l.swing }, { "seq_gate", l.gate } })
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    return true;
}

void BasslineAudioProcessor::clearEditSlot()
{
    auto p = Pat::Pattern::empty();
    p.length = bank.length (getEditSlot());
    bank.store (getEditSlot(), p);
    editStep.store (0);
}

void BasslineAudioProcessor::randomiseEditSlot()
{
    auto p = Pat::Pattern::random (static_cast<uint32_t> (juce::Random::getSystemRandom().nextInt()));
    p.length = bank.length (getEditSlot());
    bank.store (getEditSlot(), p);
}

void BasslineAudioProcessor::shiftEditSlot (int direction)
{
    const int slot = getEditSlot();
    auto p = bank.load (slot);
    const int len = p.length;
    auto src = p.steps;
    for (int i = 0; i < len; ++i)
        p.steps[static_cast<size_t> (((i + direction) % len + len) % len)] = src[static_cast<size_t> (i)];
    bank.store (slot, p);
}

//==============================================================================
// CC learn
void BasslineAudioProcessor::handleController (int cc, int value)
{
    if (cc < 0 || cc >= numCcs)
        return;
    const int armed = learnCcParam.exchange (-1);
    if (armed >= 0 && armed < Params::numParams)
    {
        for (auto& c : ccToParam)
            if (c.load() == armed) c.store (-1);
        ccToParam[static_cast<size_t> (cc)].store (armed);
        lastLearnedCc.store (cc);
        ccLearnCount.fetch_add (1);
    }
    const int idx = ccToParam[static_cast<size_t> (cc)].load();
    if (idx >= 0 && idx < Params::numParams)
        paramList[static_cast<size_t> (idx)]->setValueNotifyingHost (static_cast<float> (value) / 127.0f);
}

int BasslineAudioProcessor::ccForParam (int paramIndex) const
{
    for (int cc = 0; cc < numCcs; ++cc)
        if (ccToParam[static_cast<size_t> (cc)].load() == paramIndex)
            return cc;
    return -1;
}

void BasslineAudioProcessor::clearCcForParam (int paramIndex)
{
    for (auto& c : ccToParam)
        if (c.load() == paramIndex) c.store (-1);
}

void BasslineAudioProcessor::clearAllCcs()
{
    for (auto& c : ccToParam) c.store (-1);
}

juce::String BasslineAudioProcessor::ccMapToString() const
{
    juce::StringArray items;
    for (int cc = 0; cc < numCcs; ++cc)
    {
        const int idx = ccToParam[static_cast<size_t> (cc)].load();
        if (idx >= 0 && idx < Params::numParams)
            items.add (juce::String (cc) + "=" + Params::specs[idx].id);
    }
    return items.joinIntoString (",");
}

void BasslineAudioProcessor::ccMapFromString (const juce::String& text)
{
    clearAllCcs();
    for (const auto& item : juce::StringArray::fromTokens (text, ",", ""))
    {
        const int cc = item.upToFirstOccurrenceOf ("=", false, false).getIntValue();
        const int idx = paramIndexForId (item.fromFirstOccurrenceOf ("=", false, false));
        if (cc >= 0 && cc < numCcs && idx >= 0 && item.containsChar ('='))
            ccToParam[static_cast<size_t> (cc)].store (idx);
    }
}

//==============================================================================
juce::AudioProcessorEditor* BasslineAudioProcessor::createEditor()
{
    return new BasslineAudioProcessorEditor (*this);
}

void BasslineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree extra ("YF21Extra");
    extra.setProperty ("ccMap", ccMapToString(), nullptr);
    extra.setProperty ("uiScale", uiScale, nullptr);
    extra.setProperty ("patchName", patches.getCurrentName(), nullptr);
    extra.setProperty ("patchModified", patches.isModified(), nullptr);
    extra.setProperty ("editSlot", getEditSlot(), nullptr);
    for (int s = 0; s < Pat::numSlots; ++s)
    {
        juce::ValueTree slot ("Slot");
        slot.setProperty ("name", slotNames[static_cast<size_t> (s)], nullptr);
        slot.setProperty ("modified", slotModified[static_cast<size_t> (s)], nullptr);
        slot.setProperty ("steps", juce::String (bank.load (s).stepsToString()), nullptr);
        extra.appendChild (slot, nullptr);
    }
    state.removeChild (state.getChildWithName ("YF21Extra"), nullptr);
    state.appendChild (extra, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BasslineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto extra = state.getChildWithName ("YF21Extra");
    state.removeChild (extra, nullptr);
    apvts.replaceState (state);

    if (extra.isValid())
    {
        ccMapFromString (extra.getProperty ("ccMap").toString());
        uiScale = juce::jlimit (0.5f, 1.25f, static_cast<float> (extra.getProperty ("uiScale", 0.75f)));
        setEditSlot (static_cast<int> (extra.getProperty ("editSlot", 0)));
        int s = 0;
        for (const auto& slot : extra)
        {
            if (! slot.hasType ("Slot") || s >= Pat::numSlots) continue;
            Pat::Pattern p;
            if (p.stepsFromString (slot.getProperty ("steps").toString().toStdString()))
                bank.store (s, p);
            slotNames[static_cast<size_t> (s)] = slot.getProperty ("name", "EMPTY").toString();
            slotModified[static_cast<size_t> (s)] = slot.getProperty ("modified", false);
            ++s;
        }
        const auto patchName = extra.getProperty ("patchName").toString();
        patches.restoreSession (patchName, extra.getProperty ("patchModified", patchName.isEmpty()));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasslineAudioProcessor();
}
