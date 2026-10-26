#include "Visualizers.h"
#include "Controls.h"
#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    float value (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        return apvts.getRawParameterValue (id)->load();
    }

    int choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        return (int) value (apvts, id);
    }

    juce::String choiceText (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
        return p != nullptr ? p->getCurrentChoiceName() : juce::String();
    }

    // Naive (non band-limited) shapes; fine for drawing.
    float oscShape (int wave, double t, float pw)
    {
        switch (wave)
        {
            case 0:  return (float) std::sin (t * juce::MathConstants<double>::twoPi);
            case 1:  return (float) (2.0 * t - 1.0);
            case 2:  return t < pw ? 1.0f : -1.0f;
            default: return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t);
        }
    }

    float lfoShape (int wave, double t, const std::array<float, 8>& steps)
    {
        switch (wave)
        {
            case 0:  return (float) std::sin (t * juce::MathConstants<double>::twoPi);
            case 1:  return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t);
            case 2:  return (float) (2.0 * t - 1.0);
            case 3:  return t < 0.5 ? 1.0f : -1.0f;
            default: return steps[(size_t) juce::jlimit (0, 7, (int) (t * 8.0))];
        }
    }

    void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (Colours::display);
        g.fillRoundedRectangle (area, 4.0f);
    }

    void drawCaption (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text,
                      juce::Justification just = juce::Justification::topRight)
    {
        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (text, area.reduced (6.0f, 3.0f), just);
    }

    juce::String formatHz (float hz)
    {
        return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, 1) + " kHz";
    }
}

//==============================================================================
void WaveformView::paint (juce::Graphics& g)
{
    const auto area = drawPanel (g, getLocalBounds(), "Waveform").toFloat();
    drawDisplayBackground (g, area);

    float levels[2] {}, pw[2] {};
    int waves[2] {};
    double pitch[2] {};
    for (int i = 0; i < 2; ++i)
    {
        waves[i] = choice (apvts, ParamID::oscWave (i + 1));
        levels[i] = value (apvts, ParamID::oscLevel (i + 1));
        pw[i] = value (apvts, ParamID::oscPW (i + 1));
        pitch[i] = value (apvts, ParamID::oscOctave (i + 1)) * 12.0 + value (apvts, ParamID::oscSemi (i + 1));
    }
    const auto ratio = std::exp2 ((pitch[1] - pitch[0]) / 12.0); // fine detune is left out to keep the picture still
    const auto sync = value (apvts, ParamID::osc2Sync) > 0.5f;
    const auto noise = value (apvts, ParamID::noiseLevel);

    const auto inner = area.reduced (4.0f, 8.0f);
    const auto numPoints = juce::jmax (2, (int) inner.getWidth());
    constexpr double cycles = 2.0;

    std::vector<float> samples ((size_t) numPoints);
    juce::Random random (1); // fixed seed: the noise looks the same on every repaint
    float peak = 0.0f;

    for (int x = 0; x < numPoints; ++x)
    {
        const auto t = cycles * x / (numPoints - 1);
        const auto p1 = t - std::floor (t);
        auto t2 = sync ? p1 * ratio : t * ratio;
        t2 -= std::floor (t2);

        const auto s = levels[0] * oscShape (waves[0], p1, pw[0])
                     + levels[1] * oscShape (waves[1], t2, pw[1])
                     + noise * (random.nextFloat() * 2.0f - 1.0f);
        samples[(size_t) x] = s;
        peak = juce::jmax (peak, std::abs (s));
    }

    g.setColour (Colours::grid);
    g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());

    if (peak < 1.0e-4f)
    {
        drawCaption (g, area, "silent", juce::Justification::centred);
        return;
    }

    juce::Path path;
    for (int x = 0; x < numPoints; ++x)
    {
        const auto px = inner.getX() + (float) x;
        const auto py = inner.getCentreY() - samples[(size_t) x] / peak * inner.getHeight() * 0.5f;
        x == 0 ? path.startNewSubPath (px, py) : path.lineTo (px, py);
    }

    g.setColour (Colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.6f));
    drawCaption (g, area, "2 cycles of Osc 1");
}

