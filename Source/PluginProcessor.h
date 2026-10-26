#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Lfo.h"
#include "dsp/SynthVoice.h"

class SynthAudioProcessor : public juce::AudioProcessor
{
public:
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

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };
    juce::MidiKeyboardState keyboardState;
    PresetManager presets { apvts };

private:
    static constexpr int numVoices = 24;

    struct Osc   { std::atomic<float> *wave, *octave, *semi, *fine, *pw, *level; };
    struct LfoP  { std::atomic<float> *wave, *rate, *depth, *dest; };

    void cacheParameters();
    void updateVoiceParams();
    void renderLfos (int numSamples);
    void processEffects (juce::AudioBuffer<float>&);

    std::atomic<float>* param (const juce::String& id) { return apvts.getRawParameterValue (id); }

    Osc oscP[2] {};
    LfoP lfoP[2] {};
    std::atomic<float> *sync {}, *noise {};
    std::atomic<float> *filterType {}, *cutoff {}, *reso {}, *envAmt {}, *keyTrack {}, *filterVel {};
    std::atomic<float> *fA {}, *fD {}, *fS {}, *fR {}, *aA {}, *aD {}, *aS {}, *aR {}, *ampVel {};
    std::atomic<float> *chorusRate {}, *chorusDepth {}, *chorusMix {};
    std::atomic<float> *delayTime {}, *delayFeedback {}, *delayMix {};
    std::atomic<float> *reverbSize {}, *reverbDamping {}, *reverbMix {};
    std::atomic<float> *masterGain {};

    juce::Synthesiser synth;
    VoiceParams voiceParams;
    Lfo lfos[2];
    std::vector<float> lfoBuffers[2];

    juce::dsp::Chorus<float> chorus;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 192000 };
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> smoothedDelaySamples, smoothedGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessor)
};
