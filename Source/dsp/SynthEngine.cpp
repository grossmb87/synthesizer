#include "SynthEngine.h"
#include "SynthSound.h"

SynthEngine::SynthEngine (const ParamLookup& param, int numVoices)
{
    for (int i = 0; i < 2; ++i)
    {
        oscP[i] = { param (ParamID::oscWave (i + 1)), param (ParamID::oscOctave (i + 1)), param (ParamID::oscSemi (i + 1)),
                    param (ParamID::oscFine (i + 1)), param (ParamID::oscPW (i + 1)), param (ParamID::oscLevel (i + 1)) };
        lfoP[i] = { param (ParamID::lfoWave (i + 1)), param (ParamID::lfoRate (i + 1)),
                    param (ParamID::lfoDepth (i + 1)), param (ParamID::lfoDest (i + 1)) };
    }

    sync = param (ParamID::osc2Sync);
    noise = param (ParamID::noiseLevel);

    filterType = param (ParamID::filterType);
    cutoff     = param (ParamID::filterCutoff);
    reso       = param (ParamID::filterReso);
    envAmt     = param (ParamID::filterEnvAmt);
    keyTrack   = param (ParamID::filterKeyTrack);
    filterVel  = param (ParamID::filterVelocity);

    fA = param (ParamID::fAttack);  fD = param (ParamID::fDecay);  fS = param (ParamID::fSustain);  fR = param (ParamID::fRelease);
    aA = param (ParamID::aAttack);  aD = param (ParamID::aDecay);  aS = param (ParamID::aSustain);  aR = param (ParamID::aRelease);
    ampVel = param (ParamID::ampVelocity);

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (voiceParams));
    synth.addSound (new SynthSound());
}

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, maxBlockSize);

    for (int i = 0; i < 2; ++i)
    {
        lfos[i].prepare (sampleRate);
        lfoBuffers[i].assign ((size_t) maxBlockSize, 0.0f);
    }
}

void SynthEngine::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int numSamples)
{
    renderLfos (numSamples);
    updateVoiceParams();
    synth.renderNextBlock (buffer, midi, 0, numSamples);
}

void SynthEngine::allNotesOff (int midiChannel)
{
    synth.handleSustainPedal (midiChannel, false);
    synth.allNotesOff (midiChannel, true);
}

void SynthEngine::updateVoiceParams()
{
    auto& p = voiceParams;

    for (int i = 0; i < 2; ++i)
    {
        p.osc[i].wave = (Oscillator::Wave) (int) oscP[i].wave->load();
        p.osc[i].semitoneOffset = oscP[i].octave->load() * 12.0f + oscP[i].semi->load() + oscP[i].fine->load() / 100.0f;
        p.osc[i].pulseWidth = oscP[i].pw->load();
        p.osc[i].level = oscP[i].level->load();
    }
    p.sync = sync->load() > 0.5f;
    p.noiseLevel = noise->load();

    using FT = juce::dsp::StateVariableTPTFilterType;
    constexpr FT types[] { FT::lowpass, FT::highpass, FT::bandpass };
    p.filterType = types[juce::jlimit (0, 2, (int) filterType->load())];
    p.cutoffHz = cutoff->load();
    p.resonance = 0.707f * std::pow (12.0f / 0.707f, reso->load()); // Q from 0.707 to 12
    p.envAmount = envAmt->load();
    p.keyTrack = keyTrack->load();
    p.filterVelocity = filterVel->load();

    p.filterEnv = { fA->load(), fD->load(), fS->load(), fR->load() };
    p.ampEnv = { aA->load(), aD->load(), aS->load(), aR->load() };
    p.ampVelocity = ampVel->load();

    for (int i = 0; i < 2; ++i)
    {
        p.lfo[i] = lfoBuffers[i].data();
        p.lfoDest[i] = (LfoDest) (int) lfoP[i].dest->load();
        p.lfoDepth[i] = lfoP[i].depth->load();
    }
}

void SynthEngine::renderLfos (int numSamples)
{
    for (int i = 0; i < 2; ++i)
    {
        if ((int) lfoBuffers[i].size() < numSamples)
            lfoBuffers[i].resize ((size_t) numSamples); // only if the host exceeds the announced block size

        lfos[i].setWave ((Lfo::Wave) (int) lfoP[i].wave->load());
        lfos[i].setRate (lfoP[i].rate->load());

        for (int s = 0; s < numSamples; ++s)
            lfoBuffers[i][(size_t) s] = lfos[i].process();

        lfoPhase[i].store ((float) lfos[i].getPhase(), std::memory_order_relaxed);
    }
}

float SynthEngine::getLiveCutoff() const
{
    float cutoffHz = -1.0f;
    juce::uint32 newest = 0;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
        {
            const auto hz = voice->getLiveCutoff();
            if (hz > 0.0f && voice->getStartStamp() >= newest)
            {
                newest = voice->getStartStamp();
                cutoffHz = hz;
            }
        }
    }

    return cutoffHz;
}
