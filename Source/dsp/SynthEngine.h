#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Lfo.h"
#include "SynthVoice.h"

// One polyphonic sound: voices + LFOs, reading its settings from a set of
// parameter values (the APVTS for live play, or a looper track's own sound).
class SynthEngine
{
public:
    using ParamLookup = std::function<std::atomic<float>* (const juce::String& id)>;

    explicit SynthEngine (const ParamLookup& lookup, int numVoices = 16);

    void prepare (double sampleRate, int maxBlockSize);

    // Adds this engine's output to `buffer`.
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int numSamples);

    void allNotesOff (int midiChannel);

    // For the UI (message thread).
    float getLiveCutoff() const;
    float getLfoPhase (int index) const noexcept { return lfoPhase[index].load (std::memory_order_relaxed); }

private:
    struct Osc  { std::atomic<float> *wave, *octave, *semi, *fine, *pw, *level; };
    struct LfoP { std::atomic<float> *wave, *rate, *depth, *dest; };

    void updateVoiceParams();
    void renderLfos (int numSamples);

    Osc oscP[2] {};
    LfoP lfoP[2] {};
    std::atomic<float> *sync {}, *noise {};
    std::atomic<float> *filterType {}, *cutoff {}, *reso {}, *envAmt {}, *keyTrack {}, *filterVel {};
    std::atomic<float> *fA {}, *fD {}, *fS {}, *fR {}, *aA {}, *aD {}, *aS {}, *aR {}, *ampVel {};

    juce::Synthesiser synth;
    VoiceParams voiceParams;
    Lfo lfos[2];
    std::vector<float> lfoBuffers[2];
    std::atomic<float> lfoPhase[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEngine)
};
