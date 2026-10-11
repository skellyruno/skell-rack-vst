#include "PluginEditor.h"

ReverserPanel::ReverserPanel(ModularFXAudioProcessor& p) : processor(p)
{
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(true, juce::dontSendNotification);
    bypassButton.onClick = [this] { processor.reverser.setBypassed(!bypassButton.getToggleState()); };
    addAndMakeVisible(bypassButton);

    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mixSlider);

    timeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    timeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(timeSlider);

    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "REV_MIX", mixSlider);
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "REV_TIME", timeSlider);
}

void ReverserPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF121212));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Reverser", 55, 10, 120, 24, juce::Justification::left);

    g.setColour(juce::Colours::grey);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Mix", mixSlider.getX(), mixSlider.getBottom() + 4, mixSlider.getWidth(), 18, juce::Justification::centred);
    g.drawText("Time", timeSlider.getX(), timeSlider.getBottom() + 4, timeSlider.getWidth(), 18, juce::Justification::centred);
}

void ReverserPanel::resized()
{
    bypassButton.setBounds(12, 10, 36, 22);
    mixSlider.setBounds(getWidth() / 2 - 40, 60, 80, 80);
    timeSlider.setBounds(getWidth() / 2 - 25, 180, 50, 50);
}

NoiseGatePanel::NoiseGatePanel(ModularFXAudioProcessor& p) : processor(p)
{
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(true, juce::dontSendNotification);
    bypassButton.onClick = [this] { processor.noiseGate.setBypassed(!bypassButton.getToggleState()); };
    addAndMakeVisible(bypassButton);

    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible(attackSlider);

    releaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible(releaseSlider);

    threshSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    threshSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible(threshSlider);

    attackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_ATTACK", attackSlider);
    releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_RELEASE", releaseSlider);
    threshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_THRESH", threshSlider);
}

void NoiseGatePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0F0F0F));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Noise Gate", 55, 10, 120, 24, juce::Justification::left);

    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("Threshold", threshSlider.getX(), threshSlider.getY() - 16, threshSlider.getWidth(), 16, juce::Justification::centred);
    g.drawText("Attack", attackSlider.getX(), attackSlider.getY() - 16, attackSlider.getWidth(), 16, juce::Justification::centred);
    g.drawText("Release", releaseSlider.getX(), releaseSlider.getY() - 16, releaseSlider.getWidth(), 16, juce::Justification::centred);
}

void NoiseGatePanel::resized()
{
    bypassButton.setBounds(12, 10, 36, 22);
    int w = getWidth() - 30;
    threshSlider.setBounds(15, 60, w, 35);
    attackSlider.setBounds(15, 140, w, 45);
    releaseSlider.setBounds(15, 215, w, 45);
}

ChorusPanel::ChorusPanel(ModularFXAudioProcessor& p) : processor(p)
{
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(true, juce::dontSendNotification);
    bypassButton.onClick = [this] { processor.chorus.setBypassed(!bypassButton.getToggleState()); };
    addAndMakeVisible(bypassButton);

    auto setupKnob = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
    };

    setupKnob(depthSlider); setupKnob(amountSlider); setupKnob(speedSlider); setupKnob(bassSlider); setupKnob(trebleSlider);
    depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_DEPTH", depthSlider);
    amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_AMOUNT", amountSlider);
    speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_SPEED", speedSlider);
    bassAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_BASS", bassSlider);
    trebleAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_TREBLE", trebleSlider);
}

void ChorusPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF3B627C));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Chorus Ensemble", 55, 10, 160, 24, juce::Justification::left);
}

void ChorusPanel::resized()
{
    bypassButton.setBounds(12, 10, 36, 22);
    int third = getWidth() / 3;
    depthSlider.setBounds(20, 60, 55, 55);
    amountSlider.setBounds(getWidth() - 75, 60, 55, 55);

    speedSlider.setBounds(5, 160, third - 10, 50);
    bassSlider.setBounds(third + 5, 160, third - 10, 50);
    trebleSlider.setBounds(third * 2 + 5, 160, third - 10, 50);
}

ModularFXAudioProcessorEditor::ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      reverserPanel(p), noiseGatePanel(p), chorusPanel(p)
{
    addAndMakeVisible(reverserPanel);
    addAndMakeVisible(noiseGatePanel);
    addAndMakeVisible(chorusPanel);

    setSize(900, 320);
}

void ModularFXAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("Modular FX Suite", 16, 10, 200, 30, juce::Justification::left);
}

void ModularFXAudioProcessorEditor::resized()
{
    auto rackArea = getLocalBounds().removeFromBottom(getHeight() - 45).reduced(10);
    int panelWidth = (rackArea.getWidth() - 20) / 3;

    reverserPanel.setBounds(rackArea.removeFromLeft(panelWidth));
    rackArea.removeFromLeft(10);

    noiseGatePanel.setBounds(rackArea.removeFromLeft(panelWidth));
    rackArea.removeFromLeft(10);

    chorusPanel.setBounds(rackArea.removeFromLeft(panelWidth));
}
