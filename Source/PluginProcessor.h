#pragma once
#include <JuceHeader.h>
#include "Modules/DSPModules.h"

enum ModuleID
{
    ID_Reverser = 1,
    ID_NoiseGate = 2,
    ID_Chorus = 3
};

class ModularFXAudioProcessor : public juce::AudioProcessor
{
public:
    ModularFXAudioProcessor();
    ~ModularFXAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Modular FX Suite"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updateModuleOrder(const std::vector<int>& newOrder);
    std::vector<int> getModuleOrder() const;

    juce::AudioProcessorValueTreeState apvts;

    ReverserDSP reverser;
    NoiseGateDSP noiseGate;
    ChorusDSP chorus;

private:
    std::atomic<int> moduleCount { 3 };
    std::atomic<int> moduleOrder[3];

    std::atomic<float>* revMixParam = nullptr;
    std::atomic<float>* gateThreshParam = nullptr;
    std::atomic<float>* choDepthParam = nullptr;
    std::atomic<float>* choSpeedParam = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
