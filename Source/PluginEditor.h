#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

class ModuleCardBase : public juce::Component
{
public:
    ModuleCardBase(const juce::String& title, int id)
        : moduleTitle(title), moduleId(id)
    {
        bypassButton.setClickingTogglesState(true);
        bypassButton.setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(bypassButton);

        infoButton.setButtonText("i");
        addAndMakeVisible(infoButton);

        moveLeftButton.setButtonText("<");
        addAndMakeVisible(moveLeftButton);

        moveRightButton.setButtonText(">");
        addAndMakeVisible(moveRightButton);

        removeButton.setButtonText("X");
        addAndMakeVisible(removeButton);
    }

    virtual ~ModuleCardBase() override = default;

    std::function<void(int, int)> onMoveRequested;
    std::function<void(int)> onRemoveRequested;

    int getModuleID() const { return moduleId; }

protected:
    void paintHeader(juce::Graphics& g, juce::Colour textColour)
    {
        g.setColour(textColour);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText(moduleTitle, 85, 6, 120, 24, juce::Justification::left);

        // 4 Corner Drag Handles
        g.setColour(textColour.withAlpha(0.3f));
        int w = getWidth(), h = getHeight();
        if (w > 20 && h > 20)
        {
            g.fillRect(3, 3, 6, 6);
            g.fillRect(w - 9, 3, 6, 6);
            g.fillRect(3, h - 9, 6, 6);
            g.fillRect(w - 9, h - 9, 6, 6);
        }
    }

    void resizedHeader()
    {
        bypassButton.setBounds(12, 6, 36, 22);
        infoButton.setBounds(52, 6, 22, 22);
        moveLeftButton.setBounds(getWidth() - 80, 6, 22, 22);
        moveRightButton.setBounds(getWidth() - 55, 6, 22, 22);
        removeButton.setBounds(getWidth() - 30, 6, 22, 22);
    }

    void setupHeaderCallbacks()
    {
        moveLeftButton.onClick = [this] { if (onMoveRequested) onMoveRequested(moduleId, -1); };
        moveRightButton.onClick = [this] { if (onMoveRequested) onMoveRequested(moduleId, 1); };
        removeButton.onClick = [this] { if (onRemoveRequested) onRemoveRequested(moduleId); };
    }

    juce::String moduleTitle;
    int moduleId;
    juce::ToggleButton bypassButton;
    juce::TextButton infoButton, moveLeftButton, moveRightButton, removeButton;
};

// --- Reverser Card ---
class ReverserCard : public ModuleCardBase
{
public:
    ReverserCard(ModularFXAudioProcessor& p)
        : ModuleCardBase("Reverser", ID_Reverser), processor(p)
    {
        setupHeaderCallbacks();

        mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        mixSlider.setLookAndFeel(&purpleStyle);
        addAndMakeVisible(mixSlider);

        timeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        timeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        timeSlider.setLookAndFeel(&purpleStyle);
        addAndMakeVisible(timeSlider);

        mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "REV_MIX", mixSlider);
        timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "REV_TIME", timeSlider);

        beatButton.setButtonText("beat");
        beatButton.setClickingTogglesState(true);
        addAndMakeVisible(beatButton);

        bypassButton.onClick = [this] { processor.reverser.setBypassed(!bypassButton.getToggleState()); };
    }

    ~ReverserCard() override
    {
        mixSlider.setLookAndFeel(nullptr);
        timeSlider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF121212));
        paintHeader(g, juce::Colours::white);

        g.setColour(juce::Colours::grey);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Mix", mixSlider.getX(), mixSlider.getBottom() + 4, mixSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Time", timeSlider.getX() - 30, timeSlider.getBounds().getCentreY() - 10, 40, 20, juce::Justification::right);
    }

    void resized() override
    {
        resizedHeader();
        mixSlider.setBounds(getWidth() / 2 - 50, 45, 100, 100);
        timeSlider.setBounds(getWidth() / 2 - 25, 175, 50, 50);
        beatButton.setBounds(getWidth() / 2 + 30, 185, 45, 28);
    }

private:
    ModularFXAudioProcessor& processor;
    PurpleArcKnobLookAndFeel purpleStyle;
    juce::Slider mixSlider, timeSlider;
    juce::TextButton beatButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach, timeAttach;
};

