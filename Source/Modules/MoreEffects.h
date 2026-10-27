#pragma once
#include "FxBase.h"

namespace fx
{

inline float semitonesToRatio (float st) noexcept { return std::pow (2.0f, st / 12.0f); }

// ======================================================================= Pitch Shifter
class PitchShifterFx : public Processor
{
public:
    explicit PitchShifterFx (Bank& s) : type (s, "PS_TYPE"), semi (s, "PS_SHIFT"), mix (s, "PS_MIX") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        shifter.prepare (sr);
        initSmooth (ratioSm, sr, 0.03, 1.0f);
        initSmooth (mixSm, sr, 0.02, mix());
        reset();
    }
    void reset() override { shifter.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        shifter.setWindowSeconds (windowFor (type.index()), sr);
        ratioSm.setTargetValue (semitonesToRatio (std::clamp (semi(), -12.0f, 12.0f)));
        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            const float ratio = ratioSm.getNextValue(), m = mixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch) shifter.push (ch, sanitise (buffer.getReadPointer (ch)[i]));
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                d[i] = d[i] * (1.0f - m) + shifter.read (ch) * m;
            }
            shifter.advance (ratio);
        }
    }

    static double windowFor (int inputType) noexcept
    {
        static const double w[3] = { 0.060, 0.045, 0.025 };      // Vocal, Instrument, Drums
        return w[std::clamp (inputType, 0, 2)];
    }

private:
    Param type, semi, mix;
    double sr = 44100.0;
    PitchShifter shifter;
    Smooth ratioSm, mixSm;
};

// ======================================================================= Multi Shifter
// Six fixed harmony voices (octave, fifth and fourth, above and below), each with its own level.
class MultiShifterFx : public Processor
{
public:
    static constexpr int kVoices = 6;

    explicit MultiShifterFx (Bank& s)
        : type (s, "MS_TYPE"),
          lvl { Param (s, "MS_OCTUP"), Param (s, "MS_FIFTHUP"), Param (s, "MS_FOURTHUP"),
                Param (s, "MS_OCTDN"), Param (s, "MS_FIFTHDN"), Param (s, "MS_FOURTHDN") },
          dry (s, "MS_DRY") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (int v = 0; v < kVoices; ++v) { sh[v].prepare (sr); initSmooth (levelSm[v], sr, 0.02, lvl[v]()); }
        initSmooth (drySm, sr, 0.02, dry());
        reset();
    }
    void reset() override { for (auto& s : sh) s.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        static const float semis[kVoices] = { 12.0f, 7.0f, 5.0f, -12.0f, -7.0f, -5.0f };
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        float ratio[kVoices];
        for (int v = 0; v < kVoices; ++v)
        {
            ratio[v] = semitonesToRatio (semis[v]);
            levelSm[v].setTargetValue (std::clamp (lvl[v](), 0.0f, 1.0f));
            sh[v].setWindowSeconds (PitchShifterFx::windowFor (type.index()), sr);
        }
        drySm.setTargetValue (std::clamp (dry(), 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            const float dg = drySm.getNextValue();
            float inS[kMaxCh] {}, out[kMaxCh] {};
            for (int ch = 0; ch < numCh; ++ch) { inS[ch] = sanitise (buffer.getReadPointer (ch)[i]); out[ch] = inS[ch] * dg; }

            for (int v = 0; v < kVoices; ++v)
            {
                const float lv = levelSm[v].getNextValue();
                for (int ch = 0; ch < numCh; ++ch) sh[v].push (ch, inS[ch]);
                if (lv > 0.0005f)
                    for (int ch = 0; ch < numCh; ++ch) out[ch] += sh[v].read (ch) * lv;
                sh[v].advance (ratio[v]);
            }
            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] = out[ch];
        }
    }

private:
    Param type, lvl[kVoices], dry;
    double sr = 44100.0;
    PitchShifter sh[kVoices];
    Smooth levelSm[kVoices], drySm;
};

