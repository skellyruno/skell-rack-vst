#pragma once
#include "FxBase.h"

namespace fx
{

// ======================================================================= Spring Reverb
// A spring tank: a delay loop with a chain of all-pass filters (which gives the "boing" chirp) and a dark feedback path.
class SpringReverbFx : public Processor
{
public:
    static constexpr int kAp = 6;
    explicit SpringReverbFx (Bank& s) : amount (s, "SPR_AMOUNT"), type (s, "SPR_TYPE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            dl[c].prepare ((int) (sr * 0.14));
            for (int a = 0; a < kAp; ++a) { ap[c][a].prepare ((int) (sr * (0.0011 + 0.0007 * a + 0.00015 * c))); ap[c][a].g = 0.62f; }
        }
        initSmooth (amtSm, sr, 0.03, amount());
        reset();
    }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { dl[c].clear(); lp[c] = hpS[c] = 0.0f; for (auto& a : ap[c]) a.reset(); }
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const float lengthMs[3] = { 36.0f, 52.0f, 74.0f };         // American, British, Vintage
        static const float brightHz[3] = { 3800.0f, 2600.0f, 1900.0f };
        const int t = std::clamp (type.index(), 0, 2);
        amtSm.setTargetValue (std::clamp (amount(), 0.0f, 1.0f));
        const float lpC = onePoleCoef (sr, brightHz[t]);
        const float hpC = onePoleCoef (sr, 150.0f);

        for (int i = 0; i < n; ++i)
        {
            const float a = amtSm.getNextValue();
            const float fb = 0.45f + 0.40f * a;
            float inS[2] {}; inS[0] = sanitise (buffer.getReadPointer (0)[i]);
            inS[1] = numCh > 1 ? sanitise (buffer.getReadPointer (1)[i]) : inS[0];
            const float mono = 0.5f * (inS[0] + inS[1]);

            for (int c = 0; c < std::min (numCh, 2); ++c)
            {
                const float len = lengthMs[t] * (c == 0 ? 1.0f : 1.17f) * 0.001f * (float) sr;
                float y = dl[c].read (0, len);
                float z = y;
                for (auto& x : ap[c]) z = x.process (z);                   // dispersion
                lp[c] += lpC * (z - lp[c]);                               // dark feedback path
                const float driven = mono - hpS[c];
                hpS[c] += hpC * (mono - hpS[c]);                          // keep rumble out of the tank
                dl[c].push (0, sanitise (driven + fb * lp[c]));
                dl[c].advance();
                buffer.getWritePointer (c)[i] = inS[c] + y * a * 1.1f;
            }
        }
    }

private:
    Param amount, type;
    double sr = 44100.0;
    DelayLine dl[2];
    Allpass ap[2][kAp];
    float lp[2] {}, hpS[2] {};
    Smooth amtSm;
};

// ======================================================================= Multi Filter
class MultiFilterFx : public Processor
{
public:
    explicit MultiFilterFx (Bank& s) : freq (s, "MF_FREQ"), res (s, "MF_RES"), gain (s, "MF_GAIN"), type (s, "MF_TYPE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (freqSm, sr, 0.03, freq());  initSmooth (resSm, sr, 0.03, res());  initSmooth (gainSm, sr, 0.03, gain());
        reset();
    }
    void reset() override { bq.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        freqSm.setTargetValue (freq());  resSm.setTargetValue (res());  gainSm.setTargetValue (gain());
        for (int start = 0; start < n; start += kStep)           // coefficients glide, so turning a knob never clicks
        {
            const int len = std::min (kStep, n - start);
            setCoeffs (freqSm.skip (len), resSm.skip (len), gainSm.skip (len));
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch) + start;
                for (int i = 0; i < len; ++i) d[i] = sanitise (bq.process (ch, sanitise (d[i])));
            }
        }
    }

