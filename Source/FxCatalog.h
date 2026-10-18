#pragma once
#include <JuceHeader.h>
#include <utility>
#include <vector>
#include "FxRegistry.h"

// How effects are grouped and described in the "Add Effect" browser.
struct FxCategory { juce::String name, blurb; };

// One line in the browser.  An effect can be listed in several categories, and one effect can be offered in
// several flavours (the One Knobs styles): `initial` holds the control values the new effect starts with.
struct FxCatalogEntry
{
    int id = 0;
    juce::String category, tagline;
    juce::String name;                       // empty = the effect's own name
    juce::uint32 tint = 0;                   // thumbnail colour for the One Knob styles
    std::vector<std::pair<juce::String, float>> initial;
};

inline const std::vector<FxCategory>& getFxCategories()
{
    static const std::vector<FxCategory> c = {
        { "Recommended", "Our top picks to get you started." },
        { "One Knobs",   "Get studio quality sound quickly with these powerful One Knob effects." },
        { "Polish",      "Eliminate unwanted background noise, such as hiss or hum, for a cleaner sound." },
        { "Amps & Cabs", "Recreate the tone of guitar amplifiers and speaker cabinets for a rich, authentic sound." },
        { "Pitch Shift", "Effects for altering and shifting pitch creatively." },
        { "Compressor",  "Manage your sound's volume to keep it smooth and balanced." },
        { "Limiter",     "Tame sharp and sudden volume changes." },
        { "Rhythmic",    "Add movement to your track with tempo-synced effects." },
        { "EQ",          "Shape your tone by emphasizing or reducing specific frequencies." },
        { "Delay",       "Create echoes or repeats to add depth and rhythm to your sound." },
        { "Modulation",  "Introduce motion or texture with effects like chorus or flanger." },
        { "Distortion",  "Add grit or edge for a more powerful, aggressive sound." },
        { "Saturation",  "Enhance warmth and fullness with subtle \"natural\" distortion." },
        { "Reverb",      "Recreate the ambiance of spaces like rooms, halls, or churches for added depth." },
        { "Utility",     "Essential effects for enhancing and shaping sound." },
        { "Legacy",      "Outdated effects nearing deprecation." },
    };
    return c;
}

inline const juce::String& oneKnobStyleName (int mode)
{
    static const juce::StringArray names { "Brighter", "Echo", "Punchy", "Room", "Toasty", "Bassy", "Leveller", "Dirty" };
    static const juce::String none;
    return mode >= 0 && mode < names.size() ? names.getReference (mode) : none;
}