//==============================================================================
void EnvelopeView::paint (juce::Graphics& g)
{
    const auto area = drawPanel (g, getLocalBounds(), "Envelopes").toFloat();
    drawDisplayBackground (g, area);
    const auto inner = area.reduced (6.0f, 10.0f).withTrimmedTop (8.0f);

    auto drawEnvelope = [&] (const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r, juce::Colour colour)
    {
        const auto attack = value (apvts, a), decay = value (apvts, d), sustain = value (apvts, s), release = value (apvts, r);

        // 25 % of the width shows the held sustain; the rest is split by sqrt(time).
        const auto wa = std::sqrt (attack), wd = std::sqrt (decay), wr = std::sqrt (release);
        const auto scale = inner.getWidth() * 0.75f / (wa + wd + wr);
        const auto xA = inner.getX() + wa * scale;
        const auto xD = xA + wd * scale;
        const auto xS = xD + inner.getWidth() * 0.25f;
        const auto xR = xS + wr * scale;
        const auto yFor = [&] (float level) { return inner.getBottom() - level * inner.getHeight(); };

        juce::Path path;
        path.startNewSubPath (inner.getX(), inner.getBottom());
        path.lineTo (xA, yFor (1.0f));
        path.lineTo (xD, yFor (sustain));
        path.lineTo (xS, yFor (sustain));
        path.lineTo (xR, inner.getBottom());

        auto fill = path;
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    };

    drawEnvelope (ParamID::fAttack, ParamID::fDecay, ParamID::fSustain, ParamID::fRelease, Colours::secondary);
    drawEnvelope (ParamID::aAttack, ParamID::aDecay, ParamID::aSustain, ParamID::aRelease, Colours::accent);

    g.setFont (juce::FontOptions (11.0f));
    const auto legend = area.reduced (6.0f, 3.0f).removeFromTop (14.0f);
    g.setColour (Colours::accent);
    g.drawText ("Amp", legend, juce::Justification::left);
    g.setColour (Colours::secondary);
    g.drawText ("Filter", legend.withTrimmedLeft (34.0f), juce::Justification::left);
}

//==============================================================================
LfoView::LfoView (SynthAudioProcessor& p) : processor (p) {}

