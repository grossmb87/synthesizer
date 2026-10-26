#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Looper.h"

class SynthAudioProcessor;

// Track selector + looper transport. Clicking a track selects it: the sound
// controls then edit that track's sound and the keyboard plays it.
class LooperPanel : public juce::Component
{
public:
    explicit LooperPanel (SynthAudioProcessor&);

    // Refresh from the processor (called by the editor's UI timer).
    void update();

    std::function<void()> onTrackSelected;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class TrackButton : public juce::Component
    {
    public:
        TrackButton (LooperPanel& owner, int index);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        juce::TextButton muteButton { "M" }, clearButton { "X" };

    private:
        LooperPanel& owner;
        int index;
    };

    SynthAudioProcessor& processor;
    Looper& looper;

    std::vector<std::unique_ptr<TrackButton>> trackButtons;
    juce::TextButton recordButton, playButton, clearAllButton { "Clear All" };
    juce::Slider bpmSlider;
    juce::ComboBox barsBox;
    juce::ToggleButton metronomeButton { "Click" };
    juce::Rectangle<int> progressArea;
};
