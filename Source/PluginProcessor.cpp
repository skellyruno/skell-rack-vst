#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BandLabImport.h"
#include "Modules/DSPModules.h"
#include "Modules/NewEffects.h"
#include "Modules/MoreEffects.h"
#include "Modules/ExtraEffects.h"
#include "Modules/BandLabFx.h"

namespace
{
    std::unique_ptr<fx::Processor> createEffect (int type, juce::AudioProcessorValueTreeState& apvts, int slot)
    {
        const FxInfo* info = getFxInfo (type);
        if (info == nullptr) return nullptr;
        fx::Bank s (apvts, slot, *info);

        switch (type)
        {
            case ID_Reverser:   return std::make_unique<fx::ReverserFx> (s);
            case ID_NoiseGate:  return std::make_unique<fx::NoiseGateFx> (s);
            case ID_Chorus:     return std::make_unique<fx::ChorusFx> (s);
            case ID_VisualEQ:   return std::make_unique<fx::VisualEq> (s);
            case ID_Compressor: return std::make_unique<fx::CompressorFx> (s);
            case ID_MultiBand:  return std::make_unique<fx::MultiBandFx> (s);
            case ID_Limiter:    return std::make_unique<fx::LimiterFx> (s);
            case ID_VocalVerb:  return std::make_unique<fx::VocalVerbFx> (s);
            case ID_Shimmer:    return std::make_unique<fx::ShimmerFx> (s);
            case ID_Reverb:     return std::make_unique<fx::ReverbFx> (s);
            case ID_Delay:      return std::make_unique<fx::DelayFx> (s);
            case ID_BitBandit:  return std::make_unique<fx::BitBanditFx> (s);
            case ID_Gater:      return std::make_unique<fx::GaterFx> (s);
            case ID_Shaper:     return std::make_unique<fx::ShaperFx> (s);
            case ID_Flanger:    return std::make_unique<fx::FlangerFx> (s);
            case ID_Phaser:     return std::make_unique<fx::PhaserFx> (s);
            case ID_Tremolo:    return std::make_unique<fx::TremoloFx> (s);
            case ID_Distortion: return std::make_unique<fx::DistortionFx> (s);
            case ID_OneKnob:    return std::make_unique<fx::OneKnobFx> (s);
            case ID_PitchShifter:  return std::make_unique<fx::PitchShifterFx> (s);
            case ID_MultiShifter:  return std::make_unique<fx::MultiShifterFx> (s);
            case ID_DualOctaver:   return std::make_unique<fx::DualOctaverFx> (s);
            case ID_GuitarAmp:     return std::make_unique<fx::GuitarAmpFx> (s);
            case ID_GuitarCab:     return std::make_unique<fx::GuitarCabFx> (s);
            case ID_FeedbackKiller:return std::make_unique<fx::FeedbackKillerFx> (s);
            case ID_DeEsser:       return std::make_unique<fx::DeEsserFx> (s);
            case ID_Gain: return std::make_unique<fx::GainFx> (s);
            case ID_Pumper: return std::make_unique<fx::PumperFx> (s);
            case ID_AutoPan: return std::make_unique<fx::AutoPanFx> (s);
            case ID_AutoFilter: return std::make_unique<fx::AutoFilterFx> (s);
            case ID_AutoWah: return std::make_unique<fx::AutoWahFx> (s);
            case ID_Bitcrusher: return std::make_unique<fx::BitcrusherFx> (s);
            case ID_Driver: return std::make_unique<fx::DriverFx> (s);
            case ID_Exciter: return std::make_unique<fx::ExciterFx> (s);
            case ID_FetComp: return std::make_unique<fx::FetCompFx> (s);
            case ID_OptoComp: return std::make_unique<fx::OptoCompFx> (s);
            case ID_DelayPlus: return std::make_unique<fx::DelayPlusFx> (s);
            case ID_FilterEcho: return std::make_unique<fx::FilterEchoFx> (s);
            case ID_LegacyDelay: return std::make_unique<fx::DDelayPlusFx> (s);
            case ID_DDelay: return std::make_unique<fx::DDelayFx> (s);
            case ID_StereoSpreader: return std::make_unique<fx::StereoSpreaderFx> (s);
            case ID_SpringReverb: return std::make_unique<fx::SpringReverbFx> (s);
            case ID_MultiFilter: return std::make_unique<fx::MultiFilterFx> (s);
            case ID_EzEq: return std::make_unique<fx::EzEqFx> (s);
            case ID_GraphicEq: return std::make_unique<fx::GraphicEqFx> (s);
            case ID_VintageLimiter: return std::make_unique<fx::VintageLimiterFx> (s);
            case ID_Eq3M: return std::make_unique<fx::Eq3MFx> (s);
            case ID_GtrSqueeze: return std::make_unique<fx::GtrSqueezeFx> (s);
            case ID_FbkComp: return std::make_unique<fx::FbkCompFx> (s);
            default:            return nullptr;
        }
    }

