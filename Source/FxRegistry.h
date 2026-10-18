#pragma once
#include <JuceHeader.h>
#include <vector>

// Single source of truth for every effect in the rack:
// its ID, name, colours, and its parameters.  The processor builds the APVTS
// layout from this table and the editor builds the knobs from it.

enum ModuleID : int
{
    ID_None = 0,
    ID_Reverser,        // 1  (IDs 1-3 are unchanged from the first version)
    ID_NoiseGate,       // 2
    ID_Chorus,          // 3
    ID_VisualEQ,
    ID_Compressor,
    ID_MultiBand,
    ID_Limiter,
    ID_VocalVerb,
    ID_Shimmer,
    ID_Reverb,
    ID_Delay,
    ID_BitBandit,
    ID_Gater,
    ID_Shaper,
    ID_Flanger,
    ID_Phaser,
    ID_Tremolo,
    ID_Distortion,
    ID_OneKnob,
    ID_PitchShifter,
    ID_MultiShifter,
    ID_DualOctaver,
    ID_GuitarAmp,
    ID_GuitarCab,
    ID_FeedbackKiller,
    ID_DeEsser,
    ID_Gain,
    ID_Pumper,
    ID_AutoPan,
    ID_AutoFilter,
    ID_AutoWah,
    ID_Bitcrusher,
    ID_Driver,
    ID_Exciter,
    ID_FetComp,
    ID_OptoComp,
    ID_DelayPlus,
    ID_FilterEcho,
    ID_LegacyDelay,
    ID_NumModules
};

constexpr int kMaxModules = ID_NumModules - 1;   // number of different effects that exist
constexpr int kMaxRack = 10;                     // how many can be stacked at once (same limit as BandLab)
constexpr int kSlotParams = 8;                   // generic controls per rack slot (no effect needs more than 8)

enum class ParamKind   { Knob, Toggle, Choice };
enum class DisplayKind { None, EqCurve, GainReduction };

struct ParamSpec
{
    juce::String id, label, unit;
    ParamKind kind = ParamKind::Knob;
    float minV = 0.0f, maxV = 1.0f, defV = 0.0f;
    float step = 0.0f;           // 0 = continuous
    float skewCentre = 0.0f;     // 0 = linear
    float scale = 1.0f;          // display multiplier (100 turns 0..1 into %)
    int   decimals = 2;
    juce::StringArray choices;
    juce::String dependsOn;      // id of a toggle that must be in `dependsValue` for this control to be active
    bool  dependsValue = true;
};

struct FxInfo
{
    int          id = ID_None;
    juce::String name, prefix, blurb;
    juce::Colour bg, accent, text;
    DisplayKind  display = DisplayKind::None;
    bool         customCard = false;     // drawn by a hand-built card in PluginEditor.h
    std::vector<ParamSpec> params;

    juce::String onId() const { return prefix + "_ON"; }
};

namespace fxreg
{
    inline ParamSpec K (const char* id, const char* label, float mn, float mx, float def,
                        const char* unit = "", int dec = 2, float skewCentre = 0.0f, float scale = 1.0f, float step = 0.0f)
    {
        ParamSpec s;
        s.id = id; s.label = label; s.unit = unit; s.kind = ParamKind::Knob;
        s.minV = mn; s.maxV = mx; s.defV = def; s.decimals = dec; s.skewCentre = skewCentre; s.scale = scale; s.step = step;
        return s;
    }
    inline ParamSpec Pct (const char* id, const char* label, float def)
    {
        return K (id, label, 0.0f, 1.0f, def, "%", 0, 0.0f, 100.0f);
    }
    inline ParamSpec T (const char* id, const char* label, bool def)
    {
        ParamSpec s;
        s.id = id; s.label = label; s.kind = ParamKind::Toggle; s.defV = def ? 1.0f : 0.0f; s.maxV = 1.0f;
        return s;
    }
    inline ParamSpec C (const char* id, const char* label, const juce::StringArray& choices, int def)
    {
        ParamSpec s;
        s.id = id; s.label = label; s.kind = ParamKind::Choice; s.choices = choices; s.defV = (float) def;
        s.maxV = (float) (choices.size() - 1);
        return s;
    }
    inline ParamSpec dep (ParamSpec s, const char* toggleId, bool value)
    {
        s.dependsOn = toggleId; s.dependsValue = value;
        return s;
    }
}