private:
    static constexpr int kStep = 32;
    void setCoeffs (float fHz, float qIn, float gDb)
    {
        const double f = clampFreq (fHz, sr), q = std::clamp (qIn, 0.3f, 14.0f), g = gDb;
        const double w0 = kTwoPi * f / sr, cw = std::cos (w0), sw = std::sin (w0), alpha = sw / (2.0 * q);
        switch (std::clamp (type.index(), 0, 6))
        {
            case 0: bq.set (BiquadCoeffs::lowpass (sr, f, q));  break;
            case 1: bq.set (BiquadCoeffs::highpass (sr, f, q)); break;
            case 2: bq.set (BiquadCoeffs::make (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha)); break;   // band-pass
            case 3: bq.set (BiquadCoeffs::make (1.0, -2.0 * cw, 1.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha)); break;  // notch
            case 4: bq.set (BiquadCoeffs::peak (sr, f, g, q));      break;
            case 5: bq.set (BiquadCoeffs::lowShelf (sr, f, g));     break;
            default: bq.set (BiquadCoeffs::highShelf (sr, f, g));   break;
        }
    }

    Param freq, res, gain, type;
    double sr = 44100.0;
    Biquad bq;
    Smooth freqSm, resSm, gainSm;
};

// ======================================================================= EZ EQ  (high-pass, one bell, low-pass)
class EzEqFx : public Processor
{
public:
    explicit EzEqFx (Bank& s) : hp (s, "EZ_HP"), gain (s, "EZ_GAIN"), freq (s, "EZ_FREQ"), width (s, "EZ_WIDTH"), lp (s, "EZ_LP") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (hpSm, sr, 0.03, hp());  initSmooth (gainSm, sr, 0.03, gain());  initSmooth (freqSm, sr, 0.03, freq());
        initSmooth (widthSm, sr, 0.03, width());  initSmooth (lpSm, sr, 0.03, lp());
        reset();
    }
    void reset() override { bHp.reset(); bBell.reset(); bLp.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        hpSm.setTargetValue (hp());  gainSm.setTargetValue (gain());  freqSm.setTargetValue (freq());
        widthSm.setTargetValue (width());  lpSm.setTargetValue (lp());
        for (int start = 0; start < n; start += 32)
        {
            const int len = std::min (32, n - start);
            const float h = hpSm.skip (len), l = lpSm.skip (len);
            const bool useHp = h > 25.0f, useLp = l < 19000.0f;
            if (useHp) bHp.set (BiquadCoeffs::highpass (sr, clampFreq (h, sr)));
            if (useLp) bLp.set (BiquadCoeffs::lowpass (sr, clampFreq (l, sr)));
            bBell.set (BiquadCoeffs::peak (sr, clampFreq (freqSm.skip (len), sr), gainSm.skip (len), 1.0 / std::clamp (widthSm.skip (len), 0.2f, 4.0f)));
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch) + start;
                for (int i = 0; i < len; ++i)
                {
                    float x = sanitise (d[i]);
                    if (useHp) x = bHp.process (ch, x);
                    x = bBell.process (ch, x);
                    if (useLp) x = bLp.process (ch, x);
                    d[i] = sanitise (x);
                }
            }
        }
    }

private:
    Param hp, gain, freq, width, lp;
    double sr = 44100.0;
    Biquad bHp, bBell, bLp;
    Smooth hpSm, gainSm, freqSm, widthSm, lpSm;
};

// ======================================================================= Graphic EQ (7 bands)
class GraphicEqFx : public Processor
{
public:
    static constexpr int kBands = 7;
    static constexpr float kFreq[kBands] = { 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f };

    explicit GraphicEqFx (Bank& s)
        : band { Param (s, "GEQ_1"), Param (s, "GEQ_2"), Param (s, "GEQ_3"), Param (s, "GEQ_4"), Param (s, "GEQ_5"), Param (s, "GEQ_6"), Param (s, "GEQ_7") },
          level (s, "GEQ_LEVEL") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (int b = 0; b < kBands; ++b) initSmooth (bandSm[b], sr, 0.03, band[b]());
        initSmooth (levelSm, sr, 0.03, level());
        reset();
    }
    void reset() override { for (auto& b : bq) b.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        for (int b = 0; b < kBands; ++b) bandSm[b].setTargetValue (band[b]());
        levelSm.setTargetValue (level());
        for (int start = 0; start < n; start += 32)
        {
            const int len = std::min (32, n - start);
            bool active[kBands];
            for (int b = 0; b < kBands; ++b)
            {
                const float g = bandSm[b].skip (len);
                active[b] = std::abs (g) > 0.02f || std::abs (bandSm[b].getTargetValue()) > 0.02f;
                if (active[b]) bq[b].set (BiquadCoeffs::peak (sr, kFreq[b], g, 1.4));
            }
            const float outG = dbToGain (levelSm.skip (len));
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch) + start;
                for (int i = 0; i < len; ++i)
                {
                    float x = sanitise (d[i]);
                    for (int b = 0; b < kBands; ++b) if (active[b]) x = bq[b].process (ch, x);
                    d[i] = sanitise (x) * outG;
                }
            }
        }
    }

