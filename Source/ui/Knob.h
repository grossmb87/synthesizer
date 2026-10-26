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

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto stroke = 3.5f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::track);
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius - stroke, radius - stroke, 0.0f, startAngle, angle, true);
        g.setColour (Colours::accent);
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto knobRadius = radius - stroke * 2.5f;
        g.setColour (Colours::panel.brighter (0.15f));
        g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));

        const auto tip = centre.getPointOnCircumference (knobRadius * 0.8f, angle);
        g.setColour (Colours::text);
        g.drawLine ({ centre.getPointOnCircumference (knobRadius * 0.25f, angle), tip }, 2.0f);
    }
};

// Rotary slider with a caption, bound to one parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
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

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        g.setColour (Colours::accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), r.reduced (10.0f, 4.0f).removeFromTop (18.0f), juce::Justification::left);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        r.removeFromTop (20);

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
