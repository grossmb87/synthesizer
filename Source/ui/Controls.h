#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Colours
{
    inline const juce::Colour background { 0xff15171c };
    inline const juce::Colour panel      { 0xff1f232b };
    inline const juce::Colour outline    { 0xff2e3440 };
    inline const juce::Colour text       { 0xffd8dee9 };
    inline const juce::Colour dimText    { 0xff8a93a5 };
    inline const juce::Colour accent     { 0xfff0a35e };
    inline const juce::Colour track      { 0xff3b4252 };
    inline const juce::Colour secondary  { 0xff6cb6e8 };
    inline const juce::Colour grid       { 0xff2a2f39 };
    inline const juce::Colour display    { 0xff111318 };
}

// Rounded panel with an upper-case title; returns the area below the title.
inline juce::Rectangle<int> drawPanel (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
{
    const auto r = bounds.toFloat().reduced (3.0f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), r.reduced (10.0f, 4.0f).removeFromTop (18.0f), juce::Justification::left);

    return bounds.reduced (8).withTrimmedTop (20);
}

class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Colours::dimText);
        setColour (juce::ComboBox::backgroundColourId, Colours::track);
        setColour (juce::ComboBox::outlineColourId, Colours::outline);
        setColour (juce::ComboBox::textColourId, Colours::text);
        setColour (juce::ComboBox::arrowColourId, Colours::accent);
        setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.3f));
        setColour (juce::TextButton::buttonColourId, Colours::track);
        setColour (juce::TextButton::textColourOffId, Colours::text);
        setColour (juce::ToggleButton::tickColourId, Colours::accent);
        setColour (juce::ToggleButton::textColourId, Colours::dimText);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.isBar()) // e.g. the BPM field: keep the stock look
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const auto centreX = bounds.getCentreX();
        const auto trackWidth = 6.0f;
        const auto track = juce::Rectangle<float> (trackWidth, bounds.getHeight()).withCentre (bounds.getCentre());

        g.setColour (Colours::track);
        g.fillRoundedRectangle (track, trackWidth / 2.0f);

        // Bipolar parameters fill from zero, the rest from the bottom.
        const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto originY = bipolar ? (float) slider.getPositionOfValue (0.0) : track.getBottom();
        g.setColour (Colours::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX(), juce::jmin (originY, sliderPos),
                                                        trackWidth, std::abs (originY - sliderPos)), trackWidth / 2.0f);

        const auto thumb = juce::Rectangle<float> (juce::jmin (28.0f, bounds.getWidth() - 4.0f), 12.0f)
                               .withCentre ({ centreX, sliderPos });
        g.setColour (slider.isMouseOverOrDragging() ? Colours::text : Colours::panel.brighter (0.4f));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (Colours::background);
        g.drawHorizontalLine ((int) sliderPos, thumb.getX() + 4.0f, thumb.getRight() - 4.0f);
    }
};

// Vertical fader with a caption, bound to one parameter. Click anywhere on
// the track to jump there; double-click resets to the default.
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider.setSliderSnapsToMousePosition (true);
        slider.setMouseDragSensitivity (150);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

        if (auto* param = apvts.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Combo box with a caption, bound to a choice parameter.
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 24));
    }

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Toggle button with a caption, bound to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption)
    {
        button.setButtonText (caption);
        addAndMakeVisible (button);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, button);
    }

    void resized() override { button.setBounds (getLocalBounds().withSizeKeepingCentre (getWidth() - 4, 24)); }

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Titled panel that lays its controls out in a single row.
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& sectionTitle) : title (sectionTitle) {}

    template <typename Control>
    void add (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption)
    {
        auto& c = controls.emplace_back (std::make_unique<Control> (apvts, paramId, caption));
        addAndMakeVisible (*c);
    }

    int getNumControls() const { return (int) controls.size(); }

    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds(), title); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8).withTrimmedTop (20);

        if (controls.empty())
            return;

        const auto w = r.getWidth() / (int) controls.size();
        for (auto& c : controls)
            c->setBounds (r.removeFromLeft (w));
    }

private:
    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> controls;
};