// ======================================================================= Dual Octaver
class DualOctaverFx : public Processor
{
public:
    explicit DualOctaverFx (Bank& s) : up (s, "OCT_UP"), down (s, "OCT_DOWN"), dry (s, "OCT_DRY"), tone (s, "OCT_TONE") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        shUp.prepare (sr);  shDown.prepare (sr);
        initSmooth (upSm, sr, 0.02, up());  initSmooth (downSm, sr, 0.02, down());  initSmooth (drySm, sr, 0.02, dry());
        reset();
    }
    void reset() override { shUp.reset(); shDown.reset(); lpU.reset(); lpD.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        upSm.setTargetValue (std::clamp (up(), 0.0f, 1.0f));
        downSm.setTargetValue (std::clamp (down(), 0.0f, 1.0f));
        drySm.setTargetValue (std::clamp (dry(), 0.0f, 1.0f));
        const float c = onePoleCoef (sr, clampFreq (1500.0f * std::pow (10.0f, 1.0f * std::clamp (tone(), 0.0f, 1.0f)), sr));   // 1.5k .. 15k

        for (int i = 0; i < n; ++i)
        {
            const float u = upSm.getNextValue(), d = downSm.getNextValue(), dg = drySm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = sanitise (buffer.getReadPointer (ch)[i]);
                shUp.push (ch, x);  shDown.push (ch, x);
            }
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* o = buffer.getWritePointer (ch);
                const float x = o[i];
                const float a = lpU.process (ch, shUp.read (ch), c), b = lpD.process (ch, shDown.read (ch), c);
                o[i] = x * dg + a * u + b * d;
            }
            shUp.advance (2.0f);  shDown.advance (0.5f);
        }
    }

private:
    Param up, down, dry, tone;
    double sr = 44100.0;
    PitchShifter shUp, shDown;
    OnePoleLP lpU, lpD;
    Smooth upSm, downSm, drySm;
};

// ======================================================================= Guitar Amp
class GuitarAmpFx : public Processor
{
public:
    explicit GuitarAmpFx (Bank& s)
        : type (s, "AMP_TYPE"), gain (s, "AMP_GAIN"), bass (s, "AMP_BASS"), mid (s, "AMP_MID"),
          treble (s, "AMP_TREBLE"), pres (s, "AMP_PRES"), level (s, "AMP_LEVEL") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        initSmooth (gainSm, sr, 0.02, 1.0f);
        initSmooth (levelSm, sr, 0.02, dbToGain (level()));
        reset();
    }
    void reset() override { hp.reset(); bs.reset(); md.reset(); tr.reset(); pr.reset(); lp.reset(); dc.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const int t = std::clamp (type.index(), 0, 3);
        static const float maxGainDb[4]   = { 16.0f, 28.0f, 38.0f, 46.0f };
        static const float tightHz[4]     = { 60.0f, 90.0f, 140.0f, 200.0f };
        static const float outTrim[4]     = { 1.0f, 0.85f, 0.75f, 0.65f };

        const float g = std::clamp (gain(), 0.0f, 1.0f);
        gainSm.setTargetValue (dbToGain (g * maxGainDb[t]));
        levelSm.setTargetValue (dbToGain (level()) * outTrim[t]);

        hp.set (BiquadCoeffs::highpass (sr, tightHz[t]));
        bs.set (BiquadCoeffs::lowShelf (sr, 120.0, (std::clamp (bass(), 0.0f, 1.0f) - 0.5f) * 20.0f));
        md.set (BiquadCoeffs::peak (sr, t >= 2 ? 750.0 : 600.0, (std::clamp (mid(), 0.0f, 1.0f) - 0.5f) * 18.0f, 0.8));
        tr.set (BiquadCoeffs::highShelf (sr, 3200.0, (std::clamp (treble(), 0.0f, 1.0f) - 0.5f) * 20.0f));
        pr.set (BiquadCoeffs::highShelf (sr, 6000.0, (std::clamp (pres(), 0.0f, 1.0f) - 0.5f) * 16.0f));
        lp.set (BiquadCoeffs::lowpass (sr, std::min (9000.0, sr * 0.45)));

        for (int i = 0; i < n; ++i)
        {
            const float gn = gainSm.getNextValue(), lv = levelSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                float x = hp.process (ch, sanitise (d[i])) * gn;
                float y;
                switch (t)
                {
                    case 0:  y = std::tanh (x * 0.8f); break;                                            // clean: very gentle
                    case 1:  y = std::tanh (x + 0.15f) - 0.1489f; break;                                 // crunch: asymmetric soft clip
                    case 2:  y = std::tanh (std::tanh (x) * 2.2f); break;                                // lead: two cascaded stages
                    default: y = std::tanh (std::tanh (x * 1.4f) * 3.0f); break;                         // metal: tight and hard
                }
                y = dc.process (ch, y);
                y = bs.process (ch, y);
                y = md.process (ch, y);
                y = tr.process (ch, y);
                y = pr.process (ch, y);
                y = lp.process (ch, y);
                d[i] = y * lv;
            }
        }
    }

