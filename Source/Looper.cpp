#include "Looper.h"

Looper::Looper()
{
    for (auto& t : tracks)
        t.events.reserve (maxEventsPerTrack); // the audio thread never allocates
}

//==============================================================================
void Looper::sendCommand (Command command, int track)
{
    const juce::SpinLock::ScopedLockType lock (commandWriteLock);
    const auto scope = commandFifo.write (1);
    if (scope.blockSize1 > 0)
        commandBuffer[(size_t) scope.startIndex1] = { command, track };
}

bool Looper::hasContent() const
{
    for (int t = 0; t < numTracks; ++t)
        if (getNumEvents (t) > 0 || getState (t) != State::Empty)
            return true;
    return false;
}

//==============================================================================
void Looper::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    clickDecay = (float) std::exp (-1.0 / (0.025 * sampleRate));
}

void Looper::process (const juce::MidiBuffer& liveIn, std::array<juce::MidiBuffer, numTracks>& out,
                      int numSamples, int selectedTrack)
{
    numClicks = 0;

    // Tempo changes only take effect while the transport is idle.
    if (! running && countInRemaining == 0)
    {
        beatLength = juce::jmax ((juce::int64) 1, (juce::int64) std::llround (60.0 / settingsBpm.load() * sampleRate));
        barLength = 4 * beatLength;
        loopLength = settingsBars.load() * barLength;
    }

    {
        const auto scope = commandFifo.read (commandFifo.getNumReady());
        auto run = [&] (int start, int size)
        {
            for (int i = 0; i < size; ++i)
                handleCommand (commandBuffer[(size_t) (start + i)].first, commandBuffer[(size_t) (start + i)].second, selectedTrack);
        };
        run (scope.startIndex1, scope.blockSize1);
        run (scope.startIndex2, scope.blockSize2);
    }

    // Holding the pedal for a second clears the track.
    if (pedalDown && ! longPressHandled && sampleClock - pedalPressTime >= (juce::int64) sampleRate)
    {
        clearTrack (pedalTrack);
        longPressHandled = true;
    }

    // One bar count-in before the first recording.
    if (countInRemaining > 0)
    {
        const auto elapsed = barLength - countInRemaining;
        for (auto b = ((elapsed + beatLength - 1) / beatLength) * beatLength; b < elapsed + numSamples && b < barLength; b += beatLength)
            addClick ((int) (b - elapsed), b == 0);

        countInRemaining -= numSamples;
        if (countInRemaining <= 0)
        {
            countInRemaining = 0;
            running = true;
            playhead = 0;
            if (countInTrack >= 0)
                startRecording (countInTrack);
            countInTrack = -1;
        }
    }

    const auto blockStart = playhead;

    if (running)
    {
        const auto blockEnd = blockStart + numSamples;
        for (int t = 0; t < numTracks; ++t)
        {
            if (tracks[(size_t) t].state != State::Playing && tracks[(size_t) t].state != State::Overdub)
                continue;

            schedulePlayback (t, out[(size_t) t], blockStart, juce::jmin (blockEnd, loopLength), 0);
            if (blockEnd > loopLength)
                schedulePlayback (t, out[(size_t) t], 0, blockEnd - loopLength, (int) (loopLength - blockStart));
        }

        if (metronome.load())
            for (auto b = ((blockStart + beatLength - 1) / beatLength) * beatLength; b < blockEnd; b += beatLength)
                addClick ((int) (b - blockStart), (b % loopLength) % barLength == 0);
    }

    for (const auto metadata : liveIn)
    {
        auto message = metadata.getMessage();
        const auto offset = metadata.samplePosition;

        if (message.isControllerOfType (pedalController))
        {
            const auto down = message.getControllerValue() >= 64;
            if (down && ! pedalDown)
            {
                pedalDown = true;
                longPressHandled = false;
                pedalPressTime = sampleClock + offset;
                pedalTrack = selectedTrack;
                pedalAction (selectedTrack);
            }
            else if (! down)
            {
                pedalDown = false;
            }
            continue;
        }

        if (selectedTrack < 0)
            continue;

        if (message.getChannel() > 0)
            message.setChannel (1);
        out[(size_t) selectedTrack].addEvent (message, offset);

        if (running)
        {
            const auto& track = tracks[(size_t) selectedTrack];
            const auto inWindow = (track.state == State::Recording && offset < track.recordRemaining)
                               || track.state == State::Overdub;
            record (selectedTrack, message, (blockStart + offset) % loopLength, inWindow);
        }
    }

    if (running)
    {
        for (int t = 0; t < numTracks; ++t)
        {
            auto& track = tracks[(size_t) t];
            if (track.state == State::Recording)
            {
                track.recordRemaining -= numSamples;
                if (track.recordRemaining <= 0)
                    track.state = State::Playing; // first pass done, loop it
            }
        }

        playhead = (blockStart + numSamples) % loopLength;

        bool anyContent = false;
        for (const auto& t : tracks)
            anyContent = anyContent || ! t.events.empty() || t.state == State::Recording;
        if (! anyContent)
            stopTransport();
    }

    sampleClock += numSamples;
    updateUi();
}

