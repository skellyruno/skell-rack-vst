#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModuleCards.h"
#include "EffectBrowser.h"
#include "MasterPanels.h"

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

// The empty slot at the end of the rack: click to open the effect browser.
class AddEffectTile : public juce::Component
{
public:
    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(hover ? juce::Colour(0xFF1D2025) : juce::Colour(0xFF181A1F));
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(juce::Colour(0xFF262930));
        g.drawRoundedRectangle(r, 12.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(hover ? 1.0f : 0.85f));
        const float cx = r.getCentreX(), cy = r.getCentreY() - 14.0f;
        g.drawLine(cx - 9.0f, cy, cx + 9.0f, cy, 2.0f);
        g.drawLine(cx, cy - 9.0f, cx, cy + 9.0f, 2.0f);
        g.setFont(juce::Font(juce::FontOptions(17.0f)));
        g.drawText("Add Effect", getLocalBounds().withY((int) cy + 18).withHeight(26), juce::Justification::centred);
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override  { if (getLocalBounds().contains(e.getPosition()) && onClick) onClick(); }

private:
    bool hover = false;
};

class RackPanel : public juce::Component
{
public:
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF14151A)); }
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
    void closeBrowser();
    void addModule(int type);
    void moveModule(int slot, int direction);
    void removeModule(int slot);
    void dragBegin(int slot);
    void dragMove(int slot, int newX);
    void dragEnd(int slot);

    ModularFXAudioProcessor& audioProcessor;
    std::vector<int> dragOrder;      // slots, working order while a card is being dragged

    // Declaration order matters: cards are destroyed first, then the viewport, then the rack they live in.
    PresetPanel presetPanel;
    OutputPanel outputPanel;
    RackPanel rack;
    AddEffectTile addTile;
    RackViewport viewport;
    std::array<std::unique_ptr<ModuleCardBase>, kMaxRack> cards;     // one per slot
    EffectBrowser browser;                                           // overlay, hidden until Add Effect is clicked
};
