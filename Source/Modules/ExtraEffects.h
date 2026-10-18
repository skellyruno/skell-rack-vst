#pragma once
#include "FxBase.h"

namespace fx
{

// ------------------------------------------------------------------ shared helpers for this file
// Topology-preserving state variable filter (low / band / high from one pass).
struct Svf
{
    float ic1[kMaxCh] {}, ic2[kMaxCh] {};
    void reset() noexcept { for (int i = 0; i < kMaxCh; ++i) ic1[i] = ic2[i] = 0.0f; }
    static float gFor (float fc, double sr) noexcept { return std::tan (kPi * std::clamp (fc, 10.0f, (float) (sr * 0.45)) / (float) sr); }

    void process (int ch, float x, float g, float k, float& lo, float& bp, float& hi) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        lo = v2;  bp = v1;  hi = x - k * v1 - v2;
    }
};

// p in 0..1 -> -1..1
inline float lfoShape (int shape, float p) noexcept
{
    switch (shape)
    {
        case 1:  return 4.0f * std::abs (p - 0.5f) - 1.0f;          // triangle
        case 2:  return 2.0f * p - 1.0f;                            // ramp
        default: return std::sin (kTwoPi * p);                      // sine
    }
}

inline double wrap01 (double p) noexcept { p -= std::floor (p); return p; }
inline double safeBpm (double bpm) noexcept { return std::clamp (bpm, 20.0, 400.0); }

// ======================================================================= Gain
class GainFx : public Processor
{
public:
    explicit GainFx (Bank& s) : gain (s, "GAIN_DB"), inv (s, "GAIN_INV"), mono (s, "GAIN_MONO") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; initSmooth (gSm, sr, 0.02, 1.0f); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;
        gSm.setTargetValue (dbToGain (gain()) * (inv.on() ? -1.0f : 1.0f));
        const bool m = mono.on() && numCh > 1;
        for (int i = 0; i < n; ++i)
        {
            const float g = gSm.getNextValue();
            if (m)
            {
                const float v = 0.5f * (buffer.getReadPointer (0)[i] + buffer.getReadPointer (1)[i]) * g;
                buffer.getWritePointer (0)[i] = buffer.getWritePointer (1)[i] = v;
            }
            else
                for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
        }
    }

private:
    Param gain, inv, mono;
    double sr = 44100.0;
    Smooth gSm;
};

// ======================================================================= Pumper (simulated side-chain)
class PumperFx : public Processor
{
public:
    explicit PumperFx (Bank& s) : rate (s, "PMP_RATE"), depth (s, "PMP_DEPTH"), rel (s, "PMP_RELEASE"), curve (s, "PMP_CURVE") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; smC = timeToCoef (sr, 1.5f); reset(); }
    void reset() override { g = 1.0f; freePhase = 0.0; }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const double cyclesPerBeat[4] = { 0.5, 1.0, 2.0, 4.0 };       // 1/2, 1/4, 1/8, 1/16
        const double cyc = cyclesPerBeat[std::clamp (rate.index(), 0, 3)];
        const double inc = safeBpm (ctx.bpm) / 60.0 / sr * cyc;
        double p = (ctx.hasPpq && ctx.playing) ? wrap01 (ctx.ppq * cyc) : freePhase;

        const float d = std::clamp (depth(), 0.0f, 1.0f);
        const float relF = 0.1f + 0.85f * std::clamp (rel(), 0.0f, 1.0f);
        const float expo = 0.4f + 3.2f * std::clamp (curve(), 0.0f, 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float e = p < relF ? std::pow ((float) p / relF, expo) : 1.0f;
            const float target = 1.0f - d * (1.0f - e);
            g = smC * g + (1.0f - smC) * target;
            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
            p += inc;  if (p >= 1.0) p -= 1.0;
        }
        freePhase = p;
    }

private:
    Param rate, depth, rel, curve;
    double sr = 44100.0, freePhase = 0.0;
    float smC = 0.0f, g = 1.0f;
};