void Looper::selectionChanged (int previousTrack)
{
    if (previousTrack < 0 || ! running)
        return;

    auto& track = tracks[(size_t) previousTrack];
    for (int note = 0; note < 128; ++note)
        if (track.heldNotes[(size_t) note])
            record (previousTrack, juce::MidiMessage::noteOff (1, note), playhead, false);
    if (track.sustainHeld)
        record (previousTrack, juce::MidiMessage::controllerEvent (1, 64, 0), playhead, false);
}

//==============================================================================
void Looper::handleCommand (Command command, int track, int selectedTrack)
{
    switch (command)
    {
        case Command::Pedal:
            pedalAction (track >= 0 ? track : selectedTrack);
            break;

        case Command::Clear:
            clearTrack (track);
            break;

        case Command::ToggleMute:
        {
            if (track < 0)
                break;
            auto& t = tracks[(size_t) track];
            if (t.state == State::Playing || t.state == State::Overdub)
            {
                t.state = State::Muted;
                if (onSilenceTrack != nullptr)
                    onSilenceTrack (track);
            }
            else if (t.state == State::Muted)
            {
                t.state = State::Playing;
            }
            break;
        }

        case Command::Play:
            for (const auto& t : tracks)
                if (! t.events.empty())
                {
                    running = true;
                    playhead = 0;
                    break;
                }
            break;

        case Command::Stop:
            stopTransport();
            break;

        case Command::ClearAll:
            stopTransport();
            for (int t = 0; t < numTracks; ++t)
                clearTrack (t);
            break;
    }
}

void Looper::pedalAction (int trackIndex)
{
    if (trackIndex < 0)
        return;

    auto& track = tracks[(size_t) trackIndex];
    switch (track.state)
    {
        case State::Empty:
            if (running)
            {
                startRecording (trackIndex);
            }
            else
            {
                if (countInTrack >= 0)
                    tracks[(size_t) countInTrack].state = State::Empty;
                track.state = State::CountIn;
                countInTrack = trackIndex;
                countInRemaining = barLength;
            }
            break;

        case State::CountIn:
            track.state = State::Empty;
            countInRemaining = 0;
            countInTrack = -1;
            break;

        case State::Recording: track.state = State::Playing; break;
        case State::Playing:
        case State::Muted:     track.state = State::Overdub; break;
        case State::Overdub:   track.state = State::Playing; break;
    }
}

void Looper::startRecording (int trackIndex)
{
    auto& track = tracks[(size_t) trackIndex];
    track.events.clear();
    track.heldNotes.reset();
    track.sustainHeld = false;
    track.recordRemaining = loopLength;
    track.state = State::Recording;
}

void Looper::clearTrack (int trackIndex)
{
    if (trackIndex < 0)
        return;

    auto& track = tracks[(size_t) trackIndex];
    if (track.state == State::CountIn)
    {
        countInRemaining = 0;
        countInTrack = -1;
    }

    track.events.clear();
    track.heldNotes.reset();
    track.sustainHeld = false;
    track.state = State::Empty;

    if (onSilenceTrack != nullptr)
        onSilenceTrack (trackIndex);
}