inline const std::vector<FxInfo>& getFxRegistry()
{
    using namespace fxreg;
    using juce::Colour;
    using juce::StringArray;

    static const std::vector<FxInfo> registry = []
    {
        std::vector<FxInfo> r;
        auto add = [&r] (int id, const char* name, const char* prefix, const char* blurb,
                         juce::uint32 bg, juce::uint32 accent, DisplayKind display, bool custom,
                         std::vector<ParamSpec> params)
        {
            FxInfo f;
            f.id = id; f.name = name; f.prefix = prefix; f.blurb = blurb;
            f.bg = Colour (bg); f.accent = Colour (accent); f.text = juce::Colours::white;
            f.display = display; f.customCard = custom; f.params = std::move (params);
            r.push_back (std::move (f));
        };

        // ------------------------------------------------------------ original three (custom cards)
        add (ID_Reverser, "Reverser", "REV",
             "Records short grains of audio and plays each one back backwards. Turn on Beat to lock the grain length to the project tempo.",
             0xFF121212, 0xFFA020F0, DisplayKind::None, true,
             { Pct ("REV_MIX", "Mix", 0.5f),
               K ("REV_TIME", "Time", 0.0f, 1.0f, 0.55f, "", 2),
               T ("REV_BEAT", "Beat", false) });

        add (ID_NoiseGate, "Noise Gate", "GATE",
             "Silences the signal when it drops below the threshold. Drag the Threshold pill up and down. Attack and release shape how fast the gate opens and closes.",
             0xFF0F0F0F, 0xFFFFB703, DisplayKind::None, true,
             { K ("GATE_THRESH", "Threshold", -60.0f, 0.0f, -30.0f, "dB", 1),
               K ("GATE_ATTACK", "Attack", 1.0f, 100.0f, 37.0f, "ms", 0),
               K ("GATE_RELEASE", "Release", 10.0f, 1000.0f, 200.0f, "ms", 0, 150.0f) });

        add (ID_Chorus, "Chorus Ensemble", "CHO",
             "Three detuned, modulated voices spread across the stereo field for a lush ensemble sound. Bass and Treble shape the chorus signal only.",
             0xFF6B93AC, 0xFFFFFFFF, DisplayKind::None, true,
             { Pct ("CHO_DEPTH", "Depth", 0.6f),
               Pct ("CHO_AMOUNT", "Amount", 0.5f),
               K ("CHO_SPEED", "Speed", 0.1f, 5.0f, 1.2f, "Hz", 2, 1.0f),
               K ("CHO_BASS", "Bass", -12.0f, 12.0f, 0.0f, "dB", 1),
               K ("CHO_TREBLE", "Treble", -12.0f, 12.0f, 0.0f, "dB", 1) });

        // ------------------------------------------------------------ new effects (generic cards)
        add (ID_VisualEQ, "Visual EQ", "EQ",
             "Three-band equaliser with a high-pass filter. The curve above shows exactly what you are doing to the sound.",
             0xFF14202B, 0xFF4CC9F0, DisplayKind::EqCurve, false,
             { K ("EQ_HPF", "HPF", 20.0f, 500.0f, 20.0f, "Hz", 0, 80.0f),
               K ("EQ_LF", "Low Freq", 30.0f, 400.0f, 100.0f, "Hz", 0, 100.0f),
               K ("EQ_LG", "Low Gain", -15.0f, 15.0f, 0.0f, "dB", 1),
               K ("EQ_MF", "Mid Freq", 200.0f, 8000.0f, 1000.0f, "Hz", 0, 1200.0f),
               K ("EQ_MG", "Mid Gain", -15.0f, 15.0f, 0.0f, "dB", 1),
               K ("EQ_MQ", "Mid Q", 0.3f, 6.0f, 1.0f, "", 2, 1.0f),
               K ("EQ_HF", "High Freq", 2000.0f, 16000.0f, 8000.0f, "Hz", 0, 5600.0f),
               K ("EQ_HG", "High Gain", -15.0f, 15.0f, 0.0f, "dB", 1) });

        add (ID_Compressor, "Compressor", "COMP",
             "Turns down loud parts so the level evens out. The bar shows how much gain is being removed. Use Makeup to bring the level back up, and Mix for parallel compression.",
             0xFF1B1B22, 0xFFFF6B6B, DisplayKind::GainReduction, false,
             { K ("COMP_THRESH", "Threshold", -60.0f, 0.0f, -18.0f, "dB", 1),
               K ("COMP_RATIO", "Ratio", 1.0f, 20.0f, 4.0f, ": 1", 1, 4.0f),
               K ("COMP_ATTACK", "Attack", 0.1f, 100.0f, 10.0f, "ms", 1, 10.0f),
               K ("COMP_RELEASE", "Release", 10.0f, 1000.0f, 120.0f, "ms", 0, 150.0f),
               K ("COMP_MAKEUP", "Makeup", 0.0f, 24.0f, 0.0f, "dB", 1),
               Pct ("COMP_MIX", "Mix", 1.0f) });

        add (ID_MultiBand, "Multi-Band Comp", "MB",
             "Splits the sound into low, mid and high bands and compresses each on its own, so you can tighten the lows without squashing the highs.",
             0xFF1E1B2E, 0xFFB388FF, DisplayKind::GainReduction, false,
             { K ("MB_XLOW", "Low X-over", 60.0f, 400.0f, 150.0f, "Hz", 0, 150.0f),
               K ("MB_XHIGH", "High X-over", 1000.0f, 8000.0f, 2500.0f, "Hz", 0, 2500.0f),
               K ("MB_LTHR", "Low Thresh", -60.0f, 0.0f, -20.0f, "dB", 1),
               K ("MB_MTHR", "Mid Thresh", -60.0f, 0.0f, -20.0f, "dB", 1),
               K ("MB_HTHR", "High Thresh", -60.0f, 0.0f, -20.0f, "dB", 1),
               K ("MB_RATIO", "Ratio", 1.0f, 10.0f, 3.0f, ": 1", 1, 3.0f),
               K ("MB_GAIN", "Output", -12.0f, 12.0f, 0.0f, "dB", 1) });

        add (ID_Limiter, "Clean Limiter", "LIM",
             "A transparent peak limiter. Push Gain to make things louder, the Ceiling is the level nothing will ever go above.",
             0xFF10261F, 0xFF3DDC97, DisplayKind::GainReduction, false,
             { K ("LIM_GAIN", "Gain", 0.0f, 24.0f, 0.0f, "dB", 1),
               K ("LIM_CEIL", "Ceiling", -12.0f, 0.0f, -0.3f, "dB", 1),
               K ("LIM_REL", "Release", 10.0f, 1000.0f, 100.0f, "ms", 0, 100.0f) });

        add (ID_VocalVerb, "Vocal Verb", "VV",
             "A clean reverb that ducks while you sing and blooms back in the gaps, so vocals stay clear and upfront.",
             0xFF2A1B33, 0xFFFF8FD8, DisplayKind::None, false,
             { Pct ("VV_MIX", "Mix", 0.25f),
               Pct ("VV_REFLECT", "Reflect", 0.5f),
               dep (K ("VV_THRESH", "Threshold", -60.0f, 0.0f, -30.0f, "dB", 1), "VV_DUCK", true),
               dep (Pct ("VV_AMOUNT", "Amount", 0.6f), "VV_DUCK", true),
               T ("VV_DUCK", "Ducking", true) });

        add (ID_Shimmer, "Shimmer", "SH",
             "A spacious reverb that feeds an octave-up shifted copy of its own tail back into itself, giving a floating, ethereal glow.",
             0xFF1A2540, 0xFF9AD1FF, DisplayKind::None, false,
             { Pct ("SH_MIX", "Mix", 0.35f),
               Pct ("SH_MOD", "Modulation", 0.3f),
               Pct ("SH_TAIL", "Tail", 0.6f),
               Pct ("SH_DIFF", "Diffusion", 0.6f) });

        add (ID_Reverb, "Reverb", "RV",
             "Adds space. Size sets how long the room rings, Damping removes the highs from the tail, and Pre-delay keeps the dry sound clear of the reverb.",
             0xFF16282E, 0xFF5EEAD4, DisplayKind::None, false,
             { Pct ("RV_MIX", "Mix", 0.3f),
               Pct ("RV_SIZE", "Size", 0.5f),
               Pct ("RV_DAMP", "Damping", 0.5f),
               Pct ("RV_WIDTH", "Width", 1.0f),
               K ("RV_PRE", "Pre-delay", 0.0f, 200.0f, 15.0f, "ms", 0) });

        add (ID_Delay, "Delay", "DLY",
             "Echoes. Free-time in milliseconds, or Sync to lock the echoes to the project tempo. Ping-Pong bounces them between left and right.",
             0xFF2B2415, 0xFFFFC857, DisplayKind::None, false,
             { dep (K ("DLY_TIME", "Time", 1.0f, 2000.0f, 350.0f, "ms", 0, 300.0f), "DLY_SYNC", false),
               dep (C ("DLY_DIV", "Note", StringArray { "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/8 triplet", "1/2" }, 1), "DLY_SYNC", true),
               Pct ("DLY_FB", "Feedback", 0.4f),
               Pct ("DLY_TONE", "Tone", 0.6f),
               Pct ("DLY_MIX", "Mix", 0.3f),
               T ("DLY_SYNC", "Sync", false),
               T ("DLY_PING", "Ping-Pong", false) });

        add (ID_BitBandit, "Bit Bandit", "BIT",
             "Lo-fi crunch. Lower the bit depth and the sample rate for retro game sounds, robot voices and telephone tones.",
             0xFF0D1F0D, 0xFF7CFF4F, DisplayKind::None, false,
             { K ("BIT_BITS", "Bit Depth", 1.0f, 16.0f, 8.0f, "bit", 0, 0.0f, 1.0f, 1.0f),
               K ("BIT_RATE", "Sample Rate", 1000.0f, 44100.0f, 12000.0f, "Hz", 0, 8000.0f),
               Pct ("BIT_MIX", "Mix", 1.0f) });

        add (ID_Gater, "Gater", "GTR",
             "Chops the sound on and off in time with the tempo for rhythmic stutters. Depth sets how far the volume drops, Smooth softens the edges.",
             0xFF2A1515, 0xFFF43F5E, DisplayKind::None, false,
             { C ("GTR_TIME", "Time", StringArray { "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8 triplet", "1/16 triplet" }, 3),
               Pct ("GTR_DEPTH", "Depth", 1.0f),
               Pct ("GTR_SMOOTH", "Smooth", 0.2f) });

        add (ID_Shaper, "Shaper", "SHP",
             "Controls the attack and the tail of a sound. Boost Transient for punch, boost Sustain to lengthen decays, or turn either down to soften things.",
             0xFF202428, 0xFFFFD166, DisplayKind::None, false,
             { K ("SHP_TRANS", "Transient", -100.0f, 100.0f, 0.0f, "%", 0),
               K ("SHP_SUST", "Sustain", -100.0f, 100.0f, 0.0f, "%", 0),
               K ("SHP_OUT", "Output", -12.0f, 12.0f, 0.0f, "dB", 1) });

        add (ID_Flanger, "Flanger", "FLG",
             "A short, swept delay mixed with the dry sound for the classic jet-plane whoosh. Negative feedback gives a hollower sound.",
             0xFF0F2B2B, 0xFF2DD4BF, DisplayKind::None, false,
             { K ("FLG_RATE", "Rate", 0.05f, 5.0f, 0.3f, "Hz", 2, 0.5f),
               Pct ("FLG_DEPTH", "Depth", 0.7f),
               K ("FLG_FB", "Feedback", -0.95f, 0.95f, 0.5f, "%", 0, 0.0f, 100.0f),
               Pct ("FLG_MIX", "Mix", 0.5f) });

        add (ID_Phaser, "Phaser", "PHS",
             "Sweeps a set of notches through the sound for a swirling, watery movement. More stages give a deeper, more complex swirl.",
             0xFF2B1535, 0xFFE879F9, DisplayKind::None, false,
             { K ("PHS_RATE", "Rate", 0.05f, 5.0f, 0.4f, "Hz", 2, 0.5f),
               Pct ("PHS_DEPTH", "Depth", 0.7f),
               K ("PHS_FB", "Feedback", 0.0f, 0.9f, 0.4f, "%", 0, 0.0f, 100.0f),
               Pct ("PHS_MIX", "Mix", 0.5f),
               C ("PHS_STAGES", "Stages", StringArray { "4 stages", "6 stages", "8 stages" }, 1) });

        add (ID_Tremolo, "Tremolo", "TRM",
             "Wobbles the volume. Switch on Auto-Pan to move the sound left and right instead (stereo only).",
             0xFF2B1A12, 0xFFFB923C, DisplayKind::None, false,
             { K ("TRM_RATE", "Rate", 0.1f, 20.0f, 5.0f, "Hz", 2, 3.0f),
               Pct ("TRM_DEPTH", "Depth", 0.6f),
               C ("TRM_SHAPE", "Shape", StringArray { "Sine", "Triangle", "Square" }, 0),
               T ("TRM_PAN", "Auto-Pan", false) });

        add (ID_Distortion, "Distortion", "DST",
             "Adds grit. Overdrive is warm and smooth, Distortion is harder, and Fuzz is gated and spitty. Tone rolls off the harshness.",
             0xFF2B0F0F, 0xFFEF4444, DisplayKind::None, false,
             { C ("DST_MODE", "Type", StringArray { "Overdrive", "Distortion", "Fuzz" }, 0),
               Pct ("DST_DRIVE", "Drive", 0.4f),
               Pct ("DST_TONE", "Tone", 0.6f),
               K ("DST_LEVEL", "Level", -24.0f, 6.0f, -6.0f, "dB", 1),
               Pct ("DST_MIX", "Mix", 1.0f) });

        add (ID_OneKnob, "One Knobs", "ONE",
             "One knob, eight flavours. Pick a character, then turn Amount up until it sounds right.",
             0xFF2A2A2A, 0xFFFFFFFF, DisplayKind::None, false,
             { Pct ("ONE_AMT", "Amount", 0.5f),
               C ("ONE_MODE", "Style", StringArray { "Brighter", "Echo", "Punchy", "Room", "Toasty", "Bassy", "Leveller", "Dirty" }, 0) });


        // ------------------------------------------------------------ pitch, amps, polish
        add (ID_PitchShifter, "Pitch Shifter", "PS",
             "Moves the pitch up or down by up to an octave. Semitones are whole steps on the keyboard, Fine tunes in cents.",
             0xFF1C2230, 0xFF7DD3FC, DisplayKind::None, false,
             { K ("PS_SEMI", "Semitones", -12.0f, 12.0f, 0.0f, "st", 0, 0.0f, 1.0f, 1.0f),
               K ("PS_FINE", "Fine", -50.0f, 50.0f, 0.0f, "ct", 0),
               Pct ("PS_MIX", "Mix", 1.0f) });

        add (ID_MultiShifter, "Multi Shifter", "MS",
             "Three pitch-shifted voices blended with the original, each with its own pitch and level. Spread pans the voices across the stereo field. Great for instant harmonies.",
             0xFF211B36, 0xFFC4B5FD, DisplayKind::None, false,
             { K ("MS_P1", "Pitch 1", -24.0f, 24.0f, 4.0f, "st", 0, 0.0f, 1.0f, 1.0f),
               Pct ("MS_L1", "Level 1", 0.5f),
               K ("MS_P2", "Pitch 2", -24.0f, 24.0f, 7.0f, "st", 0, 0.0f, 1.0f, 1.0f),
               Pct ("MS_L2", "Level 2", 0.5f),
               K ("MS_P3", "Pitch 3", -24.0f, 24.0f, -12.0f, "st", 0, 0.0f, 1.0f, 1.0f),
               Pct ("MS_L3", "Level 3", 0.3f),
               Pct ("MS_DRY", "Dry", 1.0f),
               Pct ("MS_SPREAD", "Spread", 0.5f) });

        add (ID_DualOctaver, "Dual Octaver", "OCT",
             "Adds one octave above and one octave below the original. Tone darkens the shifted voices.",
             0xFF1B2A2A, 0xFF5EEAD4, DisplayKind::None, false,
             { Pct ("OCT_UP", "Octave Up", 0.5f),
               Pct ("OCT_DOWN", "Octave Down", 0.5f),
               Pct ("OCT_DRY", "Dry", 1.0f),
               Pct ("OCT_TONE", "Tone", 0.7f) });

        add (ID_GuitarAmp, "Guitar Amp", "AMP",
             "A guitar amplifier with four basic characters, from sparkling Clean to tight, high-gain Metal. Follow it with a Guitar Cab for the full sound.",
             0xFF221A14, 0xFFFB923C, DisplayKind::None, false,
             { C ("AMP_TYPE", "Type", StringArray { "Clean", "Crunch", "Lead", "Metal" }, 1),
               Pct ("AMP_GAIN", "Gain", 0.5f),
               Pct ("AMP_BASS", "Bass", 0.5f),
               Pct ("AMP_MID", "Mid", 0.5f),
               Pct ("AMP_TREBLE", "Treble", 0.5f),
               Pct ("AMP_PRES", "Presence", 0.5f),
               K ("AMP_LEVEL", "Level", -24.0f, 6.0f, -6.0f, "dB", 1) });

        add (ID_GuitarCab, "Guitar Cab", "CAB",
             "Models the sound of popular speaker cabinets. Move the microphone from the centre of the speaker (bright) to the edge (dark), and add a touch of room.",
             0xFF1E1A17, 0xFFD6A77A, DisplayKind::None, false,
             { C ("CAB_TYPE", "Cabinet", StringArray { "1x12 Open", "2x12 Combo", "4x12 Closed", "1x10 Vintage" }, 2),
               Pct ("CAB_MIC", "Mic Position", 0.5f),
               K ("CAB_LOW", "Low Cut", 40.0f, 300.0f, 80.0f, "Hz", 0, 100.0f),
               K ("CAB_HIGH", "High Cut", 2000.0f, 12000.0f, 7000.0f, "Hz", 0, 5000.0f),
               Pct ("CAB_ROOM", "Room", 0.15f),
               Pct ("CAB_MIX", "Mix", 1.0f),
               K ("CAB_LEVEL", "Level", -12.0f, 12.0f, 0.0f, "dB", 1) });

        add (ID_FeedbackKiller, "Feedback Killer", "FK",
             "Listens for a loud, sustained whistle (feedback) and automatically places a narrow notch on it. Raise Sensitivity to catch quieter feedback sooner.",
             0xFF1F2428, 0xFF93C5FD, DisplayKind::None, false,
             { Pct ("FK_SENS", "Sensitivity", 0.5f),
               K ("FK_DEPTH", "Depth", 6.0f, 36.0f, 24.0f, "dB", 0) });

        add (ID_DeEsser, "DeEsser", "DS",
             "Turns down harsh 's' and 'sh' sounds in a voice. Frequency picks where sibilance lives, Threshold sets how loud it must be, and Listen lets you hear only that band.",
             0xFF2A2312, 0xFFFACC15, DisplayKind::GainReduction, false,
             { K ("DS_FREQ", "Frequency", 3000.0f, 10000.0f, 6000.0f, "Hz", 0, 6000.0f),
               K ("DS_THRESH", "Threshold", -50.0f, 0.0f, -30.0f, "dB", 1),
               Pct ("DS_AMOUNT", "Amount", 0.6f),
               T ("DS_LISTEN", "Listen", false) });


        // ------------------------------------------------------------ utility, dynamics, modulation, delay, saturation
        add (ID_Gain, "Gain", "GN",
             "Turns the level up or down. Invert Phase flips the polarity and Mono sums left and right together.",
             0xFF1C1F26, 0xFF9CA3AF, DisplayKind::None, false,
             { K ("GAIN_DB", "Gain", -24.0f, 24.0f, 0.0f, "dB", 1),
               T ("GAIN_INV", "Invert Phase", false),
               T ("GAIN_MONO", "Mono", false) });

        add (ID_Pumper, "Pumper", "PMP",
             "Simulated side-chain compression: the volume ducks on every beat and swells back, the classic pumping dance sound. Locked to the project tempo.",
             0xFF1F1B2E, 0xFF818CF8, DisplayKind::None, false,
             { C ("PMP_RATE", "Rate", StringArray { "1/2", "1/4", "1/8", "1/16" }, 1),
               Pct ("PMP_DEPTH", "Depth", 0.8f),
               Pct ("PMP_RELEASE", "Release", 0.6f),
               Pct ("PMP_CURVE", "Curve", 0.5f) });

        add (ID_AutoPan, "Auto Pan", "APN",
             "Moves the sound between the left and right speakers. Sync locks the movement to the tempo (stereo only).",
             0xFF132A2A, 0xFF22D3EE, DisplayKind::None, false,
             { dep (K ("APN_RATE", "Rate", 0.1f, 10.0f, 1.0f, "Hz", 2, 1.0f), "APN_SYNC", false),
               dep (C ("APN_DIV", "Note", StringArray { "1 bar", "1/2", "1/4", "1/8", "1/16" }, 2), "APN_SYNC", true),
               Pct ("APN_DEPTH", "Depth", 0.8f),
               C ("APN_SHAPE", "Shape", StringArray { "Sine", "Triangle", "Square" }, 0),
               T ("APN_SYNC", "Sync", false) });

        add (ID_AutoFilter, "Auto Filter", "AF",
             "A filter that sweeps up and down by itself. Pick low-pass, band-pass or high-pass, then set how fast and how far it moves.",
             0xFF1A2433, 0xFF60A5FA, DisplayKind::None, false,
             { C ("AF_TYPE", "Type", StringArray { "Low-pass", "Band-pass", "High-pass" }, 0),
               K ("AF_CUTOFF", "Cutoff", 80.0f, 12000.0f, 1200.0f, "Hz", 0, 1000.0f),
               K ("AF_RES", "Resonance", 0.5f, 12.0f, 2.0f, "", 1, 2.0f),
               K ("AF_RATE", "Rate", 0.05f, 10.0f, 0.8f, "Hz", 2, 1.0f),
               Pct ("AF_DEPTH", "Depth", 0.6f),
               C ("AF_SHAPE", "Shape", StringArray { "Sine", "Triangle", "Ramp" }, 0),
               Pct ("AF_MIX", "Mix", 1.0f) });

        add (ID_AutoWah, "Auto Wah", "AW",
             "A wah pedal that you play with your picking: the louder you play, the further the filter opens (or closes, in Down mode).",
             0xFF2A1F12, 0xFFFBBF24, DisplayKind::None, false,
             { C ("AW_MODE", "Direction", StringArray { "Up", "Down" }, 0),
               Pct ("AW_SENS", "Sensitivity", 0.6f),
               K ("AW_FREQ", "Frequency", 200.0f, 2000.0f, 500.0f, "Hz", 0, 600.0f),
               K ("AW_RES", "Resonance", 1.0f, 12.0f, 4.0f, "", 1, 4.0f),
               K ("AW_DECAY", "Decay", 20.0f, 500.0f, 120.0f, "ms", 0, 120.0f),
               Pct ("AW_MIX", "Mix", 1.0f) });

        add (ID_Bitcrusher, "Bitcrusher", "BCR",
             "Downsamples the sound and cuts its bit depth for digital, crunchy lo-fi tones.",
             0xFF241A33, 0xFFC026D3, DisplayKind::None, false,
             { K ("BCR_BITS", "Bit Depth", 1.0f, 16.0f, 8.0f, "bit", 0, 0.0f, 1.0f, 1.0f),
               K ("BCR_DOWN", "Downsample", 1.0f, 64.0f, 4.0f, "x", 0, 8.0f, 1.0f, 1.0f),
               Pct ("BCR_MIX", "Mix", 1.0f) });

        add (ID_Driver, "Driver", "DRV",
             "Warms up a clean signal and adds gritty harmonics. Turn Drive up for more edge, Tone to brighten or soften it.",
             0xFF2B1612, 0xFFF87171, DisplayKind::None, false,
             { Pct ("DRV_DRIVE", "Drive", 0.4f),
               Pct ("DRV_TONE", "Tone", 0.5f),
               K ("DRV_LEVEL", "Level", -24.0f, 6.0f, -6.0f, "dB", 1),
               Pct ("DRV_MIX", "Mix", 1.0f) });

        add (ID_Exciter, "Exciter", "EXC",
             "Adds sparkle by generating fresh high harmonics above the Frequency. Great for dull vocals and mixes.",
             0xFF2A1E2F, 0xFFF0ABFC, DisplayKind::None, false,
             { K ("EXC_FREQ", "Frequency", 1500.0f, 10000.0f, 4000.0f, "Hz", 0, 4000.0f),
               Pct ("EXC_AMT", "Amount", 0.5f),
               Pct ("EXC_MIX", "Mix", 1.0f) });

        add (ID_FetComp, "FET Compressor", "FET",
             "A fast, punchy compressor in the style of the classic FET limiting amplifier. Input drives the signal into the compressor, Output sets the final level.",
             0xFF1E2024, 0xFFFB7185, DisplayKind::GainReduction, false,
             { K ("FET_INPUT", "Input", 0.0f, 40.0f, 14.0f, "dB", 1),
               K ("FET_OUTPUT", "Output", -24.0f, 12.0f, -6.0f, "dB", 1),
               K ("FET_ATTACK", "Attack", 0.02f, 0.8f, 0.2f, "ms", 2, 0.2f),
               K ("FET_RELEASE", "Release", 50.0f, 1100.0f, 300.0f, "ms", 0, 300.0f),
               C ("FET_RATIO", "Ratio", StringArray { "4:1", "8:1", "12:1", "20:1", "All" }, 0),
               Pct ("FET_MIX", "Mix", 1.0f) });

        add (ID_OptoComp, "Opto Compressor", "OPT",
             "Smooth, musical compression in the style of a classic optical compressor. Peak Reduction sets how much it squeezes, Gain brings the level back up.",
             0xFF26221B, 0xFFE5C07B, DisplayKind::GainReduction, false,
             { Pct ("OPT_PEAK", "Peak Reduction", 0.5f),
               K ("OPT_GAIN", "Gain", 0.0f, 30.0f, 6.0f, "dB", 1),
               C ("OPT_MODE", "Mode", StringArray { "Compress", "Limit" }, 0),
               Pct ("OPT_MIX", "Mix", 1.0f) });

        add (ID_DelayPlus, "Delay +", "DDP",
             "Rhythmic multi-tap delay. Each pattern drops four echoes on the beat grid, spread across the stereo field.",
             0xFF132B2E, 0xFF2DD4BF, DisplayKind::None, false,
             { C ("DDP_PATTERN", "Pattern", StringArray { "Straight", "Dotted", "Triplet", "Syncopated", "Cascade" }, 3),
               C ("DDP_BASE", "Note", StringArray { "1/4", "1/8", "1/16" }, 1),
               Pct ("DDP_FB", "Feedback", 0.4f),
               Pct ("DDP_TONE", "Tone", 0.6f),
               Pct ("DDP_SPREAD", "Spread", 0.7f),
               Pct ("DDP_MIX", "Mix", 0.35f) });

        add (ID_FilterEcho, "Filter Echo +", "FEC",
             "A tempo-synced echo where every repeat passes through a filter, so the echoes get darker, thinner or more resonant as they fade.",
             0xFF2B1535, 0xFFD946EF, DisplayKind::None, false,
             { C ("FEC_DIV", "Note", StringArray { "1/4", "1/8", "1/8 dotted", "1/16", "1/2", "1/8 triplet" }, 1),
               Pct ("FEC_FB", "Feedback", 0.5f),
               K ("FEC_FREQ", "Filter", 150.0f, 12000.0f, 2000.0f, "Hz", 0, 1500.0f),
               K ("FEC_RES", "Resonance", 0.5f, 8.0f, 1.5f, "", 1, 1.5f),
               C ("FEC_TYPE", "Type", StringArray { "Low-pass", "Band-pass", "High-pass" }, 0),
               Pct ("FEC_MOD", "Modulation", 0.2f),
               Pct ("FEC_MIX", "Mix", 0.35f) });

        add (ID_LegacyDelay, "D-Delay", "DDL",
             "A classic delay with separate left and right times and cross-feedback for ping-pong style movement.",
             0xFF2A2415, 0xFFFDE047, DisplayKind::None, false,
             { K ("DDL_TL", "Left Time", 1.0f, 2000.0f, 300.0f, "ms", 0, 300.0f),
               K ("DDL_TR", "Right Time", 1.0f, 2000.0f, 450.0f, "ms", 0, 300.0f),
               Pct ("DDL_FB", "Feedback", 0.4f),
               Pct ("DDL_CROSS", "Cross", 0.3f),
               Pct ("DDL_TONE", "Tone", 0.6f),
               Pct ("DDL_MIX", "Mix", 0.3f) });

        return r;
    }();

    return registry;
}