// ======================================================================= Auto Pan
class AutoPanFx : public Processor
{
public:
    explicit AutoPanFx (Bank& s) : rate (s, "APN_RATE"), div (s, "APN_DIV"), sync (s, "APN_SYNC"), depth (s, "APN_DEPTH"), shape (s, "APN_SHAPE") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; smC = timeToCoef (sr, 3.0f); reset(); }
    void reset() override { freePhase = 0.0; sq = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh < 2) return;                                   // panning needs two channels

        static const double cyclesPerBeat[5] = { 0.25, 0.5, 1.0, 2.0, 4.0 };    // 1 bar, 1/2, 1/4, 1/8, 1/16
        double inc, p;
        if (sync.on())
        {
            const double cyc = cyclesPerBeat[std::clamp (div.index(), 0, 4)];
            inc = safeBpm (ctx.bpm) / 60.0 / sr * cyc;
            p = (ctx.hasPpq && ctx.playing) ? wrap01 (ctx.ppq * cyc) : freePhase;
        }
        else
        {
            inc = (double) std::clamp (rate(), 0.05f, 20.0f) / sr;
            p = freePhase;
        }
        const float d = std::clamp (depth(), 0.0f, 1.0f);
        const int sh = std::clamp (shape.index(), 0, 2);

        for (int i = 0; i < n; ++i)
        {
            float l;
            if (sh == 2) { sq = smC * sq + (1.0f - smC) * (p < 0.5 ? 1.0f : -1.0f); l = sq; }
            else         l = lfoShape (sh, (float) p);
            const float a = (l * d + 1.0f) * kPi * 0.25f;
            buffer.getWritePointer (0)[i] *= std::cos (a) * 1.41421356f;
            buffer.getWritePointer (1)[i] *= std::sin (a) * 1.41421356f;
            p += inc;  if (p >= 1.0) p -= 1.0;
        }
        freePhase = p;
    }

private:
    Param rate, div, sync, depth, shape;
    double sr = 44100.0, freePhase = 0.0;
    float smC = 0.0f, sq = 0.0f;
};

// ======================================================================= Auto Filter
class AutoFilterFx : public Processor
{
public:
    explicit AutoFilterFx (Bank& s)
        : type (s, "AF_TYPE"), cutoff (s, "AF_CUTOFF"), res (s, "AF_RES"), rate (s, "AF_RATE"),
          depth (s, "AF_DEPTH"), shape (s, "AF_SHAPE"), mix (s, "AF_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; initSmooth (mixSm, sr, 0.02, mix()); reset(); }
    void reset() override { svf.reset(); phase = 0.0; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const int t = std::clamp (type.index(), 0, 2), sh = std::clamp (shape.index(), 0, 2);
        const float fc0 = cutoff(), k = 1.0f / std::clamp (res(), 0.5f, 12.0f);
        const float d = std::clamp (depth(), 0.0f, 1.0f) * 3.0f;           // up to +/- 3 octaves
        const double inc = (double) std::clamp (rate(), 0.05f, 20.0f) / sr;
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        float g = Svf::gFor (fc0, sr);
        for (int i = 0; i < n; ++i)
        {
            if ((i & 7) == 0)
                g = Svf::gFor (fc0 * std::pow (2.0f, d * lfoShape (sh, (float) phase)), sr);
            phase += inc;  if (phase >= 1.0) phase -= 1.0;

            const float m = mixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                float lo, bp, hi;
                svf.process (ch, sanitise (x[i]), g, k, lo, bp, hi);
                const float wet = t == 0 ? lo : (t == 1 ? bp * k * 1.6f : hi);
                x[i] = x[i] * (1.0f - m) + sanitise (wet) * m;
            }
        }
    }

private:
    Param type, cutoff, res, rate, depth, shape, mix;
    double sr = 44100.0, phase = 0.0;
    Svf svf;
    Smooth mixSm;
};

