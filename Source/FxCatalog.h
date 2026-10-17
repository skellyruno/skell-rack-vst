#pragma once
#include <JuceHeader.h>
#include "FxRegistry.h"

// How effects are grouped and described in the "Add Effect" browser.
struct FxCategory { juce::String name, blurb; };
struct FxCatalogEntry { int id; juce::String category, tagline; };

inline const std::vector<FxCategory>& getFxCategories()
{
    static const std::vector<FxCategory> c = {
        { "One Knobs",   "Get studio quality sound quickly with these powerful One Knob effects." },
        { "Polish",      "Clean up unwanted noise such as hiss, hum, harshness and feedback for a cleaner sound." },
        { "Amps & Cabs", "Recreate the tone of guitar amplifiers and speaker cabinets for a rich, authentic sound." },
        { "Pitch Shift", "Effects for altering and shifting pitch creatively." },
        { "Compressor",  "Control dynamics and even out the level of your sound." },
        { "Limiter",     "Keep peaks under control and make things louder, safely." },
        { "Rhythmic",    "Effects that move in time with the music." },
        { "EQ",          "Shape the tone by boosting or cutting frequencies." },
        { "Delay",       "Echoes and repeats." },
        { "Reverb",      "Add space and depth, from small rooms to endless halls." },
        { "Modulation",  "Movement and sweep: chorus, flanger and phaser." },
        { "Distortion",  "Add grit, crunch and character." },
    };
    return c;
}

// Display order inside each category follows this table.
inline const std::vector<FxCatalogEntry>& getFxCatalog()
{
    static const std::vector<FxCatalogEntry> e = {
        { ID_OneKnob,        "One Knobs",   "Studio-quality sound from a single knob." },
        { ID_FeedbackKiller, "Polish",      "Detects any loud feedback and suppresses it." },
        { ID_DeEsser,        "Polish",      "Helps eliminate excess sibilance (\"ess\" sounds)." },
        { ID_NoiseGate,      "Polish",      "Allows signal above a threshold to pass through." },
        { ID_GuitarAmp,      "Amps & Cabs", "Guitar amplifier with 4 basic amp types." },
        { ID_GuitarCab,      "Amps & Cabs", "Models of popular guitar cabinets." },
        { ID_MultiShifter,   "Pitch Shift", "Mix multiple pitch-shifted versions of a signal." },
        { ID_PitchShifter,   "Pitch Shift", "Simple pitch shifting effect." },
        { ID_DualOctaver,    "Pitch Shift", "Two octaves to increase or decrease frequency." },
        { ID_Compressor,     "Compressor",  "Evens out the level of loud and quiet parts." },
        { ID_MultiBand,      "Compressor",  "Compresses the low, mid and high bands separately." },
        { ID_Shaper,         "Compressor",  "Reshapes the attack and sustain of a sound." },
        { ID_Limiter,        "Limiter",     "Transparent limiter that keeps peaks under a ceiling." },
        { ID_Reverser,       "Rhythmic",    "Plays short slices of audio backwards." },
        { ID_Gater,          "Rhythmic",    "Chops the sound on and off in time with the tempo." },
        { ID_Tremolo,        "Rhythmic",    "Wobbles the volume, or pans left and right." },
        { ID_VisualEQ,       "EQ",          "Three-band EQ with a high-pass filter and a live curve." },
        { ID_Delay,          "Delay",       "Echoes in free time or synced to the tempo." },
        { ID_Reverb,         "Reverb",      "Adds a room, hall or plate-style space." },
        { ID_VocalVerb,      "Reverb",      "Reverb that ducks while the vocal is singing." },
        { ID_Shimmer,        "Reverb",      "Ethereal reverb with an octave-up glow." },
        { ID_Chorus,         "Modulation",  "Lush ensemble of detuned, modulated voices." },
        { ID_Flanger,        "Modulation",  "Jet-plane sweeping comb filter." },
        { ID_Phaser,         "Modulation",  "Swirling notches swept through the sound." },
        { ID_Distortion,     "Distortion",  "Overdrive, distortion and fuzz." },
        { ID_BitBandit,      "Distortion",  "Lo-fi bit and sample-rate crusher." },
    };
    return e;
}
