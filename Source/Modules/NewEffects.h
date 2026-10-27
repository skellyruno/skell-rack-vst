#pragma once
#include "FxBase.h"

namespace fx
{

// ======================================================================= Visual EQ
class VisualEq : public Processor
{
public:
    explicit VisualEq (Bank& s)
        : hpf (s, "EQ_HPF"), lf (s, "EQ_LF"), lg (s, "EQ_LG"), mf (s, "EQ_MF"),
          mg (s, "EQ_MG"), mq (s, "EQ_MQ"), hf (s, "EQ_HF"), hg (s, "EQ_HG") {}

    struct Settings { float hpf, lf, lg, mf, mg, mq, hf, hg; };

    // Shared with the editor so the curve on the card is exactly what the audio does.
    static void makeCoeffs (double sr, const Settings& s, std::array<BiquadCoeffs, 4>& out, bool& hpfOn)
    {
        hpfOn = s.hpf > 20.5f;
        out[0] = BiquadCoeffs::highpass (sr, clampFreq (s.hpf, sr));
        out[1] = BiquadCoeffs::lowShelf (sr, clampFreq (s.lf, sr), s.lg);
        out[2] = BiquadCoeffs::peak (sr, clampFreq (s.mf, sr), s.mg, std::max (s.mq, 0.1f));
        out[3] = BiquadCoeffs::highShelf (sr, clampFreq (s.hf, sr), s.hg);
    }

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; reset(); }
    void reset() override { for (auto& f : filt) f.reset(); have = false; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const Settings cur { hpf(), lf(), lg(), mf(), mg(), mq(), hf(), hg() };
        if (! have || std::memcmp (&cur, &last, sizeof (Settings)) != 0)
        {
            makeCoeffs (sr, cur, coeffs, hpfOn);
            for (size_t k = 0; k < 4; ++k) filt[k].set (coeffs[k]);
            last = cur;  have = true;
        }

        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float y = d[i];
                if (hpfOn) y = filt[0].process (ch, y);
                y = filt[1].process (ch, y);
                y = filt[2].process (ch, y);
                y = filt[3].process (ch, y);
                d[i] = y;
            }
        }
    }

private:
    Param hpf, lf, lg, mf, mg, mq, hf, hg;
    double sr = 44100.0;
    std::array<Biquad, 4> filt;
    std::array<BiquadCoeffs, 4> coeffs;
    Settings last {};
    bool hpfOn = false, have = false;
};

// ======================================================================= Compressor
class CompressorFx : public Processor
{
public:
    explicit CompressorFx (Bank& s)
        : thresh (s, "COMP_THRESH"), ratio (s, "COMP_RATIO"), attack (s, "COMP_ATTACK"),
          release (s, "COMP_RELEASE"), knee (s, "COMP_KNEE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (makeupSm, sr, 0.05, 0.0f);
        reset();
    }
    void reset() override { core.reset(); meter.store (0.0f); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        core.thresholdDb = thresh();
        core.ratio = std::max (ratio(), 1.0f);
        core.setTimes (sr, attack(), release());
        core.kneeDb = std::clamp (knee(), 0.1f, 24.0f);
        // gentle automatic make-up so that squashing the signal doesn't just make it quieter
        makeupSm.setTargetValue (-0.4f * std::min (0.0f, thresh()) * (1.0f - 1.0f / core.ratio));

        float worst = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i]));

            const float gr = core.tick (peak);
            worst = std::min (worst, gr);
            const float g = dbToGain (gr + makeupSm.getNextValue());

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                d[i] = sanitise (d[i]) * g;
            }
        }
        meter.store (-worst, std::memory_order_relaxed);
    }

    float getMeter() const noexcept override { return meter.load (std::memory_order_relaxed); }

private:
    Param thresh, ratio, attack, release, knee;
    double sr = 44100.0;
    CompCore core;
    Smooth makeupSm;
    std::atomic<float> meter { 0.0f };
};