// ======================================================================= Auto Wah
class AutoWahFx : public Processor
{
public:
    explicit AutoWahFx (Bank& s)
        : mode (s, "AW_MODE"), sens (s, "AW_SENS"), freq (s, "AW_FREQ"), res (s, "AW_RES"), decay (s, "AW_DECAY"), mix (s, "AW_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; ctlC = timeToCoef (sr, 5.0f); reset(); }
    void reset() override { svf.reset(); env.reset(); ctl = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        env.set (sr, 4.0f, std::clamp (decay(), 10.0f, 1000.0f));
        const bool down = mode.index() == 1;
        const float s = std::clamp (sens(), 0.0f, 1.0f), k = 1.0f / std::clamp (res(), 1.0f, 12.0f), base = freq();
        const float m = std::clamp (mix(), 0.0f, 1.0f);
        float g = Svf::gFor (base, sr);

        for (int i = 0; i < n; ++i)
        {
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) det = std::max (det, std::abs (buffer.getReadPointer (ch)[i]));
            const float lv = std::clamp ((gainToDb (env.process (det)) + 50.0f) / 45.0f, 0.0f, 1.0f);
            ctl = ctlC * ctl + (1.0f - ctlC) * std::clamp (lv * (0.4f + 1.2f * s), 0.0f, 1.0f);

            if ((i & 3) == 0)
                g = Svf::gFor (base * std::pow (2.0f, 3.5f * (down ? 1.0f - ctl : ctl)), sr);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                float lo, bp, hi;
                svf.process (ch, sanitise (x[i]), g, k, lo, bp, hi);
                x[i] = x[i] * (1.0f - m) + sanitise (bp * k * 1.8f) * m;
            }
        }
    }

private:
    Param mode, sens, freq, res, decay, mix;
    double sr = 44100.0;
    Svf svf;
    EnvFollower env;
    float ctl = 0.0f, ctlC = 0.0f;
};

// ======================================================================= Bitcrusher
class BitcrusherFx : public Processor
{
public:
    explicit BitcrusherFx (Bank& s) : bits (s, "BCR_BITS"), down (s, "BCR_DOWN"), mix (s, "BCR_MIX") {}

    void prepare (double, int) override { reset(); }
    void reset() override { for (int i = 0; i < kMaxCh; ++i) { hold[i] = 0.0f; cnt[i] = 0; } }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;
        const float levels = std::pow (2.0f, std::clamp (std::round (bits()), 1.0f, 16.0f) - 1.0f);
        const int factor = std::max (1, (int) std::round (down()));
        const float m = std::clamp (mix(), 0.0f, 1.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* x = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                if (cnt[ch] <= 0) { hold[ch] = std::round (std::clamp (sanitise (x[i]), -1.0f, 1.0f) * levels) / levels; cnt[ch] = factor; }
                --cnt[ch];
                x[i] = x[i] * (1.0f - m) + hold[ch] * m;
            }
        }
    }

private:
    Param bits, down, mix;
    float hold[kMaxCh] {};
    int cnt[kMaxCh] {};
};

// ======================================================================= Driver
class DriverFx : public Processor
{
public:
    explicit DriverFx (Bank& s) : drive (s, "DRV_DRIVE"), tone (s, "DRV_TONE"), level (s, "DRV_LEVEL"), mix (s, "DRV_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (gSm, sr, 0.02, 1.0f);  initSmooth (lvSm, sr, 0.02, dbToGain (level()));  initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dc.reset(); lp.reset(); hp.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        gSm.setTargetValue (dbToGain (std::clamp (drive(), 0.0f, 1.0f) * 30.0f));
        lvSm.setTargetValue (dbToGain (level()));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        const float lpC = onePoleCoef (sr, clampFreq (1200.0f * std::pow (10.0f, 1.2f * std::clamp (tone(), 0.0f, 1.0f)), sr));
        hp.set (BiquadCoeffs::highpass (sr, 90.0));

        for (int i = 0; i < n; ++i)
        {
            const float g = gSm.getNextValue(), lv = lvSm.getNextValue(), m = mixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                const float dry = x[i];
                float y = hp.process (ch, sanitise (dry)) * g;
                y = std::tanh (y + 0.12f) - 0.1194f;              // slightly asymmetric: adds even harmonics
                y = dc.process (ch, y);
                y = lp.process (ch, y, lpC);
                x[i] = dry * (1.0f - m) + y * lv * m;
            }
        }
    }

private:
    Param drive, tone, level, mix;
    double sr = 44100.0;
    Biquad hp;  DcBlocker dc;  OnePoleLP lp;
    Smooth gSm, lvSm, mixSm;
};

