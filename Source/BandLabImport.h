#pragma once
#include <JuceHeader.h>
#include "Presets.h"

// Imports BandLab effect presets (the JSON a BandLab preset consists of) into a PresetDef.
// BandLab units: times in seconds, percentages 0..100 (delay) or 0..1 (spreader), dB and Hz as they are.
namespace bandlab
{
    struct ImportResult
    {
        PresetDef def;
        juce::StringArray notes;          // things that were skipped or approximated
        bool ok = false;
    };

    inline float num (const juce::var& params, const char* key, float fallback)
    {
        if (auto* o = params.getDynamicObject())
            if (o->hasProperty (key)) return (float) (double) o->getProperty (key);
        return fallback;
    }

    // Accepts the preset object itself, or an API response that wraps it ({"preset":{...}} / {"data":{...}}).
    inline juce::var findPresetObject (const juce::var& v)
    {
        if (auto* o = v.getDynamicObject())
        {
            if (o->hasProperty ("effects")) return v;
            for (auto& nv : o->getProperties())
            {
                auto r = findPresetObject (nv.value);
                if (r.isObject()) return r;
            }
        }
        return {};
    }

    inline ImportResult fromJson (const juce::String& text)
    {
        ImportResult res;
        const auto parsed = juce::JSON::parse (text.trim());
        const auto root = findPresetObject (parsed);
        auto* obj = root.getDynamicObject();
        if (obj == nullptr || ! obj->getProperty ("effects").isArray()) return res;

        res.def.name = obj->getProperty ("displayName").toString().trim();
        if (res.def.name.isEmpty()) res.def.name = obj->getProperty ("name").toString().trim();      // the Skelly extractor's export uses "name"
        if (res.def.name.isEmpty()) res.def.name = "BandLab preset";
        res.def.tags = "Imported";

        auto& list = *obj->getProperty ("effects").getArray();
        for (const auto& e : list)
        {
            const auto slug = e.getProperty ("slug", {}).toString();
            const auto p = e.getProperty ("params", {});
            const bool on = ! (bool) e.getProperty ("bypass", false);
            auto n = [&] (const char* k, float d) { return num (p, k, d); };

            PresetFx fx;
            bool known = true;
            if (slug == "compressor")
                fx = makePresetFx (ID_Compressor, { { "COMP_ATTACK", n ("attack", 0.01f) * 1000.0f }, { "COMP_RELEASE", n ("release", 0.12f) * 1000.0f },
                                                    { "COMP_THRESH", n ("threshold", -18.0f) }, { "COMP_RATIO", n ("ratio", 4.0f) }, { "COMP_KNEE", n ("knee", 6.0f) } });
            else if (slug == "expGate")
                fx = makePresetFx (ID_NoiseGate, { { "GATE_THRESH", n ("threshold", -48.0f) }, { "GATE_ATTACK", n ("attack", 0.005f) * 1000.0f }, { "GATE_RELEASE", n ("release", 0.2f) * 1000.0f } });
            else if (slug == "deEsser")
                fx = makePresetFx (ID_DeEsser, { { "DS_THRESH", n ("threshold", -19.0f) }, { "DS_FREQ", n ("frequency", 9000.0f) } });
            else if (slug == "threeBandEq")
            {
                const float mf = std::clamp (n ("midFreq", 1000.0f), 250.0f, 4000.0f);
                fx = makePresetFx (ID_Eq3M, { { "EQ3_LOW", n ("lowGain", 0.0f) }, { "EQ3_MID", n ("midGain", 0.0f) }, { "EQ3_HIGH", n ("highGain", 0.0f) },
                                              { "EQ3_SWEEP", std::log (mf / 250.0f) / std::log (16.0f) } });
            }
            else if (slug == "ddelay")
                fx = makePresetFx (ID_DDelay, { { "DD_TIME", n ("time", 0.3f) * 1000.0f }, { "DD_DRY", n ("dry", 100.0f) / 100.0f }, { "DD_WET", n ("wet", 30.0f) / 100.0f },
                                                { "DD_LEVEL", n ("level", 0.0f) }, { "DD_REGEN", n ("regen", 30.0f) / 100.0f },
                                                { "DD_LPF", n ("lpf", 20000.0f) }, { "DD_HPF", n ("hpf", 20.0f) } });
            else if (slug == "bossGE7")
                fx = makePresetFx (ID_GraphicEq, { { "GEQ_1", n ("gainAt100Hz", 0.0f) }, { "GEQ_2", n ("gainAt200Hz", 0.0f) }, { "GEQ_3", n ("gainAt400Hz", 0.0f) },
                                                   { "GEQ_4", n ("gainAt800Hz", 0.0f) }, { "GEQ_5", n ("gainAt1600Hz", 0.0f) }, { "GEQ_6", n ("gainAt3200Hz", 0.0f) },
                                                   { "GEQ_7", n ("gainAt6400Hz", 0.0f) }, { "GEQ_LEVEL", n ("level", 0.0f) } });
            else if (slug == "stereoSpreader")
                fx = makePresetFx (ID_StereoSpreader, { { "SSP_LOW", n ("lowerIntensity", 0.5f) }, { "SSP_UPPER", n ("upperIntensity", 0.5f) }, { "SSP_FREQ", n ("upperFrequency", 632.0f) } });
            else if (slug == "simpleStudioReverb")      // BandLab knobs run 0..10; colour (bright) maps to less damping
            {
                fx = makePresetFx (ID_Reverb, { { "RV_MIX", n ("mix", 3.0f) / 10.0f }, { "RV_SIZE", n ("size", 5.0f) / 10.0f }, { "RV_DAMP", 1.0f - n ("color", 5.0f) / 10.0f } });
                res.notes.add ("Studio Reverb was approximated with Reverb (size, mix and colour carried over)");
            }
            else if (slug == "pingpongDelay" || slug == "delay")
            {
                const float cutoff = std::clamp (n ("cutoff", 8000.0f), 500.0f, 20000.0f);
                const float sec = n (slug == "delay" ? "delayTime" : "delayTimeSec", 0.35f);
                fx = makePresetFx (ID_Delay, { { "DLY_TIME", sec * 1000.0f }, { "DLY_FB", n ("feedback", 0.3f) }, { "DLY_TONE", std::log (cutoff / 500.0f) / std::log (40.0f) },
                                               { "DLY_MIX", n ("dryWetMix", 0.2f) }, { "DLY_PING", slug == "pingpongDelay" ? 1.0f : 0.0f } });
                if (p.getProperty ("delayUnitSelection", {}).toString().contains ("Beats"))
                    res.notes.add ("Delay was tempo-synced in BandLab; imported as its fixed time of " + juce::String (juce::roundToInt (sec * 1000.0f)) + " ms");
            }
            else if (slug == "gain" || slug == "gain2")
                fx = makePresetFx (ID_Gain, { { "GAIN_DB", n ("gain", 0.0f) } });
            else
                known = false;

            if (! known) { res.notes.add ("Skipped \"" + slug + "\" (no matching effect yet)"); continue; }
            if ((int) res.def.fx.size() >= kMaxRack) { res.notes.add ("Skipped \"" + slug + "\" (the rack holds " + juce::String (kMaxRack) + " effects)"); continue; }
            fx.on = on;
            res.def.fx.push_back (fx);
        }

        res.ok = ! res.def.fx.empty();
        return res;
    }