// --- Noise Gate Card ---
class NoiseGateCard : public ModuleCardBase
{
public:
    NoiseGateCard(ModularFXAudioProcessor& p)
        : ModuleCardBase("Noise Gate", ID_NoiseGate), processor(p)
    {
        setupHeaderCallbacks();

        attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        attackSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(attackSlider);

        releaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        releaseSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(releaseSlider);

        threshSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        threshSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        attackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_ATTACK", attackSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_RELEASE", releaseSlider);
        threshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "GATE_THRESH", threshSlider);

        bypassButton.onClick = [this] { processor.noiseGate.setBypassed(!bypassButton.getToggleState()); };
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF0F0F0F));
        paintHeader(g, juce::Colours::white);

        auto bounds = getLocalBounds();
        if (bounds.getHeight() < 180) return;

        auto scopeArea = bounds.removeFromTop(180).removeFromBottom(130).toFloat();
        if (scopeArea.getHeight() <= 0.0f) return;

        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRect(scopeArea);

        juce::Path wave;
        wave.startNewSubPath(scopeArea.getX(), scopeArea.getCentreY());
        for (float x = scopeArea.getX(); x < scopeArea.getRight(); x += 5.0f)
        {
            float y = scopeArea.getCentreY() + std::sin(x * 0.05f) * 35.0f;
            wave.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xFFFFB703));
        g.strokePath(wave, juce::PathStrokeType(1.5f));

        float threshVal = static_cast<float>(threshSlider.getValue());
        float normalizedThresh = juce::jmap(threshVal, -60.0f, 0.0f, scopeArea.getBottom(), scopeArea.getY());
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawDashedLine(juce::Line<float>(scopeArea.getX(), normalizedThresh, scopeArea.getRight(), normalizedThresh), nullptr, 0, 2);

        thresholdPillBounds = juce::Rectangle<float>(scopeArea.getCentreX() - 55.0f, normalizedThresh - 14.0f, 110.0f, 28.0f);
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(thresholdPillBounds, 14.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(thresholdPillBounds, 14.0f, 1.2f);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("Threshold  ↕", thresholdPillBounds, juce::Justification::centred);

        g.drawText(juce::String(threshVal, 1) + " dB", static_cast<int>(scopeArea.getX() + 10), static_cast<int>(normalizedThresh - 22), 70, 20, juce::Justification::left);

        g.setFont(juce::FontOptions(12.0f));
        g.setColour(juce::Colours::grey);
        g.drawText("Attack", 20, getHeight() - 55, 100, 18, juce::Justification::left);
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(static_cast<float>(attackSlider.getValue()), 0) + " ms", 20, getHeight() - 35, 100, 20, juce::Justification::left);

        g.setColour(juce::Colours::grey);
        g.drawText("Release", 150, getHeight() - 55, 100, 18, juce::Justification::left);
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(static_cast<float>(releaseSlider.getValue()), 0) + " ms", 150, getHeight() - 35, 100, 20, juce::Justification::left);
    }

    void resized() override
    {
        resizedHeader();
        attackSlider.setBounds(0, 0, 0, 0);
        releaseSlider.setBounds(0, 0, 0, 0);
        threshSlider.setBounds(0, 0, 0, 0);
    }

    void mouseDown(const juce::MouseEvent& e) override { if (thresholdPillBounds.contains(e.position)) isDraggingPill = true; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isDraggingPill)
        {
            auto bounds = getLocalBounds();
            if (bounds.getHeight() < 180) return;
            auto scopeArea = bounds.removeFromTop(180).removeFromBottom(130).toFloat();
            if (scopeArea.getHeight() <= 0.0f) return;

            float norm = juce::jlimit(0.0f, 1.0f, (e.position.y - scopeArea.getY()) / scopeArea.getHeight());
            threshSlider.setValue(juce::jmap(norm, 1.0f, 0.0f, -60.0f, 0.0f));
            repaint();
        }
    }

private:
    ModularFXAudioProcessor& processor;
    juce::Slider attackSlider, releaseSlider, threshSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach, releaseAttach, threshAttach;
    juce::Rectangle<float> thresholdPillBounds;
    bool isDraggingPill = false;
};

// --- Chorus Card ---
class ChorusCard : public ModuleCardBase
{
public:
    ChorusCard(ModularFXAudioProcessor& p)
        : ModuleCardBase("Chorus Ensemble", ID_Chorus), processor(p)
    {
        setupHeaderCallbacks();

        auto setupKnob = [this](juce::Slider& slider) {
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setLookAndFeel(&teardropStyle);
            addAndMakeVisible(slider);
        };

        setupKnob(depthSlider); setupKnob(amountSlider); setupKnob(speedSlider); setupKnob(bassSlider); setupKnob(trebleSlider);
        depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_DEPTH", depthSlider);
        amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_AMOUNT", amountSlider);
        speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_SPEED", speedSlider);
        bassAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_BASS", bassSlider);
        trebleAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "CHO_TREBLE", trebleSlider);

        bypassButton.onClick = [this] { processor.chorus.setBypassed(!bypassButton.getToggleState()); };
    }

    ~ChorusCard() override
    {
        depthSlider.setLookAndFeel(nullptr); amountSlider.setLookAndFeel(nullptr);
        speedSlider.setLookAndFeel(nullptr); bassSlider.setLookAndFeel(nullptr); trebleSlider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF6B93AC));
        SeigaihaDrawer::drawPattern(g, getLocalBounds().toFloat(), juce::Colour(0xFF3B627C));
        paintHeader(g, juce::Colours::white);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("Depth", depthSlider.getX(), depthSlider.getBottom() + 4, depthSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Amount", amountSlider.getX(), amountSlider.getBottom() + 4, amountSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Speed", speedSlider.getX(), speedSlider.getBottom() + 4, speedSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Bass", bassSlider.getX(), bassSlider.getBottom() + 4, bassSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Treble", trebleSlider.getX(), trebleSlider.getBottom() + 4, trebleSlider.getWidth(), 18, juce::Justification::centred);
    }

    void resized() override
    {
        resizedHeader();
        depthSlider.setBounds(getWidth() / 4 - 32, 55, 64, 64);
        amountSlider.setBounds((getWidth() * 3) / 4 - 32, 55, 64, 64);

        int third = getWidth() / 3;
        speedSlider.setBounds(third / 2 - 28, 165, 56, 56);
        bassSlider.setBounds(third + third / 2 - 28, 165, 56, 56);
        trebleSlider.setBounds(third * 2 + third / 2 - 28, 165, 56, 56);
    }

private:
    ModularFXAudioProcessor& processor;
    TeardropKnobLookAndFeel teardropStyle;
    juce::Slider depthSlider, amountSlider, speedSlider, bassSlider, trebleSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach, amountAttach, speedAttach, bassAttach, trebleAttach;
};

// --- Main Editor ---
class ModularFXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p);
    ~ModularFXAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ModularFXAudioProcessor& audioProcessor;
    juce::ComboBox addModuleBox;

    ReverserCard reverserCard;
    NoiseGateCard noiseGateCard;
    ChorusCard chorusCard;

    void updateCardCallbacks();
};