    juce::String formatValue (float v, const juce::String& unit, int decimals, float scale)
    {
        const float d = v * scale;
        if (unit == "Hz" && std::abs (d) >= 1000.0f)
            return juce::String (d / 1000.0f, 2) + " kHz";
        if (unit.isEmpty())  return juce::String (d, decimals);
        if (unit == "%")     return juce::String (d, decimals) + "%";
        return juce::String (d, decimals) + " " + unit;
    }

    juce::String slotText (int type, int index, float norm)
    {
        const FxInfo* info = getFxInfo (type);
        if (info == nullptr || index >= (int) info->params.size())
            return juce::String (norm, 2);

        const auto& s = info->params[(size_t) index];
        const float real = realFromNorm (s, norm);
        switch (s.kind)
        {
            case ParamKind::Toggle: return real >= 0.5f ? "On" : "Off";
            case ParamKind::Choice: return s.choices[juce::jlimit (0, juce::jmax (0, s.choices.size() - 1), (int) real)];
            default:                return formatValue (real, s.unit, s.decimals, s.scale);
        }
    }

    float slotValueFromText (int type, int index, const juce::String& text)
    {
        const FxInfo* info = getFxInfo (type);
        const juce::String t = text.trim();
        if (info == nullptr || index >= (int) info->params.size())
            return juce::jlimit (0.0f, 1.0f, t.getFloatValue());

        const auto& s = info->params[(size_t) index];
        switch (s.kind)
        {
            case ParamKind::Toggle: return (t.equalsIgnoreCase ("on") || t.getIntValue() > 0) ? 1.0f : 0.0f;
            case ParamKind::Choice:
                for (int i = 0; i < s.choices.size(); ++i)
                    if (s.choices[i].equalsIgnoreCase (t)) return normFromReal (s, (float) i);
                return normFromReal (s, (float) t.getIntValue());
            default:
            {
                float v = t.getFloatValue();
                if (t.containsIgnoreCase ("k")) v *= 1000.0f;
                return normFromReal (s, juce::jlimit (s.minV, s.maxV, v / s.scale));
            }
        }
    }
}

ModularFXAudioProcessor::ModularFXAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      slotType (),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < kSlots; ++i)
    {
        onRaw[(size_t) i] = apvts.getRawParameterValue (slotOnId (i));
        fxView[(size_t) i].store (nullptr);
    }
    masterRaw[kBlend] = apvts.getRawParameterValue ("M_BLEND");
    masterRaw[kVol]   = apvts.getRawParameterValue ("M_VOL");
    masterRaw[kPan]   = apvts.getRawParameterValue ("M_PAN");
    masterRaw[kRev]   = apvts.getRawParameterValue ("M_REV");
    masterRaw[kByp]   = apvts.getRawParameterValue ("M_BYP");
    dryBuf.setSize (2, 512);
    preBuf.setSize (2, 512);

    // New instances start with an empty rack.
    commitRack ({}, true);
}

juce::AudioProcessorValueTreeState::ParameterLayout ModularFXAudioProcessor::createParameterLayout()
{
    for (auto& t : slotType) t.store (0);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotOnId (slot), 1 },
                                                                "Slot " + juce::String (slot + 1) + " On", true));

        std::atomic<int>* typeRef = &slotType[(size_t) slot];

        for (int i = 0; i < kSlotParams; ++i)
        {
            auto attrs = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([typeRef, i] (float v, int) { return slotText (typeRef->load(), i, v); })
                .withValueFromStringFunction ([typeRef, i] (const juce::String& t) { return slotValueFromText (typeRef->load(), i, t); });

            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotParamId (slot, i), 1 },
                                                                     "Slot " + juce::String (slot + 1) + " Param " + juce::String (i + 1),
                                                                     juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, attrs));
        }
    }
    // master section
    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto fromPct = [] (const juce::String& t) { return t.getFloatValue() / 100.0f; };
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "M_BLEND", 1 }, "Blend", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (fromPct)));

    juce::NormalisableRange<float> volRange (-60.0f, 6.0f, 0.1f);
    volRange.setSkewForCentre (-18.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "M_VOL", 1 }, "Volume", volRange, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? juce::String ("-inf") : juce::String (v, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.containsIgnoreCase ("inf") ? -60.0f : t.getFloatValue(); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "M_PAN", 1 }, "Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int n = juce::roundToInt (std::abs (v) * 100.0f);
            return n == 0 ? juce::String ("C") : (v < 0.0f ? "L " : "R ") + juce::String (n);
        })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "M_REV", 1 }, "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (fromPct)));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "M_BYP", 1 }, "Bypass Effects", false));
    return layout;
}