void LfoView::paint (juce::Graphics& g)
{
    auto& apvts = processor.apvts;
    const auto area = drawPanel (g, getLocalBounds(), "LFOs").toFloat();
    drawDisplayBackground (g, area);

    std::array<float, 8> steps {};
    juce::Random random (7);
    for (auto& s : steps)
        s = random.nextFloat() * 2.0f - 1.0f;

    auto lanes = area.reduced (6.0f, 4.0f);
    const auto laneHeight = lanes.getHeight() / 2.0f;

    for (int i = 0; i < 2; ++i)
    {
        auto lane = lanes.removeFromTop (laneHeight);
        const auto caption = lane.removeFromTop (14.0f);
        const auto plot = lane.reduced (0.0f, 3.0f);

        const auto wave = choice (apvts, ParamID::lfoWave (i + 1));
        const auto active = choice (apvts, ParamID::lfoDest (i + 1)) != (int) LfoDest::Off
                         && value (apvts, ParamID::lfoDepth (i + 1)) > 0.0f;
        const auto colour = (i == 0 ? Colours::accent : Colours::secondary).withAlpha (active ? 1.0f : 0.35f);

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (colour);
        g.drawText ("LFO " + juce::String (i + 1) + "  " + juce::String (value (apvts, ParamID::lfoRate (i + 1)), 2) + " Hz"
                        + "  >  " + choiceText (apvts, ParamID::lfoDest (i + 1)),
                    caption, juce::Justification::left);

        const auto numPoints = juce::jmax (2, (int) plot.getWidth());
        const auto yFor = [&] (float v) { return plot.getCentreY() - v * plot.getHeight() * 0.5f; };

        juce::Path path;
        for (int x = 0; x < numPoints; ++x)
        {
            const auto t = (double) x / (numPoints - 1);
            const auto px = plot.getX() + (float) x, py = yFor (lfoShape (wave, juce::jmin (t, 0.9999), steps));
            x == 0 ? path.startNewSubPath (px, py) : path.lineTo (px, py);
        }
        g.strokePath (path, juce::PathStrokeType (1.4f));

        const auto phase = (double) processor.getLfoPhase (i);
        const auto dot = juce::Point<float> (plot.getX() + (float) phase * plot.getWidth(), yFor (lfoShape (wave, phase, steps)));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
    }
}

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    const auto area = drawPanel (g, getLocalBounds(), "Oscilloscope").toFloat();
    drawDisplayBackground (g, area);
    const auto inner = area.reduced (4.0f, 8.0f);

    g.setColour (Colours::grid);
    g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());

    constexpr int windowSize = 1024;
    const auto size = (int) history.size();
    if (size < windowSize * 2)
        return;

    // Trigger on the newest rising zero crossing that still leaves a full window.
    auto start = size - windowSize;
    for (int i = size - windowSize; i > size - windowSize * 2; --i)
    {
        if (history[(size_t) i - 1] <= 0.0f && history[(size_t) i] > 0.0f)
        {
            start = i;
            break;
        }
    }

    float peak = 0.0f;
    for (int i = 0; i < windowSize; ++i)
        peak = juce::jmax (peak, std::abs (history[(size_t) (start + i)]));
    displayPeak = juce::jmax (peak, displayPeak * 0.92f, 0.01f);

    juce::Path path;
    for (int i = 0; i < windowSize; ++i)
    {
        const auto px = inner.getX() + inner.getWidth() * (float) i / (windowSize - 1);
        const auto py = inner.getCentreY() - history[(size_t) (start + i)] / displayPeak * inner.getHeight() * 0.48f;
        i == 0 ? path.startNewSubPath (px, py) : path.lineTo (px, py);
    }

    g.setColour (Colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.4f));
    drawCaption (g, area, peak < 1.0e-5f ? juce::String ("silent")
                                         : "peak " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS");
}

//==============================================================================
SpectrumView::SpectrumView (SynthAudioProcessor& p) : processor (p) {}

double SpectrumView::sampleRate() const
{
    const auto sr = processor.getSampleRate();
    return sr > 0.0 ? sr : 48000.0;
}

void SpectrumView::update (const std::vector<float>& history)
{
    if ((int) history.size() < fftSize)
        return;

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (history.end() - fftSize, history.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Hann window has a coherent gain of 0.5, so a full-scale sine reads 0 dBFS.
    const auto norm = 4.0f / (float) fftSize;
    for (size_t i = 0; i < levelsDb.size(); ++i)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[i] * norm, minDb);
        levelsDb[i] = juce::jmax (db, levelsDb[i] - 2.5f); // fast attack, slow fall
    }
}

float SpectrumView::frequencyToX (float hz, juce::Rectangle<float> area) const
{
    const auto norm = std::log (hz / 20.0f) / std::log (20000.0f / 20.0f);
    return area.getX() + norm * area.getWidth();
}

float SpectrumView::dbToY (float db, juce::Rectangle<float> area) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, area.getBottom(), area.getY());
}

