#pragma once

#include "PluginProcessor.h"
#include "ui/Knob.h"

class SynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SynthAudioProcessorEditor (SynthAudioProcessor&);
    ~SynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buildSections();
    void refreshPresetList();
    void presetChosen();
    void savePreset();
    void deletePreset();

    static constexpr int userPresetIdOffset = 1000;

    SynthAudioProcessor& processor;
    SynthLookAndFeel lookAndFeel;

    std::vector<std::vector<std::unique_ptr<Section>>> rows;

    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" }, deleteButton { "Delete" };
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessorEditor)
};
