#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Bipolar (-1..1) low-frequency oscillator.
class Lfo
{
public:
    enum class Wave { Sine, Triangle, Saw, Square, SampleHold };

    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; reset(); }
    void reset() noexcept                        { phase = 0.0; holdValue = 0.0f; }

    void setWave (Wave w) noexcept    { wave = w; }
    void setRate (float hz) noexcept  { increment = hz / sampleRate; }

    float process() noexcept
    {
        const auto t = phase;
        float out = 0.0f;

        switch (wave)
        {
            case Wave::Sine:       out = (float) std::sin (t * 6.283185307179586); break;
            case Wave::Triangle:   out = (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t); break;
            case Wave::Saw:        out = (float) (2.0 * t - 1.0); break;
            case Wave::Square:     out = t < 0.5 ? 1.0f : -1.0f; break;
            case Wave::SampleHold: out = holdValue; break;
        }

        phase += increment;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            holdValue = random.nextFloat() * 2.0f - 1.0f;
        }

        return out;
    }

private:
    double sampleRate = 44100.0, phase = 0.0, increment = 0.0;
    float holdValue = 0.0f;
    Wave wave = Wave::Sine;
    juce::Random random;
};