// ======================================================================= Multi-band compressor
class MultiBandFx : public Processor
{
public:
    explicit MultiBandFx (Bank& s)
        : xLow (s, "MB_XLOW"), xHigh (s, "MB_XHIGH"), thrL (s, "MB_LTHR"), thrM (s, "MB_MTHR"),
          thrH (s, "MB_HTHR"), ratio (s, "MB_RATIO"), gain (s, "MB_GAIN") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        band[0].setTimes (sr, 30.0f, 200.0f);
        band[1].setTimes (sr, 15.0f, 150.0f);
        band[2].setTimes (sr, 5.0f, 100.0f);
        initSmooth (gainSm, sr, 0.02, dbToGain (gain()));
        reset();
    }

    void reset() override
    {
        lp1.reset(); hp1.reset(); lp2.reset(); hp2.reset(); apLo.reset(); apHi.reset();
        for (auto& b : band) b.reset();
        haveCoeffs = false;
        meter.store (0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        float f1 = clampFreq (xLow(), sr), f2 = clampFreq (xHigh(), sr);
        f2 = std::max (f2, f1 * 1.5f);
        if (! haveCoeffs || f1 != lastF1 || f2 != lastF2)
        {
            lp1.lowpass (sr, f1);  hp1.highpass (sr, f1);
            lp2.lowpass (sr, f2);  hp2.highpass (sr, f2);
            apLo.lowpass (sr, f2); apHi.highpass (sr, f2);       // keeps the low band phase-aligned with the others
            lastF1 = f1;  lastF2 = f2;  haveCoeffs = true;
        }

        const float r = std::max (ratio(), 1.0f);
        band[0].thresholdDb = thrL();  band[1].thresholdDb = thrM();  band[2].thresholdDb = thrH();
        for (auto& b : band) { b.ratio = r; b.kneeDb = 6.0f; }
        gainSm.setTargetValue (dbToGain (gain()));

        float worst = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float b[3][kMaxCh] {};
            float peak[3] = { 0.0f, 0.0f, 0.0f };

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = buffer.getReadPointer (ch)[i];
                const float low  = lp1.process (ch, x);
                const float rest = hp1.process (ch, x);
                b[0][ch] = apLo.process (ch, low) + apHi.process (ch, low);
                b[1][ch] = lp2.process (ch, rest);
                b[2][ch] = hp2.process (ch, rest);
                for (int k = 0; k < 3; ++k) peak[k] = std::max (peak[k], std::abs (b[k][ch]));
            }

            float g[3];
            for (int k = 0; k < 3; ++k)
            {
                const float gr = band[k].tick (peak[k]);
                worst = std::min (worst, gr);
                g[k] = dbToGain (gr);
            }

            const float og = gainSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = (b[0][ch] * g[0] + b[1][ch] * g[1] + b[2][ch] * g[2]) * og;
        }
        meter.store (-worst, std::memory_order_relaxed);
    }

    float getMeter() const noexcept override { return meter.load (std::memory_order_relaxed); }

private:
    Param xLow, xHigh, thrL, thrM, thrH, ratio, gain;
    double sr = 44100.0;
    LR4 lp1, hp1, lp2, hp2, apLo, apHi;
    CompCore band[3];
    Smooth gainSm;
    float lastF1 = 0.0f, lastF2 = 0.0f;
    bool haveCoeffs = false;
    std::atomic<float> meter { 0.0f };
};

// ======================================================================= Clean limiter
class LimiterFx : public Processor
{
public:
    explicit LimiterFx (Bank& s)
        : gain (s, "LIM_GAIN"), ceiling (s, "LIM_CEIL"), release (s, "LIM_REL") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (gainSm, sr, 0.02, dbToGain (gain()));
        reset();
    }
    void reset() override { gr = 1.0f; meter.store (0.0f); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float ceil = dbToGain (ceiling());
        const float relC = timeToCoef (sr, std::max (release(), 1.0f));
        gainSm.setTargetValue (dbToGain (gain()));

        float worst = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.getNextValue();
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i] * g));

            const float need = peak > ceil ? ceil / peak : 1.0f;
            if (need < gr) gr = need;                                    // instant attack: never overshoots
            else           gr = std::min (relC * gr + (1.0f - relC), need);   // smooth release, but never above what this sample allows
            worst = std::min (worst, gr);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                d[i] = std::clamp (d[i] * g * gr, -ceil, ceil);
            }
        }
        meter.store (-gainToDb (worst), std::memory_order_relaxed);
    }

    float getMeter() const noexcept override { return meter.load (std::memory_order_relaxed); }

private:
    Param gain, ceiling, release;
    double sr = 44100.0;
    float gr = 1.0f;
    Smooth gainSm;
    std::atomic<float> meter { 0.0f };
};