// ======================================================================= Exciter
class ExciterFx : public Processor
{
public:
    explicit ExciterFx (Bank& s) : freq (s, "EXC_FREQ"), amount (s, "EXC_AMT"), mix (s, "EXC_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; initSmooth (mixSm, sr, 0.02, mix()); reset(); }
    void reset() override { h1.reset(); h2.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;
        const auto c = BiquadCoeffs::highpass (sr, clampFreq (freq(), sr));
        h1.set (c);  h2.set (c);
        const float a = std::clamp (amount(), 0.0f, 1.0f) * 1.5f;
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                const float dry = sanitise (x[i]);
                const float e = h1.process (ch, dry);
                const float harm = h2.process (ch, std::tanh (6.0f * e + 0.6f) - 0.537f);     // asymmetric: odd + even harmonics
                x[i] = dry + a * 0.5f * harm * m;
            }
        }
    }

private:
    Param freq, amount, mix;
    double sr = 44100.0;
    Biquad h1, h2;
    Smooth mixSm;
};

// ======================================================================= FET Compressor (1176-style)
class FetCompFx : public Processor
{
public:
    explicit FetCompFx (Bank& s)
        : input (s, "FET_INPUT"), output (s, "FET_OUTPUT"), attack (s, "FET_ATTACK"), release (s, "FET_RELEASE"),
          ratio (s, "FET_RATIO"), mix (s, "FET_MIX") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; core.thresholdDb = -10.0f; core.kneeDb = 1.0f; reset(); }
    void reset() override { core.reset(); meter = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const float ratios[5] = { 4.0f, 8.0f, 12.0f, 20.0f, 60.0f };
        core.ratio = ratios[std::clamp (ratio.index(), 0, 4)];
        core.setTimes (sr, std::max (attack(), 0.01f), std::max (release(), 20.0f));
        const float inG = dbToGain (input()), outG = dbToGain (output()), m = std::clamp (mix(), 0.0f, 1.0f);
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) det = std::max (det, std::abs (buffer.getReadPointer (ch)[i]) * inG);
            const float gr = core.tick (det);
            worst = std::max (worst, -gr);
            const float gain = dbToGain (gr) * inG * outG;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                x[i] = x[i] * (1.0f - m) + sanitise (x[i]) * gain * m;
            }
        }
        meter = std::max (worst, meter * 0.9f);
    }

    float getMeter() const noexcept override { return meter; }

private:
    Param input, output, attack, release, ratio, mix;
    double sr = 44100.0;
    CompCore core;
    float meter = 0.0f;
};

// ======================================================================= Opto Compressor (LA-2A-style)
class OptoCompFx : public Processor
{
public:
    explicit OptoCompFx (Bank& s) : peakRed (s, "OPT_PEAK"), gain (s, "OPT_GAIN"), mode (s, "OPT_MODE"), mix (s, "OPT_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        attC = timeToCoef (sr, 10.0f);  relFast = timeToCoef (sr, 80.0f);  relSlow = timeToCoef (sr, 1500.0f);
        core.kneeDb = 10.0f;  core.setTimes (sr, 0.01f, 0.01f);
        reset();
    }
    void reset() override { core.reset(); fast = slow = 0.0f; meter = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        core.thresholdDb = -2.0f - 36.0f * std::clamp (peakRed(), 0.0f, 1.0f);
        core.ratio = mode.index() == 1 ? 10.0f : 3.0f;
        const float outG = dbToGain (gain()), m = std::clamp (mix(), 0.0f, 1.0f);
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) det = std::max (det, std::abs (buffer.getReadPointer (ch)[i]));
            const float target = core.tick (det);                       // dB, <= 0
            fast = (target < fast ? attC : relFast) * fast + (1.0f - (target < fast ? attC : relFast)) * target;
            slow = (target < slow ? attC : relSlow) * slow + (1.0f - (target < slow ? attC : relSlow)) * target;
            const float gr = std::min (0.0f, 0.65f * fast + 0.35f * slow);
            worst = std::max (worst, -gr);
            const float g = dbToGain (gr) * outG;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* x = buffer.getWritePointer (ch);
                x[i] = x[i] * (1.0f - m) + sanitise (x[i]) * g * m;
            }
        }
        meter = std::max (worst, meter * 0.9f);
    }

    float getMeter() const noexcept override { return meter; }

