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

    juce::Component::SafePointer<ModularFXAudioProcessorEditor> safeThis(this);
    addModuleBox.onChange = [safeThis] {
        if (safeThis == nullptr) return;

        int id = safeThis->addModuleBox.getSelectedId();
        if (id <= 0) return;

        auto currentOrder = safeThis->audioProcessor.getModuleOrder();
        if (std::find(currentOrder.begin(), currentOrder.end(), id) == currentOrder.end())
        {
            currentOrder.push_back(id);
            safeThis->audioProcessor.updateModuleOrder(currentOrder);
        }

        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis != nullptr)
            {
                safeThis->addModuleBox.setSelectedId(0, juce::dontSendNotification);
                safeThis->resized();
                safeThis->repaint();
            }
        });
    };
    addAndMakeVisible(addModuleBox);

    updateCardCallbacks();
    setSize(900, 340);
}

ModularFXAudioProcessorEditor::~ModularFXAudioProcessorEditor()
{
    removeAllChildren();
}

void ModularFXAudioProcessorEditor::updateCardCallbacks()
{
    juce::Component::SafePointer<ModularFXAudioProcessorEditor> safeThis(this);

    auto handleMove = [safeThis](int id, int direction) {
        if (safeThis == nullptr) return;
        auto order = safeThis->audioProcessor.getModuleOrder();
        auto it = std::find(order.begin(), order.end(), id);
        if (it != order.end())
        {
            int idx = static_cast<int>(std::distance(order.begin(), it));
            int newIdx = idx + direction;
            if (newIdx >= 0 && newIdx < static_cast<int>(order.size()))
            {
                std::swap(order[idx], order[newIdx]);
                safeThis->audioProcessor.updateModuleOrder(order);
                safeThis->resized();
                safeThis->repaint();
            }
        }
    };

    auto handleRemove = [safeThis](int id) {
        if (safeThis == nullptr) return;
        auto order = safeThis->audioProcessor.getModuleOrder();
        order.erase(std::remove(order.begin(), order.end(), id), order.end());
        safeThis->audioProcessor.updateModuleOrder(order);
        safeThis->resized();
        safeThis->repaint();
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
    if (getWidth() <= 0 || getHeight() <= 0) return;
    g.fillAll(juce::Colour(0xFF1E1E1E));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Modular FX Suite", 16, 10, 200, 30, juce::Justification::centredLeft);
}

void ModularFXAudioProcessorEditor::resized()
{
    addModuleBox.setBounds(getWidth() - 170, 10, 150, 24);

    auto rackArea = getLocalBounds().removeFromBottom(getHeight() - 45).reduced(10);
    auto currentOrder = audioProcessor.getModuleOrder();

    reverserCard.setVisible(false);
    noiseGateCard.setVisible(false);
    chorusCard.setVisible(false);

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
            card->setVisible(true);
            card->setBounds(rackArea.removeFromLeft(modWidth));
            rackArea.removeFromLeft(10);
        }
    }
}
