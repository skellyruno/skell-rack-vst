#pragma once
#include <JuceHeader.h>
#include "../FxRegistry.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

// Shared DSP building blocks for every effect in the rack.
//
// Rules every effect follows so it is safe inside any host:
//   * no allocation in process()        (all buffers are sized in prepare())
//   * no dependence on the block size   (everything is per-sample, no scratch buffers)
//   * parameters are read from lock-free atomics owned by the APVTS
namespace fx
{
constexpr int   kMaxCh = 2;
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

// Host information handed to every effect once per block.
struct Context
{
    double sampleRate = 44100.0;
    double bpm        = 120.0;
    bool   playing    = false;
    bool   hasPpq     = false;
    double ppq        = 0.0;     // quarter-note position at the start of the block
};

// Where an effect finds its parameters: the generic controls of one rack slot.
struct Bank
{
    Bank (juce::AudioProcessorValueTreeState& s, int slotIndex, const FxInfo& i) : apvts (s), slot (slotIndex), info (i) {}
    juce::AudioProcessorValueTreeState& apvts;
    int slot;
    const FxInfo& info;
};

// Lock-free read access to one control.  Looks the control up by its name in the registry
// ("EQ_HPF") and returns the real value (Hz, dB, ...) whatever the host stored.
class Param
{
public:
    Param() = default;
    Param (const Bank& bank, const juce::String& id)
    {
        const int idx = specIndex (bank.info, id);
        jassert (idx >= 0);                                  // typo in a parameter name
        if (idx < 0) return;
        auto* raw = bank.apvts.getRawParameterValue (slotParamId (bank.slot, idx));
        jassert (raw != nullptr);
        if (raw == nullptr) return;

        p = raw;
        const auto& spec = bank.info.params[(size_t) idx];
        kind = spec.kind;
        range = makeRange (spec);
        nChoices = std::max (1, spec.choices.size());
    }

    float get() const noexcept
    {
        const float v = p->load (std::memory_order_relaxed);
        switch (kind)
        {
            case ParamKind::Toggle: return v >= 0.5f ? 1.0f : 0.0f;
            case ParamKind::Choice: return (float) std::min (nChoices - 1, (int) (std::clamp (v, 0.0f, 1.0f) * (float) nChoices));
            default:                return range.convertFrom0to1 (v);
        }
    }

    float operator()() const noexcept { return get(); }
    bool  on() const noexcept         { return get() >= 0.5f; }
    int   index() const noexcept      { return (int) (get() + 0.5f); }

private:
    static std::atomic<float>& fallback() { static std::atomic<float> f { 0.0f }; return f; }
    std::atomic<float>* p = &fallback();
    ParamKind kind = ParamKind::Knob;
    juce::NormalisableRange<float> range { 0.0f, 1.0f };
    int nChoices = 1;
};

class Processor
{
public:
    virtual ~Processor() = default;
    virtual void  prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void  reset() {}
    virtual void  process (juce::AudioBuffer<float>& buffer, const Context& ctx) = 0;
    // Optional value shown on the card (gain reduction in dB, positive number).
    virtual float getMeter() const noexcept { return 0.0f; }
};

// ------------------------------------------------------------------ small helpers
inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }
inline float timeToCoef (double sr, float ms) noexcept
{
    return (float) std::exp (-1.0 / (sr * 0.001 * (double) std::max (ms, 0.001f)));
}
inline float onePoleCoef (double sr, float freq) noexcept
{
    return 1.0f - std::exp (-kTwoPi * freq / (float) sr);
}
inline float clampFreq (float f, double sr) noexcept
{
    return std::clamp (f, 10.0f, (float) (sr * 0.45));
}
inline float sanitise (float x) noexcept    { return std::isfinite (x) ? x : 0.0f; }

using Smooth = juce::SmoothedValue<float>;
inline void initSmooth (Smooth& s, double sr, double seconds, float value)
{
    s.reset (sr, seconds);
    s.setCurrentAndTargetValue (value);
}

