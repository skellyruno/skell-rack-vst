#include "PluginEditor.h"

ModularFXAudioProcessorEditor::ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      reverserCard(p), noiseGateCard(p), chorusCard(p)
{
    addAndMakeVisible(reverserCard);
    addAndMakeVisible(noiseGateCard);
    addAndMakeVisible(chorusCard);

    addModuleBox.addItem("+ Add Reverser", ID_Reverser);
    addModuleBox.addItem("+ Add Noise Gate", ID_NoiseGate);
    addModuleBox.addItem("+ Add Chorus Ensemble", ID_Chorus);
    addModuleBox.onChange = [this] {
        int id = addModuleBox.getSelectedId();
        if (id <= 0) return;

        auto currentOrder = audioProcessor.getModuleOrder();
        if (std::find(currentOrder.begin(), currentOrder.end(), id) == currentOrder.end())
        {
            currentOrder.push_back(id);
            audioProcessor.updateModuleOrder(currentOrder);
        }

        juce::MessageManager::callAsync([this]() {
            addModuleBox.setSelectedId(0, juce::dontSendNotification);
            resized();
            repaint();
        });
    };
    addAndMakeVisible(addModuleBox);

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

    auto handleRemove = [this](int id) {
        auto order = audioProcessor.getModuleOrder();
        order.erase(std::remove(order.begin(), order.end(), id), order.end());
        audioProcessor.updateModuleOrder(order);
        resized();
        repaint();
    };

    reverserCard.onMoveRequested = handleMove;
    reverserCard.onRemoveRequested = handleRemove;

    noiseGateCard.onMoveRequested = handleMove;
    noiseGateCard.onRemoveRequested = handleRemove;

    chorusCard.onMoveRequested = handleMove;
    chorusCard.onRemoveRequested = handleRemove;
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
    addModuleBox.setBounds(getWidth() - 170, 10, 150, 24);

    auto rackArea = getLocalBounds().removeFromBottom(getHeight() - 45).reduced(10);
    auto currentOrder = audioProcessor.getModuleOrder();

    reverserCard.setVisible(std::find(currentOrder.begin(), currentOrder.end(), ID_Reverser) != currentOrder.end());
    noiseGateCard.setVisible(std::find(currentOrder.begin(), currentOrder.end(), ID_NoiseGate) != currentOrder.end());
    chorusCard.setVisible(std::find(currentOrder.begin(), currentOrder.end(), ID_Chorus) != currentOrder.end());

    int numVisible = static_cast<int>(currentOrder.size());
    if (numVisible == 0) return;

    int modWidth = (rackArea.getWidth() - (10 * (numVisible - 1))) / numVisible;

    for (int id : currentOrder)
    {
        ModuleCardBase* card = nullptr;
        if (id == ID_Reverser) card = &reverserCard;
        else if (id == ID_NoiseGate) card = &noiseGateCard;
        else if (id == ID_Chorus) card = &chorusCard;

        if (card != nullptr && card->isVisible())
        {
            card->setBounds(rackArea.removeFromLeft(modWidth));
            rackArea.removeFromLeft(10);
        }
    }
}
