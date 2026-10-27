#pragma once
#include <JuceHeader.h>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>
#include "FxRegistry.h"

// A preset is a rack: which effects, in which order, with which control positions (0..1 slot values).
struct PresetFx
{
    int  type = 0;
    bool on = true;
    std::array<float, kSlotParams> norm {};
};

struct PresetDef
{
    juce::String name;
    std::vector<PresetFx> fx;
};

// Start from the effect's defaults and override the named controls with real values (Hz, dB, %, choice index ...).
inline PresetFx makePresetFx (int type, std::initializer_list<std::pair<const char*, float>> values = {})
{
    PresetFx f;
    f.type = type;
    f.norm.fill (0.5f);
    if (const FxInfo* info = getFxInfo (type))
    {
        for (size_t i = 0; i < info->params.size() && i < (size_t) kSlotParams; ++i)
            f.norm[i] = defaultNorm (info->params[i]);
        for (const auto& v : values)
        {
            const int idx = specIndex (*info, v.first);
            jassert (idx >= 0);
            if (idx >= 0 && idx < kSlotParams) f.norm[(size_t) idx] = normFromReal (info->params[(size_t) idx], v.second);
        }
    }
    return f;
}

inline const std::vector<PresetDef>& getFactoryPresets()
{
    static const std::vector<PresetDef> presets = []
    {
        std::vector<PresetDef> p;
        p.push_back ({ "Vocal Polish", {
            makePresetFx (ID_DeEsser,    { { "DS_THRESH", -32.0f } }),
            makePresetFx (ID_Compressor, { { "COMP_THRESH", -22.0f }, { "COMP_RATIO", 3.0f } }),
            makePresetFx (ID_VisualEQ,   { { "EQ_HPF", 90.0f }, { "EQ_LG", -1.0f }, { "EQ_HF", 9000.0f }, { "EQ_HG", 2.5f } }),
            makePresetFx (ID_Reverb,     { { "RV_MIX", 0.15f }, { "RV_SIZE", 0.4f } }) } });

        p.push_back ({ "Radio Voice", {
            makePresetFx (ID_VisualEQ,    { { "EQ_HPF", 350.0f }, { "EQ_MF", 2200.0f }, { "EQ_MG", 6.0f }, { "EQ_HF", 6000.0f }, { "EQ_HG", -8.0f } }),
            makePresetFx (ID_Distortion,  { { "DST_MODE", 0.0f }, { "DST_DRIVE", 0.25f }, { "DST_LEVEL", -6.0f }, { "DST_MIX", 0.5f } }),
            makePresetFx (ID_Compressor,  { { "COMP_THRESH", -26.0f }, { "COMP_RATIO", 6.0f } }) } });

        p.push_back ({ "Crunch Guitar", {
            makePresetFx (ID_GuitarAmp, { { "AMP_TYPE", 1.0f }, { "AMP_GAIN", 0.6f } }),
            makePresetFx (ID_GuitarCab, { { "CAB_TYPE", 2.0f } }),
            makePresetFx (ID_Delay,     { { "DLY_TIME", 320.0f }, { "DLY_MIX", 0.18f }, { "DLY_FB", 0.3f } }) } });

        p.push_back ({ "Wide Chorus Pad", {
            makePresetFx (ID_Chorus,  { { "CHO_DEPTH", 0.7f }, { "CHO_AMOUNT", 0.6f } }),
            makePresetFx (ID_Delay,   { { "DLY_PING", 1.0f }, { "DLY_FB", 0.45f }, { "DLY_MIX", 0.3f } }),
            makePresetFx (ID_Shimmer, { { "SH_MIX", 0.3f } }) } });

        p.push_back ({ "Lo-Fi Tape", {
            makePresetFx (ID_BitBandit, { { "BIT_BITS", 10.0f }, { "BIT_RATE", 16000.0f } }),
            makePresetFx (ID_VisualEQ,  { { "EQ_HPF", 120.0f }, { "EQ_HF", 5000.0f }, { "EQ_HG", -6.0f } }),
            makePresetFx (ID_Tremolo,   { { "TRM_RATE", 4.5f }, { "TRM_DEPTH", 0.3f } }) } });

        p.push_back ({ "Octave Glow", {
            makePresetFx (ID_DualOctaver, { { "OCT_UP", 0.4f }, { "OCT_DOWN", 0.5f } }),
            makePresetFx (ID_Reverb,      { { "RV_MIX", 0.3f }, { "RV_SIZE", 0.7f } }) } });

        p.push_back ({ "Drum Punch", {
            makePresetFx (ID_Shaper,     { { "SHP_TRANS", 60.0f }, { "SHP_SUST", -20.0f } }),
            makePresetFx (ID_Compressor, { { "COMP_THRESH", -18.0f }, { "COMP_RATIO", 4.0f }, { "COMP_ATTACK", 25.0f } }),
            makePresetFx (ID_Limiter,    { { "LIM_GAIN", 3.0f } }) } });

        p.push_back ({ "Clean Master", {
            makePresetFx (ID_MultiBand, {}),
            makePresetFx (ID_Limiter,   { { "LIM_GAIN", 2.0f }, { "LIM_CEIL", -0.5f } }) } });

        p.push_back ({ "Spring Surf Guitar", {
            makePresetFx (ID_GtrSqueeze,   { { "GSQ_SUSTAIN", 0.45f } }),
            makePresetFx (ID_GuitarAmp,    { { "AMP_TYPE", 0.0f }, { "AMP_GAIN", 0.35f } }),
            makePresetFx (ID_SpringReverb, { { "SPR_AMOUNT", 0.7f }, { "SPR_TYPE", 0.0f } }),
            makePresetFx (ID_Tremolo,      { { "TRM_RATE", 5.0f }, { "TRM_DEPTH", 0.4f } }) } });

        p.push_back ({ "Vintage Vocal", {
            makePresetFx (ID_FetComp,    { { "FET_INPUT", 16.0f }, { "FET_RATIO", 1.0f }, { "FET_ATTACK", 0.3f } }),
            makePresetFx (ID_OptoComp,   { { "OPT_PEAK", 0.4f }, { "OPT_GAIN", 5.0f } }),
            makePresetFx (ID_Eq3M,       { { "EQ3_LOW", -2.0f }, { "EQ3_MID", 2.0f }, { "EQ3_HIGH", 3.0f }, { "EQ3_SWEEP", 0.55f } }),
            makePresetFx (ID_DeEsser,    { { "DS_THRESH", -26.0f } }),
            makePresetFx (ID_SpringReverb, { { "SPR_AMOUNT", 0.25f }, { "SPR_TYPE", 1.0f } }) } });

        p.push_back ({ "Telephone", {
            makePresetFx (ID_EzEq,       { { "EZ_HP", 450.0f }, { "EZ_LP", 3400.0f }, { "EZ_GAIN", 5.0f }, { "EZ_FREQ", 1500.0f }, { "EZ_WIDTH", 1.5f } }),
            makePresetFx (ID_Driver,     {}),
            makePresetFx (ID_VintageLimiter, { { "VL_GAIN", 8.0f }, { "VL_ENHANCE", 0.4f } }) } });

        p.push_back ({ "Filter Sweep Echo", {
            makePresetFx (ID_MultiFilter, { { "MF_TYPE", 2.0f }, { "MF_FREQ", 1800.0f }, { "MF_RES", 2.5f } }),
            makePresetFx (ID_FilterEcho,  { { "FEC_DIV", 2.0f }, { "FEC_FB", 0.5f }, { "FEC_FREQ", 3000.0f }, { "FEC_MIX", 0.45f } }),
            makePresetFx (ID_Pumper,      { { "PMP_AMOUNT", 0.5f } }) } });

        p.push_back ({ "Dub Delay", {
            makePresetFx (ID_LegacyDelay, { { "DDX_TIME", 3.0f }, { "DDX_FB", 0.6f }, { "DDX_HP", 300.0f }, { "DDX_LP", 4500.0f }, { "DDX_SPREAD", 0.8f }, { "DDX_WET", 0.6f } }),
            makePresetFx (ID_SpringReverb, { { "SPR_AMOUNT", 0.5f }, { "SPR_TYPE", 2.0f } }) } });

        p.push_back ({ "Bright Mix Bus", {
            makePresetFx (ID_GraphicEq,   { { "GEQ_1", 1.5f }, { "GEQ_3", -1.5f }, { "GEQ_6", 1.5f }, { "GEQ_7", 3.0f } }),
            makePresetFx (ID_FbkComp,     { { "FBK_THRESH", -16.0f }, { "FBK_RATIO", 2.5f }, { "FBK_GAIN", 2.0f } }),
            makePresetFx (ID_VintageLimiter, { { "VL_GAIN", 3.0f }, { "VL_ENHANCE", 0.2f } }) } });

        p.push_back ({ "Harmony Stack", {
            makePresetFx (ID_MultiShifter, { { "MS_FIFTHUP", 0.4f }, { "MS_OCTDN", 0.3f }, { "MS_FOURTHUP", 0.25f } }),
            makePresetFx (ID_Reverb,       { { "RV_MIX", 0.25f }, { "RV_SIZE", 0.6f } }) } });

        p.push_back ({ "Clean Up Podcast", {
            makePresetFx (ID_NoiseGate,   { { "GATE_THRESH", -45.0f } }),
            makePresetFx (ID_FeedbackKiller, { { "FK_AMOUNT", 0.5f } }),
            makePresetFx (ID_EzEq,        { { "EZ_HP", 80.0f } }),
            makePresetFx (ID_Compressor,  { { "COMP_THRESH", -20.0f }, { "COMP_RATIO", 3.0f } }),
            makePresetFx (ID_DeEsser,     {}) } });
        return p;
    }();
    return presets;
}