// ======================================================================= Vocal verb (ducking reverb)
class VocalVerbFx : public Processor
{
public:
    explicit VocalVerbFx (Bank& s)
        : mix (s, "VV_MIX"), reflect (s, "VV_REFLECT"), thresh (s, "VV_THRESH"), amount (s, "VV_AMOUNT"), duck (s, "VV_DUCK") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        rev.prepare (sr);
        env.set (sr, 3.0f, 150.0f);
        duckAtt = timeToCoef (sr, 8.0f);
        duckRel = timeToCoef (sr, 250.0f);
        initSmooth (mixSm, sr, 0.03, mix());
        reset();
    }
    void reset() override { rev.reset(); env.reset(); duckGain = 1.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        rev.setParams (0.35f + 0.6f * reflect(), 0.45f, 1.0f);
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        const bool ducking = duck.on();
        const float thr = thresh();
        const float amt = std::clamp (amount(), 0.0f, 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float inL = buffer.getReadPointer (0)[i];
            const float inR = numCh > 1 ? buffer.getReadPointer (1)[i] : inL;

            const float level = env.process (std::max (std::abs (inL), std::abs (inR)));
            float target = 1.0f;
            if (ducking)
                target = 1.0f - amt * std::clamp ((gainToDb (level) - thr) / 10.0f, 0.0f, 1.0f);
            const float c = target < duckGain ? duckAtt : duckRel;
            duckGain = c * duckGain + (1.0f - c) * target;

            float wl = inL, wr = inR;
            rev.process (wl, wr);

            const float k = mixSm.getNextValue() * duckGain;
            buffer.getWritePointer (0)[i] = inL + wl * k;
            if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wr * k;
        }
    }

private:
    Param mix, reflect, thresh, amount, duck;
    double sr = 44100.0;
    ReverbCore rev;
    EnvFollower env;
    Smooth mixSm;
    float duckAtt = 0.0f, duckRel = 0.0f, duckGain = 1.0f;
};

// ======================================================================= Shimmer
class ShimmerFx : public Processor
{
public:
    explicit ShimmerFx (Bank& s)
        : mix (s, "SH_MIX"), mod (s, "SH_MOD"), tail (s, "SH_TAIL"), diff (s, "SH_DIFF") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        rev.prepare (sr);
        pitch.prepare (sr);
        const int lens[2][2] = { { 142, 107 }, { 149, 113 } };      // slightly different per channel for width
        for (int ch = 0; ch < kMaxCh; ++ch)
            for (int k = 0; k < 2; ++k)
                ap[ch][k].prepare ((int) ((double) lens[ch][k] * sr / 44100.0) + 1);
        lpCoef = onePoleCoef (sr, 5500.0f);
        fbHp.set (BiquadCoeffs::highpass (sr, 250.0));
        initSmooth (mixSm, sr, 0.03, mix());
        reset();
    }

    void reset() override
    {
        rev.reset();  pitch.reset();
        for (auto& a : ap) for (auto& x : a) x.reset();
        for (auto& l : lp) l.reset();
        fbHp.reset();
        fbState[0] = fbState[1] = 0.0f;
        lfoPhase = 0.0;
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float tl = std::clamp (tail(), 0.0f, 1.0f);
        const float df = std::clamp (diff(), 0.0f, 1.0f);
        const float md = std::clamp (mod(), 0.0f, 1.0f);
        rev.setParams (0.6f + 0.38f * tl, 0.55f, 1.0f);
        const float fb = 0.10f + 0.30f * tl;                      // how much shifted tail re-enters the reverb
        for (auto& a : ap) for (auto& x : a) x.g = 0.7f * df;
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        const double lfoInc = 0.7 / sr;

        for (int i = 0; i < n; ++i)
        {
            float in[kMaxCh], rin[kMaxCh];
            in[0] = buffer.getReadPointer (0)[i];
            in[1] = numCh > 1 ? buffer.getReadPointer (1)[i] : in[0];

            for (int ch = 0; ch < kMaxCh; ++ch)
            {
                const float d = ap[ch][1].process (ap[ch][0].process (in[ch]));
                rin[ch] = d + fb * fbState[ch];
            }

            float wl = rin[0], wr = rin[1];
            rev.process (wl, wr);
            const float wet[kMaxCh] = { wl, wr };

            // octave-up with a touch of slow pitch modulation
            const float ratio = 2.0f * (1.0f + 0.012f * md * std::sin (kTwoPi * (float) lfoPhase));
            for (int ch = 0; ch < kMaxCh; ++ch) pitch.push (ch, wet[ch]);
            for (int ch = 0; ch < kMaxCh; ++ch)
            {
                // high-pass the feedback: the reverb's combs amplify DC / very low frequencies, which would otherwise run away
                const float shifted = fbHp.process (ch, lp[ch].process (ch, pitch.read (ch), lpCoef));
                fbState[ch] = std::tanh (shifted);                // soft limit keeps the loop stable
            }
            pitch.advance (ratio);
            lfoPhase += lfoInc;  if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            const float m = mixSm.getNextValue();
            buffer.getWritePointer (0)[i] = in[0] * (1.0f - m) + wl * m;
            if (numCh > 1) buffer.getWritePointer (1)[i] = in[1] * (1.0f - m) + wr * m;
        }
    }

