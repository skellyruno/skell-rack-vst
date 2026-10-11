#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReverserPanel : public juce::Component
{
public:
    ReverserPanel(ModularFXAudioProcessor& p);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    ModularFXAudioProcessor& processor;
    juce::ToggleButton bypassButton;
    juce::Slider mixSlider, timeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach, timeAttach;
};

class NoiseGatePanel : public juce::Component
{
public:
    NoiseGatePanel(ModularFXAudioProcessor& p);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    ModularFXAudioProcessor& processor;
    juce::ToggleButton bypassButton;
    juce::Slider attackSlider, releaseSlider, threshSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, releaseAttach, threshAttach;
};

class ChorusPanel : public juce::Component
{
public:
    ChorusPanel(ModularFXAudioProcessor& p);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    ModularFXAudioProcessor& processor;
    juce::ToggleButton bypassButton;
    juce::Slider depthSlider, amountSlider, speedSlider, bassSlider, trebleSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach, amountAttach, speedAttach, bassAttach, trebleAttach;
};

class ModularFXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p);
    ~ModularFXAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ModularFXAudioProcessor& audioProcessor;
    ReverserPanel reverserPanel;
    NoiseGatePanel noiseGatePanel;
    ChorusPanel chorusPanel;
};