void ModularFXAudioProcessor::resetSlotParams (int slot, int type)
{
    const FxInfo* info = getFxInfo (type);
    if (auto* on = apvts.getParameter (slotOnId (slot)))
        on->setValueNotifyingHost (1.0f);

    for (int i = 0; i < kSlotParams; ++i)
        if (auto* p = apvts.getParameter (slotParamId (slot, i)))
            p->setValueNotifyingHost (info != nullptr && i < (int) info->params.size() ? defaultNorm (info->params[(size_t) i]) : 0.5f);
}

void ModularFXAudioProcessor::commitRack (const std::vector<RackEntry>& wantedIn, bool resetNewSlots)
{
    const juce::ScopedLock editGuard (editLock);

    // sanitise: valid, unique slots, valid types, at most kSlots
    std::vector<RackEntry> wanted;
    std::array<bool, kSlots> used {};
    for (auto e : wantedIn)
    {
        if (e.slot < 0 || e.slot >= kSlots || used[(size_t) e.slot]) continue;
        if (getFxInfo (e.type) == nullptr) continue;
        used[(size_t) e.slot] = true;
        wanted.push_back (e);
    }

    std::array<int, kSlots> newType {};
    for (auto& e : wanted) newType[(size_t) e.slot] = e.type;

    // build whatever is new OUTSIDE the audio lock (allocation), then swap pointers inside it
    std::array<std::unique_ptr<fx::Processor>, kSlots> fresh;
    std::array<bool, kSlots> replace {};
    for (int i = 0; i < kSlots; ++i)
    {
        const int cur = slotType[(size_t) i].load();
        if (newType[(size_t) i] != cur)
        {
            replace[(size_t) i] = true;
            if (newType[(size_t) i] != 0)
            {
                if (resetNewSlots) resetSlotParams (i, newType[(size_t) i]);
                slotType[(size_t) i].store (newType[(size_t) i]);       // text callbacks follow
                fresh[(size_t) i] = createEffect (newType[(size_t) i], apvts, i);
                if (fresh[(size_t) i] != nullptr)
                    fresh[(size_t) i]->prepare (currentSampleRate, currentBlockSize);
            }
            else
                slotType[(size_t) i].store (0);
        }
    }

    std::array<std::unique_ptr<fx::Processor>, kSlots> old;
    {
        const juce::SpinLock::ScopedLockType lock (audioLock);
        for (int i = 0; i < kSlots; ++i)
        {
            if (! replace[(size_t) i]) continue;
            old[(size_t) i] = std::move (slotFx[(size_t) i]);
            slotFx[(size_t) i] = std::move (fresh[(size_t) i]);
            wasRunning[(size_t) i] = false;
            fxView[(size_t) i].store (slotFx[(size_t) i].get());
        }
        audioCount = 0;
        for (auto& e : wanted)
            if (slotFx[(size_t) e.slot] != nullptr) audioOrder[(size_t) audioCount++] = e.slot;
    }
    old = {};      // freed here, never on the audio thread

    rack = wanted;

    juce::StringArray parts;
    for (auto& e : rack) parts.add (juce::String (e.slot) + ":" + juce::String (e.type));
    apvts.state.setProperty ("rack", parts.joinIntoString (","), nullptr);
}

int ModularFXAudioProcessor::addEffect (int type)
{
    std::vector<RackEntry> r;
    {
        const juce::ScopedLock g (editLock);
        if ((int) rack.size() >= kMaxRack || getFxInfo (type) == nullptr) return -1;
        r = rack;
    }
    int slot = -1;
    for (int i = 0; i < kSlots && slot < 0; ++i)
        if (slotType[(size_t) i].load() == 0) slot = i;
    if (slot < 0) return -1;

    r.push_back ({ slot, type });
    commitRack (r, true);
    return slot;
}

void ModularFXAudioProcessor::removeEffect (int slot)
{
    std::vector<RackEntry> r = getRack();
    r.erase (std::remove_if (r.begin(), r.end(), [slot] (const RackEntry& e) { return e.slot == slot; }), r.end());
    commitRack (r, false);
}

void ModularFXAudioProcessor::setSlotParam (int slot, const juce::String& specId, float realValue)
{
    const FxInfo* info = getFxInfo (getSlotType (slot));
    if (info == nullptr) return;
    const int idx = specIndex (*info, specId);
    if (idx < 0) return;
    if (auto* p = apvts.getParameter (slotParamId (slot, idx)))
        p->setValueNotifyingHost (normFromReal (info->params[(size_t) idx], realValue));
}