private:
    Param mix, mod, tail, diff;
    double sr = 44100.0, lfoPhase = 0.0;
    ReverbCore rev;
    PitchShifter pitch;
    Allpass ap[kMaxCh][2];
    OnePoleLP lp[kMaxCh];
    Biquad fbHp;
    float fbState[kMaxCh] {};
    float lpCoef = 0.5f;
    Smooth mixSm;
};

// ======================================================================= Reverb
class ReverbFx : public Processor
{
public:
    explicit ReverbFx (Bank& s)
        : mix (s, "RV_MIX"), size (s, "RV_SIZE"), damp (s, "RV_DAMP"), width (s, "RV_WIDTH"), pre (s, "RV_PRE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        rev.prepare (sr);
        dl.prepare ((int) (sr * 0.25));
        initSmooth (mixSm, sr, 0.03, mix());
        initSmooth (preSm, sr, 0.05, pre() * (float) sr * 0.001f);
        reset();
    }
    void reset() override { rev.reset(); dl.clear(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        rev.setParams (0.25f + 0.73f * size(), damp(), width());
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        preSm.setTargetValue (std::clamp (pre(), 0.0f, 200.0f) * (float) sr * 0.001f);

        for (int i = 0; i < n; ++i)
        {
            const float inL = buffer.getReadPointer (0)[i];
            const float inR = numCh > 1 ? buffer.getReadPointer (1)[i] : inL;
            dl.push (0, inL);  dl.push (1, inR);
            const float d = preSm.getNextValue();
            float wl = dl.read (0, d), wr = dl.read (1, d);
            dl.advance();
            rev.process (wl, wr);

            const float m = mixSm.getNextValue();
            buffer.getWritePointer (0)[i] = inL * (1.0f - m) + wl * m;
            if (numCh > 1) buffer.getWritePointer (1)[i] = inR * (1.0f - m) + wr * m;
        }
    }

private:
    Param mix, size, damp, width, pre;
    double sr = 44100.0;
    ReverbCore rev;
    DelayLine dl;
    Smooth mixSm, preSm;
};

// ======================================================================= Delay
class DelayFx : public Processor
{
public:
    explicit DelayFx (Bank& s)
        : time (s, "DLY_TIME"), div (s, "DLY_DIV"), fb (s, "DLY_FB"), tone (s, "DLY_TONE"),
          mix (s, "DLY_MIX"), sync (s, "DLY_SYNC"), ping (s, "DLY_PING") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 4.0));
        initSmooth (delaySm, sr, 0.08, 0.35f * (float) sr);
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dl.clear(); lp[0].reset(); lp[1].reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const double beatsFor[] = { 1.0, 0.5, 0.25, 1.5, 0.75, 1.0 / 3.0, 2.0 };
        double seconds;
        if (sync.on()) seconds = beatsFor[std::clamp (div.index(), 0, 6)] * 60.0 / std::clamp (ctx.bpm, 20.0, 400.0);
        else           seconds = (double) time() * 0.001;
        seconds = std::clamp (seconds, 0.001, 3.9);

        delaySm.setTargetValue ((float) (seconds * sr));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        const float feedback = std::clamp (fb(), 0.0f, 0.92f);
        const float lpC = onePoleCoef (sr, 500.0f * std::pow (32.0f, std::clamp (tone(), 0.0f, 1.0f)));
        const bool pingPong = ping.on() && numCh > 1;

        for (int i = 0; i < n; ++i)
        {
            const float d = delaySm.getNextValue();
            const float m = mixSm.getNextValue();
            const float inL = buffer.getReadPointer (0)[i];
            const float inR = numCh > 1 ? buffer.getReadPointer (1)[i] : inL;

            const float wetL = dl.read (0, d);
            const float wetR = numCh > 1 ? dl.read (1, d) : wetL;
            const float fL = lp[0].process (0, wetL, lpC);
            const float fR = lp[1].process (1, wetR, lpC);

            if (pingPong) { dl.push (0, 0.5f * (inL + inR) + feedback * fR);  dl.push (1, feedback * fL); }
            else          { dl.push (0, inL + feedback * fL);                 dl.push (1, inR + feedback * fR); }
            dl.advance();

            buffer.getWritePointer (0)[i] = inL + wetL * m;
            if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wetR * m;
        }
    }

