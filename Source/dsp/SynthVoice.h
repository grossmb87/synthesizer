#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Oscillator.h"
#include "Parameters.h"

// Snapshot of all per-voice parameters, refreshed by the processor once per block.
struct VoiceParams
{
    struct Osc
    {
        Oscillator::Wave wave = Oscillator::Wave::Saw;
        float semitoneOffset = 0.0f; // octave * 12 + semi + fine / 100
        float pulseWidth = 0.5f;
        float level = 0.0f;
    };

    Osc osc[2];
    bool sync = false;
    float noiseLevel = 0.0f;

    juce::dsp::StateVariableTPTFilterType filterType = juce::dsp::StateVariableTPTFilterType::lowpass;
    float cutoffHz = 2500.0f, resonance = 0.7f, envAmount = 0.0f, keyTrack = 0.0f, filterVelocity = 0.0f;

    juce::ADSR::Parameters filterEnv, ampEnv;
    float ampVelocity = 0.0f;

    // Global LFOs: one value per sample of the current block, plus routing.
    const float* lfo[2] {};
    LfoDest lfoDest[2] { LfoDest::Off, LfoDest::Off };
    float lfoDepth[2] {};
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const VoiceParams& p) : params (p) {}

    void prepare (double sampleRate, int maxBlockSize);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    void updateChunk (int lfoIndex);

    static constexpr int chunkSize = 32;

    const VoiceParams& params;
    Oscillator osc[2];
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR ampEnv, filterEnv;
    juce::Random noise;

    int note = 60;
    float velocity = 1.0f;
    float filterEnvValue = 0.0f;
    float smoothedCutoffSt = 0.0f; // log2-domain cutoff, in semitones above 20 Hz
    float lfoAmpGain = 1.0f;
    bool firstChunk = true;
};
