// Bassline YF-21 - Copyright (C) 2026 LexLongcoding
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PatternManager.h"
#include "FactoryPatterns.h"

juce::File PatternManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Bassline YF-21").getChildFile ("Patterns");
}

juce::String PatternManager::sanitiseName (const juce::String& name)
{
    auto n = juce::File::createLegalFileName (name.trim()).trim();
    return n.substring (0, 32).toUpperCase();
}

void PatternManager::refresh()
{
    entries.clear();
    const auto& defs = FactoryPatterns::all();
    for (int i = 0; i < static_cast<int> (defs.size()); ++i)
        entries.push_back ({ defs[static_cast<size_t> (i)].name, true, i, {} });

    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    for (const auto& f : files)
        entries.push_back ({ f.getFileNameWithoutExtension().toUpperCase(), false, -1, f });
}

int PatternManager::getNumFactory() const { return static_cast<int> (FactoryPatterns::all().size()); }

int PatternManager::indexOf (const juce::String& name) const
{
    for (int i = static_cast<int> (entries.size()) - 1; i >= 0; --i)   // user patterns win over factory
        if (entries[static_cast<size_t> (i)].name == name)
            return i;
    return -1;
}

bool PatternManager::userPatternExists (const juce::String& name) const
{
    return getUserFolder().getChildFile (sanitiseName (name) + fileExtension).existsAsFile();
}

bool PatternManager::load (int index, Loaded& out) const
{
    if (index < 0 || index >= static_cast<int> (entries.size()))
        return false;
    const auto& e = entries[static_cast<size_t> (index)];

    if (e.factory)
    {
        const auto& d = FactoryPatterns::all()[static_cast<size_t> (e.factoryIndex)];
        out.pattern = FactoryPatterns::parse (d);
        out.swing = d.swing;
        out.gate = d.gate;
        return true;
    }

    auto xml = juce::XmlDocument::parse (e.file);
    if (xml == nullptr || ! xml->hasTagName ("YF21Pattern"))
        return false;
    Pat::Pattern p;
    if (! p.stepsFromString (xml->getStringAttribute ("steps").toStdString()))
        return false;
    p.name = e.name.toStdString();
    out.pattern = p;
    out.swing = juce::jlimit (0.0f, 1.0f, static_cast<float> (xml->getDoubleAttribute ("swing", 0.0)));
    out.gate  = juce::jlimit (0.1f, 1.0f, static_cast<float> (xml->getDoubleAttribute ("gate", 0.55)));
    return true;
}

bool PatternManager::saveUser (const juce::String& rawName, const Pat::Pattern& p, float swing, float gate)
{
    const auto name = sanitiseName (rawName);
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("YF21Pattern");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    xml.setAttribute ("steps", juce::String (p.stepsToString()));
    xml.setAttribute ("swing", static_cast<double> (swing));
    xml.setAttribute ("gate", static_cast<double> (gate));

    const auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    if (! xml.writeTo (folder.getChildFile (name + fileExtension)))
        return false;
    refresh();
    return true;
}

bool PatternManager::deleteUser (int index)
{
    if (index < 0 || index >= static_cast<int> (entries.size()) || entries[static_cast<size_t> (index)].factory)
        return false;
    if (! entries[static_cast<size_t> (index)].file.deleteFile())
        return false;
    refresh();
    return true;
}
