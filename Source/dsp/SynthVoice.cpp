#include "SynthVoice.h"
#include "SynthSound.h"

void SynthVoice::prepare (double sampleRate, int maxBlockSize)
{
    ampEnv.setSampleRate (sampleRate);
    filterEnv.setSampleRate (sampleRate);
    filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::startNote (int midiNote, float noteVelocity, juce::SynthesiserSound*, int)
{
    note = midiNote;
    velocity = noteVelocity;

    osc[0].reset();
    osc[1].reset();
    filter.reset();

    ampEnv.setParameters (params.ampEnv);
    filterEnv.setParameters (params.filterEnv);
    ampEnv.noteOn();
    filterEnv.noteOn();

    filterEnvValue = 0.0f;
    firstChunk = true;
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        filterEnv.reset();
        clearCurrentNote();
    }
}

void SynthVoice::updateChunk (int lfoIndex)
{
    float pitchMod = 0.0f, cutoffMod = 0.0f, pwMod = 0.0f;
    lfoAmpGain = 1.0f;

    for (int k = 0; k < 2; ++k)
    {
        if (params.lfo[k] == nullptr || params.lfoDest[k] == LfoDest::Off)
            continue;

        const auto lfo = params.lfo[k][lfoIndex];
        const auto depth = params.lfoDepth[k];

        switch (params.lfoDest[k])
        {
            case LfoDest::Pitch:      pitchMod  += lfo * depth * 12.0f; break;
            case LfoDest::Cutoff:     cutoffMod += lfo * depth * 48.0f; break;
            case LfoDest::Amp:        lfoAmpGain *= 1.0f - depth * (0.5f - 0.5f * lfo); break;
            case LfoDest::PulseWidth: pwMod     += lfo * depth * 0.45f; break;
            case LfoDest::Off:        break;
        }
    }

    const auto sampleRate = getSampleRate();

    for (int i = 0; i < 2; ++i)
    {
        const auto& p = params.osc[i];
        const auto semis = (float) (note - 69) + p.semitoneOffset + pitchMod;
        osc[i].setWave (p.wave);
        osc[i].setFrequency (440.0 * std::exp2 (semis / 12.0), sampleRate);
        osc[i].setPulseWidth (juce::jlimit (0.05f, 0.95f, p.pulseWidth + pwMod));
    }

    // Smooth only the knob value; envelope, LFO and key tracking must stay fast.
    const auto baseSt = 12.0f * std::log2 (params.cutoffHz / 20.0f);
    smoothedCutoffSt = firstChunk ? baseSt : smoothedCutoffSt + (baseSt - smoothedCutoffSt) * 0.25f;
    firstChunk = false;

    const auto cutoffSt = smoothedCutoffSt
                        + filterEnvValue * params.envAmount
                        + params.keyTrack * (float) (note - 60)
                        + params.filterVelocity * (velocity - 1.0f) * 24.0f
                        + cutoffMod;

    const auto maxHz = juce::jmin (20000.0f, (float) sampleRate * 0.45f);
    filter.setType (params.filterType);
    filter.setResonance (params.resonance);
    filter.setCutoffFrequency (juce::jlimit (20.0f, maxHz, 20.0f * std::exp2 (cutoffSt / 12.0f)));
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    ampEnv.setParameters (params.ampEnv);
    filterEnv.setParameters (params.filterEnv);

    auto* left = output.getWritePointer (0);
    auto* right = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    const auto velocityGain = 1.0f - params.ampVelocity * (1.0f - velocity);
    const auto level1 = params.osc[0].level, level2 = params.osc[1].level, noiseLevel = params.noiseLevel;

    for (int pos = startSample, end = startSample + numSamples; pos < end;)
    {
        const auto n = juce::jmin (chunkSize, end - pos);
        updateChunk (pos);

        const auto gain = velocityGain * lfoAmpGain * 0.25f;

        for (int i = 0; i < n; ++i)
        {
            const auto s1 = osc[0].process();
            if (params.sync && osc[0].hasWrapped())
                osc[1].syncTo (osc[0].getPhase(), osc[0].getIncrement());
            const auto s2 = osc[1].process();

            const auto x = s1 * level1 + s2 * level2 + (noise.nextFloat() * 2.0f - 1.0f) * noiseLevel;

            filterEnvValue = filterEnv.getNextSample();
            const auto y = filter.processSample (0, x) * ampEnv.getNextSample() * gain;

            left[pos + i] += y;
            if (right != nullptr)
                right[pos + i] += y;
        }

        pos += n;

        if (! ampEnv.isActive())
        {
            filterEnv.reset();
            clearCurrentNote();
            break;
        }
    }
}