void Looper::stopTransport()
{
    running = false;
    playhead = 0;
    countInRemaining = 0;
    countInTrack = -1;

    for (int t = 0; t < numTracks; ++t)
    {
        auto& track = tracks[(size_t) t];
        switch (track.state)
        {
            case State::CountIn:   track.state = State::Empty; break;
            case State::Recording: track.state = track.events.empty() ? State::Empty : State::Playing; break;
            case State::Overdub:   track.state = State::Playing; break;
            default: break;
        }
        track.heldNotes.reset();
        track.sustainHeld = false;

        if (onSilenceTrack != nullptr)
            onSilenceTrack (t);
    }
}

void Looper::record (int trackIndex, const juce::MidiMessage& message, juce::int64 position, bool inWindow)
{
    auto& track = tracks[(size_t) trackIndex];
    bool store = false;

    if (message.isNoteOn())
    {
        if (inWindow)
        {
            store = true;
            track.heldNotes.set ((size_t) message.getNoteNumber());
        }
    }
    else if (message.isNoteOff())
    {
        // Also close notes that were started inside the recording window.
        if (inWindow || track.heldNotes[(size_t) message.getNoteNumber()])
        {
            store = true;
            track.heldNotes.reset ((size_t) message.getNoteNumber());
        }
    }
    else if (message.isSustainPedalOn())
    {
        if (inWindow)
        {
            store = true;
            track.sustainHeld = true;
        }
    }
    else if (message.isSustainPedalOff())
    {
        if (inWindow || track.sustainHeld)
        {
            store = true;
            track.sustainHeld = false;
        }
    }

    if (! store || (int) track.events.size() >= maxEventsPerTrack)
        return;

    const auto* raw = message.getRawData();
    const Event event { position, raw[0], raw[1], raw[2] };
    const auto it = std::upper_bound (track.events.begin(), track.events.end(), position,
                                      [] (juce::int64 pos, const Event& e) { return pos < e.position; });
    track.events.insert (it, event);
}

void Looper::schedulePlayback (int trackIndex, juce::MidiBuffer& out, juce::int64 from, juce::int64 to, int offset)
{
    const auto& events = tracks[(size_t) trackIndex].events;
    auto it = std::lower_bound (events.begin(), events.end(), from,
                                [] (const Event& e, juce::int64 pos) { return e.position < pos; });

    for (; it != events.end() && it->position < to; ++it)
    {
        const auto status = (juce::uint8) ((it->status & 0xf0) | 0x01); // channel 2
        out.addEvent (juce::MidiMessage (status, it->data1, it->data2), offset + (int) (it->position - from));
    }
}

//==============================================================================
void Looper::addClick (int offset, bool downbeat)
{
    if (numClicks < maxClicksPerBlock)
        clicks[(size_t) numClicks++] = { offset, downbeat };
}

void Looper::renderClicks (juce::AudioBuffer<float>& buffer)
{
    if (numClicks == 0 && clickLevel < 1.0e-4f)
        return;

    std::sort (clicks.begin(), clicks.begin() + numClicks);

    int next = 0;
    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        while (next < numClicks && clicks[(size_t) next].first <= s)
        {
            clickLevel = 0.35f;
            clickPhase = 0.0;
            clickIncrement = (clicks[(size_t) next].second ? 1600.0 : 1000.0) / sampleRate;
            ++next;
        }

        if (clickLevel < 1.0e-4f)
            continue;

        const auto v = (float) std::sin (clickPhase * juce::MathConstants<double>::twoPi) * clickLevel;
        clickPhase += clickIncrement;
        clickLevel *= clickDecay;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample (ch, s, v);
    }
}

void Looper::updateUi()
{
    for (int t = 0; t < numTracks; ++t)
    {
        ui[(size_t) t].state.store ((int) tracks[(size_t) t].state, std::memory_order_relaxed);
        ui[(size_t) t].numEvents.store ((int) tracks[(size_t) t].events.size(), std::memory_order_relaxed);
    }

    uiRunning.store (running, std::memory_order_relaxed);
    uiProgress.store (running && loopLength > 0 ? (float) playhead / (float) loopLength : 0.0f, std::memory_order_relaxed);
    uiCountInBeats.store (countInRemaining > 0 ? (int) ((countInRemaining + beatLength - 1) / beatLength) : 0,
                          std::memory_order_relaxed);
}