private:
    Param time, div, fb, tone, mix, sync, ping;
    double sr = 44100.0;
    DelayLine dl;
    OnePoleLP lp[2];
    Smooth delaySm, mixSm;
};

// ======================================================================= Bit Bandit
class BitBanditFx : public Processor
{
public:
    explicit BitBanditFx (Bank& s)
        : bits (s, "BIT_BITS"), rate (s, "BIT_RATE"), mix (s, "BIT_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; lastMix = mix(); reset(); }
    void reset() override { counter = 1.0; hold[0] = hold[1] = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float levels = std::pow (2.0f, std::clamp (bits(), 1.0f, 16.0f) - 1.0f);
        const double step = std::clamp ((double) rate(), 500.0, sr) / sr;
        const float mixNow = std::clamp (mix(), 0.0f, 1.0f);

        for (int i = 0; i < n; ++i)
        {
            counter += step;
            const bool take = counter >= 1.0;
            if (take) counter -= 1.0;
            const float m = lastMix + (mixNow - lastMix) * (float) (i + 1) / (float) n;

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                if (take) hold[ch] = d[i];
                const float q = std::round (hold[ch] * levels) / levels;
                d[i] = d[i] * (1.0f - m) + q * m;
            }
        }
        lastMix = mixNow;
    }

private:
    Param bits, rate, mix;
    double sr = 44100.0, counter = 1.0;
    float hold[kMaxCh] {};
    float lastMix = 1.0f;
};

// ======================================================================= Gater
class GaterFx : public Processor
{
public:
    explicit GaterFx (Bank& s)
        : time (s, "GTR_TIME"), depth (s, "GTR_DEPTH"), smooth (s, "GTR_SMOOTH") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; reset(); }
    void reset() override { freePhase = 0.0; }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        // 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/8 triplet, 1/16 triplet  (in quarter-note beats)
        static const double beatsFor[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        const double beats = beatsFor[std::clamp (time.index(), 0, 7)];
        const double bpm = std::clamp (ctx.bpm, 20.0, 400.0);
        const double inc = (bpm / 60.0) / (beats * sr);          // cycles per sample

        const bool synced = ctx.playing && ctx.hasPpq;
        double phase = synced ? std::fmod (ctx.ppq / beats, 1.0) : freePhase;
        if (phase < 0.0) phase += 1.0;

        const float dp = std::clamp (depth(), 0.0f, 1.0f);
        const float edge = 0.002f + 0.25f * std::clamp (smooth(), 0.0f, 1.0f);   // edge width in cycles

        for (int i = 0; i < n; ++i)
        {
            const float p = (float) phase;
            const float up   = smoothstep (p / edge);
            const float down = 1.0f - smoothstep ((p - 0.5f) / edge);
            const float open = std::min (up, down);
            const float g = 1.0f - dp * (1.0f - open);

            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;

            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
        if (! synced) freePhase = phase;
    }

private:
    static float smoothstep (float t) noexcept { t = std::clamp (t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

    Param time, depth, smooth;
    double sr = 44100.0, freePhase = 0.0;
};

// ======================================================================= Shaper (transient designer)
class ShaperFx : public Processor
{
public:
    explicit ShaperFx (Bank& s)
        : trans (s, "SHP_TRANS"), sust (s, "SHP_SUST"), out (s, "SHP_OUT") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        core.prepare (sr);
        initSmooth (outSm, sr, 0.02, dbToGain (out()));
    }
    void reset() override { core.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float t = std::clamp (trans() * 0.01f, -1.0f, 1.0f);
        const float s = std::clamp (sust() * 0.01f, -1.0f, 1.0f);
        outSm.setTargetValue (dbToGain (out()));

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i]));
            const float g = core.tick (peak, t, s) * outSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
        }
    }

private:
    Param trans, sust, out;
    double sr = 44100.0;
    ShaperCore core;
    Smooth outSm;
};

