#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Modules/DSPModules.h"
#include "Modules/NewEffects.h"

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

    // Default rack: the original three effects.
    commitRack ({ { 0, ID_Reverser }, { 1, ID_NoiseGate }, { 2, ID_Chorus } }, true);
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
}

void ModularFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

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

    {
        // the UI only holds this lock for a pointer swap, so waiting here is at most a few microseconds
        const juce::SpinLock::ScopedLockType lock (audioLock);

        std::array<bool, kSlots> running {};
        for (int i = 0; i < audioCount; ++i)
        {
            const int slot = audioOrder[(size_t) i];
            auto* e = slotFx[(size_t) slot].get();
            auto* on = onRaw[(size_t) slot];
            if (e == nullptr || on == nullptr || on->load (std::memory_order_relaxed) < 0.5f) continue;

            if (! wasRunning[(size_t) slot]) e->reset();      // fresh state when an effect is added or switched back on
            e->process (buffer, ctx);
            running[(size_t) slot] = true;
        }
        wasRunning = running;
    }

    // safety net: never hand NaN / runaway values to the host
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = std::isfinite (d[i]) ? juce::jlimit (-8.0f, 8.0f, d[i]) : 0.0f;
    }
}

juce::AudioProcessorEditor* ModularFXAudioProcessor::createEditor()
{
    return new ModularFXAudioProcessorEditor (*this);
}

void ModularFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void ModularFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType())) return;

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

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
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModularFXAudioProcessor();
}
