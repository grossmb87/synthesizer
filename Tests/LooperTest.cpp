// Headless checks for the looper: count-in, recording, loop playback,
// overdub, pending note-offs and the long-press clear.

#include "Looper.h"
#include <iostream>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int loopLength = 96000; // 1 bar at 120 BPM

    int failures = 0;

    void check (bool condition, const char* what)
    {
        std::cout << (condition ? "PASS  " : "FAIL  ") << what << "\n";
        failures += condition ? 0 : 1;
    }

    struct Harness
    {
        Looper looper;
        std::array<juce::MidiBuffer, Looper::numTracks> out;
        juce::int64 time = 0;
        int silenced = 0;

        Harness()
        {
            looper.setBpm (120.0f);
            looper.setBars (1);
            looper.setMetronome (true);
            looper.onSilenceTrack = [this] (int) { ++silenced; };
            looper.prepare (testSampleRate);
        }

        // Runs one block; `live` events use offsets within the block.
        void block (const juce::MidiBuffer& live = {}, int selected = 0)
        {
            for (auto& m : out)
                m.clear();
            looper.process (live, out, blockSize, selected);
            time += blockSize;
        }

        void blocks (int n) { for (int i = 0; i < n; ++i) block(); }

        static juce::MidiBuffer single (const juce::MidiMessage& m, int offset = 0)
        {
            juce::MidiBuffer b;
            b.addEvent (m, offset);
            return b;
        }

        int countEvents (int track, bool noteOn, int channel) const
        {
            int n = 0;
            for (const auto meta : out[(size_t) track])
            {
                const auto m = meta.getMessage();
                if (m.getChannel() == channel && (noteOn ? m.isNoteOn() : m.isNoteOff()))
                    ++n;
            }
            return n;
        }
    };
}

int main()
{
    Harness h;
    using State = Looper::State;

    // Pedal press starts a one-bar count-in.
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)));
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 0)));
    check (h.looper.getState (0) == State::CountIn, "pedal on empty track starts count-in");
    check (h.looper.getCountInBeatsLeft() == 4, "count-in shows 4 beats");

    // Wait until recording starts.
    int guard = 0;
    while (h.looper.getState (0) == State::CountIn && guard++ < 1000)
        h.block();
    check (h.looper.getState (0) == State::Recording, "count-in ends in recording");
    check (h.looper.isRunning(), "transport runs after count-in");

    // Live notes are forwarded on channel 1 and recorded.
    h.block (Harness::single (juce::MidiMessage::noteOn (1, 60, 0.8f), 100));
    check (h.countEvents (0, true, 1) == 1, "live note-on forwarded on channel 1");
    h.blocks (10);
    h.block (Harness::single (juce::MidiMessage::noteOff (1, 60), 0));
    check (h.looper.getNumEvents (0) == 2, "note-on and note-off recorded");

    // Hold a note across the end of the recording pass.
    guard = 0;
    while (h.looper.getState (0) == State::Recording && guard++ < 1000)
    {
        const auto remaining = loopLength - (int) (h.looper.getProgress() * loopLength);
        if (remaining < 3 * blockSize && remaining >= 2 * blockSize)
            h.block (Harness::single (juce::MidiMessage::noteOn (1, 64, 0.8f), 0));
        else
            h.block();
    }
    check (h.looper.getState (0) == State::Playing, "recording switches to playing after one loop");
    check (h.looper.getNumEvents (0) == 3, "note-on just before the loop end recorded");

    h.blocks (5);
    h.block (Harness::single (juce::MidiMessage::noteOff (1, 64), 0));
    check (h.looper.getNumEvents (0) == 4, "pending note-off recorded after the pass ended");

    // Over exactly two cycles (375 blocks = 2 loops) each recorded note plays twice, on channel 2.
    static_assert (375 * blockSize == 2 * loopLength);
    int playedOn = 0;
    for (int i = 0; i < 375; ++i)
    {
        h.block();
        playedOn += h.countEvents (0, true, 2);
    }
    check (playedOn == 4, "loop plays both recorded notes on channel 2 once per cycle");

    // Overdub adds notes; tapping again returns to playing.
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)));
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 0)));
    check (h.looper.getState (0) == State::Overdub, "pedal on playing track starts overdub");
    h.block (Harness::single (juce::MidiMessage::noteOn (1, 67, 0.5f)));
    h.block (Harness::single (juce::MidiMessage::noteOff (1, 67)));
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)));
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 0)));
    check (h.looper.getState (0) == State::Playing, "second tap ends overdub");
    check (h.looper.getNumEvents (0) == 6, "overdub notes recorded");

    // A second track records immediately (transport already running), no count-in.
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)), 1);
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 0)), 1);
    check (h.looper.getState (1) == State::Recording, "second track records without count-in");

    // Mute silences the track.
    const auto silencedBefore = h.silenced;
    h.looper.sendCommand (Looper::Command::ToggleMute, 0);
    h.block();
    check (h.looper.getState (0) == State::Muted && h.silenced > silencedBefore, "mute silences the track");
    h.looper.sendCommand (Looper::Command::ToggleMute, 0);
    h.block();
    check (h.looper.getState (0) == State::Playing, "unmute resumes playing");

    // Holding the pedal for a second clears the selected track.
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)));
    h.blocks ((int) (testSampleRate / blockSize) + 2);
    h.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 0)));
    check (h.looper.getState (0) == State::Empty && h.looper.getNumEvents (0) == 0, "long press clears the track");

    // Clear all stops the transport.
    h.looper.sendCommand (Looper::Command::ClearAll);
    h.block();
    check (! h.looper.isRunning() && ! h.looper.hasContent(), "clear all empties everything and stops");

    // Metronome clicks render audio.
    Harness m;
    m.block (Harness::single (juce::MidiMessage::controllerEvent (1, 67, 127)));
    juce::AudioBuffer<float> audio (2, blockSize);
    audio.clear();
    m.block();
    m.looper.renderClicks (audio);
    check (audio.getMagnitude (0, 0, blockSize) > 0.0f, "count-in click is audible");

    std::cout << (failures == 0 ? "\nAll looper tests passed\n" : "\nSome looper tests FAILED\n");
    return failures == 0 ? 0 : 1;
}