// ------------------------------------------------------------------ biquad (RBJ cookbook)
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static BiquadCoeffs make (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        BiquadCoeffs c;
        c.b0 = (float) (b0 / a0);  c.b1 = (float) (b1 / a0);  c.b2 = (float) (b2 / a0);
        c.a1 = (float) (a1 / a0);  c.a2 = (float) (a2 / a0);
        return c;
    }

    static BiquadCoeffs lowpass (double sr, double f, double q = 0.70710678) noexcept
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make ((1 - cw) * 0.5, 1 - cw, (1 - cw) * 0.5, 1 + al, -2 * cw, 1 - al);
    }
    static BiquadCoeffs highpass (double sr, double f, double q = 0.70710678) noexcept
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make ((1 + cw) * 0.5, -(1 + cw), (1 + cw) * 0.5, 1 + al, -2 * cw, 1 - al);
    }
    static BiquadCoeffs peak (double sr, double f, double gainDb, double q = 1.0) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }
    static BiquadCoeffs lowShelf (double sr, double f, double gainDb, double q = 0.70710678) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double sA = 2.0 * std::sqrt (A) * al;
        return make (A * ((A + 1) - (A - 1) * cw + sA),  2 * A * ((A - 1) - (A + 1) * cw),  A * ((A + 1) - (A - 1) * cw - sA),
                     (A + 1) + (A - 1) * cw + sA,       -2 * ((A - 1) + (A + 1) * cw),     (A + 1) + (A - 1) * cw - sA);
    }
    static BiquadCoeffs highShelf (double sr, double f, double gainDb, double q = 0.70710678) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double sA = 2.0 * std::sqrt (A) * al;
        return make (A * ((A + 1) + (A - 1) * cw + sA), -2 * A * ((A - 1) + (A + 1) * cw),  A * ((A + 1) + (A - 1) * cw - sA),
                     (A + 1) - (A - 1) * cw + sA,        2 * ((A - 1) - (A + 1) * cw),     (A + 1) - (A - 1) * cw - sA);
    }

    // Linear magnitude of the filter at `freq`.
    float magnitude (double freq, double sr) const noexcept
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cw = std::cos (w), c2 = std::cos (2.0 * w);
        const double num = (double) b0 * b0 + (double) b1 * b1 + (double) b2 * b2
                         + 2.0 * ((double) b0 * b1 + (double) b1 * b2) * cw + 2.0 * (double) b0 * b2 * c2;
        const double den = 1.0 + (double) a1 * a1 + (double) a2 * a2
                         + 2.0 * ((double) a1 + (double) a1 * a2) * cw + 2.0 * (double) a2 * c2;
        return (float) std::sqrt (std::max (num / std::max (den, 1.0e-12), 1.0e-12));
    }
};

class Biquad
{
public:
    void set (const BiquadCoeffs& c) noexcept { k = c; }
    void reset() noexcept { for (int i = 0; i < kMaxCh; ++i) z1[i] = z2[i] = 0.0f; }
    float process (int ch, float x) noexcept
    {
        const float y = k.b0 * x + z1[ch];
        z1[ch] = k.b1 * x - k.a1 * y + z2[ch];
        z2[ch] = k.b2 * x - k.a2 * y;
        return y;
    }
private:
    BiquadCoeffs k;
    float z1[kMaxCh] {}, z2[kMaxCh] {};
};

// 4th-order Linkwitz-Riley section (two cascaded Butterworth biquads).
class LR4
{
public:
    void lowpass  (double sr, double f) noexcept { const auto c = BiquadCoeffs::lowpass (sr, f);  a.set (c); b.set (c); }
    void highpass (double sr, double f) noexcept { const auto c = BiquadCoeffs::highpass (sr, f); a.set (c); b.set (c); }
    void reset() noexcept { a.reset(); b.reset(); }
    float process (int ch, float x) noexcept { return b.process (ch, a.process (ch, x)); }
private:
    Biquad a, b;
};

// ------------------------------------------------------------------ one-pole filters
struct OnePoleLP
{
    float z[kMaxCh] {};
    void reset() noexcept { z[0] = z[1] = 0.0f; }
    float process (int ch, float x, float coef) noexcept { z[ch] += coef * (x - z[ch]); return z[ch]; }
};

struct DcBlocker
{
    float x1[kMaxCh] {}, y1[kMaxCh] {};
    void reset() noexcept { x1[0] = x1[1] = y1[0] = y1[1] = 0.0f; }
    float process (int ch, float x, float r = 0.995f) noexcept
    {
        const float y = x - x1[ch] + r * y1[ch];
        x1[ch] = x;  y1[ch] = y;
        return y;
    }
};

// ------------------------------------------------------------------ envelope + dynamics
struct EnvFollower
{
    float a = 0.0f, r = 0.0f, y = 0.0f;
    void set (double sr, float attackMs, float releaseMs) noexcept { a = timeToCoef (sr, attackMs); r = timeToCoef (sr, releaseMs); }
    void reset() noexcept { y = 0.0f; }
    float process (float x) noexcept
    {
        x = std::abs (x);
        const float c = x > y ? a : r;
        y = c * y + (1.0f - c) * x;
        return y;
    }
};

// Feed-forward compressor gain computer with soft knee and smoothed gain reduction (dB, <= 0).
struct CompCore
{
    float thresholdDb = -18.0f, ratio = 4.0f, kneeDb = 6.0f;
    float attC = 0.0f, relC = 0.0f, grDb = 0.0f;

    void setTimes (double sr, float attackMs, float releaseMs) noexcept { attC = timeToCoef (sr, attackMs); relC = timeToCoef (sr, releaseMs); }
    void reset() noexcept { grDb = 0.0f; }

