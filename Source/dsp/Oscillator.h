#pragma once

#include <cmath>

// Band-limited oscillator. Saw and square use PolyBLEP to suppress aliasing;
// the triangle is naive (its harmonics fall off at 1/n^2, so aliasing is low).
class Oscillator
{
public:
    enum class Wave { Sine, Saw, Square, Triangle };

    void reset (double startPhase = 0.0) noexcept { phase = startPhase; }

    void setFrequency (double hz, double sampleRate) noexcept { increment = hz / sampleRate; }
    void setWave (Wave w) noexcept                            { wave = w; }
    void setPulseWidth (float pw) noexcept                    { pulseWidth = pw; }

    double getPhase() const noexcept     { return phase; }
    double getIncrement() const noexcept { return increment; }
    bool hasWrapped() const noexcept     { return wrapped; }

    // Hard sync: restart the cycle, keeping the master's sub-sample offset.
    void syncTo (double masterPhase, double masterIncrement) noexcept
    {
        phase = masterIncrement > 0.0 ? masterPhase * (increment / masterIncrement) : 0.0;
        phase -= std::floor (phase);
    }

    float process() noexcept
    {
        const auto t = phase;
        float out = 0.0f;

        switch (wave)
        {
            case Wave::Sine:
                out = (float) std::sin (t * twoPi);
                break;

            case Wave::Saw:
                out = (float) (2.0 * t - 1.0 - polyBlep (t, increment));
                break;

            case Wave::Square:
            {
                const auto pw = (double) pulseWidth;
                auto v = t < pw ? 1.0 : -1.0;
                v += polyBlep (t, increment);
                v -= polyBlep (std::fmod (t + 1.0 - pw, 1.0), increment);
                out = (float) v;
                break;
            }

            case Wave::Triangle:
                out = (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t);
                break;
        }

        phase += increment;
        wrapped = phase >= 1.0;
        if (wrapped)
            phase -= 1.0;

        return out;
    }

private:
    static constexpr double twoPi = 6.283185307179586;

    static double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double phase = 0.0, increment = 0.0;
    float pulseWidth = 0.5f;
    bool wrapped = false;
    Wave wave = Wave::Saw;
};
