#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamID
{
    // Oscillators: prefix "osc1" / "osc2"
    inline juce::String oscWave (int i)   { return "osc" + juce::String (i) + "Wave"; }
    inline juce::String oscOctave (int i) { return "osc" + juce::String (i) + "Octave"; }
    inline juce::String oscSemi (int i)   { return "osc" + juce::String (i) + "Semi"; }
    inline juce::String oscFine (int i)   { return "osc" + juce::String (i) + "Fine"; }
    inline juce::String oscPW (int i)     { return "osc" + juce::String (i) + "PW"; }
    inline juce::String oscLevel (int i)  { return "osc" + juce::String (i) + "Level"; }
    inline const juce::String osc2Sync   { "osc2Sync" };
    inline const juce::String noiseLevel { "noiseLevel" };

    inline const juce::String filterType     { "filterType" };
    inline const juce::String filterCutoff   { "filterCutoff" };
    inline const juce::String filterReso     { "filterReso" };
    inline const juce::String filterEnvAmt   { "filterEnvAmt" };
    inline const juce::String filterKeyTrack { "filterKeyTrack" };
    inline const juce::String filterVelocity { "filterVelocity" };

    inline const juce::String fAttack  { "fAttack" };
    inline const juce::String fDecay   { "fDecay" };
    inline const juce::String fSustain { "fSustain" };
    inline const juce::String fRelease { "fRelease" };

    inline const juce::String aAttack     { "aAttack" };
    inline const juce::String aDecay      { "aDecay" };
    inline const juce::String aSustain    { "aSustain" };
    inline const juce::String aRelease    { "aRelease" };
    inline const juce::String ampVelocity { "ampVelocity" };

    // LFOs: prefix "lfo1" / "lfo2"
    inline juce::String lfoWave (int i)  { return "lfo" + juce::String (i) + "Wave"; }
    inline juce::String lfoRate (int i)  { return "lfo" + juce::String (i) + "Rate"; }
    inline juce::String lfoDepth (int i) { return "lfo" + juce::String (i) + "Depth"; }
    inline juce::String lfoDest (int i)  { return "lfo" + juce::String (i) + "Dest"; }

    inline const juce::String chorusRate  { "chorusRate" };
    inline const juce::String chorusDepth { "chorusDepth" };
    inline const juce::String chorusMix   { "chorusMix" };

    inline const juce::String delayTime     { "delayTime" };
    inline const juce::String delayFeedback { "delayFeedback" };
    inline const juce::String delayMix      { "delayMix" };

    inline const juce::String reverbSize    { "reverbSize" };
    inline const juce::String reverbDamping { "reverbDamping" };
    inline const juce::String reverbMix     { "reverbMix" };

    inline const juce::String masterGain { "masterGain" };
}

enum class LfoDest { Off, Pitch, Cutoff, Amp, PulseWidth };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
