# Synthesizer

A polyphonic subtractive synthesizer, built with [JUCE 9](https://juce.com) as a standalone app and a VST3 plugin. It's designed to be played from a Yamaha Clavinova CLP-875 over USB MIDI.

## Features
- 2 oscillators (sine / saw / square with pulse width / triangle, PolyBLEP band-limited) plus noise
- Resonant state-variable filter (LP / HP / BP) with its own ADSR, key tracking, and velocity
- Amp ADSR with velocity
- 2 LFOs (pitch / cutoff / amp / pulse width)
- Chorus, delay, reverb, and master gain with soft clipping
- Sustain pedal support and presets
- Live displays: oscillator waveform, envelope shapes, LFOs with their current phase, oscilloscope, and an FFT spectrum with the filter's response curve at the live cutoff
- 4-track MIDI looper. Each track has its own sound, and there's tempo, bar length, a 1-bar count-in and a metronome
- Vertical faders: click to jump, drag, Ctrl+drag for fine adjustment, double-click to reset

## Build (Windows)
Requirements: Visual Studio 2022 (Desktop C++) and CMake >= 3.22. JUCE downloads automatically on the first configure.

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The standalone app is written to `build/Synth_artefacts/Release/Standalone/Synth.exe`.

## Connecting the CLP-875
1. Connect the piano's **USB TO HOST** port to the PC. No driver is needed.
2. Start the standalone app and open **Options → Audio/MIDI Settings**.
3. Enable the piano as a MIDI input and choose an audio output (ASIO or WASAPI, 128–256 samples).

## Looper
- **Click a track** to select it. The faders and presets then edit that track's sound, and the keyboard plays it.
- **Left pedal (soft pedal) or the Rec button:**
  - On an empty track it starts a 1-bar count-in, then records one loop. If the loop is already running, recording starts immediately.
  - While recording, it stops recording early.
  - On a playing track it toggles overdub on and off.
  - Holding it for 1 second clears the selected track.
- **M** mutes a track, **X** clears it, and **Clear All** clears every track. Tempo and bar count are locked once something has been recorded.
- The effects (chorus, delay, reverb, master) are shared by all tracks.
- Tests: `cmake --build build --config Release --target LooperTest`, then run `build/LooperTest_artefacts/Release/LooperTest.exe`.
