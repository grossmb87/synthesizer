#include "LooperPanel.h"
#include "Controls.h"
#include "PluginProcessor.h"

namespace
{
    using State = Looper::State;

    juce::Colour stateColour (State s)
    {
        switch (s)
        {
            case State::CountIn:   return juce::Colour (0xffe5c07b);
            case State::Recording: return juce::Colour (0xffe06c75);
            case State::Playing:   return juce::Colour (0xff98c379);
            case State::Overdub:   return Colours::accent;
            case State::Muted:     return Colours::dimText;
            case State::Empty:     break;
        }
        return Colours::track;
    }

    juce::String stateText (State s)
    {
        switch (s)
        {
            case State::CountIn:   return "Count-in";
            case State::Recording: return "Recording";
            case State::Playing:   return "Playing";
            case State::Overdub:   return "Overdub";
            case State::Muted:     return "Muted";
            case State::Empty:     break;
        }
        return "Empty";
    }

    juce::String recordButtonText (State s)
    {
        switch (s)
        {
            case State::CountIn:   return "Cancel";
            case State::Recording: return "Stop Rec";
            case State::Playing:
            case State::Muted:     return "Overdub";
            case State::Overdub:   return "Stop Dub";
            case State::Empty:     break;
        }
        return "Rec";
    }
}

//==============================================================================
LooperPanel::TrackButton::TrackButton (LooperPanel& o, int i) : owner (o), index (i)
{
    muteButton.setTooltip ("Mute");
    clearButton.setTooltip ("Clear track");
    muteButton.setClickingTogglesState (false);
    muteButton.onClick = [this] { owner.looper.sendCommand (Looper::Command::ToggleMute, index); };
    clearButton.onClick = [this] { owner.looper.sendCommand (Looper::Command::Clear, index); };
    addAndMakeVisible (muteButton);
    addAndMakeVisible (clearButton);
}

void LooperPanel::TrackButton::paint (juce::Graphics& g)
{
    const auto state = owner.looper.getState (index);
    const auto selected = owner.processor.getSelectedTrack() == index;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (state == State::Empty ? Colours::display : stateColour (state).withAlpha (0.22f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? Colours::accent : Colours::outline);
    g.drawRoundedRectangle (r, 6.0f, selected ? 2.0f : 1.0f);

    auto text = r.reduced (8.0f, 3.0f).withTrimmedRight (24.0f);
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String (index + 1) + "  " + owner.processor.getTrackSoundName (index),
                text.removeFromTop (text.getHeight() / 2.0f), juce::Justification::centredLeft, true);

    g.setColour (stateColour (state).brighter (0.2f));
    g.setFont (juce::FontOptions (11.0f));
    auto status = stateText (state);
    if (const auto n = owner.looper.getNumEvents (index); n > 0)
        status << "  (" << n << ")";
    g.drawText (status, text, juce::Justification::centredLeft, true);
}

void LooperPanel::TrackButton::resized()
{
    auto r = getLocalBounds().reduced (4).removeFromRight (22);
    muteButton.setBounds (r.removeFromTop (r.getHeight() / 2).reduced (0, 1));
    clearButton.setBounds (r.reduced (0, 1));
}

void LooperPanel::TrackButton::mouseDown (const juce::MouseEvent&)
{
    owner.processor.selectTrack (index);
    if (owner.onTrackSelected != nullptr)
        owner.onTrackSelected();
}