private:
    Param type, gain, bass, mid, treble, pres, level;
    double sr = 44100.0;
    Biquad hp, bs, md, tr, pr, lp;
    DcBlocker dc;
    Smooth gainSm, levelSm;
};

// ======================================================================= Guitar Cab
class GuitarCabFx : public Processor
{
public:
    explicit GuitarCabFx (Bank& s)
        : type (s, "CAB_TYPE"), mic (s, "CAB_MIC"), low (s, "CAB_LOW"), high (s, "CAB_HIGH"),
          room (s, "CAB_ROOM"), mix (s, "CAB_MIX"), level (s, "CAB_LEVEL") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        rv.prepare (sr);
        rv.setParams (0.3f, 0.7f, 1.0f);
        initSmooth (mixSm, sr, 0.02, mix());
        initSmooth (levelSm, sr, 0.02, dbToGain (level()));
        initSmooth (roomSm, sr, 0.02, room());
        reset();
    }
    void reset() override { hpA.reset(); hpB.reset(); lpA.reset(); lpB.reset(); bump.reset(); dip.reset(); pres.reset(); rv.reset(); }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        struct Voicing { float hp, bumpF, bumpDb, dipF, dipDb, presF, presDb, lp; };
        static const Voicing v[4] = {
            {  90.0f, 120.0f, 3.0f, 700.0f, -1.5f, 2800.0f, 4.0f, 5600.0f },     // 1x12 open back
            {  80.0f, 110.0f, 4.0f, 600.0f, -2.0f, 2500.0f, 3.0f, 5800.0f },     // 2x12 combo
            {  70.0f,  95.0f, 5.0f, 500.0f, -4.0f, 2200.0f, 3.0f, 5000.0f },     // 4x12 closed back
            { 110.0f, 150.0f, 2.0f, 800.0f, -1.0f, 3200.0f, 5.0f, 4800.0f } };   // 1x10 vintage
        const auto& c = v[std::clamp (type.index(), 0, 3)];

        const float m = std::clamp (mic(), 0.0f, 1.0f);
        const float hpF = clampFreq (std::max (c.hp, low()), sr);
        const float lpF = clampFreq (std::min (c.lp * (0.7f + 0.6f * m), high()), sr);
        hpA.set (BiquadCoeffs::highpass (sr, hpF));  hpB.set (BiquadCoeffs::highpass (sr, hpF));
        lpA.set (BiquadCoeffs::lowpass (sr, lpF));   lpB.set (BiquadCoeffs::lowpass (sr, lpF));
        bump.set (BiquadCoeffs::peak (sr, c.bumpF, c.bumpDb, 1.2));
        dip.set (BiquadCoeffs::peak (sr, c.dipF, c.dipDb, 0.9));
        pres.set (BiquadCoeffs::peak (sr, c.presF, c.presDb + (m - 0.5f) * 8.0f, 1.1));

        mixSm.setTargetValue (std::clamp (mix(), 0.0f, 1.0f));
        levelSm.setTargetValue (dbToGain (level()));
        roomSm.setTargetValue (std::clamp (room(), 0.0f, 1.0f));

        for (int i = 0; i < n; ++i)
        {
            const float mx = mixSm.getNextValue(), lv = levelSm.getNextValue(), rm = roomSm.getNextValue();
            float out[kMaxCh] {}, dryS[kMaxCh] {};
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = sanitise (buffer.getReadPointer (ch)[i]);
                dryS[ch] = x;
                float y = hpB.process (ch, hpA.process (ch, x));
                y = bump.process (ch, y);  y = dip.process (ch, y);  y = pres.process (ch, y);
                out[ch] = lpB.process (ch, lpA.process (ch, y));
            }
            // a touch of room around the cabinet
            float rl = out[0], rr = numCh > 1 ? out[1] : out[0];
            rv.process (rl, rr);
            out[0] += rl * rm * 0.5f;
            if (numCh > 1) out[1] += rr * rm * 0.5f;

            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = dryS[ch] * (1.0f - mx) + out[ch] * lv * mx;
        }
    }