    float tick (float inputPeak) noexcept
    {
        const float lvl  = gainToDb (inputPeak);
        const float over = lvl - thresholdDb;
        float target;
        if (2.0f * over < -kneeDb)                target = 0.0f;
        else if (2.0f * std::abs (over) <= kneeDb) { const float t = over + kneeDb * 0.5f; target = (1.0f / ratio - 1.0f) * t * t / (2.0f * kneeDb); }
        else                                      target = (1.0f / ratio - 1.0f) * over;

        const float c = target < grDb ? attC : relC;
        grDb = c * grDb + (1.0f - c) * target;
        return grDb;
    }
};

// Transient designer: differential envelope follower.
struct ShaperCore
{
    EnvFollower fast, slow;
    float gain = 1.0f, smooth = 0.0f;

    void prepare (double sr) noexcept { fast.set (sr, 0.5f, 25.0f); slow.set (sr, 25.0f, 150.0f); smooth = timeToCoef (sr, 1.0f); reset(); }
    void reset() noexcept { fast.reset(); slow.reset(); gain = 1.0f; }

    // transient / sustain in -1..+1.  Returns a linear gain.
    float tick (float peak, float transient, float sustain) noexcept
    {
        const float f = gainToDb (fast.process (peak));
        const float s = gainToDb (slow.process (peak));
        float gDb = 0.0f;
        if (s > -70.0f)
        {
            const float d = f - s;
            gDb = d > 0.0f ? transient * d * 0.7f : sustain * (-d) * 1.5f;
            gDb = std::clamp (gDb, -12.0f, 12.0f);
        }
        gain = smooth * gain + (1.0f - smooth) * dbToGain (gDb);
        return gain;
    }
};

// ------------------------------------------------------------------ delay lines
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        size = std::max (maxDelaySamples, 1) + 8;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        pos = 0;
    }
    void clear() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); pos = 0; }

    void push (int ch, float x) noexcept { buf[(size_t) ch][(size_t) pos] = x; }
    void advance() noexcept { if (++pos >= size) pos = 0; }

    // delay measured back from the sample most recently pushed (linear interpolation)
    float read (int ch, float delay) const noexcept
    {
        delay = std::clamp (delay, 0.0f, (float) (size - 4));
        float rp = (float) pos - delay;
        if (rp < 0.0f) rp += (float) size;
        int i0 = (int) rp;
        const float fr = rp - (float) i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1;
        if (i1 >= size) i1 = 0;
        const auto& b = buf[(size_t) ch];
        return b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
    }
    int maxDelay() const noexcept { return size - 8; }

private:
    std::array<std::vector<float>, kMaxCh> buf;
    int size = 16, pos = 0;
};

// Schroeder all-pass, used for input diffusion.
struct Allpass
{
    std::vector<float> buf;
    int idx = 0;
    float g = 0.5f;
    void prepare (int len) { buf.assign ((size_t) std::max (len, 1), 0.0f); idx = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
    float process (float x) noexcept
    {
        const float d = buf[(size_t) idx];
        const float w = x + g * d;
        buf[(size_t) idx] = w;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * w;
    }
};

// Freeverb (juce::Reverb) used as a wet-only reverb core.
struct ReverbCore
{
    juce::Reverb reverb;
    juce::Reverb::Parameters p;

    void prepare (double sr) { reverb.setSampleRate (sr); reverb.reset(); }
    void reset() { reverb.reset(); }
    void setParams (float room, float damping, float width)
    {
        p.roomSize = std::clamp (room, 0.0f, 1.0f);
        p.damping  = std::clamp (damping, 0.0f, 1.0f);
        p.width    = std::clamp (width, 0.0f, 1.0f);
        p.wetLevel = 0.33f;       // juce::Reverb scales wet by 3, so this is ~unity
        p.dryLevel = 0.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }
    void process (float& l, float& r) noexcept { reverb.processStereo (&l, &r, 1); }
};

// Octave shifter built from two cross-faded, moving delay taps.
class PitchShifter
{
public:
    void prepare (double sr)
    {
        window = (float) (0.06 * sr);
        dl.prepare ((int) window + 8);
        phase = 0.0f;
    }
    void reset() { dl.clear(); phase = 0.0f; }

    void push (int ch, float x) noexcept { dl.push (ch, x); }
    float read (int ch) const noexcept
    {
        const float p1 = phase;
        float p2 = phase + 0.5f;  if (p2 >= 1.0f) p2 -= 1.0f;
        const float w1 = std::sin (kPi * p1), w2 = std::sin (kPi * p2);
        return dl.read (ch, (1.0f - p1) * window) * (w1 * w1) + dl.read (ch, (1.0f - p2) * window) * (w2 * w2);
    }
    void advance (float ratio) noexcept
    {
        phase += (ratio - 1.0f) / window;
        phase -= std::floor (phase);
        dl.advance();
    }

private:
    DelayLine dl;
    float window = 2048.0f, phase = 0.0f;
};

} // namespace fx