// ---- user presets live as small XML files
inline juce::File getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("ModularFXSuite").getChildFile ("Presets");
}

inline std::unique_ptr<juce::XmlElement> presetToXml (const PresetDef& def)
{
    auto xml = std::make_unique<juce::XmlElement> ("ModularFXPreset");
    xml->setAttribute ("name", def.name);
    for (const auto& f : def.fx)
    {
        auto* e = xml->createNewChildElement ("FX");
        e->setAttribute ("type", f.type);
        e->setAttribute ("on", f.on ? 1 : 0);
        for (int i = 0; i < kSlotParams; ++i) e->setAttribute ("p" + juce::String (i), (double) f.norm[(size_t) i]);
    }
    return xml;
}

inline bool presetFromXml (const juce::XmlElement& xml, PresetDef& out)
{
    if (! xml.hasTagName ("ModularFXPreset")) return false;
    out.name = xml.getStringAttribute ("name");
    out.fx.clear();
    for (auto* e : xml.getChildWithTagNameIterator ("FX"))
    {
        PresetFx f;
        f.type = e->getIntAttribute ("type");
        if (getFxInfo (f.type) == nullptr) continue;
        f.on = e->getIntAttribute ("on", 1) != 0;
        for (int i = 0; i < kSlotParams; ++i)
            f.norm[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) e->getDoubleAttribute ("p" + juce::String (i), 0.5));
        out.fx.push_back (f);
        if ((int) out.fx.size() >= kMaxRack) break;
    }
    return out.name.isNotEmpty();
}
