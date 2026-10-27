# Modular FX Suite (VST3)

A rack of 47 effects in one plugin, modelled on BandLab's effects. Add, remove and reorder
effects from the **+ Add Effect** button and the **< >  X** buttons on each card. The rack
scrolls sideways (mouse wheel works). Stack up to 10 effects (the same effect can be added as many times as you like) and drag a card by its header to change the signal order. Order and on/off state are saved with your project.

| | |
|---|---|
| Dynamics | Compressor, Multi-Band Comp, Clean Limiter, Noise Gate, Shaper |
| EQ / tone | Visual EQ, One Knobs |
| Space | Reverb, Vocal Verb (ducking), Shimmer, Delay |
| Modulation | Chorus Ensemble, Flanger, Phaser, Tremolo / Auto-Pan |
| Creative | Reverser, Gater, Bit Bandit, Distortion (Overdrive / Distortion / Fuzz) |

Tempo-synced controls (Delay sync, Gater, Reverser "beat") follow the host tempo.

## Adding another effect
1. Add an ID to `ModuleID` and an `add(...)` entry in `Source/FxRegistry.h` (parameters, colours, description).
2. Write a class deriving `fx::Processor` in `Source/Modules/` and add it to `createEffect()` in `PluginProcessor.cpp`.
The knobs and card are generated from the registry entry.

## Slots and automation

The rack has 10 slots. Each slot exposes 8 generic host parameters (`Slot N Param 1..8`) plus `Slot N On`, so any effect can live in any slot. In FL Studio's automation list, the parameter for a given knob is the one in the slot where that effect sits, in the order the knobs appear on the card; the displayed value text follows the effect in the slot. Adding an effect to a slot resets that slot to the effect's defaults.

Projects saved by earlier versions (one fixed instance per effect) are migrated automatically when loaded: effects, order, settings and on/off state are kept. Automation recorded against the old per-effect parameter names has to be re-linked.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Needs CMake 3.22+ and a C++17 compiler. The GitHub Actions workflow in `build.yml` builds the Windows VST3.

## Adding effects

Click the **Add Effect** tile at the end of the rack. The browser lists every effect by category (Recommended, One Knobs, Polish, Amps & Cabs, Pitch Shift, Compressor, Limiter, Rhythmic, EQ, Delay, Modulation, Distortion, Saturation, Reverb, Utility, Legacy), scrolls, and has a search box. Click an effect to add it; the same effect can be added more than once. To list a new effect in the browser, add a line to `Source/FxCatalog.h`.

## Side panels

- **Presets (left):** pick with the arrows or by clicking the name. Eight built-in presets are included. **Save** stores the current rack (effects, order, settings) as your own preset in `Documents`-independent app data (`~/.../ModularFXSuite/Presets` as small XML files). The ⋮ menu clears the rack or deletes the selected user preset. The **fx** button bypasses every effect.
- **Blend:** dry/wet for the whole rack (100% = fully effected).
- **Volume, Pan, Reverb (right):** output fader, left/right balance, and a reverb send applied after the rack.

These appear to the host as the automatable parameters Blend, Volume, Pan, Reverb and Bypass Effects.

## Window size & moving effects
- The editor opens at 75% of the full layout and is resizable (drag the bottom-right corner; the aspect ratio is kept). The size is saved with the project.
- Effect cards can be dragged from anywhere on the card (header, background, labels). Knobs, sliders, combo boxes, buttons and the Noise Gate threshold pill still work normally.

## Card design
Effect cards now follow BandLab's look: white knobs with a cyan arc, the name under each knob (it shows the value while you hover or drag), and rounded drop-downs with a caption. DIGI Comp, FBK Compressor, TecLab BA-2A, Multi Shifter, Pitch Shifter, Feedback Killer and DeEsser were rebuilt to match BandLab's controls. The DeEsser has its own card with a draggable Threshold pill.

## More BandLab cards
New: Spring Reverb, Multi Filter, EZ EQ, Graphic EQ, EQ3-M, Vintage Limiter and GTR Squeeze. Restyled to match BandLab: BL 1176 (the old FET Compressor), Pumper, Filter Echo +, D-Delay + (the old D-Delay), plus pill-style Feedback/Amount cards with ripple graphics. Parameters changed on Pumper, Filter Echo +, D-Delay + and BL 1176, so older presets of those effects will load with different values.

## Polish
- Switching an effect on or off (or adding it) now cross-fades over one block instead of clicking.
- Card headers use BandLab-style controls: power switch, info circle, close cross, centred title. Cards are moved by dragging them.
- The test suite now also checks CPU per effect and click-free switching for all effects.

## Tuning & presets
- Compressor slopes were measured against the hardware ratios they emulate (BL 1176 4/8/12/20:1, BA-2A about 3:1 compress and 10:1 limit). Defaults of Noise Gate, DeEsser and BL 1176 follow BandLab's cards.
- 8 new factory presets (16 in total) use the newer effects. Every factory preset is rendered in the test suite.
