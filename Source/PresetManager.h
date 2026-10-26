#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Built-in factory presets plus user presets saved as XML in
// Documents/Synth/Presets.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;

    void loadFactoryPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    juce::String getCurrentPresetName() const;

    static juce::File getUserPresetDirectory();

private:
    struct Value
    {
        juce::String id;
        float value; // plain (unnormalised) value
    };

    struct FactoryPreset
    {
        juce::String name;
        std::vector<Value> values; // everything not listed uses its default
    };

    void setCurrentPresetName (const juce::String&);
    juce::File fileFor (const juce::String& name) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<FactoryPreset> factory;
};
