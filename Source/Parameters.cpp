#include "Parameters.h"

namespace
{
    using Float  = juce::AudioParameterFloat;
    using Int    = juce::AudioParameterInt;
    using Choice = juce::AudioParameterChoice;
    using Bool   = juce::AudioParameterBool;
    using Attr   = juce::AudioParameterFloatAttributes;

    const juce::StringArray oscWaves { "Sine", "Saw", "Square", "Triangle" };
    const juce::StringArray lfoWaves { "Sine", "Triangle", "Saw", "Square", "S&H" };
    const juce::StringArray lfoDests { "Off", "Pitch", "Cutoff", "Amp", "PW" };

    juce::NormalisableRange<float> skewed (float min, float max, float centre)
    {
        juce::NormalisableRange<float> r (min, max);
        r.setSkewForCentre (centre);
        return r;
    }

    Attr percent()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    }

    Attr seconds()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                            : juce::String (v, 2) + " s";
        });
    }

    Attr hertz()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz"
                               : juce::String (v / 1000.0f, 1) + " kHz";
        });
    }

    Attr withUnit (const juce::String& unit, int decimals)
    {
        return Attr().withStringFromValueFunction ([unit, decimals] (float v, int)
        {
            // juce::String (v, 0) means "full precision", so whole numbers need rounding.
            return (decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals)) + " " + unit;
        });
    }

    template <typename Param, typename... Args>
    void add (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id, Args&&... args)
    {
        layout.add (std::make_unique<Param> (juce::ParameterID { id, 1 }, std::forward<Args> (args)...));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 1; i <= 2; ++i)
    {
        const auto name = "Osc " + juce::String (i) + " ";
        add<Choice> (layout, ParamID::oscWave (i), name + "Wave", oscWaves, 1);
        add<Int>    (layout, ParamID::oscOctave (i), name + "Octave", -3, 3, 0);
        add<Int>    (layout, ParamID::oscSemi (i), name + "Semi", -12, 12, 0);
        add<Float>  (layout, ParamID::oscFine (i), name + "Fine", juce::NormalisableRange<float> (-100.0f, 100.0f), i == 2 ? 7.0f : 0.0f, withUnit ("ct", 0));
        add<Float>  (layout, ParamID::oscPW (i), name + "Pulse Width", juce::NormalisableRange<float> (0.05f, 0.95f), 0.5f, percent());
        add<Float>  (layout, ParamID::oscLevel (i), name + "Level", juce::NormalisableRange<float> (0.0f, 1.0f), i == 1 ? 0.8f : 0.5f, percent());
    }
    add<Bool>  (layout, ParamID::osc2Sync, "Osc 2 Sync", false);
    add<Float> (layout, ParamID::noiseLevel, "Noise Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());

    add<Choice> (layout, ParamID::filterType, "Filter Type", juce::StringArray { "Low Pass", "High Pass", "Band Pass" }, 0);
    add<Float>  (layout, ParamID::filterCutoff, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 2500.0f, hertz());
    add<Float>  (layout, ParamID::filterReso, "Resonance", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent());
    add<Float>  (layout, ParamID::filterEnvAmt, "Filter Env Amount", juce::NormalisableRange<float> (-48.0f, 48.0f), 24.0f, withUnit ("st", 1));
    add<Float>  (layout, ParamID::filterKeyTrack, "Key Tracking", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());
    add<Float>  (layout, ParamID::filterVelocity, "Filter Velocity", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent());

    add<Float> (layout, ParamID::fAttack,  "Filter Attack",  skewed (0.001f, 10.0f, 0.5f), 0.005f, seconds());
    add<Float> (layout, ParamID::fDecay,   "Filter Decay",   skewed (0.001f, 10.0f, 0.5f), 0.4f, seconds());
    add<Float> (layout, ParamID::fSustain, "Filter Sustain", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent());
    add<Float> (layout, ParamID::fRelease, "Filter Release", skewed (0.001f, 10.0f, 0.5f), 0.4f, seconds());

    add<Float> (layout, ParamID::aAttack,     "Amp Attack",   skewed (0.001f, 10.0f, 0.5f), 0.003f, seconds());
    add<Float> (layout, ParamID::aDecay,      "Amp Decay",    skewed (0.001f, 10.0f, 0.5f), 0.3f, seconds());
    add<Float> (layout, ParamID::aSustain,    "Amp Sustain",  juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f, percent());
    add<Float> (layout, ParamID::aRelease,    "Amp Release",  skewed (0.001f, 10.0f, 0.5f), 0.3f, seconds());
    add<Float> (layout, ParamID::ampVelocity, "Amp Velocity", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent());

    for (int i = 1; i <= 2; ++i)
    {
        const auto name = "LFO " + juce::String (i) + " ";
        add<Choice> (layout, ParamID::lfoWave (i), name + "Wave", lfoWaves, 0);
        add<Float>  (layout, ParamID::lfoRate (i), name + "Rate", skewed (0.05f, 20.0f, 3.0f), i == 1 ? 5.0f : 0.3f, hertz());
        add<Float>  (layout, ParamID::lfoDepth (i), name + "Depth", skewed (0.0f, 1.0f, 0.25f), 0.0f, percent());
        add<Choice> (layout, ParamID::lfoDest (i), name + "Destination", lfoDests, i == 1 ? 1 : 2);
    }

    add<Float> (layout, ParamID::chorusRate,  "Chorus Rate",  skewed (0.05f, 5.0f, 1.0f), 0.8f, hertz());
    add<Float> (layout, ParamID::chorusDepth, "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent());
    add<Float> (layout, ParamID::chorusMix,   "Chorus Mix",   juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());

    add<Float> (layout, ParamID::delayTime,     "Delay Time",     skewed (0.01f, 1.5f, 0.35f), 0.35f, seconds());
    add<Float> (layout, ParamID::delayFeedback, "Delay Feedback", juce::NormalisableRange<float> (0.0f, 0.95f), 0.35f, percent());
    add<Float> (layout, ParamID::delayMix,      "Delay Mix",      juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());

    add<Float> (layout, ParamID::reverbSize,    "Reverb Size",    juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent());
    add<Float> (layout, ParamID::reverbDamping, "Reverb Damping", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());
    add<Float> (layout, ParamID::reverbMix,     "Reverb Mix",     juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f, percent());

    add<Float> (layout, ParamID::masterGain, "Master Gain", juce::NormalisableRange<float> (-36.0f, 6.0f), -6.0f, withUnit ("dB", 1));

    return layout;
}
