#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/LooperPanel.h"
#include "ui/Visualizers.h"

class SynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit SynthAudioProcessorEditor (SynthAudioProcessor&);
    ~SynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildSections();
    void refreshPresetList();
    void presetChosen();
    void savePreset();
    void deletePreset();

    static constexpr int userPresetIdOffset = 1000;

    SynthAudioProcessor& processor;
    SynthLookAndFeel lookAndFeel;

    std::vector<std::vector<std::unique_ptr<Section>>> rows;

    std::vector<float> outputHistory = std::vector<float> (8192); // newest samples of the output
    WaveformView waveformView { processor.apvts };
    EnvelopeView envelopeView { processor.apvts };
    LfoView lfoView { processor };
    ScopeView scopeView { outputHistory };
    SpectrumView spectrumView { processor };
    LooperPanel looperPanel { processor };

    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" }, deleteButton { "Delete" };
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessorEditor)
};