// Display order inside each category follows this table.
inline const std::vector<FxCatalogEntry>& getFxCatalog()
{
    static const std::vector<FxCatalogEntry> e = []
    {
        auto one = [] (const char* cat, int mode, const char* tagline, juce::uint32 tint)
        {
            return FxCatalogEntry { ID_OneKnob, cat, tagline, oneKnobStyleName (mode), tint, { { "ONE_MODE", (float) mode } } };
        };
        auto plain = [] (int id, const char* cat, const char* tagline) { return FxCatalogEntry { id, cat, tagline, {}, 0, {} }; };

        std::vector<FxCatalogEntry> v;

        // Recommended
        v.push_back (plain (ID_VisualEQ,   "Recommended", "EQ with a real-time curve display."));
        v.push_back (plain (ID_Compressor, "Recommended", "Evens out the level of loud and quiet parts."));
        v.push_back (plain (ID_Limiter,    "Recommended", "Transparent limiter for studio-grade sound."));
        v.push_back (plain (ID_VocalVerb,  "Recommended", "Clean reverb for an uncolored ambiance."));
        v.push_back (plain (ID_Delay,      "Recommended", "Echoes in free time or synced to the tempo."));
        v.push_back (plain (ID_DeEsser,    "Recommended", "Helps eliminate excess sibilance (\"ess\") sounds."));

        // One Knobs
        v.push_back (one ("One Knobs", 0, "Boosts highs, preserves natural tone.",         0xFFFFB703));
        v.push_back (one ("One Knobs", 1, "Clean delay for space and presence.",           0xFF2F9BFF));
        v.push_back (one ("One Knobs", 2, "Tight, energized sound with clarity.",          0xFFFF3B1D));
        v.push_back (one ("One Knobs", 3, "Natural reverb for space and clarity.",         0xFF0FCB78));
        v.push_back (one ("One Knobs", 4, "Tape-style warmth and saturation.",             0xFFD9531E));
        v.push_back (one ("One Knobs", 5, "Boosts lows, keeps tone full and warm.",        0xFFA600FF));
        v.push_back (one ("One Knobs", 6, "Balanced compression, natural sound.",          0xFFFF7A18));
        v.push_back (one ("One Knobs", 7, "Overdriven sound with gritty character.",       0xFFC98A0E));

        // Polish
        v.push_back (plain (ID_FeedbackKiller, "Polish", "Detects any loud feedback and suppresses it."));
        v.push_back (plain (ID_DeEsser,        "Polish", "Helps eliminate excess sibilance (\"ess\") sounds."));
        v.push_back (plain (ID_NoiseGate,      "Polish", "Allows signal above a threshold to pass through."));

        // Amps & Cabs
        v.push_back (plain (ID_GuitarAmp, "Amps & Cabs", "Guitar amplifier with 4 basic amp types."));
        v.push_back (plain (ID_GuitarCab, "Amps & Cabs", "Models of popular guitar cabinets."));

        // Pitch Shift
        v.push_back (plain (ID_MultiShifter, "Pitch Shift", "Mix multiple pitch-shifted versions of a signal."));
        v.push_back (plain (ID_PitchShifter, "Pitch Shift", "Simple pitch shifting effect."));
        v.push_back (plain (ID_DualOctaver,  "Pitch Shift", "Two octaves to increase or decrease frequency."));

        // Compressor
        v.push_back (one   ("Compressor", 6, "Balanced compression, natural sound.",        0xFFFF7A18));
        v.push_back (one   ("Compressor", 2, "Tight, energized sound with clarity.",        0xFFFF3B1D));
        v.push_back (plain (ID_MultiBand,  "Compressor", "Precise control over specific frequency ranges."));
        v.push_back (plain (ID_Compressor, "Compressor", "Evens out the level of loud and quiet parts."));
        v.push_back (plain (ID_Shaper,     "Compressor", "Reshapes the attack and sustain of a sound."));

        // Limiter
        v.push_back (plain (ID_Limiter,   "Limiter", "Transparent limiter for studio-grade sound."));
        v.push_back (plain (ID_FetComp,   "Limiter", "Classic limiting amplifier using FET compression."));
        v.push_back (plain (ID_OptoComp,  "Limiter", "Emulation of a famous optical compressor."));

        // Rhythmic
        v.push_back (plain (ID_Pumper,   "Rhythmic", "Simulated side-chain compression."));
        v.push_back (plain (ID_Gater,    "Rhythmic", "Create rhythmic, stutter-like patterns."));
        v.push_back (plain (ID_Tremolo,  "Rhythmic", "Wobbles the volume, or pans left and right."));
        v.push_back (plain (ID_Reverser, "Rhythmic", "Plays short slices of audio backwards."));

        // EQ
        v.push_back (one   ("EQ", 5, "Boosts lows, keeps tone full and warm.",   0xFFA600FF));
        v.push_back (one   ("EQ", 0, "Boosts highs, preserves natural tone.",    0xFFFFB703));
        v.push_back (plain (ID_VisualEQ, "EQ", "EQ with a real-time curve display."));

        // Delay
        v.push_back (one   ("Delay", 1, "Clean delay for space and presence.", 0xFF2F9BFF));
        v.push_back (plain (ID_DelayPlus,   "Delay", "Create atmospheric and syncopated delay patterns."));
        v.push_back (plain (ID_FilterEcho,  "Delay", "Delays audio signal in musical time, with a filter on every repeat."));
        v.push_back (plain (ID_Delay,       "Delay", "Echoes in free time or synced to the tempo."));

        // Modulation
        v.push_back (plain (ID_AutoFilter, "Modulation", "Creates moving filter effects."));
        v.push_back (plain (ID_AutoPan,    "Modulation", "Auto panning between left and right channels."));
        v.push_back (plain (ID_AutoWah,    "Modulation", "Alters the audio signal to create a wah-wah sound."));
        v.push_back (plain (ID_Chorus,     "Modulation", "Lush ensemble of detuned, modulated voices."));
        v.push_back (plain (ID_Flanger,    "Modulation", "Jet-plane sweeping comb filter."));
        v.push_back (plain (ID_Phaser,     "Modulation", "Swirling notches swept through the sound."));

        // Distortion
        v.push_back (plain (ID_BitBandit,  "Distortion", "Emulate vintage sampler and synth tones for classic sound."));
        v.push_back (plain (ID_Bitcrusher, "Distortion", "Downsample signals with customisable bit-depth."));
        v.push_back (plain (ID_Driver,     "Distortion", "Enhances or adds grit to clean signal."));
        v.push_back (plain (ID_Distortion, "Distortion", "Overdrive, distortion and fuzz."));

        // Saturation
        v.push_back (one   ("Saturation", 7, "Overdriven sound with gritty character.", 0xFFC98A0E));
        v.push_back (one   ("Saturation", 4, "Tape-style warmth and saturation.",       0xFFD9531E));
        v.push_back (plain (ID_Exciter, "Saturation", "Enhance a signal using subtle harmonic excitation."));

        // Reverb
        v.push_back (one   ("Reverb", 3, "Natural reverb for space and clarity.", 0xFF0FCB78));
        v.push_back (plain (ID_Reverb,    "Reverb", "Adds a room, hall or plate-style space."));
        v.push_back (plain (ID_VocalVerb, "Reverb", "Clean reverb for an uncolored ambiance."));
        v.push_back (plain (ID_Shimmer,   "Reverb", "Ethereal reverb with modulation and pitch shifting."));

        // Utility
        v.push_back (plain (ID_FeedbackKiller, "Utility", "Detects any loud feedback and suppresses it."));
        v.push_back (plain (ID_Gain,           "Utility", "Controls the gain of the audio signal."));
        v.push_back (plain (ID_Reverser,       "Utility", "Innovative sound shaping with adjustable reverse."));

        // Legacy
        v.push_back (plain (ID_LegacyDelay, "Legacy", "More customisable delay with extra options."));
        return v;
    }();
    return e;
}

inline FxCatalogEntry findCatalogEntry (int id)
{
    for (const auto& e : getFxCatalog()) if (e.id == id && e.initial.empty()) return e;
    FxCatalogEntry e; e.id = id; return e;
}

inline juce::String catalogDisplayName (const FxCatalogEntry& e)
{
    if (e.name.isNotEmpty()) return e.name;
    if (const auto* info = getFxInfo (e.id)) return info->name;
    return {};
}
