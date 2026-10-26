#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SynthAudioProcessor;

// One cycle-accurate preview of the oscillator mix (osc 1 + osc 2 + noise),
// computed from the current settings.
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
    void paint (juce::Graphics&) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
};

// Amp and filter ADSR shapes. Segment widths scale with the square root of
// their time so that both short and long stages stay readable.
class EnvelopeView : public juce::Component
{
public:
    explicit EnvelopeView (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
    void paint (juce::Graphics&) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
};

// Both LFO shapes with their live phase and routing.
class LfoView : public juce::Component
{
public:
    explicit LfoView (SynthAudioProcessor& p);
    void paint (juce::Graphics&) override;

private:
    SynthAudioProcessor& processor;
};

// Zero-crossing triggered oscilloscope of the output, auto-scaled.
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (const std::vector<float>& h) : history (h) {}
    void paint (juce::Graphics&) override;

private:
    const std::vector<float>& history;
    float displayPeak = 0.05f;
};

// FFT spectrum of the output with the filter's frequency response on top.
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (SynthAudioProcessor& p);

    void update (const std::vector<float>& history); // call from the UI timer
    void paint (juce::Graphics&) override;

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float minDb = -96.0f, maxDb = 24.0f;

    double sampleRate() const;
    float frequencyToX (float hz, juce::Rectangle<float> area) const;
    float dbToY (float db, juce::Rectangle<float> area) const;
    float filterResponseDb (float hz, float cutoffHz) const;

    SynthAudioProcessor& processor;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftData = std::vector<float> (2 * fftSize);
    std::vector<float> levelsDb = std::vector<float> (fftSize / 2, minDb);
};
