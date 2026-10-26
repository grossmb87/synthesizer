#include "PresetManager.h"
#include "Parameters.h"

namespace
{
    const juce::Identifier presetProperty { "presetName" };
    const juce::String extension { ".xml" };

    // Choice values are item indices, e.g. osc wave: 0 Sine, 1 Saw, 2 Square, 3 Triangle;
    // LFO dest: 0 Off, 1 Pitch, 2 Cutoff, 3 Amp, 4 PW.
    using namespace ParamID;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    factory = {
        { "Init", {} },

        { "Warm Pad", {
            { oscWave (1), 1 }, { oscWave (2), 1 }, { oscFine (2), 9 }, { oscLevel (2), 0.7f },
            { filterCutoff, 1200 }, { filterReso, 0.15f }, { filterEnvAmt, 12 }, { filterVelocity, 0.2f },
            { fAttack, 1.5f }, { fDecay, 2.0f }, { fSustain, 0.6f }, { fRelease, 2.5f },
            { aAttack, 0.8f }, { aDecay, 1.0f }, { aSustain, 0.9f }, { aRelease, 2.5f }, { ampVelocity, 0.4f },
            { lfoDest (1), 1 }, { lfoRate (1), 5.0f }, { lfoDepth (1), 0.03f },
            { lfoDest (2), 2 }, { lfoRate (2), 0.2f }, { lfoDepth (2), 0.15f },
            { chorusMix, 0.5f }, { reverbMix, 0.35f }, { reverbSize, 0.85f }, { masterGain, -8 } } },

        { "Bass", {
            { oscWave (1), 1 }, { oscWave (2), 2 }, { oscOctave (2), -1 }, { oscFine (2), 0 }, { oscLevel (2), 0.6f },
            { filterCutoff, 300 }, { filterReso, 0.35f }, { filterEnvAmt, 36 }, { filterKeyTrack, 0.3f },
            { fAttack, 0.001f }, { fDecay, 0.25f }, { fSustain, 0.1f }, { fRelease, 0.15f },
            { aAttack, 0.001f }, { aDecay, 0.4f }, { aSustain, 0.7f }, { aRelease, 0.12f },
            { reverbMix, 0.0f }, { masterGain, -6 } } },

        { "Lead", {
            { oscWave (1), 2 }, { oscPW (1), 0.35f }, { oscWave (2), 1 }, { oscFine (2), 5 }, { oscLevel (2), 0.5f },
            { filterCutoff, 1800 }, { filterReso, 0.45f }, { filterEnvAmt, 20 },
            { fDecay, 0.5f }, { fSustain, 0.4f },
            { aAttack, 0.005f }, { aSustain, 0.9f }, { aRelease, 0.25f },
            { lfoDest (1), 1 }, { lfoRate (1), 5.5f }, { lfoDepth (1), 0.04f },
            { lfoDest (2), 4 }, { lfoRate (2), 0.7f }, { lfoDepth (2), 0.4f },
            { delayMix, 0.25f }, { delayTime, 0.38f }, { delayFeedback, 0.4f }, { reverbMix, 0.2f } } },

        { "Brass", {
            { oscWave (1), 1 }, { oscWave (2), 1 }, { oscFine (2), -6 }, { oscLevel (2), 0.7f },
            { filterCutoff, 600 }, { filterReso, 0.1f }, { filterEnvAmt, 30 }, { filterVelocity, 0.6f },
            { fAttack, 0.08f }, { fDecay, 0.5f }, { fSustain, 0.5f }, { fRelease, 0.3f },
            { aAttack, 0.05f }, { aDecay, 0.3f }, { aSustain, 0.85f }, { aRelease, 0.3f },
            { chorusMix, 0.2f }, { reverbMix, 0.2f } } },

        { "Electric Keys", {
            { oscWave (1), 0 }, { oscWave (2), 3 }, { oscOctave (2), 1 }, { oscFine (2), 0 }, { oscLevel (2), 0.3f },
            { filterCutoff, 4000 }, { filterEnvAmt, 0 }, { filterVelocity, 0.5f },
            { aAttack, 0.002f }, { aDecay, 1.5f }, { aSustain, 0.3f }, { aRelease, 0.6f }, { ampVelocity, 0.9f },
            { lfoDest (1), 3 }, { lfoRate (1), 4.0f }, { lfoDepth (1), 0.3f },
            { chorusMix, 0.4f }, { reverbMix, 0.25f } } },
    };
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : factory)
        names.add (p.name);
    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*" + extension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

void PresetManager::loadFactoryPreset (const juce::String& name)
{
    for (const auto& preset : factory)
    {
        if (preset.name != name)
            continue;

        for (auto* p : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

        for (const auto& [id, value] : preset.values)
            if (auto* ranged = apvts.getParameter (id))
                ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));

        setCurrentPresetName (name);
        return;
    }
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (fileFor (name));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    setCurrentPresetName (name);
    return true;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto file = fileFor (name);
    if (! file.getParentDirectory().createDirectory())
        return false;

    setCurrentPresetName (name);
    auto xml = apvts.copyState().createXml();
    return xml != nullptr && xml->writeTo (file);
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    return fileFor (name).deleteFile();
}

juce::String PresetManager::getCurrentPresetName() const
{
    return apvts.state.getProperty (presetProperty, "Init").toString();
}

void PresetManager::setCurrentPresetName (const juce::String& name)
{
    apvts.state.setProperty (presetProperty, name, nullptr);
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Synth")
               .getChildFile ("Presets");
}

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return getUserPresetDirectory().getChildFile (juce::File::createLegalFileName (name) + extension);
}
