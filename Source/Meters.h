#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Input meter | spectrum of the output | output meter, drawn in the top bar.
class MeterStrip : public juce::Component, private juce::Timer
{
public:
    explicit MeterStrip (ModularFXAudioProcessor& p) : proc (p)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }
    ~MeterStrip() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float meterW = 34.0f;
        auto inR = r.removeFromLeft (meterW);
        auto outR = r.removeFromRight (meterW);
        r = r.reduced (6.0f, 0.0f);

        drawMeter (g, inR, level[0], level[1], "IN");
        drawMeter (g, outR, level[2], level[3], "OUT");

        g.setColour (juce::Colour (0xFF15171B));
        g.fillRoundedRectangle (r, 6.0f);
        const int n = (int) bands.size();
        const float bw = r.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float v = juce::jlimit (0.0f, 1.0f, bands[(size_t) i]);
            const float h = v * (r.getHeight() - 6.0f);
            if (h < 0.5f) continue;
            g.setColour (juce::Colour (0xFF22C7DB).withAlpha (0.35f + 0.6f * v));
            g.fillRoundedRectangle (r.getX() + (float) i * bw + 0.6f, r.getBottom() - 3.0f - h, juce::jmax (1.0f, bw - 1.2f), h, 1.0f);
        }
    }

    // exposed for tests: current bar heights (0..1) and level readouts in dB
    const std::array<float, 56>& getBands() const noexcept { return bands; }
    float getLevelDb (int i) const noexcept { return toDb (level[(size_t) i]); }
    void tick() { timerCallback(); }

private:
    static float toDb (float g) noexcept { return 20.0f * std::log10 (juce::jmax (g, 1.0e-6f)); }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float a, float b, const char* name)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (name, r.removeFromBottom (11.0f), juce::Justification::centred);
        r = r.reduced (4.0f, 2.0f);
        const float w = (r.getWidth() - 3.0f) * 0.5f;
        int k = 0;
        for (float v : { a, b })
        {
            juce::Rectangle<float> bar (r.getX() + (float) k * (w + 3.0f), r.getY(), w, r.getHeight());
            g.setColour (juce::Colour (0xFF15171B));
            g.fillRoundedRectangle (bar, 2.0f);
            const float db = toDb (v);
            const float h = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * bar.getHeight();
            g.setColour (db > -0.5f ? juce::Colour (0xFFF05252) : db > -6.0f ? juce::Colour (0xFFFFB703) : juce::Colour (0xFF22C7DB));
            g.fillRoundedRectangle (bar.withTop (bar.getBottom() - h), 2.0f);
            ++k;
        }
    }

    void timerCallback() override
    {
        // level meters: take the peak since the last tick, fall back smoothly
        const float raw[4] = { proc.inPeakL.exchange (0.0f), proc.inPeakR.exchange (0.0f), proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f) };
        for (size_t i = 0; i < 4; ++i) level[i] = juce::jmax (raw[i], level[i] * 0.86f);

        // spectrum of the most recent samples
        const int w = proc.scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < kN; ++i) fftData[(size_t) i] = proc.scopeBuf[(size_t) ((w - kN + i) & (ModularFXAudioProcessor::kScope - 1))];
        window.multiplyWithWindowingTable (fftData.data(), (size_t) kN);
        std::fill (fftData.begin() + kN, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double sr = proc.getSampleRate() > 1000.0 ? proc.getSampleRate() : 44100.0;
        const int n = (int) bands.size();
        for (int b = 0; b < n; ++b)
        {
            const double f0 = 30.0 * std::pow (18000.0 / 30.0, (double) b / n), f1 = 30.0 * std::pow (18000.0 / 30.0, (double) (b + 1) / n);
            const int k0 = juce::jlimit (1, kN / 2 - 1, (int) std::floor (f0 * kN / sr)), k1 = juce::jlimit (k0, kN / 2 - 1, (int) std::ceil (f1 * kN / sr));
            float m = 0.0f;
            for (int k = k0; k <= k1; ++k) m = juce::jmax (m, fftData[(size_t) k]);
            float db = toDb (m / ((float) kN * 0.25f));
            db += 3.0f * std::log2 ((float) std::sqrt (f0 * f1) / 1000.0f);            // tilt so that music looks balanced
            const float v = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 72.0f);
            bands[(size_t) b] = juce::jmax (v, bands[(size_t) b] * 0.84f);
        }
        repaint();
    }

    static constexpr int kN = 2048;
    ModularFXAudioProcessor& proc;
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { (size_t) kN, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kN * 2> fftData {};
    std::array<float, 56> bands {};
    std::array<float, 4> level {};
};