void ModularFXAudioProcessor::setRackOrder (const std::vector<int>& slots)
{
    const auto cur = getRack();
    std::vector<RackEntry> r;
    for (int s : slots)
        for (auto& e : cur)
            if (e.slot == s) { r.push_back (e); break; }
    // anything the caller forgot keeps its place at the end
    for (auto& e : cur)
    {
        bool found = false;
        for (auto& x : r) found = found || x.slot == e.slot;
        if (! found) r.push_back (e);
    }
    commitRack (r, false);
}

std::vector<ModularFXAudioProcessor::RackEntry> ModularFXAudioProcessor::getRack() const
{
    const juce::ScopedLock g (editLock);
    return rack;
}

int ModularFXAudioProcessor::getSlotType (int slot) const noexcept
{
    return (slot >= 0 && slot < kSlots) ? slotType[(size_t) slot].load() : 0;
}

const fx::Processor* ModularFXAudioProcessor::getFx (int slot) const noexcept
{
    return (slot >= 0 && slot < kSlots) ? fxView[(size_t) slot].load() : nullptr;
}

bool ModularFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut || mainIn.isDisabled();
}

void ModularFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType lock (audioLock);
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    for (auto& e : slotFx)
        if (e != nullptr) e->prepare (sampleRate, samplesPerBlock);
    wasRunning.fill (false);

    dryBuf.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
    preBuf.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
    reverbHolder.core.prepare (sampleRate);
    reverbHolder.core.setParams (0.55f, 0.55f, 1.0f);
    fx::initSmooth (blendSm, sampleRate, 0.02, masterRaw[kBlend]->load());
    fx::initSmooth (volSm, sampleRate, 0.02, masterRaw[kVol]->load() <= -59.9f ? 0.0f : fx::dbToGain (masterRaw[kVol]->load()));
    fx::initSmooth (panLSm, sampleRate, 0.02, 1.0f);
    fx::initSmooth (panRSm, sampleRate, 0.02, 1.0f);
    fx::initSmooth (revSm, sampleRate, 0.02, masterRaw[kRev]->load());
}

void ModularFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // input meters
    if (buffer.getNumChannels() > 0)
    {
        const float pl = buffer.getMagnitude (0, 0, buffer.getNumSamples());
        const float pr = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, buffer.getNumSamples()) : pl;
        if (pl > inPeakL.load (std::memory_order_relaxed)) inPeakL.store (pl, std::memory_order_relaxed);
        if (pr > inPeakR.load (std::memory_order_relaxed)) inPeakR.store (pr, std::memory_order_relaxed);
    }

    // host tempo / transport
    fx::Context ctx;
    ctx.sampleRate = currentSampleRate;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())          ctx.bpm = *bpm;
            ctx.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())  { ctx.ppq = *ppq; ctx.hasPpq = true; }
        }
    }

    // hosts may send blocks larger than announced: work through them in pieces that fit the dry buffer
    const int total = buffer.getNumSamples();
    const int cap = dryBuf.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int start = 0; start < total; start += cap)
    {
        const int n = juce::jmin (cap, total - start);
        float* ptrs[2] { nullptr, nullptr };
        for (int ch = 0; ch < numCh; ++ch) ptrs[ch] = buffer.getWritePointer (ch) + start;
        juce::AudioBuffer<float> sub (ptrs, numCh, n);

        auto c = ctx;
        if (ctx.hasPpq && start > 0) c.ppq = ctx.ppq + (double) start * ctx.bpm / 60.0 / juce::jmax (1.0, currentSampleRate);
        processChunk (sub, c);
    }

    // safety net: never hand NaN / runaway values to the host
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
            d[i] = std::isfinite (d[i]) ? juce::jlimit (-8.0f, 8.0f, d[i]) : 0.0f;
    }

    // output meters and the spectrum feed
    if (buffer.getNumChannels() > 0)
    {
        const float pl = buffer.getMagnitude (0, 0, total);
        const float pr = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, total) : pl;
        if (pl > outPeakL.load (std::memory_order_relaxed)) outPeakL.store (pl, std::memory_order_relaxed);
        if (pr > outPeakR.load (std::memory_order_relaxed)) outPeakR.store (pr, std::memory_order_relaxed);

        int w = scopeWrite.load (std::memory_order_relaxed);
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < total; ++i) { scopeBuf[(size_t) (w & (kScope - 1))] = 0.5f * (l[i] + r[i]); ++w; }
        scopeWrite.store (w & 0x3FFFFFFF, std::memory_order_release);
    }
}

