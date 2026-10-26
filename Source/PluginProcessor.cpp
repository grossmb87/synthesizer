#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/SynthSound.h"

SynthAudioProcessor::SynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    cacheParameters();

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (voiceParams));
    synth.addSound (new SynthSound());
}

void SynthAudioProcessor::cacheParameters()
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

    chorusRate = param (ParamID::chorusRate);  chorusDepth = param (ParamID::chorusDepth);  chorusMix = param (ParamID::chorusMix);
    delayTime = param (ParamID::delayTime);    delayFeedback = param (ParamID::delayFeedback);  delayMix = param (ParamID::delayMix);
    reverbSize = param (ParamID::reverbSize);  reverbDamping = param (ParamID::reverbDamping);  reverbMix = param (ParamID::reverbMix);
    masterGain = param (ParamID::masterGain);
}

void SynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);

    for (int i = 0; i < 2; ++i)
    {
        lfos[i].prepare (sampleRate);
        lfoBuffers[i].assign ((size_t) samplesPerBlock, 0.0f);
    }

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };

    chorus.prepare (spec);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);

    delayLine.prepare (spec);
    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0));

    reverb.prepare (spec);

    smoothedDelaySamples.reset (sampleRate, 0.2);
    smoothedDelaySamples.setCurrentAndTargetValue (delayTime->load() * (float) sampleRate);
    smoothedGain.reset (sampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGain->load()));
}

bool SynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SynthAudioProcessor::updateVoiceParams()
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

void SynthAudioProcessor::renderLfos (int numSamples)
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

float SynthAudioProcessor::getLiveCutoff() const
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

void SynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    renderLfos (numSamples);
    updateVoiceParams();
    synth.renderNextBlock (buffer, midi, 0, numSamples);

    processEffects (buffer);
    outputTap.push (buffer);
}

void SynthAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (chorusMix->load() > 0.0f)
    {
        chorus.setRate (chorusRate->load());
        chorus.setDepth (chorusDepth->load());
        chorus.setMix (chorusMix->load());
        chorus.process (context);
    }

    smoothedDelaySamples.setTargetValue (delayTime->load() * (float) getSampleRate());
    const auto feedback = delayFeedback->load();
    const auto delayWet = delayMix->load();

    for (int s = 0; s < numSamples; ++s)
    {
        const auto delaySamples = smoothedDelaySamples.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const auto wet = delayLine.popSample (ch, delaySamples);
            delayLine.pushSample (ch, data[s] + wet * feedback);
            data[s] += wet * delayWet;
        }
    }

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = reverbSize->load();
    rp.damping = reverbDamping->load();
    rp.wetLevel = reverbMix->load();
    rp.dryLevel = 1.0f - 0.5f * reverbMix->load();
    rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.process (context);

    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (masterGain->load()));
    for (int s = 0; s < numSamples; ++s)
    {
        const auto gain = smoothedGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            data[s] = std::tanh (data[s] * gain); // soft clip
        }
    }
}

juce::AudioProcessorEditor* SynthAudioProcessor::createEditor()
{
    return new SynthAudioProcessorEditor (*this);
}

void SynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthAudioProcessor();
}
