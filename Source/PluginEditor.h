#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModuleCards.h"

// Mouse wheel scrolls the rack sideways.
class RackViewport : public juce::Viewport
{
public:
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto d = w;
        if (d.deltaX == 0.0f && d.deltaY != 0.0f) { d.deltaX = d.deltaY; d.deltaY = 0.0f; }
        juce::Viewport::mouseWheelMove(e, d);
    }
};

class ModularFXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::ChangeListener
{
public:
    explicit ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p);
    ~ModularFXAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { layoutRack(); }
    void layoutRack();
    void showAddMenu();
    void addModule(int type);
    void moveModule(int slot, int direction);
    void removeModule(int slot);
    void dragBegin(int slot);
    void dragMove(int slot, int newX);
    void dragEnd(int slot);

    ModularFXAudioProcessor& audioProcessor;
    juce::TextButton addButton { "+ Add Effect" };
    std::vector<int> dragOrder;      // slots, working order while a card is being dragged

    // Declaration order matters: cards are destroyed first, then the viewport, then the rack they live in.
    juce::Component rack;
    RackViewport viewport;
    std::array<std::unique_ptr<ModuleCardBase>, kMaxRack> cards;     // one per slot
};