private:
    Param peakRed, gain, mode, mix;
    double sr = 44100.0;
    CompCore core;
    float attC = 0.0f, relFast = 0.0f, relSlow = 0.0f, fast = 0.0f, slow = 0.0f, meter = 0.0f;
};

// ======================================================================= Delay + (rhythmic multi-tap)
class DelayPlusFx : public Processor
{
public:
    explicit DelayPlusFx (Bank& s)
        : pattern (s, "DDP_PATTERN"), base (s, "DDP_BASE"), fb (s, "DDP_FB"), tone (s, "DDP_TONE"), spread (s, "DDP_SPREAD"), mix (s, "DDP_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 6.0));
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dl.clear(); lp[0].reset(); lp[1].reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const float taps[5][4] = {
            { 1.0f, 2.0f, 3.0f, 4.0f },                  // straight
            { 1.5f, 3.0f, 4.5f, 6.0f },                  // dotted
            { 2.0f / 3.0f, 4.0f / 3.0f, 2.0f, 8.0f / 3.0f },   // triplet
            { 0.75f, 2.0f, 2.75f, 4.0f },                // syncopated
            { 1.0f, 1.5f, 2.5f, 4.0f } };                // cascade
        static const float baseBeats[3] = { 1.0f, 0.5f, 0.25f };
        static const float weights[4] = { 0.40f, 0.30f, 0.20f, 0.10f };

        const float beatSamples = (float) (60.0 / std::clamp (ctx.bpm, 40.0, 300.0) * sr);
        const auto& pt = taps[std::clamp (pattern.index(), 0, 4)];
        const float b = baseBeats[std::clamp (base.index(), 0, 2)];
        float t[4], gl[4], gr[4];
        const float sp = std::clamp (spread(), 0.0f, 1.0f);
        static const float pos[4] = { -1.0f, 1.0f, -0.5f, 0.5f };
        for (int i = 0; i < 4; ++i)
        {
            t[i] = std::min (pt[i] * b * beatSamples, (float) (dl.maxDelay() - 4));
            const float a = (pos[i] * sp + 1.0f) * kPi * 0.25f;
            gl[i] = std::cos (a) * 1.41421356f;  gr[i] = std::sin (a) * 1.41421356f;
        }
        const float feedback = std::clamp (fb(), 0.0f, 0.9f);
        const float lpC = onePoleCoef (sr, 600.0f * std::pow (25.0f, std::clamp (tone(), 0.0f, 1.0f)));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue();
            const float inL = sanitise (buffer.getReadPointer (0)[i]);
            const float inR = numCh > 1 ? sanitise (buffer.getReadPointer (1)[i]) : inL;
            float wl = 0.0f, wr = 0.0f, sl = 0.0f, sr2 = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const float a = dl.read (0, t[k]), c = numCh > 1 ? dl.read (1, t[k]) : a;
                wl += a * weights[k] * gl[k];  wr += c * weights[k] * gr[k];
                sl += a * weights[k];          sr2 += c * weights[k];
            }
            dl.push (0, inL + feedback * lp[0].process (0, sl, lpC));
            dl.push (1, inR + feedback * lp[1].process (1, sr2, lpC));
            dl.advance();
            buffer.getWritePointer (0)[i] = inL + wl * m;
            if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wr * m;
        }
    }

private:
    Param pattern, base, fb, tone, spread, mix;
    double sr = 44100.0;
    DelayLine dl;
    OnePoleLP lp[2];
    Smooth mixSm;
};