float SpectrumView::filterResponseDb (float hz, float cutoffHz) const
{
    // The TPT state-variable filter equals the analog prototype under the
    // bilinear transform, so evaluate that at the pre-warped frequency.
    auto& apvts = processor.apvts;
    const auto sr = sampleRate();
    const auto q = 0.707 * std::pow (12.0 / 0.707, (double) value (apvts, ParamID::filterReso));
    const auto w = std::tan (juce::MathConstants<double>::pi * juce::jmin ((double) hz, sr * 0.499) / sr)
                 / std::tan (juce::MathConstants<double>::pi * cutoffHz / sr);

    const auto denominator = std::hypot (1.0 - w * w, w / q);
    double numerator = 1.0;
    switch (choice (apvts, ParamID::filterType))
    {
        case 1:  numerator = w * w; break; // high pass
        case 2:  numerator = w; break;     // band pass
        default: break;                    // low pass
    }

    return juce::Decibels::gainToDecibels ((float) (numerator / denominator), -200.0f);
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto area = drawPanel (g, getLocalBounds(), "Spectrum + Filter").toFloat();
    drawDisplayBackground (g, area);
    const auto plot = area.reduced (6.0f, 6.0f).withTrimmedBottom (12.0f);

    // Grid
    g.setFont (juce::FontOptions (10.0f));
    for (auto hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = frequencyToX (hz, plot);
        g.setColour (Colours::grid);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (Colours::dimText);
        g.drawText (hz < 1000.0f ? juce::String ((int) hz) : juce::String ((int) (hz / 1000.0f)) + "k",
                    juce::Rectangle<float> (40.0f, 12.0f).withCentre ({ x, plot.getBottom() + 7.0f }), juce::Justification::centred);
    }
    for (auto db : { 0.0f, -24.0f, -48.0f, -72.0f })
    {
        const auto y = dbToY (db, plot);
        g.setColour (Colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Colours::dimText);
        g.drawText (juce::String ((int) db) + " dB", juce::Rectangle<float> (plot.getX() + 2.0f, y - 12.0f, 50.0f, 12.0f), juce::Justification::left);
    }

    // Spectrum
    const auto sr = (float) sampleRate();
    const auto binHz = sr / (float) fftSize;
    juce::Path spectrum;
    spectrum.startNewSubPath (plot.getX(), plot.getBottom());
    for (int x = 0; x < (int) plot.getWidth(); ++x)
    {
        // Take the loudest bin under each pixel so peaks are not lost at high frequencies.
        const auto f0 = 20.0f * std::pow (1000.0f, (float) x / plot.getWidth());
        const auto f1 = 20.0f * std::pow (1000.0f, (float) (x + 1) / plot.getWidth());
        const auto b0 = juce::jlimit (1, (int) levelsDb.size() - 1, (int) (f0 / binHz));
        const auto b1 = juce::jlimit (b0, (int) levelsDb.size() - 1, (int) (f1 / binHz));
        auto db = minDb;
        for (int b = b0; b <= b1; ++b)
            db = juce::jmax (db, levelsDb[(size_t) b]);
        spectrum.lineTo (plot.getX() + (float) x, dbToY (db, plot));
    }
    spectrum.lineTo (plot.getRight(), plot.getBottom());
    spectrum.closeSubPath();

    g.setColour (Colours::accent.withAlpha (0.25f));
    g.fillPath (spectrum);
    g.setColour (Colours::accent.withAlpha (0.8f));
    g.strokePath (spectrum, juce::PathStrokeType (1.0f));

    // Filter response at the live (modulated) cutoff, or the knob value when silent.
    const auto live = processor.getLiveCutoff();
    const auto cutoffHz = live > 0.0f ? live : value (processor.apvts, ParamID::filterCutoff);

    juce::Path response;
    for (int x = 0; x < (int) plot.getWidth(); ++x)
    {
        const auto hz = 20.0f * std::pow (1000.0f, (float) x / plot.getWidth());
        const auto px = plot.getX() + (float) x, py = dbToY (filterResponseDb (hz, cutoffHz), plot);
        x == 0 ? response.startNewSubPath (px, py) : response.lineTo (px, py);
    }
    g.setColour (Colours::secondary);
    g.strokePath (response, juce::PathStrokeType (2.0f));

    const auto cx = frequencyToX (cutoffHz, plot);
    g.setColour (Colours::secondary.withAlpha (0.5f));
    g.drawVerticalLine ((int) cx, plot.getY(), plot.getBottom());
    g.setColour (Colours::secondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (choiceText (processor.apvts, ParamID::filterType) + "  " + formatHz (cutoffHz) + (live > 0.0f ? "  (live)" : ""),
                area.reduced (6.0f, 3.0f).removeFromTop (14.0f), juce::Justification::topRight);
}