// ======================================================================= Flanger
class FlangerFx : public Processor
{
public:
    explicit FlangerFx (Bank& s)
        : rate (s, "FLG_RATE"), depth (s, "FLG_DEPTH"), fb (s, "FLG_FB"), mix (s, "FLG_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; dl.prepare ((int) (sr * 0.02)); reset(); }
    void reset() override { dl.clear(); phase = 0.0; fbState[0] = fbState[1] = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const double inc = (double) std::max (rate(), 0.01f) / sr;
        const float dp = std::clamp (depth(), 0.0f, 1.0f);
        const float f = std::clamp (fb(), -0.95f, 0.95f);
        const float m = std::clamp (mix(), 0.0f, 1.0f);
        const float msToSamp = (float) (sr * 0.001);

        for (int i = 0; i < n; ++i)
        {
            float wet[kMaxCh] = {};
            float in[kMaxCh] = {};
            for (int ch = 0; ch < numCh; ++ch)
            {
                in[ch] = buffer.getReadPointer (ch)[i];
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (float) (phase + 0.25 * ch));
                wet[ch] = dl.read (ch, (0.5f + lfo * dp * 4.5f) * msToSamp);
                dl.push (ch, in[ch] + f * wet[ch]);
            }
            dl.advance();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = in[ch] * (1.0f - 0.5f * m) + wet[ch] * (0.5f * m);

            phase += inc;  if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    Param rate, depth, fb, mix;
    double sr = 44100.0, phase = 0.0;
    DelayLine dl;
    float fbState[kMaxCh] {};
};

// ======================================================================= Phaser
class PhaserFx : public Processor
{
public:
    explicit PhaserFx (Bank& s)
        : rate (s, "PHS_RATE"), depth (s, "PHS_DEPTH"), fb (s, "PHS_FB"), mix (s, "PHS_MIX"), stages (s, "PHS_STAGES") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; reset(); }
    void reset() override
    {
        phase = 0.0;
        for (auto& c : z) for (auto& v : c) v = 0.0f;
        last[0] = last[1] = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const int numStages = 4 + 2 * std::clamp (stages.index(), 0, 2);
        const double inc = (double) std::max (rate(), 0.01f) / sr;
        const float dp = std::clamp (depth(), 0.0f, 1.0f);
        const float f = std::clamp (fb(), 0.0f, 0.9f);
        const float m = std::clamp (mix(), 0.0f, 1.0f);
        const float maxF = (float) (sr * 0.45);

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = buffer.getReadPointer (ch)[i];
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (float) (phase + 0.25 * ch));
                const float freq = std::min (200.0f * std::pow (2.0f, lfo * dp * 5.0f), maxF);
                const float t = std::tan (kPi * freq / (float) sr);
                const float a = (t - 1.0f) / (t + 1.0f);

                float y = x + f * last[ch];
                for (int s = 0; s < numStages; ++s)
                {
                    const float yy = a * y + z[ch][s];
                    z[ch][s] = y - a * yy;
                    y = yy;
                }
                last[ch] = sanitise (y);
                buffer.getWritePointer (ch)[i] = x * (1.0f - 0.5f * m) + y * (0.5f * m);
            }
            phase += inc;  if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    Param rate, depth, fb, mix, stages;
    double sr = 44100.0, phase = 0.0;
    float z[kMaxCh][8] {};
    float last[kMaxCh] {};
};