// ======================================================================= Filter Echo +
class FilterEchoFx : public Processor
{
public:
    explicit FilterEchoFx (Bank& s)
        : div (s, "FEC_DIV"), fb (s, "FEC_FB"), freq (s, "FEC_FREQ"), res (s, "FEC_RES"), type (s, "FEC_TYPE"), mod (s, "FEC_MOD"), mix (s, "FEC_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 4.0));
        initSmooth (delaySm, sr, 0.1, 0.25f * (float) sr);
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dl.clear(); svf.reset(); phase = 0.0; }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        static const double beats[6] = { 1.0, 0.5, 0.75, 0.25, 2.0, 1.0 / 3.0 };
        const double seconds = std::clamp (beats[std::clamp (div.index(), 0, 5)] * 60.0 / std::clamp (ctx.bpm, 30.0, 300.0), 0.005, 3.5);
        delaySm.setTargetValue ((float) (seconds * sr));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        const float feedback = std::clamp (fb(), 0.0f, 0.92f);
        const float k = 1.0f / std::clamp (res(), 0.5f, 8.0f);
        const float g = Svf::gFor (freq(), sr);
        const int t = std::clamp (type.index(), 0, 2);
        const float modSamples = std::clamp (mod(), 0.0f, 1.0f) * (float) (sr * 0.004);
        const double inc = 0.35 / sr;

        for (int i = 0; i < n; ++i)
        {
            const float d = delaySm.getNextValue() + modSamples * std::sin (kTwoPi * (float) phase);
            phase += inc;  if (phase >= 1.0) phase -= 1.0;
            const float m = mixSm.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float in = sanitise (buffer.getReadPointer (ch)[i]);
                float lo, bp, hi;
                svf.process (ch, dl.read (ch, d), g, k, lo, bp, hi);
                const float f = sanitise (t == 0 ? lo : (t == 1 ? bp * k * 1.6f : hi));
                dl.push (ch, in + feedback * f);
                buffer.getWritePointer (ch)[i] = in + f * m;
            }
            dl.advance();
        }
    }

private:
    Param div, fb, freq, res, type, mod, mix;
    double sr = 44100.0, phase = 0.0;
    DelayLine dl;
    Svf svf;
    Smooth delaySm, mixSm;
};

// ======================================================================= Delay (legacy, independent L / R)
class LegacyDelayFx : public Processor
{
public:
    explicit LegacyDelayFx (Bank& s)
        : tl (s, "DDL_TL"), tr (s, "DDL_TR"), fb (s, "DDL_FB"), cross (s, "DDL_CROSS"), tone (s, "DDL_TONE"), mix (s, "DDL_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 2.2));
        initSmooth (tlSm, sr, 0.08, 0.3f * (float) sr);  initSmooth (trSm, sr, 0.08, 0.45f * (float) sr);
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { dl.clear(); lp[0].reset(); lp[1].reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        tlSm.setTargetValue ((float) (std::clamp (tl(), 1.0f, 2000.0f) * 0.001 * sr));
        trSm.setTargetValue ((float) (std::clamp (tr(), 1.0f, 2000.0f) * 0.001 * sr));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        const float feedback = std::clamp (fb(), 0.0f, 0.92f), x = std::clamp (cross(), 0.0f, 1.0f);
        const float lpC = onePoleCoef (sr, 500.0f * std::pow (32.0f, std::clamp (tone(), 0.0f, 1.0f)));

        for (int i = 0; i < n; ++i)
        {
            const float dL = tlSm.getNextValue(), dR = trSm.getNextValue(), m = mixSm.getNextValue();
            const float inL = sanitise (buffer.getReadPointer (0)[i]);
            const float inR = numCh > 1 ? sanitise (buffer.getReadPointer (1)[i]) : inL;
            const float wL = dl.read (0, dL), wR = dl.read (1, numCh > 1 ? dR : dL);
            const float fL = lp[0].process (0, wL, lpC), fR = lp[1].process (1, wR, lpC);
            dl.push (0, inL + feedback * ((1.0f - x) * fL + x * fR));
            dl.push (1, inR + feedback * ((1.0f - x) * fR + x * fL));
            dl.advance();
            buffer.getWritePointer (0)[i] = inL + wL * m;
            if (numCh > 1) buffer.getWritePointer (1)[i] = inR + wR * m;
        }
    }

private:
    Param tl, tr, fb, cross, tone, mix;
    double sr = 44100.0;
    DelayLine dl;
    OnePoleLP lp[2];
    Smooth tlSm, trSm, mixSm;
};

} // namespace fx
