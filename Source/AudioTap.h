#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Lock-free single-producer / single-consumer hand-off of the mono output
// from the audio thread to the UI.
class AudioTap
{
public:
    void push (const juce::AudioBuffer<float>& buffer)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = buffer.getNumChannels();
        const auto scale = numChannels > 0 ? 1.0f / (float) numChannels : 0.0f;

        const auto scope = fifo.write (numSamples); // drops samples if the UI falls behind
        auto write = [&] (int start, int size, int offset)
        {
            for (int i = 0; i < size; ++i)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    sum += buffer.getSample (ch, offset + i);
                data[(size_t) (start + i)] = sum * scale;
            }
        };
        write (scope.startIndex1, scope.blockSize1, 0);
        write (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    // Appends everything available to `history`, keeping its size constant
    // (oldest samples are shifted out).
    void pull (std::vector<float>& history)
    {
        const auto scope = fifo.read (fifo.getNumReady());
        const auto total = scope.blockSize1 + scope.blockSize2;
        if (total == 0 || history.empty())
            return;

        // Only the newest `n` samples fit; skip the rest.
        const auto n = juce::jmin (total, (int) history.size());
        std::move (history.begin() + n, history.end(), history.begin());

        auto out = history.end() - n;
        for (int k = total - n; k < total; ++k)
        {
            const auto index = k < scope.blockSize1 ? scope.startIndex1 + k
                                                    : scope.startIndex2 + (k - scope.blockSize1);
            *out++ = data[(size_t) index];
        }
    }

private:
    static constexpr int capacity = 32768;
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> data {};
};