// ======================================================================= Tremolo / auto-pan
class TremoloFx : public Processor
{
public:
    explicit TremoloFx (Bank& s)
        : rate (s, "TRM_RATE"), depth (s, "TRM_DEPTH"), shape (s, "TRM_SHAPE"), pan (s, "TRM_PAN") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; reset(); }
    void reset() override { phase = 0.0; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const double inc = (double) std::max (rate(), 0.01f) / sr;
        const float dp = std::clamp (depth(), 0.0f, 1.0f);
        const int sh = shape.index();
        const bool autoPan = pan.on() && numCh > 1;

        for (int i = 0; i < n; ++i)
        {
            const float lfo = wave ((float) phase, sh);              // -1 .. +1

            if (autoPan)
            {
                const float angle = (lfo * dp + 1.0f) * (kPi * 0.25f);   // 0 .. pi/2
                buffer.getWritePointer (0)[i] *= std::cos (angle) * 1.41421356f;
                buffer.getWritePointer (1)[i] *= std::sin (angle) * 1.41421356f;
            }
            else
            {
                const float g = 1.0f - dp * (0.5f - 0.5f * lfo);
                for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
            }
            phase += inc;  if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    static float wave (float p, int shape) noexcept
    {
        switch (shape)
        {
            case 1:  return 1.0f - 4.0f * std::abs (p - 0.5f);              // triangle
            case 2:  return std::tanh (6.0f * std::sin (kTwoPi * p));       // rounded square
            default: return std::sin (kTwoPi * p);
        }
    }

    Param rate, depth, shape, pan;
    double sr = 44100.0, phase = 0.0;
};

// ======================================================================= Distortion
class DistortionFx : public Processor
{
public:
    explicit DistortionFx (Bank& s)
        : mode (s, "DST_MODE"), drive (s, "DST_DRIVE"), tone (s, "DST_TONE"), level (s, "DST_LEVEL"), mix (s, "DST_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (driveSm, sr, 0.02, 1.0f);
        initSmooth (levelSm, sr, 0.02, dbToGain (level()));
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dc.reset(); lp.reset(); lp2.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const int md = mode.index();
        driveSm.setTargetValue (dbToGain (std::clamp (drive(), 0.0f, 1.0f) * 36.0f));
        levelSm.setTargetValue (dbToGain (level()));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        const float fc = std::min (800.0f * std::pow (15.0f, std::clamp (tone(), 0.0f, 1.0f)), (float) (sr * 0.45));
        const float lpC = onePoleCoef (sr, fc);

        for (int i = 0; i < n; ++i)
        {
            const float g = driveSm.getNextValue();
            const float lv = levelSm.getNextValue();
            const float m = mixSm.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                const float dry = d[i];
                const float x = dry * g;
                float y;
                switch (md)
                {
                    case 1:  y = std::atan (x * 1.8f) * (2.0f / kPi); break;                           // distortion
                    case 2:  y = x >= 0.0f ? 1.0f - std::exp (-x * 1.5f)                               // fuzz (asymmetric)
                                           : -0.8f * (1.0f - std::exp (x * 2.5f)); break;
                    default: y = std::tanh (x); break;                                                 // overdrive
                }
                y = dc.process (ch, y);
                y = lp2.process (ch, lp.process (ch, y, lpC), lpC);                                    // 12 dB/oct tone control
                d[i] = dry * (1.0f - m) + y * lv * m;
            }
        }
    }

private:
    Param mode, drive, tone, level, mix;
    double sr = 44100.0;
    DcBlocker dc;
    OnePoleLP lp, lp2;
    Smooth driveSm, levelSm, mixSm;
};

// ======================================================================= One Knobs
class OneKnobFx : public Processor
{
public:
    explicit OneKnobFx (Bank& s) : amt (s, "ONE_AMT"), mode (s, "ONE_MODE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        rev.prepare (sr);
        dl.prepare ((int) (sr * 1.2));
        shaper.prepare (sr);
        initSmooth (amtSm, sr, 0.03, amt());
        reset();
    }

    void reset() override
    {
        rev.reset(); dl.clear(); shaper.reset(); comp.reset();
        for (auto& b : bq) b.reset();
        lp[0].reset(); lp[1].reset(); dc.reset();
        lastMode = -1;
        lastAmt = -1.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const int md = std::clamp (mode.index(), 0, 7);
        if (md != lastMode) { rev.reset(); dl.clear(); shaper.reset(); comp.reset(); for (auto& b : bq) b.reset(); lp[0].reset(); lp[1].reset(); dc.reset(); lastMode = md; lastAmt = -1.0f; }

        const float a = std::clamp (amt(), 0.0f, 1.0f);
        amtSm.setTargetValue (a);
        if (a != lastAmt) { configure (md, a, ctx); lastAmt = a; }
        if (md == 1)    // follow tempo changes
            echoDelay = (float) (std::clamp (60.0 / std::clamp (ctx.bpm, 20.0, 400.0) * 0.5, 0.08, 1.0) * sr);

        for (int i = 0; i < n; ++i)
        {
            const float s = amtSm.getNextValue();
            switch (md)
            {
                case 0:  // Brighter
                case 5:  // Bassy
                    for (int ch = 0; ch < numCh; ++ch) { float* d = buffer.getWritePointer (ch); d[i] = bq[1].process (ch, bq[0].process (ch, d[i])); }
                    break;

                case 1:  // Echo
                {
                    const float inL = buffer.getReadPointer (0)[i];
                    const float inR = numCh > 1 ? buffer.getReadPointer (1)[i] : inL;
                    const float wl = dl.read (0, echoDelay), wr = dl.read (1, echoDelay);
                    dl.push (0, inL + echoFb * lp[0].process (0, wl, echoLp));
                    dl.push (1, inR + echoFb * lp[1].process (1, wr, echoLp));
                    dl.advance();
                    buffer.getWritePointer (0)[i] = inL + wl * s * 0.6f;
                    if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wr * s * 0.6f;
                    break;
                }

                case 2:  // Punchy
                {
                    float peak = 0.0f;
                    for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i]));
                    const float g = shaper.tick (peak, s * 0.6f, 0.0f) * trim;
                    for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
                    break;
                }

