#include "PluginEditor.h"

ModularFXAudioProcessorEditor::ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      reverserCard(p), noiseGateCard(p), chorusCard(p)
{
    addAndMakeVisible(reverserCard);
    addAndMakeVisible(noiseGateCard);
    addAndMakeVisible(chorusCard);

    updateCardCallbacks();
    setSize(900, 340);
}

void ModularFXAudioProcessorEditor::updateCardCallbacks()
{
    auto handleMove = [this](int id, int direction) {
        auto order = audioProcessor.getModuleOrder();
        auto it = std::find(order.begin(), order.end(), id);
        if (it != order.end())
        {
            int idx = static_cast<int>(std::distance(order.begin(), it));
            int newIdx = idx + direction;
            if (newIdx >= 0 && newIdx < static_cast<int>(order.size()))
            {
                std::swap(order[idx], order[newIdx]);
                audioProcessor.updateModuleOrder(order);
                resized();
                repaint();
            }
        }
    };

    reverserCard.onMoveRequested = handleMove;
    noiseGateCard.onMoveRequested = handleMove;
    chorusCard.onMoveRequested = handleMove;
}

void ModularFXAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("Modular FX Suite", 16, 10, 200, 30, juce::Justification::centredLeft);
}

void ModularFXAudioProcessorEditor::resized()
{
    auto rackArea = getLocalBounds().removeFromBottom(getHeight() - 45).reduced(10);
    auto currentOrder = audioProcessor.getModuleOrder();

    int numVisible = static_cast<int>(currentOrder.size());
    if (numVisible == 0) return;

    int modWidth = (rackArea.getWidth() - (10 * (numVisible - 1))) / numVisible;

    for (int id : currentOrder)
    {
        ModuleCardBase* card = nullptr;
        if (id == ID_Reverser) card = &reverserCard;
        else if (id == ID_NoiseGate) card = &noiseGateCard;
        else if (id == ID_Chorus) card = &chorusCard;

        if (card != nullptr)
        {
            card->setBounds(rackArea.removeFromLeft(modWidth));
            rackArea.removeFromLeft(10);
        }
    }
}