inline const FxInfo* getFxInfo (int id)
{
    for (const auto& f : getFxRegistry())
        if (f.id == id) return &f;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Slot parameters.  The rack has kMaxRack slots and any effect can sit in any slot, so the host
// sees generic parameters per slot: "S3_P1" .. "S3_P8" plus "S3_ON".  Every slot parameter is a
// plain 0..1 value; these helpers convert to and from the real value of whichever effect is
// currently in the slot (the effect's ParamSpec says how).
inline juce::String slotParamId(int slot, int index) { return "S" + juce::String(slot + 1) + "_P" + juce::String(index + 1); }
inline juce::String slotOnId(int slot)               { return "S" + juce::String(slot + 1) + "_ON"; }

inline juce::NormalisableRange<float> makeRange(const ParamSpec& s)
{
    juce::NormalisableRange<float> r(s.minV, s.maxV, s.step);
    if (s.kind == ParamKind::Knob && s.skewCentre > 0.0f)
        r.setSkewForCentre(s.skewCentre);
    return r;
}

inline float realFromNorm(const ParamSpec& s, float norm)
{
    norm = juce::jlimit(0.0f, 1.0f, norm);
    switch (s.kind)
    {
        case ParamKind::Toggle: return norm >= 0.5f ? 1.0f : 0.0f;
        case ParamKind::Choice: { const int n = juce::jmax(1, s.choices.size()); return (float) juce::jmin(n - 1, (int) (norm * (float) n)); }
        default:                return makeRange(s).convertFrom0to1(norm);
    }
}

inline float normFromReal(const ParamSpec& s, float real)
{
    switch (s.kind)
    {
        case ParamKind::Toggle: return real >= 0.5f ? 1.0f : 0.0f;
        case ParamKind::Choice: { const int n = juce::jmax(1, s.choices.size()); return ((float) juce::jlimit(0, n - 1, (int) (real + 0.5f)) + 0.5f) / (float) n; }
        default:                return makeRange(s).convertTo0to1(real);
    }
}

inline float defaultNorm(const ParamSpec& s) { return normFromReal(s, s.defV); }

inline int specIndex(const FxInfo& info, const juce::String& id)
{
    for (size_t i = 0; i < info.params.size(); ++i)
        if (info.params[i].id == id) return (int) i;
    return -1;
}

// ---- display helpers shared by the host-parameter text and the on-screen controls ----
inline juce::String formatSpecValue(const ParamSpec& s, float real)
{
    switch (s.kind)
    {
        case ParamKind::Toggle: return real >= 0.5f ? "On" : "Off";
        case ParamKind::Choice: return s.choices[juce::jlimit(0, juce::jmax(0, s.choices.size() - 1), (int) (real + 0.5f))];
        default: break;
    }
    const float d = real * s.scale;
    if (s.unit == "Hz" && std::abs(d) >= 1000.0f) return juce::String(d / 1000.0f, 2) + " kHz";
    if (s.unit.isEmpty()) return juce::String(d, s.decimals);
    if (s.unit == "%")    return juce::String(d, s.decimals) + "%";
    return juce::String(d, s.decimals) + " " + s.unit;
}

inline float parseSpecValue(const ParamSpec& s, const juce::String& text)
{
    const juce::String t = text.trim();
    float v = t.getFloatValue();
    if (t.containsIgnoreCase("k")) v *= 1000.0f;
    return juce::jlimit(s.minV, s.maxV, v / s.scale);
}
