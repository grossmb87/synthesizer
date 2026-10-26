#include "PluginEditor.h"

SynthAudioProcessorEditor::SynthAudioProcessorEditor (SynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    buildSections();
    for (auto& row : rows)
        for (auto& section : row)
            addAndMakeVisible (*section);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this] { presetChosen(); };
    addAndMakeVisible (presetBox);

    saveButton.onClick = [this] { savePreset(); };
    deleteButton.onClick = [this] { deletePreset(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);

    keyboard.setAvailableRange (21, 108); // the 88 keys of the CLP-875
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    for (auto* view : std::initializer_list<juce::Component*> { &waveformView, &envelopeView, &lfoView, &scopeView, &spectrumView })
        addAndMakeVisible (view);

    looperPanel.onTrackSelected = [this] { refreshPresetList(); };
    addAndMakeVisible (looperPanel);

    refreshPresetList();

    // Fit the default size to the screen (minus taskbar and window title).
    auto height = 1020;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        height = juce::jlimit (780, 1020, (int) display->userBounds.getHeight() - 40);

    setResizable (true, true);
    setResizeLimits (1000, 780, 2400, 1600);
    setSize (1200, height);
    startTimerHz (30);
}

SynthAudioProcessorEditor::~SynthAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SynthAudioProcessorEditor::timerCallback()
{
    processor.outputTap.pull (outputHistory);
    spectrumView.update (outputHistory);

    // The setting views are cheap to draw, so they simply follow the parameters every frame.
    waveformView.repaint();
    envelopeView.repaint();
    lfoView.repaint();
    scopeView.repaint();
    spectrumView.repaint();
    looperPanel.update();
}

void SynthAudioProcessorEditor::buildSections()
{
    auto& s = processor.apvts;
    auto section = [] (const juce::String& title) { return std::make_unique<Section> (title); };

    rows.resize (4);

    for (int i = 1; i <= 2; ++i)
    {
        auto osc = section ("Oscillator " + juce::String (i));
        osc->add<Choice> (s, ParamID::oscWave (i), "Wave");
        osc->add<Fader> (s, ParamID::oscOctave (i), "Octave");
        osc->add<Fader> (s, ParamID::oscSemi (i), "Semi");
        osc->add<Fader> (s, ParamID::oscFine (i), "Fine");
        osc->add<Fader> (s, ParamID::oscPW (i), "Pulse W.");
        osc->add<Fader> (s, ParamID::oscLevel (i), "Level");
        if (i == 2)
            osc->add<Toggle> (s, ParamID::osc2Sync, "Sync");
        rows[0].push_back (std::move (osc));
    }
    auto noise = section ("Noise");
    noise->add<Fader> (s, ParamID::noiseLevel, "Level");
    rows[0].push_back (std::move (noise));

    auto filter = section ("Filter");
    filter->add<Choice> (s, ParamID::filterType, "Type");
    filter->add<Fader> (s, ParamID::filterCutoff, "Cutoff");
    filter->add<Fader> (s, ParamID::filterReso, "Resonance");
    filter->add<Fader> (s, ParamID::filterEnvAmt, "Env Amt");
    filter->add<Fader> (s, ParamID::filterKeyTrack, "Key Track");
    filter->add<Fader> (s, ParamID::filterVelocity, "Velocity");
    rows[1].push_back (std::move (filter));

    auto filterEnv = section ("Filter Envelope");
    filterEnv->add<Fader> (s, ParamID::fAttack, "Attack");
    filterEnv->add<Fader> (s, ParamID::fDecay, "Decay");
    filterEnv->add<Fader> (s, ParamID::fSustain, "Sustain");
    filterEnv->add<Fader> (s, ParamID::fRelease, "Release");
    rows[1].push_back (std::move (filterEnv));

    auto ampEnv = section ("Amp Envelope");
    ampEnv->add<Fader> (s, ParamID::aAttack, "Attack");
    ampEnv->add<Fader> (s, ParamID::aDecay, "Decay");
    ampEnv->add<Fader> (s, ParamID::aSustain, "Sustain");
    ampEnv->add<Fader> (s, ParamID::aRelease, "Release");
    ampEnv->add<Fader> (s, ParamID::ampVelocity, "Velocity");
    rows[2].push_back (std::move (ampEnv));

    for (int i = 1; i <= 2; ++i)
    {
        auto lfo = section ("LFO " + juce::String (i));
        lfo->add<Choice> (s, ParamID::lfoWave (i), "Wave");
        lfo->add<Fader> (s, ParamID::lfoRate (i), "Rate");
        lfo->add<Fader> (s, ParamID::lfoDepth (i), "Depth");
        lfo->add<Choice> (s, ParamID::lfoDest (i), "Target");
        rows[2].push_back (std::move (lfo));
    }

    auto chorus = section ("Chorus");
    chorus->add<Fader> (s, ParamID::chorusRate, "Rate");
    chorus->add<Fader> (s, ParamID::chorusDepth, "Depth");
    chorus->add<Fader> (s, ParamID::chorusMix, "Mix");
    rows[3].push_back (std::move (chorus));

    auto delay = section ("Delay");
    delay->add<Fader> (s, ParamID::delayTime, "Time");
    delay->add<Fader> (s, ParamID::delayFeedback, "Feedback");
    delay->add<Fader> (s, ParamID::delayMix, "Mix");
    rows[3].push_back (std::move (delay));

    auto reverb = section ("Reverb");
    reverb->add<Fader> (s, ParamID::reverbSize, "Size");
    reverb->add<Fader> (s, ParamID::reverbDamping, "Damping");
    reverb->add<Fader> (s, ParamID::reverbMix, "Mix");
    rows[3].push_back (std::move (reverb));

    auto master = section ("Master");
    master->add<Fader> (s, ParamID::masterGain, "Gain");
    rows[3].push_back (std::move (master));
}

