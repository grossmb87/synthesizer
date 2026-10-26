#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    bool isEffectParameter (const juce::String& id)
    {
        return id.startsWith ("chorus") || id.startsWith ("delay") || id.startsWith ("reverb") || id == ParamID::masterGain;
    }
}

SynthAudioProcessor::SynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isEffectParameter (ranged->getParameterID()))
            {
                soundParams.push_back (ranged);
                liveValues.push_back (param (ranged->getParameterID()));
            }

    for (int t = 0; t < numTracks; ++t)
    {
        trackSounds[(size_t) t] = std::vector<std::atomic<float>> (soundParams.size());
        copyLiveSoundTo (t);
        trackSoundNames[(size_t) t] = presets.getCurrentPresetName();

        auto lookup = [this, t] (const juce::String& id) -> std::atomic<float>*
        {
            for (size_t i = 0; i < soundParams.size(); ++i)
                if (soundParams[i]->getParameterID() == id)
                    return &trackSounds[(size_t) t][i];
            jassertfalse; // not a sound parameter
            return nullptr;
        };
        engines[(size_t) t] = std::make_unique<SynthEngine> (lookup);
    }

    looper.onSilenceTrack = [this] (int t) { engines[(size_t) t]->allNotesOff (2); };

    chorusRate = param (ParamID::chorusRate);  chorusDepth = param (ParamID::chorusDepth);  chorusMix = param (ParamID::chorusMix);
    delayTime = param (ParamID::delayTime);    delayFeedback = param (ParamID::delayFeedback);  delayMix = param (ParamID::delayMix);
    reverbSize = param (ParamID::reverbSize);  reverbDamping = param (ParamID::reverbDamping);  reverbMix = param (ParamID::reverbMix);
    masterGain = param (ParamID::masterGain);
}

void SynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int t = 0; t < numTracks; ++t)
    {
        engines[(size_t) t]->prepare (sampleRate, samplesPerBlock);
        trackMidi[(size_t) t].ensureSize (4096);
    }
    looper.prepare (sampleRate);

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

//==============================================================================
void SynthAudioProcessor::copyLiveSoundTo (int track)
{
    auto& values = trackSounds[(size_t) track];
    for (size_t i = 0; i < values.size(); ++i)
        values[i].store (liveValues[i]->load(), std::memory_order_relaxed);
}

void SynthAudioProcessor::selectTrack (int track)
{
    const auto previous = selectedTrack.load();
    if (track == previous || track < 0 || track >= numTracks)
        return;

    // Detach the APVTS from every track while it is being reloaded.
    selectedTrack.store (-1);
    if (previous >= 0)
    {
        copyLiveSoundTo (previous);
        trackSoundNames[(size_t) previous] = presets.getCurrentPresetName();
    }

    const auto& values = trackSounds[(size_t) track];
    for (size_t i = 0; i < soundParams.size(); ++i)
        soundParams[i]->setValueNotifyingHost (soundParams[i]->convertTo0to1 (values[i].load()));
    presets.setCurrentPresetName (trackSoundNames[(size_t) track]);

    selectedTrack.store (track);
}

juce::String SynthAudioProcessor::getTrackSoundName (int track) const
{
    return track == selectedTrack.load() ? presets.getCurrentPresetName() : trackSoundNames[(size_t) track];
}

int SynthAudioProcessor::displayTrack() const
{
    const auto t = selectedTrack.load();
    return t >= 0 ? t : 0;
}

float SynthAudioProcessor::getLiveCutoff() const     { return engines[(size_t) displayTrack()]->getLiveCutoff(); }
float SynthAudioProcessor::getLfoPhase (int i) const { return engines[(size_t) displayTrack()]->getLfoPhase (i); }

//==============================================================================
void SynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (const auto selected = selectedTrack.load(); selected >= 0)
    {
        copyLiveSoundTo (selected);

        if (selected != audioSelectedTrack)
        {
            engines[(size_t) audioSelectedTrack]->allNotesOff (1); // live notes stay on the old track otherwise
            looper.selectionChanged (audioSelectedTrack);
            audioSelectedTrack = selected;
        }
    }

    for (auto& m : trackMidi)
        m.clear();
    looper.process (midi, trackMidi, numSamples, audioSelectedTrack);

    for (int t = 0; t < numTracks; ++t)
        engines[(size_t) t]->render (buffer, trackMidi[(size_t) t], numSamples);

    processEffects (buffer);
    looper.renderClicks (buffer);
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
