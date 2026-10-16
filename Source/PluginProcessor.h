#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "FxRegistry.h"
#include "Modules/FxBase.h"

// The rack is made of kMaxRack "slots".  Every slot can hold any effect (the same effect may sit in
// several slots).  A slot owns 8 generic 0..1 host parameters plus an ON switch; the effect that
// currently lives in the slot decides what those parameters mean.
class ModularFXAudioProcessor : public juce::AudioProcessor
{
public:
    struct RackEntry { int slot = 0; int type = 0; };

    ModularFXAudioProcessor();
    ~ModularFXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Modular FX Suite"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- rack editing (message thread) ----
    int  addEffect (int type);                          // returns the slot used, or -1 when the rack is full
    void removeEffect (int slot);
    void setRackOrder (const std::vector<int>& slotsInOrder);
    std::vector<RackEntry> getRack() const;             // in signal order
    int  getSlotType (int slot) const noexcept;         // 0 = empty

    // Read-only access for the editor's meters.
    const fx::Processor* getFx (int slot) const noexcept;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    // Sent (asynchronously, on the message thread) when a saved project restores a different rack.
    juce::ChangeBroadcaster rackChanged;

private:
    static constexpr int kSlots = kMaxRack;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void commitRack (const std::vector<RackEntry>& wanted, bool resetNewSlots);
    void resetSlotParams (int slot, int type);

    std::array<std::atomic<int>, kSlots> slotType;      // must be declared before apvts (its text callbacks read it)

public:
    juce::AudioProcessorValueTreeState apvts;

private:
    std::array<std::atomic<float>*, kSlots> onRaw {};
    std::array<std::atomic<const fx::Processor*>, kSlots> fxView;   // for the editor's meters

    mutable juce::CriticalSection editLock;     // serialises rack edits and guards 'rack'
    std::vector<RackEntry> rack;

    juce::SpinLock audioLock;                   // guards the audio-side structures below
    std::array<std::unique_ptr<fx::Processor>, kSlots> slotFx;
    std::array<int, kSlots> audioOrder {};
    int audioCount = 0;
    std::array<bool, kSlots> wasRunning {};

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
};
