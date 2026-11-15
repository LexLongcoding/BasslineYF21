// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Loads the built .vst3 the way a DAW does, plays slot 1 from its trigger key and
// checks the output. Usage: YF21HostTest "path/to/Bassline YF-21.vst3"

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: YF21HostTest <plugin.vst3>\n"); return 2; }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::String (argv[1]));
    if (found.isEmpty()) { std::printf ("FAIL  no plugin found in %s\n", argv[1]); return 1; }
    const auto& d = *found[0];
    std::printf ("found: %s by %s, instrument=%d, inputs=%d outputs=%d\n", d.name.toRawUTF8(),
                 d.manufacturerName.toRawUTF8(), d.isInstrument ? 1 : 0, d.numInputChannels, d.numOutputChannels);

    juce::String error;
    auto plugin = format.createInstanceFromDescription (d, 48000.0, 512, error);
    if (plugin == nullptr) { std::printf ("FAIL  couldn't load: %s\n", error.toRawUTF8()); return 1; }
    plugin->setPlayConfigDetails (0, 2, 48000.0, 512);
    plugin->prepareToPlay (48000.0, 512);
    std::printf ("parameters: %d\n", plugin->getParameters().size());

    juce::AudioBuffer<float> buf (2, 512);
    double sumSq = 0; bool finite = true; long n = 0;
    for (int b = 0; b < 200; ++b)   // ~2.1 s
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 24, 0.8f), 0);   // C0 -> slot 1
        buf.clear();
        plugin->processBlock (buf, midi);
        for (int i = 0; i < 512; ++i) { const float v = buf.getSample (0, i); finite = finite && std::isfinite (v); sumSq += v * v; ++n; }
    }
    const double rms = std::sqrt (sumSq / n);
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    plugin->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    std::unique_ptr<juce::AudioProcessorEditor> ed (plugin->createEditorIfNeeded());
    const bool editorOk = ed != nullptr && ed->getWidth() > 300;
    std::printf ("editor: %dx%d\n", ed ? ed->getWidth() : 0, ed ? ed->getHeight() : 0);
    ed.reset();
    plugin->releaseResources();

    const bool ok = finite && rms > 0.05 && d.isInstrument && state.getSize() > 1000 && editorOk;
    std::printf ("%s  VST3 host test: rms %.3f, state %d bytes\n", ok ? "ok  " : "FAIL", rms, static_cast<int> (state.getSize()));
    return ok ? 0 : 1;
}