private:
    Param band[kBands], level;
    double sr = 44100.0;
    Biquad bq[kBands];
    Smooth bandSm[kBands], levelSm;
};

// ======================================================================= EQ3-M  (low shelf, sweepable mid, high shelf)
class Eq3MFx : public Processor
{
public:
    explicit Eq3MFx (Bank& s) : low (s, "EQ3_LOW"), mid (s, "EQ3_MID"), high (s, "EQ3_HIGH"), sweep (s, "EQ3_SWEEP") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (lowSm, sr, 0.03, low());  initSmooth (midSm, sr, 0.03, mid());  initSmooth (highSm, sr, 0.03, high());  initSmooth (sweepSm, sr, 0.03, sweep());
        reset();
    }
    void reset() override { bl.reset(); bm.reset(); bh.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        lowSm.setTargetValue (low());  midSm.setTargetValue (mid());  highSm.setTargetValue (high());  sweepSm.setTargetValue (sweep());
        for (int start = 0; start < n; start += 32)
        {
            const int len = std::min (32, n - start);
            const float midHz = 250.0f * std::pow (16.0f, std::clamp (sweepSm.skip (len), 0.0f, 1.0f));       // 250 Hz .. 4 kHz
            bl.set (BiquadCoeffs::lowShelf (sr, 180.0, lowSm.skip (len)));
            bm.set (BiquadCoeffs::peak (sr, midHz, midSm.skip (len), 0.8));
            bh.set (BiquadCoeffs::highShelf (sr, 5000.0, highSm.skip (len)));
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch) + start;
                for (int i = 0; i < len; ++i) d[i] = sanitise (bh.process (ch, bm.process (ch, bl.process (ch, sanitise (d[i])))));
            }
        }
    }

private:
    Param low, mid, high, sweep;
    double sr = 44100.0;
    Biquad bl, bm, bh;
    Smooth lowSm, midSm, highSm, sweepSm;
};

// ======================================================================= Vintage Limiter
class VintageLimiterFx : public Processor
{
public:
    explicit VintageLimiterFx (Bank& s)
        : gain (s, "VL_GAIN"), attack (s, "VL_ATTACK"), release (s, "VL_RELEASE"), knee (s, "VL_KNEE"), enhance (s, "VL_ENHANCE"), vol (s, "VL_VOL") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; core.thresholdDb = -3.0f; core.ratio = 20.0f; reset(); }
    void reset() override { core.reset(); meter = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        core.setTimes (sr, std::max (attack(), 0.05f), std::max (release(), 10.0f));
        core.kneeDb = std::clamp (knee(), 0.1f, 12.0f);
        const float inG = dbToGain (gain()), outG = dbToGain (vol());
        const float e = std::clamp (enhance(), 0.0f, 1.0f);
        const float ceiling = dbToGain (-0.5f);
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (sanitise (buffer.getReadPointer (ch)[i])) * inG);
            const float gr = core.tick (peak);
            float g = dbToGain (gr) * inG;
            if (peak > 1.0e-6f) g = std::min (g, ceiling / (peak / inG));      // nothing ever gets past the ceiling
            worst = std::max (worst, -gr);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                float y = sanitise (x[i]) * g;
                y += e * 0.5f * (std::tanh (2.0f * y) - y);                    // enhance: gentle saturation
                x[i] = y * outG;
            }
        }
        meter = std::max (worst, meter * 0.9f);
    }

    float getMeter() const noexcept override { return meter; }

private:
    Param gain, attack, release, knee, enhance, vol;
    double sr = 44100.0;
    CompCore core;
    float meter = 0.0f;
};

