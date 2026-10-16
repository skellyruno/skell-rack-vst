#pragma once
#include "FxBase.h"

// The three original effects, rebuilt on the shared fx::Processor interface so that
// every control on their cards now actually does something.
namespace fx
{

// ======================================================================= Reverser
// Records one grain of audio, then plays it back backwards during the next grain.
// (The wet signal therefore trails the input by one grain length, as on any reverser.)
class ReverserFx : public Processor
{
public:
    explicit ReverserFx (Bank& s)
        : mix (s, "REV_MIX"), time (s, "REV_TIME"), beat (s, "REV_BEAT") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        maxGrain = (int) (sr * 6.0);
        for (int ch = 0; ch < kMaxCh; ++ch) { cur[(size_t) ch].assign ((size_t) maxGrain, 0.0f); prev[(size_t) ch].assign ((size_t) maxGrain, 0.0f); }
        reset();
    }

    void reset() override
    {
        for (int ch = 0; ch < kMaxCh; ++ch)
        {
            std::fill (cur[(size_t) ch].begin(), cur[(size_t) ch].end(), 0.0f);
            std::fill (prev[(size_t) ch].begin(), prev[(size_t) ch].end(), 0.0f);
        }
        n = 0;  prevLen = 0;  grain = std::max (64, (int) (sr * 0.25));
    }

    void process (juce::AudioBuffer<float>& buffer, const Context& ctx) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int numSamples = buffer.getNumSamples();
        if (numCh == 0 || cur[0].empty()) return;

        const float m = std::clamp (mix(), 0.0f, 1.0f);
        const int desired = desiredGrain (ctx);
        if (prevLen == 0) grain = desired;      // nothing recorded yet: follow the knob straight away

        for (int i = 0; i < numSamples; ++i)
        {
            if (n >= grain)
            {
                std::swap (cur, prev);
                prevLen = grain;
                n = 0;
                grain = desired;
            }

            // cross-fade the edges of each reversed grain to avoid clicks
            float w = 0.0f;
            int readIdx = 0;
            if (n < prevLen)
            {
                const int fade = std::max (1, std::min (prevLen / 8, (int) (sr * 0.02)));
                w = std::min ({ 1.0f, (float) (n + 1) / (float) fade, (float) (prevLen - n) / (float) fade });
                readIdx = prevLen - 1 - n;
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                const float dry = d[i];
                cur[(size_t) ch][(size_t) n] = dry;
                const float wet = w > 0.0f ? prev[(size_t) ch][(size_t) readIdx] * w : 0.0f;
                d[i] = dry * (1.0f - m) + wet * m;
            }
            ++n;
        }
    }

private:
    int desiredGrain (const Context& ctx) const
    {
        const float t = std::clamp (time(), 0.0f, 1.0f);
        double seconds;
        if (beat.on())
        {
            static const double beats[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
            const int idx = std::clamp ((int) (t * 5.0f), 0, 4);
            seconds = beats[idx] * 60.0 / std::clamp (ctx.bpm, 20.0, 400.0);
        }
        else
        {
            seconds = 0.05 * std::pow (20.0, (double) t);     // 50 ms .. 1 s
        }
        return std::clamp ((int) (seconds * sr), 64, maxGrain);
    }

    Param mix, time, beat;
    double sr = 44100.0;
    int maxGrain = 1, grain = 11025, prevLen = 0, n = 0;
    std::array<std::vector<float>, kMaxCh> cur, prev;
};

// ======================================================================= Noise gate
class NoiseGateFx : public Processor
{
public:
    explicit NoiseGateFx (Bank& s)
        : thresh (s, "GATE_THRESH"), attack (s, "GATE_ATTACK"), release (s, "GATE_RELEASE") {}