void SynthAudioProcessorEditor::refreshPresetList()
{
    auto& presets = processor.presets;
    const auto current = presets.getCurrentPresetName();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    const auto factory = presets.getFactoryPresetNames();
    presetBox.addItemList (factory, 1);

    const auto user = presets.getUserPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        presetBox.addItemList (user, userPresetIdOffset);
    }

    // User presets take precedence when a name exists in both lists.
    if (const auto u = user.indexOf (current); u >= 0)
        presetBox.setSelectedId (userPresetIdOffset + u, juce::dontSendNotification);
    else if (const auto f = factory.indexOf (current); f >= 0)
        presetBox.setSelectedId (1 + f, juce::dontSendNotification);
    else
        presetBox.setText (current, juce::dontSendNotification);

    deleteButton.setEnabled (user.contains (current));
}

void SynthAudioProcessorEditor::presetChosen()
{
    const auto id = presetBox.getSelectedId();
    const auto name = presetBox.getText();
    if (id <= 0)
        return;

    if (id >= userPresetIdOffset)
        processor.presets.loadUserPreset (name);
    else
        processor.presets.loadFactoryPreset (name);

    refreshPresetList();
}

void SynthAudioProcessorEditor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", processor.presets.getCurrentPresetName());
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        const auto name = saveDialog->getTextEditorContents ("name").trim();
        saveDialog.reset();

        if (result != 1 || name.isEmpty())
            return;

        if (! processor.presets.saveUserPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save Preset",
                                                    "Could not write to " + PresetManager::getUserPresetDirectory().getFullPathName());
        refreshPresetList();
    }));
}

void SynthAudioProcessorEditor::deletePreset()
{
    const auto name = processor.presets.getCurrentPresetName();
    juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete Preset",
                                             "Delete user preset \"" + name + "\"?", this,
                                             juce::ModalCallbackFunction::create ([this, name] (int ok)
    {
        if (ok == 0)
            return;
        processor.presets.deleteUserPreset (name);
        processor.presets.loadFactoryPreset ("Init");
        refreshPresetList();
    }));
}

void SynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SYNTH", getLocalBounds().reduced (16, 0).removeFromTop (48), juce::Justification::centredLeft);
}

void SynthAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (40).reduced (0, 6);
    deleteButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    saveButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (240));

    keyboard.setBounds (r.removeFromBottom (90).reduced (3, 4));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 52.0f); // 52 white keys
    r.removeFromBottom (4);

    looperPanel.setBounds (r.removeFromTop (84));

    juce::FlexBox displays;
    displays.items.add (juce::FlexItem (waveformView).withFlex (1.0f));
    displays.items.add (juce::FlexItem (envelopeView).withFlex (1.0f));
    displays.items.add (juce::FlexItem (lfoView).withFlex (1.0f));
    displays.items.add (juce::FlexItem (scopeView).withFlex (1.3f));
    displays.items.add (juce::FlexItem (spectrumView).withFlex (1.8f));
    displays.performLayout (r.removeFromTop (juce::jlimit (140, 320, r.getHeight() / 5)));

    const auto rowHeight = r.getHeight() / (int) rows.size();
    for (auto& row : rows)
    {
        juce::FlexBox flex;
        for (auto& section : row)
            flex.items.add (juce::FlexItem (*section).withFlex ((float) section->getNumControls()));
        flex.performLayout (r.removeFromTop (rowHeight));
    }
}