private:
    Param type, mic, low, high, room, mix, level;
    double sr = 44100.0;
    Biquad hpA, hpB, lpA, lpB, bump, dip, pres;
    ReverbCore rv;
    Smooth mixSm, levelSm, roomSm;
};

// ======================================================================= Feedback Killer
class FeedbackKillerFx : public Processor
{
public:
    explicit FeedbackKillerFx (Bank& s) : amount (s, "FK_AMOUNT") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        fftData.assign ((size_t) kN * 2, 0.0f);
        ring.assign ((size_t) kN, 0.0f);
        window.resize ((size_t) kN);
        for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) kN);
        mags.assign ((size_t) kN / 2 + 1, 0.0f);
        reset();
    }

    void reset() override
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = hop = 0;
        for (auto& nt : notches) { nt.active = false; nt.f.reset(); }
        lastBin = -1;  persist = 0;
        histPos = 0;  std::fill (hist.begin(), hist.end(), 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float depthDb = depth();
        if (std::abs (depthDb - lastDepth) > 0.01f)
        {
            lastDepth = depthDb;
            for (auto& nt : notches) if (nt.active) nt.f.set (BiquadCoeffs::peak (sr, nt.freq, -depthDb, kQ));
        }

        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) mono += buffer.getReadPointer (ch)[i];
            ring[(size_t) writePos] = sanitise (mono / (float) numCh);
            if (++writePos >= kN) writePos = 0;

            if (++hop >= kHop) { hop = 0; analyse(); }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float y = buffer.getReadPointer (ch)[i];
                for (auto& nt : notches) if (nt.active) y = nt.f.process (ch, y);
                buffer.getWritePointer (ch)[i] = y;
            }
        }
    }

    float getMeter() const noexcept override
    {
        int c = 0;
        for (const auto& nt : notches) c += nt.active ? 1 : 0;
        return (float) c;
    }