    // Pulls the preset id out of a BandLab preset link, e.g. .../effect-presets/<user>-<preset>-<rev>-user
    inline juce::String presetIdFromLink (const juce::String& link)
    {
        auto tail = link.trim().fromLastOccurrenceOf ("/effect-presets/", false, true).upToFirstOccurrenceOf ("?", false, false);
        auto parts = juce::StringArray::fromTokens (tail, "-", "");
        if (parts.size() < 2 || parts[1].length() != 32) return {};
        const auto h = parts[1];
        return h.substring (0, 8) + "-" + h.substring (8, 12) + "-" + h.substring (12, 16) + "-" + h.substring (16, 20) + "-" + h.substring (20);
    }

    inline bool looksLikeLink (const juce::String& s) { return s.trim().startsWithIgnoreCase ("http") && s.containsIgnoreCase ("effect-presets"); }

    // Best-effort download of a public preset. Blocking: call from a background thread. BandLab does not document
    // this endpoint, so a failure just returns an empty string and the UI explains how to import the JSON by hand.
    inline juce::String fetchPresetJson (const juce::String& link)
    {
        const auto id = presetIdFromLink (link);
        if (id.isEmpty()) return {};
        const char* endpoints[] = { "https://www.bandlab.com/api/v1.3/effect-presets/", "https://www.bandlab.com/api/v1.2/effect-presets/" };
        for (auto* ep : endpoints)
        {
            juce::URL url (juce::String (ep) + id);
            if (auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                     .withConnectionTimeoutMs (6000).withExtraHeaders ("Accept: application/json\r\n")))
            {
                const auto text = in->readEntireStreamAsString();
                if (fromJson (text).ok) return text;
            }
        }
        return {};
    }
}