                case 3:  // Room
                {
                    const float inL = buffer.getReadPointer (0)[i];
                    const float inR = numCh > 1 ? buffer.getReadPointer (1)[i] : inL;
                    float wl = inL, wr = inR;
                    rev.process (wl, wr);
                    buffer.getWritePointer (0)[i] = inL + wl * s * 0.4f;
                    if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wr * s * 0.4f;
                    break;
                }

                case 4:  // Toasty (tape-style warmth)
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* d = buffer.getWritePointer (ch);
                        float y = std::tanh (d[i] * toastDrive) / toastDrive;
                        y = lp[ch].process (ch, y, toastLp);
                        d[i] = bq[0].process (ch, y);
                    }
                    break;

                case 6:  // Leveller
                {
                    float peak = 0.0f;
                    for (int ch = 0; ch < numCh; ++ch) peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i]));
                    const float g = dbToGain (comp.tick (peak) + levMakeup);
                    for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
                    break;
                }

                default: // 7 Dirty
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float* d = buffer.getWritePointer (ch);
                        const float y = std::tanh (d[i] * dirtyDrive) * dirtyComp;
                        d[i] = dc.process (ch, y);
                    }
                    break;
            }
        }
    }

private:
    void configure (int md, float a, const Context& ctx)
    {
        switch (md)
        {
            case 0:  bq[0].set (BiquadCoeffs::highShelf (sr, 3500.0, 9.0 * a));
                     bq[1].set (BiquadCoeffs::peak (sr, 2500.0, 2.0 * a, 0.8));
                     break;
            case 1:  { const double beat = 60.0 / std::clamp (ctx.bpm, 20.0, 400.0);
                       echoDelay = (float) (std::clamp (beat * 0.5, 0.08, 1.0) * sr);
                       echoFb = 0.25f + 0.3f * a;
                       echoLp = onePoleCoef (sr, 3500.0f); }
                     break;
            case 2:  trim = dbToGain (-2.0f * a); break;
            case 3:  rev.setParams (0.3f, 0.65f, 0.8f); break;
            case 4:  toastDrive = 1.0f + 3.0f * a;
                     toastLp = onePoleCoef (sr, 16000.0f - 10000.0f * a);
                     bq[0].set (BiquadCoeffs::lowShelf (sr, 200.0, 3.0 * a));
                     break;
            case 5:  bq[0].set (BiquadCoeffs::lowShelf (sr, 110.0, 9.5 * a));
                     bq[1].set (BiquadCoeffs::peak (sr, 300.0, 2.5 * a, 0.9));
                     break;
            case 6:  comp.thresholdDb = -6.0f - 28.0f * a;
                     comp.ratio = 2.5f + 2.0f * a;
                     comp.kneeDb = 8.0f;
                     comp.setTimes (sr, 12.0f, 160.0f);
                     levMakeup = 0.5f * std::abs (comp.thresholdDb) * (1.0f - 1.0f / comp.ratio);
                     break;
            default: dirtyDrive = 1.0f + 14.0f * a;
                     dirtyComp = 1.0f / std::sqrt (dirtyDrive);
                     break;
        }
    }

    Param amt, mode;
    double sr = 44100.0;
    Smooth amtSm;
    int lastMode = -1;
    float lastAmt = -1.0f;

    Biquad bq[2];
    OnePoleLP lp[kMaxCh];
    DcBlocker dc;
    ReverbCore rev;
    DelayLine dl;
    ShaperCore shaper;
    CompCore comp;
    float echoDelay = 0.0f, echoFb = 0.3f, echoLp = 0.5f, trim = 1.0f;
    float toastDrive = 1.0f, toastLp = 0.5f, levMakeup = 0.0f, dirtyDrive = 1.0f, dirtyComp = 1.0f;
};

} // namespace fx