void ModularFXAudioProcessor::processChunk (juce::AudioBuffer<float>& buf, const fx::Context& ctx)
{
    const int n = buf.getNumSamples();
    const int numCh = buf.getNumChannels();
    if (n == 0 || numCh == 0) return;

    const bool bypassed = masterRaw[kByp]->load (std::memory_order_relaxed) >= 0.5f;
    for (int ch = 0; ch < numCh; ++ch) dryBuf.copyFrom (ch, 0, buf, ch, 0, n);

    if (! bypassed)
    {
        // the UI only holds this lock for a pointer swap, so waiting here is at most a few microseconds
        const juce::SpinLock::ScopedLockType lock (audioLock);

        std::array<bool, kSlots> running {};
        for (int i = 0; i < audioCount; ++i)
        {
            const int slot = audioOrder[(size_t) i];
            auto* e = slotFx[(size_t) slot].get();
            auto* on = onRaw[(size_t) slot];
            if (e == nullptr || on == nullptr) continue;
            const bool wantOn = on->load (std::memory_order_relaxed) >= 0.5f;
            const bool was = wasRunning[(size_t) slot];
            if (! wantOn && ! was) continue;

            if (wantOn && ! was) e->reset();                  // fresh state when an effect is added or switched back on
            const bool fading = wantOn != was;                // switching on / off: cross-fade over this block instead of clicking
            if (fading)
                for (int ch = 0; ch < numCh; ++ch) preBuf.copyFrom (ch, 0, buf, ch, 0, n);

            e->process (buf, ctx);

            if (fading)
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* d = buf.getWritePointer (ch);
                    const float* pre = preBuf.getReadPointer (ch);
                    for (int i = 0; i < n; ++i)
                    {
                        const float w = (float) (i + 1) / (float) n;
                        const float k = wantOn ? w : 1.0f - w;
                        d[i] = pre[i] + (d[i] - pre[i]) * k;
                    }
                }
            running[(size_t) slot] = wantOn;
        }
        wasRunning = running;
    }
    else
        wasRunning.fill (false);

    // ---- master: blend, reverb send, pan, volume
    const float blendT = bypassed ? 0.0f : juce::jlimit (0.0f, 1.0f, masterRaw[kBlend]->load());
    const float volDb = masterRaw[kVol]->load();
    const float pan = juce::jlimit (-1.0f, 1.0f, masterRaw[kPan]->load());
    blendSm.setTargetValue (blendT);
    volSm.setTargetValue (volDb <= -59.9f ? 0.0f : fx::dbToGain (volDb));
    panLSm.setTargetValue (pan > 0.0f ? 1.0f - pan : 1.0f);
    panRSm.setTargetValue (pan < 0.0f ? 1.0f + pan : 1.0f);
    revSm.setTargetValue (juce::jlimit (0.0f, 1.0f, masterRaw[kRev]->load()));
    const bool reverbOn = masterRaw[kRev]->load() > 0.001f || revSm.isSmoothing();

    for (int i = 0; i < n; ++i)
    {
        const float b = blendSm.getNextValue(), v = volSm.getNextValue(), rv = revSm.getNextValue();
        const float gl = panLSm.getNextValue(), gr = panRSm.getNextValue();

        float l = dryBuf.getReadPointer (0)[i] * (1.0f - b) + buf.getReadPointer (0)[i] * b;
        float r = numCh > 1 ? dryBuf.getReadPointer (1)[i] * (1.0f - b) + buf.getReadPointer (1)[i] * b : l;

        if (reverbOn)
        {
            float wl = l, wr = r;
            reverbHolder.core.process (wl, wr);
            l += wl * rv * 0.6f;
            r += wr * rv * 0.6f;
        }

        buf.getWritePointer (0)[i] = l * v * (numCh > 1 ? gl : 1.0f);
        if (numCh > 1) buf.getWritePointer (1)[i] = r * v * gr;
    }
}

juce::AudioProcessorEditor* ModularFXAudioProcessor::createEditor()
{
    return new ModularFXAudioProcessorEditor (*this);
}

namespace
{
    std::unique_ptr<juce::XmlElement> snapshotToXml (const ModularFXAudioProcessor::RackSnapshot& s, const char* tag)
    {
        auto wrap = std::make_unique<juce::XmlElement> (tag);
        PresetDef d = s.rack;
        d.name = "snapshot";
        auto x = presetToXml (d);
        x->setAttribute ("blend", (double) s.blend);  x->setAttribute ("vol", (double) s.vol);
        x->setAttribute ("pan", (double) s.pan);      x->setAttribute ("rev", (double) s.rev);
        wrap->addChildElement (x.release());
        return wrap;
    }

