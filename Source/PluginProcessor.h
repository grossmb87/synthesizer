#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "AudioTap.h"
#include "Looper.h"
#include "PresetManager.h"
#include "dsp/SynthEngine.h"

// Each looper track has its own sound (a copy of all non-FX parameters) and
// its own engine. The APVTS always holds the sound of the selected track: the
// UI edits it and live playing uses it. The effects are a shared bus.
class SynthAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numTracks = Looper::numTracks;

    SynthAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override                             { return 1; }
    int getCurrentProgram() override                          { return 0; }
    void setCurrentProgram (int) override                     {}
    const juce::String getProgramName (int) override          { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Track selection (message thread): stores the current sound in the old
    // track and loads the new track's sound into the APVTS.
    void selectTrack (int track);
    int getSelectedTrack() const { return selectedTrack.load(); }
    juce::String getTrackSoundName (int track) const;

    // Visualisation feeds for the selected track, read from the message thread.
    float getLiveCutoff() const;
    float getLfoPhase (int index) const;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };
    juce::MidiKeyboardState keyboardState;
    PresetManager presets { apvts };
    AudioTap outputTap;
    Looper looper;

private:
    void processEffects (juce::AudioBuffer<float>&);
    void copyLiveSoundTo (int track);
    int displayTrack() const;

    std::atomic<float>* param (const juce::String& id) { return apvts.getRawParameterValue (id); }

    // Sound parameters (everything except the shared effects), in a fixed order.
    std::vector<juce::RangedAudioParameter*> soundParams;
    std::vector<std::atomic<float>*> liveValues;
    std::array<std::vector<std::atomic<float>>, numTracks> trackSounds;
    std::array<juce::String, numTracks> trackSoundNames;

    std::array<std::unique_ptr<SynthEngine>, numTracks> engines;
    std::array<juce::MidiBuffer, numTracks> trackMidi;
    std::atomic<int> selectedTrack { 0 }; // -1 while a selection change is in progress
    int audioSelectedTrack = 0;

    std::atomic<float> *chorusRate {}, *chorusDepth {}, *chorusMix {};
    std::atomic<float> *delayTime {}, *delayFeedback {}, *delayMix {};
    std::atomic<float> *reverbSize {}, *reverbDamping {}, *reverbMix {};
    std::atomic<float> *masterGain {};

    juce::dsp::Chorus<float> chorus;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 192000 };
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> smoothedDelaySamples, smoothedGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessor)
};