//==============================================================================
LooperPanel::LooperPanel (SynthAudioProcessor& p) : processor (p), looper (p.looper)
{
    for (int i = 0; i < Looper::numTracks; ++i)
    {
        auto& b = trackButtons.emplace_back (std::make_unique<TrackButton> (*this, i));
        addAndMakeVisible (*b);
    }

    recordButton.setTooltip ("Same as the left pedal: tap = rec / overdub, hold 1 s = clear track");
    recordButton.onClick = [this] { looper.sendCommand (Looper::Command::Pedal, processor.getSelectedTrack()); };
    playButton.onClick = [this] { looper.sendCommand (looper.isRunning() ? Looper::Command::Stop : Looper::Command::Play); };
    clearAllButton.onClick = [this] { looper.sendCommand (Looper::Command::ClearAll); };

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setTextValueSuffix (" BPM");
    bpmSlider.setValue (looper.getBpm(), juce::dontSendNotification);
    bpmSlider.setDoubleClickReturnValue (true, 100.0);
    bpmSlider.setColour (juce::Slider::trackColourId, Colours::accent.withAlpha (0.35f));
    bpmSlider.onValueChange = [this] { looper.setBpm ((float) bpmSlider.getValue()); };

    for (int bars : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (bars) + (bars == 1 ? " bar" : " bars"), bars);
    barsBox.setSelectedId (looper.getBars(), juce::dontSendNotification);
    barsBox.onChange = [this] { looper.setBars (barsBox.getSelectedId()); };

    metronomeButton.setToggleState (looper.isMetronomeOn(), juce::dontSendNotification);
    metronomeButton.onClick = [this] { looper.setMetronome (metronomeButton.getToggleState()); };

    for (auto* c : std::initializer_list<juce::Component*> { &recordButton, &playButton, &clearAllButton, &bpmSlider, &barsBox, &metronomeButton })
        addAndMakeVisible (c);

    update();
}

void LooperPanel::update()
{
    const auto selected = processor.getSelectedTrack();
    const auto state = selected >= 0 ? looper.getState (selected) : State::Empty;
    const auto content = looper.hasContent();

    const auto countIn = looper.getCountInBeatsLeft();
    recordButton.setButtonText (countIn > 0 ? "Count-in " + juce::String (countIn) : recordButtonText (state));
    const auto active = state == State::Recording || state == State::Overdub || state == State::CountIn;
    recordButton.setColour (juce::TextButton::buttonColourId, active ? stateColour (state).withAlpha (0.6f) : Colours::track);

    playButton.setButtonText (looper.isRunning() ? "Stop" : "Play");
    playButton.setEnabled (looper.isRunning() || content);
    clearAllButton.setEnabled (content);

    // The loop length is fixed once something is recorded.
    bpmSlider.setEnabled (! content);
    barsBox.setEnabled (! content);

    for (int i = 0; i < (int) trackButtons.size(); ++i)
    {
        auto& b = *trackButtons[(size_t) i];
        const auto s = looper.getState (i);
        b.muteButton.setEnabled (s == State::Playing || s == State::Overdub || s == State::Muted);
        b.muteButton.setColour (juce::TextButton::buttonColourId, s == State::Muted ? Colours::accent.withAlpha (0.5f) : Colours::track);
        b.clearButton.setEnabled (s != State::Empty || looper.getNumEvents (i) > 0);
        b.repaint();
    }

    repaint (progressArea);
}

void LooperPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds(), "Looper   (left pedal: tap = rec / overdub, hold = clear)");

    // Loop position with beat (dim) and bar (bright) ticks.
    const auto r = progressArea.toFloat();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 3.0f);

    if (looper.isRunning())
    {
        g.setColour (Colours::accent.withAlpha (0.7f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * looper.getProgress()), 3.0f);
    }

    const auto beats = looper.getBars() * 4;
    for (int b = 1; b < beats; ++b)
    {
        const auto x = r.getX() + r.getWidth() * (float) b / (float) beats;
        g.setColour (b % 4 == 0 ? Colours::text : Colours::track);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
}

void LooperPanel::resized()
{
    auto r = getLocalBounds().reduced (8).withTrimmedTop (20);

    auto controls = r.removeFromRight (510);
    auto place = [&controls] (juce::Component& c, int width)
    {
        c.setBounds (controls.removeFromLeft (width).reduced (3, 6));
    };
    place (recordButton, 100);
    place (playButton, 70);
    place (bpmSlider, 110);
    place (barsBox, 80);
    place (metronomeButton, 70);
    place (clearAllButton, 80);

    progressArea = r.removeFromBottom (8).reduced (2, 0);
    r.removeFromBottom (3);

    const auto w = r.getWidth() / (int) trackButtons.size();
    for (auto& b : trackButtons)
        b->setBounds (r.removeFromLeft (w));
}