    void prepare (double sampleRate, int) override { sr = sampleRate > 1000.0 ? sampleRate : 44100.0; reset(); }
    void reset() override { env = 0.0f; gain = 0.0f; open = false; hold = 0; meter.store (0.0f); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int numSamples = buffer.getNumSamples();
        if (numCh == 0) return;

        const float openThr  = dbToGain (thresh());
        const float closeThr = openThr * 0.708f;                 // 3 dB hysteresis stops chatter
        const float attC = timeToCoef (sr, std::max (attack(), 0.1f));
        const float relC = timeToCoef (sr, std::max (release(), 1.0f));
        const float detRel = timeToCoef (sr, 30.0f);
        const int holdSamples = (int) (sr * 0.02);

        for (int i = 0; i < numSamples; ++i)
        {
            float lvl = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) lvl = std::max (lvl, std::abs (buffer.getReadPointer (ch)[i]));
            env = lvl > env ? lvl : env * detRel;

            if (! open) { if (env > openThr) { open = true; hold = holdSamples; } }
            else if (env > closeThr)  hold = holdSamples;
            else if (--hold <= 0)     open = false;

            const float target = open ? 1.0f : 0.0f;
            const float c = target > gain ? attC : relC;
            gain = c * gain + (1.0f - c) * target;

            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= gain;
        }
        meter.store (gain, std::memory_order_relaxed);
    }

    // 0 = closed, 1 = open
    float getMeter() const noexcept override { return meter.load (std::memory_order_relaxed); }

private:
    Param thresh, attack, release;
    double sr = 44100.0;
    float env = 0.0f, gain = 0.0f;
    bool open = false;
    int hold = 0;
    std::atomic<float> meter { 0.0f };
};

// ======================================================================= Chorus ensemble
class ChorusFx : public Processor
{
public:
    explicit ChorusFx (Bank& s)
        : depth (s, "CHO_DEPTH"), amount (s, "CHO_AMOUNT"), speed (s, "CHO_SPEED"), bass (s, "CHO_BASS"), treble (s, "CHO_TREBLE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        dl.prepare ((int) (sr * 0.05));
        reset();
    }

    void reset() override
    {
        dl.clear();
        phase = 0.0;
        lowShelf.reset(); highShelf.reset();
        haveCoeffs = false;
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int numSamples = buffer.getNumSamples();
        if (numCh == 0) return;

        updateTone();

        const float dpt = std::clamp (depth(), 0.0f, 1.0f);
        const float amt = std::clamp (amount(), 0.0f, 1.0f);
        const double inc = (double) std::max (speed(), 0.01f) / sr;
        const float msToSamp = (float) (sr * 0.001);

        static const float baseMs[3]  = { 10.0f, 13.5f, 17.0f };
        static const float panL[3]    = { 1.0f, 0.6f, 0.2f };
        static const float panR[3]    = { 0.2f, 0.6f, 1.0f };

        for (int i = 0; i < numSamples; ++i)
        {
            float d[3];
            for (int v = 0; v < 3; ++v)
            {
                const float lfo = std::sin (kTwoPi * (float) (phase + (double) v / 3.0));
                d[v] = (baseMs[v] + lfo * dpt * 5.0f) * msToSamp;
            }

            float in[kMaxCh];
            for (int ch = 0; ch < numCh; ++ch) { in[ch] = buffer.getReadPointer (ch)[i]; dl.push (ch, in[ch]); }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float wet = 0.0f;
                if (numCh == 1)
                    wet = (dl.read (0, d[0]) + dl.read (0, d[1]) + dl.read (0, d[2])) * (1.0f / 3.0f);
                else
                {
                    const float* pan = ch == 0 ? panL : panR;
                    wet = (dl.read (ch, d[0]) * pan[0] + dl.read (ch, d[1]) * pan[1] + dl.read (ch, d[2]) * pan[2]) * (1.0f / 1.8f);
                }
                wet = highShelf.process (ch, lowShelf.process (ch, wet));
                buffer.getWritePointer (ch)[i] = in[ch] * (1.0f - 0.5f * amt) + wet * (0.5f * amt);
            }

            dl.advance();
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    void updateTone()
    {
        const float b = bass(), t = treble();
        if (haveCoeffs && b == lastBass && t == lastTreble) return;
        lowShelf.set (BiquadCoeffs::lowShelf (sr, 200.0, b));
        highShelf.set (BiquadCoeffs::highShelf (sr, clampFreq (3500.0f, sr), t));
        lastBass = b;  lastTreble = t;  haveCoeffs = true;
    }

    Param depth, amount, speed, bass, treble;
    double sr = 44100.0, phase = 0.0;
    DelayLine dl;
    Biquad lowShelf, highShelf;
    float lastBass = 0.0f, lastTreble = 0.0f;
    bool haveCoeffs = false;
};

} // namespace fx