    bool snapshotFromXml (const juce::XmlElement& wrap, ModularFXAudioProcessor::RackSnapshot& out)
    {
        auto* x = wrap.getChildByName ("ModularFXPreset");
        if (x == nullptr || ! presetFromXml (*x, out.rack)) return false;
        out.blend = (float) x->getDoubleAttribute ("blend", 1.0);  out.vol = (float) x->getDoubleAttribute ("vol", 0.0);
        out.pan = (float) x->getDoubleAttribute ("pan", 0.0);      out.rev = (float) x->getDoubleAttribute ("rev", 0.0);
        out.valid = true;
        return true;
    }
    constexpr int kSavedUndoSteps = 12;
}

void ModularFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("uiScale", (double) uiScale.load(), nullptr);
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;

    // A / B slots and the latest undo steps travel with the project
    abSlots[(size_t) activeAB] = captureSnapshot();
    auto* hist = xml->createNewChildElement ("MFXHistory");
    hist->setAttribute ("active", activeAB);
    for (int i = 0; i < 2; ++i)
        if (abSlots[(size_t) i].valid)
        {
            auto e = snapshotToXml (abSlots[(size_t) i], "AB");
            e->setAttribute ("slot", i);
            hist->addChildElement (e.release());
        }
    const size_t first = undoStack.size() > (size_t) kSavedUndoSteps ? undoStack.size() - (size_t) kSavedUndoSteps : 0;
    for (size_t i = first; i < undoStack.size(); ++i) hist->addChildElement (snapshotToXml (undoStack[i], "Undo").release());
    copyXmlToBinary (*xml, destData);
}

void ModularFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType())) return;

    // pull the A/B + undo history out before the rest becomes the parameter state
    std::array<RackSnapshot, 2> loadedAB;
    std::vector<RackSnapshot> loadedUndo;
    int loadedActive = 0;
    if (auto* hist = xmlState->getChildByName ("MFXHistory"))
    {
        loadedActive = juce::jlimit (0, 1, hist->getIntAttribute ("active", 0));
        for (auto* e : hist->getChildIterator())
        {
            RackSnapshot s;
            if (! snapshotFromXml (*e, s)) continue;
            if (e->hasTagName ("AB")) loadedAB[(size_t) juce::jlimit (0, 1, e->getIntAttribute ("slot", 0))] = s;
            else if (e->hasTagName ("Undo")) loadedUndo.push_back (s);
        }
        xmlState->removeChildElement (hist, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    if (apvts.state.hasProperty ("uiScale")) uiScale.store (juce::jlimit (0.5f, 1.5f, (float) (double) apvts.state.getProperty ("uiScale")));

    std::vector<RackEntry> entries;
    const juce::String savedRack = apvts.state.getProperty ("rack").toString();

    if (savedRack.isNotEmpty())
    {
        juce::StringArray parts;
        parts.addTokens (savedRack, ",", "");
        for (const auto& p : parts)
            entries.push_back ({ p.upToFirstOccurrenceOf (":", false, false).getIntValue(),
                                 p.fromFirstOccurrenceOf (":", false, false).getIntValue() });
    }
    else
    {
        // Project saved by an older version: one fixed slot per effect, controls stored under the old names.
        const juce::String saved = apvts.state.getProperty ("moduleOrder").toString();
        std::vector<int> ids;
        if (saved.isNotEmpty())
        {
            juce::StringArray parts;
            parts.addTokens (saved, ",", "");
            for (const auto& p : parts) ids.push_back (p.getIntValue());
        }
        else
            ids = { ID_Reverser, ID_NoiseGate, ID_Chorus };

        auto readOld = [this] (const juce::String& id, float fallback)
        {
            auto child = apvts.state.getChildWithProperty ("id", id);
            return child.isValid() ? (float) (double) child.getProperty ("value", (double) fallback) : fallback;
        };

        int slot = 0;
        for (int id : ids)
        {
            const FxInfo* info = getFxInfo (id);
            if (info == nullptr || slot >= kSlots) continue;

            for (size_t i = 0; i < info->params.size() && i < (size_t) kSlotParams; ++i)
                if (auto* p = apvts.getParameter (slotParamId (slot, (int) i)))
                    p->setValueNotifyingHost (normFromReal (info->params[i], readOld (info->params[i].id, info->params[i].defV)));

            if (auto* on = apvts.getParameter (slotOnId (slot)))
                on->setValueNotifyingHost (readOld (info->onId(), 1.0f) >= 0.5f ? 1.0f : 0.0f);

            entries.push_back ({ slot, id });
            ++slot;
        }

        // drop the old per-effect entries so they are not saved again
        for (int i = apvts.state.getNumChildren(); --i >= 0;)
        {
            auto child = apvts.state.getChild (i);
            if (child.hasProperty ("id") && apvts.getParameter (child["id"].toString()) == nullptr)
                apvts.state.removeChild (i, nullptr);
        }
    }

    commitRack (entries, false);
    rackChanged.sendChangeMessage();

    abSlots = loadedAB;
    activeAB = abSlots[(size_t) loadedActive].valid ? loadedActive : 0;
    undoStack = loadedUndo;
    redoStack.clear();
    committed = lastSeen = captureSnapshot();
    stableTicks = 0;
}

// ------------------------------------------------------------------------------------ presets
void ModularFXAudioProcessor::clearRack()
{
    commitRack ({}, false);
    apvts.state.setProperty ("preset", "", nullptr);
    rackChanged.sendChangeMessage();
}

juce::String ModularFXAudioProcessor::getPresetName() const
{
    return apvts.state.getProperty ("preset").toString();
}

std::vector<ModularFXAudioProcessor::PresetInfo> ModularFXAudioProcessor::getPresetList() const
{
    std::vector<PresetInfo> list;
    for (const auto& p : getFactoryPresets()) list.push_back ({ p.name, true, p.tags });

    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    std::vector<PresetInfo> mine;
    for (const auto& f : files)
        if (auto xml = juce::XmlDocument::parse (f))
            if (xml->hasTagName ("ModularFXPreset")) mine.push_back ({ xml->getStringAttribute ("name"), false, xml->getStringAttribute ("tags") });
    std::sort (mine.begin(), mine.end(), [] (const PresetInfo& a, const PresetInfo& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    for (auto& m : mine) list.push_back (m);
    return list;
}

PresetDef ModularFXAudioProcessor::readPreset (int index, bool& ok) const
{
    PresetDef def;
    ok = false;
    const auto& factory = getFactoryPresets();
    if (index >= 0 && index < (int) factory.size()) { ok = true; return factory[(size_t) index]; }

    const auto list = getPresetList();
    if (index < 0 || index >= (int) list.size()) return def;
    for (const auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
        if (auto xml = juce::XmlDocument::parse (f))
            if (xml->hasTagName ("ModularFXPreset") && xml->getStringAttribute ("name") == list[(size_t) index].name)
                ok = presetFromXml (*xml, def);
    return def;
}

bool ModularFXAudioProcessor::applyPreset (const PresetDef& def, bool setName)
{
    std::vector<RackEntry> entries;
    for (int i = 0; i < (int) def.fx.size() && i < kSlots; ++i)
    {
        const auto& f = def.fx[(size_t) i];
        if (auto* on = apvts.getParameter (slotOnId (i))) on->setValueNotifyingHost (f.on ? 1.0f : 0.0f);
        for (int j = 0; j < kSlotParams; ++j)
            if (auto* p = apvts.getParameter (slotParamId (i, j))) p->setValueNotifyingHost (f.norm[(size_t) j]);
        entries.push_back ({ i, f.type });
    }
    commitRack (entries, false);
    if (setName) apvts.state.setProperty ("preset", def.name, nullptr);
    rackChanged.sendChangeMessage();
    return true;
}

bool ModularFXAudioProcessor::loadPreset (int index)
{
    bool ok = false;
    const auto def = readPreset (index, ok);
    return ok && applyPreset (def);
}

bool ModularFXAudioProcessor::savePreset (const juce::String& nameIn, const juce::String& tags)
{
    const juce::String name = nameIn.trim();
    if (name.isEmpty()) return false;

    PresetDef def;
    def.name = name;
    def.tags = tags;
    for (const auto& e : getRack())
    {
        PresetFx f;
        f.type = e.type;
        f.on = onRaw[(size_t) e.slot]->load() >= 0.5f;
        for (int j = 0; j < kSlotParams; ++j)
            f.norm[(size_t) j] = apvts.getRawParameterValue (slotParamId (e.slot, j))->load();
        def.fx.push_back (f);
    }

    const auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".xml");
    const bool ok = presetToXml (def)->writeTo (file);
    if (ok) apvts.state.setProperty ("preset", name, nullptr);
    return ok;
}

bool ModularFXAudioProcessor::deletePreset (int index)
{
    const auto list = getPresetList();
    if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory) return false;
    bool removed = false;
    for (const auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
        if (auto xml = juce::XmlDocument::parse (f))
            if (xml->hasTagName ("ModularFXPreset") && xml->getStringAttribute ("name") == list[(size_t) index].name)
                removed = f.deleteFile() || removed;
    return removed;
}

bool ModularFXAudioProcessor::exportPreset (int index, const juce::File& file) const
{
    bool ok = false;
    const auto def = readPreset (index, ok);
    return ok && presetToXml (def)->writeTo (file);
}

int ModularFXAudioProcessor::importPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    PresetDef def;
    if (xml == nullptr || ! presetFromXml (*xml, def) || def.fx.empty()) return -1;

    return addUserPreset (std::move (def));
}

int ModularFXAudioProcessor::importBandLabJson (const juce::String& jsonText, juce::StringArray& notes)
{
    auto r = bandlab::fromJson (jsonText);
    notes = r.notes;
    if (! r.ok) return -1;
    return addUserPreset (std::move (r.def));
}

int ModularFXAudioProcessor::addUserPreset (PresetDef def)
{
    bool clash = false;                                                // never replace a factory preset
    for (const auto& p : getFactoryPresets()) clash = clash || p.name == def.name;
    if (clash) def.name += " (imported)";

    const auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return -1;
    if (! presetToXml (def)->writeTo (folder.getChildFile (juce::File::createLegalFileName (def.name) + ".xml"))) return -1;

    const auto list = getPresetList();
    for (int i = 0; i < (int) list.size(); ++i)
        if (! list[(size_t) i].factory && list[(size_t) i].name == def.name) { loadPreset (i); return i; }
    return -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModularFXAudioProcessor();
}


// ============================================================================ snapshots, undo / redo, A / B
bool ModularFXAudioProcessor::RackSnapshot::sameAs (const RackSnapshot& o) const
{
    if (valid != o.valid || rack.fx.size() != o.rack.fx.size()) return false;
    if (std::abs (blend - o.blend) > 1.0e-4f || std::abs (vol - o.vol) > 1.0e-3f || std::abs (pan - o.pan) > 1.0e-4f || std::abs (rev - o.rev) > 1.0e-4f) return false;
    for (size_t i = 0; i < rack.fx.size(); ++i)
    {
        const auto& a = rack.fx[i]; const auto& b = o.rack.fx[i];
        if (a.type != b.type || a.on != b.on) return false;
        for (size_t j = 0; j < a.norm.size(); ++j) if (std::abs (a.norm[j] - b.norm[j]) > 1.0e-5f) return false;
    }
    return true;
}

ModularFXAudioProcessor::RackSnapshot ModularFXAudioProcessor::captureSnapshot() const
{
    RackSnapshot s;
    for (const auto& e : getRack())
    {
        PresetFx f;
        f.type = e.type;
        f.on = onRaw[(size_t) e.slot]->load() >= 0.5f;
        for (int j = 0; j < kSlotParams; ++j) f.norm[(size_t) j] = apvts.getRawParameterValue (slotParamId (e.slot, j))->load();
        s.rack.fx.push_back (f);
    }
    s.blend = masterRaw[kBlend]->load();  s.vol = masterRaw[kVol]->load();
    s.pan = masterRaw[kPan]->load();      s.rev = masterRaw[kRev]->load();
    s.valid = true;
    return s;
}

void ModularFXAudioProcessor::applySnapshot (const RackSnapshot& s)
{
    if (! s.valid) return;
    applyPreset (s.rack, false);
    auto setReal = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setReal ("M_BLEND", s.blend);  setReal ("M_VOL", s.vol);  setReal ("M_PAN", s.pan);  setReal ("M_REV", s.rev);
}

void ModularFXAudioProcessor::updateHistory (bool force)
{
    const auto cur = captureSnapshot();
    if (! committed.valid) { committed = lastSeen = cur; return; }

    if (! cur.sameAs (lastSeen)) { lastSeen = cur; stableTicks = 0; }
    else ++stableTicks;

    // a change counts as one undo step once it has stopped moving for a moment (so a knob drag is one step)
    if (! lastSeen.sameAs (committed) && (force || stableTicks >= 3))
    {
        undoStack.push_back (committed);
        if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
        redoStack.clear();
        committed = lastSeen;
    }
}

void ModularFXAudioProcessor::undo()
{
    updateHistory (true);
    if (undoStack.empty()) return;
    redoStack.push_back (committed);
    const auto target = undoStack.back();
    undoStack.pop_back();
    applySnapshot (target);
    committed = lastSeen = captureSnapshot();
    stableTicks = 0;
}

void ModularFXAudioProcessor::redo()
{
    updateHistory (true);
    if (redoStack.empty()) return;
    undoStack.push_back (committed);
    const auto target = redoStack.back();
    redoStack.pop_back();
    applySnapshot (target);
    committed = lastSeen = captureSnapshot();
    stableTicks = 0;
}

void ModularFXAudioProcessor::switchAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeAB) return;
    updateHistory (true);
    abSlots[(size_t) activeAB] = captureSnapshot();
    if (! abSlots[(size_t) slot].valid) abSlots[(size_t) slot] = abSlots[(size_t) activeAB];    // first visit: start from the same sound
    applySnapshot (abSlots[(size_t) slot]);
    activeAB = slot;
}
