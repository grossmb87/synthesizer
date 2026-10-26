#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <bitset>

// Tempo-based MIDI looper with a fixed number of tracks.
//
// Live input is played on MIDI channel 1, loop playback on channel 2, so a
// note released on the keyboard never cuts off the same note in the loop.
// All state changes happen on the audio thread; the UI sends commands and
// reads atomics.
class Looper
{
public:
    static constexpr int numTracks = 4;

    enum class State { Empty, CountIn, Recording, Playing, Overdub, Muted };

    enum class Command { Pedal, Clear, ToggleMute, Play, Stop, ClearAll };

    // Called on the audio thread when a track's loop voices must be silenced.
    std::function<void (int track)> onSilenceTrack;

    Looper();

    //==========================================================================
    // Message thread
    void sendCommand (Command, int track = -1);

    void setBpm (float bpm)          { settingsBpm.store (juce::jlimit (40.0f, 240.0f, bpm)); }
    void setBars (int bars)          { settingsBars.store (juce::jlimit (1, 16, bars)); }
    void setMetronome (bool enabled) { metronome.store (enabled); }
    float getBpm() const             { return settingsBpm.load(); }
    int getBars() const              { return settingsBars.load(); }
    bool isMetronomeOn() const       { return metronome.load(); }

    State getState (int track) const   { return (State) ui[track].state.load (std::memory_order_relaxed); }
    int getNumEvents (int track) const { return ui[track].numEvents.load (std::memory_order_relaxed); }
    bool hasContent() const;
    bool isRunning() const             { return uiRunning.load (std::memory_order_relaxed); }
    float getProgress() const          { return uiProgress.load (std::memory_order_relaxed); } // 0..1 through the loop
    int getCountInBeatsLeft() const    { return uiCountInBeats.load (std::memory_order_relaxed); }

    //==========================================================================
    // Audio thread
    void prepare (double sampleRate);

    // Routes `liveIn` (minus the looper pedal) to `selectedTrack` on channel 1,
    // records it, and adds loop playback on channel 2 to `out`.
    void process (const juce::MidiBuffer& liveIn, std::array<juce::MidiBuffer, numTracks>& out,
                  int numSamples, int selectedTrack);

    // The selected track changed: close notes still held on the previous one.
    void selectionChanged (int previousTrack);

    // Adds this block's metronome clicks (call after process()).
    void renderClicks (juce::AudioBuffer<float>&);

private:
    struct Event
    {
        juce::int64 position; // samples from the loop start
        juce::uint8 status, data1, data2;
    };

    struct Track
    {
        std::vector<Event> events;
        State state = State::Empty;
        juce::int64 recordRemaining = 0;
        std::bitset<128> heldNotes; // note-offs still to be recorded
        bool sustainHeld = false;
    };

    struct UiState
    {
        std::atomic<int> state { (int) State::Empty };
        std::atomic<int> numEvents { 0 };
    };

    static constexpr int pedalController = 67; // left (soft) pedal
    static constexpr int maxEventsPerTrack = 16384;
    static constexpr int maxClicksPerBlock = 16;

    void handleCommand (Command, int track, int selectedTrack);
    void pedalAction (int track);
    void startRecording (int track);
    void clearTrack (int track);
    void stopTransport();
    void record (int track, const juce::MidiMessage&, juce::int64 position, bool force);
    void schedulePlayback (int track, juce::MidiBuffer& out, juce::int64 from, juce::int64 to, int offset);
    void addClick (int offset, bool downbeat);
    void updateUi();

    // Command queue (message -> audio thread)
    juce::AbstractFifo commandFifo { 64 };
    std::array<std::pair<Command, int>, 64> commandBuffer {};
    juce::SpinLock commandWriteLock; // several UI callers may push

    std::atomic<float> settingsBpm { 100.0f };
    std::atomic<int> settingsBars { 2 };
    std::atomic<bool> metronome { true };

    std::array<Track, numTracks> tracks;
    std::array<UiState, numTracks> ui;
    std::atomic<bool> uiRunning { false };
    std::atomic<float> uiProgress { 0.0f };
    std::atomic<int> uiCountInBeats { 0 };

    double sampleRate = 48000.0;
    juce::int64 beatLength = 0, barLength = 0, loopLength = 0;
    juce::int64 playhead = 0, sampleClock = 0;
    bool running = false;
    juce::int64 countInRemaining = 0;
    int countInTrack = -1;

    bool pedalDown = false, longPressHandled = false;
    juce::int64 pedalPressTime = 0;
    int pedalTrack = -1;

    // Metronome
    std::array<std::pair<int, bool>, maxClicksPerBlock> clicks {};
    int numClicks = 0;
    double clickPhase = 0.0, clickIncrement = 0.0;
    float clickLevel = 0.0f, clickDecay = 0.0f;
};