// ======================================================================= GTR Squeeze  (guitar sustainer)
class GtrSqueezeFx : public Processor
{
public:
    explicit GtrSqueezeFx (Bank& s) : sustain (s, "GSQ_SUSTAIN"), vol (s, "GSQ_VOL") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; core.ratio = 10.0f; core.kneeDb = 10.0f; core.setTimes (sr, 4.0f, 220.0f); reset(); }
    void reset() override { core.reset(); meter = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float s = std::clamp (sustain(), 0.0f, 1.0f);
        core.thresholdDb = -8.0f - 40.0f * s;
        const float makeup = dbToGain (0.32f * (-core.thresholdDb) * (1.0f - 1.0f / core.ratio));
        const float outG = dbToGain (vol());
        float worst = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (sanitise (buffer.getReadPointer (ch)[i])));
            const float gr = core.tick (peak);
            worst = std::max (worst, -gr);
            const float g = dbToGain (gr) * makeup;
            for (int ch = 0; ch < numCh; ++ch) { float* x = buffer.getWritePointer (ch); x[i] = std::tanh (sanitise (x[i]) * g) * outG; }   // soft ceiling keeps heavy sustain tame
        }
        meter = std::max (worst, meter * 0.9f);
    }
    float getMeter() const noexcept override { return meter; }

private:
    Param sustain, vol;
    double sr = 44100.0;
    CompCore core;
    float meter = 0.0f;
};

// ======================================================================= D-Delay +
class DDelayPlusFx : public Processor
{
public:
    explicit DDelayPlusFx (Bank& s)
        : time (s, "DDX_TIME"), fb (s, "DDX_FB"), hp (s, "DDX_HP"), lp (s, "DDX_LP"), spread (s, "DDX_SPREAD"),
          wet (s, "DDX_WET"), dry (s, "DDX_DRY"), vol (s, "DDX_VOL") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 4.0));
        initSmooth (delaySm, sr, 0.1, 0.25f * (float) sr);
        reset();
    }
    void reset() override { dl.clear(); for (auto& v : lpS) v = 0.0f; for (auto& v : hpS) v = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const double beats[6] = { 1.0, 0.5, 0.25, 0.75, 2.0, 1.0 / 3.0 };    // 1/4, 1/8, 1/16, 1/8 dotted, 1/2, 1/8 triplet
        const double seconds = std::clamp (beats[std::clamp (time.index(), 0, 5)] * 60.0 / std::clamp (ctx.bpm, 30.0, 300.0), 0.005, 3.5);
        delaySm.setTargetValue ((float) (seconds * sr));
        const float feedback = std::clamp (fb(), 0.0f, 0.92f);
        const float sp = std::clamp (spread(), 0.0f, 1.0f);
        const float lpC = onePoleCoef (sr, std::clamp (lp(), 500.0f, 20000.0f));
        const float hpC = onePoleCoef (sr, std::clamp (hp(), 20.0f, 4000.0f));
        const float wetG = std::clamp (wet(), 0.0f, 1.0f), dryG = std::clamp (dry(), 0.0f, 1.0f), outG = dbToGain (vol());

        for (int i = 0; i < n; ++i)
        {
            const float d = delaySm.getNextValue();
            float in[2] {}, tap[2] {};
            in[0] = sanitise (buffer.getReadPointer (0)[i]);
            in[1] = numCh > 1 ? sanitise (buffer.getReadPointer (1)[i]) : in[0];
            for (int c = 0; c < 2; ++c) tap[c] = dl.read (c, d);

            for (int c = 0; c < 2; ++c)
            {
                // Spread sends the repeats to the opposite side each time round (ping-pong)
                float f = tap[c] * (1.0f - sp * 0.5f) + tap[1 - c] * (sp * 0.5f);
                lpS[c] += lpC * (f - lpS[c]);
                hpS[c] += hpC * (lpS[c] - hpS[c]);
                dl.push (c, sanitise (in[c] + feedback * (lpS[c] - hpS[c])));
            }
            dl.advance();

            for (int ch = 0; ch < std::min (numCh, 2); ++ch)
                buffer.getWritePointer (ch)[i] = (in[ch] * dryG + tap[ch] * wetG) * outG;
        }
    }

private:
    Param time, fb, hp, lp, spread, wet, dry, vol;
    double sr = 44100.0;
    DelayLine dl;
    float lpS[2] {}, hpS[2] {};
    Smooth delaySm;
};

} // namespace fx