private:
    static constexpr int kN = 2048, kHop = 512, kNotches = 6, kPersist = 10, kHistLen = 12;
    static constexpr float kQ = 30.0f;

    struct Notch { Biquad f; float freq = 0.0f; bool active = false; int idle = 0; int age = 0; };

    void analyse()
    {
        // windowed copy of the last kN samples, oldest first
        for (int i = 0; i < kN; ++i)
            fftData[(size_t) i] = ring[(size_t) ((writePos + i) % kN)] * window[(size_t) i];
        std::fill (fftData.begin() + kN, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        for (int k = 0; k <= kN / 2; ++k) mags[(size_t) k] = fftData[(size_t) k];

        const int kLo = std::max (4, (int) (120.0 * kN / sr)), kHi = std::min (kN / 2 - 4, (int) (sr * 0.45 * kN / sr));
        int best = kLo;
        for (int k = kLo; k <= kHi; ++k) if (mags[(size_t) k] > mags[(size_t) best]) best = k;

        const float amp = mags[(size_t) best] * 4.0f / (float) kN;          // ~ sine amplitude (Hann window)
        float floorSum = 0.0f;  int cnt = 0;
        for (int k = std::max (1, best - 40); k <= std::min (kN / 2, best + 40); ++k)
            if (std::abs (k - best) > 3) { floorSum += mags[(size_t) k]; ++cnt; }
        const float floorM = cnt > 0 ? floorSum / (float) cnt : 1.0f;
        const float ratio = mags[(size_t) best] / std::max (floorM, 1.0e-9f);

        const float s = std::clamp (sens(), 0.0f, 1.0f);
        const float ratioNeeded = 30.0f - 22.0f * s;
        const float levelGate   = 0.65f * (1.0f - s) + 0.05f;

        // track how that bin's level moved over the last ~70 ms: feedback grows
        const float oldAmp = hist[(size_t) histPos];
        hist[(size_t) histPos] = amp;
        histPos = (histPos + 1) % kHistLen;
        const bool growing = oldAmp > 0.0f && amp > oldAmp * 1.15f && amp > 0.02f;

        const bool candidate = ratio > ratioNeeded && (amp > levelGate || growing);
        if (candidate && lastBin >= 0 && std::abs (best - lastBin) <= 1) ++persist;
        else persist = candidate ? 1 : 0;
        lastBin = candidate ? best : -1;

        // existing notches that are still being hit stay alive; the others age out after ~8 s
        for (auto& nt : notches)
        {
            if (! nt.active) continue;
            ++nt.age;
            const int nb = (int) (nt.freq * kN / sr + 0.5);
            if (candidate && std::abs (nb - best) <= 2) nt.idle = 0;
            else if (++nt.idle > 8 * (int) (sr / kHop)) nt.active = false;
        }

        if (persist >= kPersist)
        {
            // interpolate the peak position for an accurate notch
            const float a = mags[(size_t) best - 1], b = mags[(size_t) best], c = mags[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            const float delta = std::abs (den) > 1.0e-12f ? std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f;
            const float freq = ((float) best + delta) * (float) sr / (float) kN;

            bool already = false;
            for (const auto& nt : notches) already = already || (nt.active && std::abs (nt.freq - freq) < freq * 0.03f);
            if (! already)
            {
                Notch* slot = nullptr;
                for (auto& nt : notches) if (! nt.active) { slot = &nt; break; }
                if (slot == nullptr) { slot = &notches[0]; for (auto& nt : notches) if (nt.age > slot->age) slot = &nt; }
                slot->freq = freq;  slot->active = true;  slot->idle = 0;  slot->age = 0;
                slot->f.reset();
                slot->f.set (BiquadCoeffs::peak (sr, freq, -depth(), kQ));
            }
            persist = 0;
        }
    }

    Param amount;                                        // one knob drives both how eagerly it listens and how deep the notch is
    float sens()  const noexcept { return std::clamp (amount(), 0.0f, 1.0f); }
    float depth() const noexcept { return 12.0f + 24.0f * std::clamp (amount(), 0.0f, 1.0f); }
    double sr = 44100.0;
    juce::dsp::FFT fft { 11 };
    std::vector<float> fftData, ring, window, mags;
    int writePos = 0, hop = 0, lastBin = -1, persist = 0, histPos = 0;
    std::array<float, kHistLen> hist {};
    std::array<Notch, kNotches> notches;
    float lastDepth = -1.0f;
};

// ======================================================================= DeEsser
class DeEsserFx : public Processor
{
public:
    explicit DeEsserFx (Bank& s) : freq (s, "DS_FREQ"), thresh (s, "DS_THRESH") {}

    void prepare (double sampleRate, int) override
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        env.set (sr, 0.3f, 40.0f);
        gainC = timeToCoef (sr, 1.5f);
        reset();
    }
    void reset() override { hp.reset(); lp.reset(); env.reset(); gDb = 0.0f; meter = 0.0f; }

    void process (juce::AudioBuffer<float>& buffer, const Context&) override
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();
        if (numCh == 0) return;

        const float fc = clampFreq (freq(), sr);
        hp.highpass (sr, fc);  lp.lowpass (sr, fc);
        const float thr = thresh();
        const float maxRed = 18.0f;
        float worst = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float band[kMaxCh] {}, low[kMaxCh] {}, in[kMaxCh] {};
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                in[ch] = sanitise (buffer.getReadPointer (ch)[i]);
                band[ch] = hp.process (ch, in[ch]);
                low[ch] = lp.process (ch, in[ch]);
                det = std::max (det, std::abs (band[ch]));
            }
            const float lvl = gainToDb (env.process (det));
            const float over = lvl - thr;
            const float target = over > 0.0f ? -std::min (over * (1.0f - 1.0f / 6.0f), maxRed) : 0.0f;
            gDb = gainC * gDb + (1.0f - gainC) * target;
            worst = std::max (worst, -gDb);
            const float g = dbToGain (gDb);

            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = low[ch] + band[ch] * g;
        }
        meter = std::max (worst, meter * 0.9f);
    }

    float getMeter() const noexcept override { return meter; }

private:
    Param freq, thresh;
    double sr = 44100.0;
    LR4 hp, lp;                    // Linkwitz-Riley split: low + high sums back to the original
    EnvFollower env;
    float gainC = 0.0f, gDb = 0.0f;
    float meter = 0.0f;
};

} // namespace fx
