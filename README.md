# Modular FX Suite (VST3)

A rack of 19 effects in one plugin, modelled on BandLab's effects. Add, remove and reorder
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
