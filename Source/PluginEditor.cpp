#include "PluginEditor.h"

namespace { constexpr int kTopBar = 46; constexpr int kPad = 10; constexpr int kLeftW = 200; constexpr int kRightW = 150; constexpr int kCentreW = 880; }

ModularFXAudioProcessorEditor::ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetPanel(p), outputPanel(p)
{
    addAndMakeVisible(presetPanel);
    addAndMakeVisible(outputPanel);
    viewport.setViewedComponent(&rack, false);
    viewport.setScrollBarsShown(false, true);
    viewport.setScrollBarThickness(10);
    addAndMakeVisible(viewport);

    rack.addChildComponent(addTile);
    addTile.onClick = [this] { showAddMenu(); };

    browser.onPick  = [this](int type) { addModule(type); };
    browser.onClose = [this] { closeBrowser(); };
    addChildComponent(browser);

    audioProcessor.rackChanged.addChangeListener(this);
    setSize(kLeftW + kCentreW + kRightW, kTopBar + kCardH + 2 * kPad + 12);
    layoutRack();
}

ModularFXAudioProcessorEditor::~ModularFXAudioProcessorEditor()
{
    audioProcessor.rackChanged.removeChangeListener(this);
}

void ModularFXAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    g.fillAll(juce::Colour(0xFF0F1013));
    g.setColour(juce::Colour(0xFF24262B));
    g.drawHorizontalLine(kTopBar - 1, 0.0f, (float) getWidth());
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText("Modular FX Suite", 16, 8, 220, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(juce::String((int) audioProcessor.getRack().size()) + " / " + juce::String(kMaxRack) + " in rack",
               240, 8, 120, 30, juce::Justification::centredLeft);
}

void ModularFXAudioProcessorEditor::resized()
{
    presetPanel.setBounds(0, kTopBar, kLeftW, getHeight() - kTopBar);
    outputPanel.setBounds(getWidth() - kRightW, kTopBar, kRightW, getHeight() - kTopBar);
    viewport.setBounds(kLeftW, kTopBar, getWidth() - kLeftW - kRightW, getHeight() - kTopBar);
    browser.setBounds(getLocalBounds());
    layoutRack();
}

void ModularFXAudioProcessorEditor::layoutRack()
{
    const auto entries = audioProcessor.getRack();

    // drop cards whose slot is empty or now holds another effect, create the missing ones
    for (int slot = 0; slot < kMaxRack; ++slot)
    {
        auto& card = cards[(size_t) slot];
        const int type = audioProcessor.getSlotType(slot);
        if (card != nullptr && card->getType() != type)
            card.reset();
        if (card == nullptr && type != 0)
        {
            card = createCard(audioProcessor, slot, type);
            card->onMoveRequested   = [this](int s, int dir) { moveModule(s, dir); };
            card->onRemoveRequested = [this](int s) { removeModule(s); };
            card->onDragStart = [this](int s) { dragBegin(s); };
            card->onDragMove  = [this](int s, int x) { dragMove(s, x); };
            card->onDragEnd   = [this](int s) { dragEnd(s); };
            card->setVisible(false);
            rack.addChildComponent(*card);
        }
        if (card != nullptr) card->setVisible(false);
    }

    int x = kPad;
    for (const auto& e : entries)
    {
        auto& card = cards[(size_t) e.slot];
        if (card == nullptr) continue;
        card->setBounds(x, kPad, kCardW, kCardH);
        card->setVisible(true);
        x += kCardW + kPad;
    }

    const bool full = (int) entries.size() >= kMaxRack;
    addTile.setVisible(! full);
    if (! full)
    {
        addTile.setBounds(x, kPad, kCardW, kCardH);
        x += kCardW + kPad;
    }

    rack.setSize(juce::jmax(viewport.getMaximumVisibleWidth(), x), kCardH + 2 * kPad);
    presetPanel.refresh();
    repaint();
}

void ModularFXAudioProcessorEditor::showAddMenu()
{
    if ((int) audioProcessor.getRack().size() >= kMaxRack) return;
    browser.setBounds(getLocalBounds());
    browser.open();
}

void ModularFXAudioProcessorEditor::closeBrowser()
{
    browser.setVisible(false);
    repaint();
}

void ModularFXAudioProcessorEditor::addModule(int type)
{
    if (audioProcessor.addEffect(type) < 0) return;
    layoutRack();
    viewport.setViewPosition(juce::jmax(0, rack.getWidth() - viewport.getMaximumVisibleWidth()), 0);   // reveal the new card
}

void ModularFXAudioProcessorEditor::moveModule(int slot, int direction)
{
    auto order = std::vector<int>();
    for (auto& e : audioProcessor.getRack()) order.push_back(e.slot);
    const auto it = std::find(order.begin(), order.end(), slot);
    if (it == order.end()) return;
    const int idx = (int) std::distance(order.begin(), it);
    const int newIdx = idx + direction;
    if (newIdx < 0 || newIdx >= (int) order.size()) return;
    std::swap(order[(size_t) idx], order[(size_t) newIdx]);
    audioProcessor.setRackOrder(order);
    layoutRack();
}

void ModularFXAudioProcessorEditor::removeModule(int slot)
{
    // This runs inside the card's own button callback, so the card itself is destroyed a moment later.
    if (slot >= 0 && slot < kMaxRack && cards[(size_t) slot] != nullptr) cards[(size_t) slot]->setVisible(false);
    audioProcessor.removeEffect(slot);
    juce::Component::SafePointer<ModularFXAudioProcessorEditor> safe(this);
    juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->layoutRack(); });
}

// ---- drag a card by its header to change the signal order
void ModularFXAudioProcessorEditor::dragBegin(int slot)
{
    dragOrder.clear();
    for (auto& e : audioProcessor.getRack()) dragOrder.push_back(e.slot);
    if (cards[(size_t) slot] != nullptr) cards[(size_t) slot]->toFront(false);
}

void ModularFXAudioProcessorEditor::dragMove(int id, int newX)
{
    if (dragOrder.empty() || cards[(size_t) id] == nullptr) return;
    auto& dragged = *cards[(size_t) id];
    const int n = (int) dragOrder.size();

    newX = juce::jlimit(0, juce::jmax(0, rack.getWidth() - kCardW), newX);
    dragged.setTopLeftPosition(newX, kPad);

    const int position = juce::jlimit(0, n - 1, (newX + kCardW / 2 - kPad) / (kCardW + kPad));
    const auto it = std::find(dragOrder.begin(), dragOrder.end(), id);
    if (it != dragOrder.end() && (int) std::distance(dragOrder.begin(), it) != position)
    {
        dragOrder.erase(it);
        dragOrder.insert(dragOrder.begin() + position, id);
    }

    // the other cards slide aside to show where the dragged one will land
    for (int i = 0; i < n; ++i)
    {
        const int other = dragOrder[(size_t) i];
        if (other == id || cards[(size_t) other] == nullptr) continue;
        const juce::Rectangle<int> target(kPad + i * (kCardW + kPad), kPad, kCardW, kCardH);
        if (cards[(size_t) other]->getBounds() != target)
            juce::Desktop::getInstance().getAnimator().animateComponent(cards[(size_t) other].get(), target, 1.0f, 120, false, 1.0, 1.0);
    }

    // scroll when the card is pushed against either edge of the window
    const int viewX = viewport.getViewPositionX();
    const int inView = dragged.getX() - viewX;
    if (inView < 24)
        viewport.setViewPosition(juce::jmax(0, viewX - 24), 0);
    else if (inView + kCardW > viewport.getMaximumVisibleWidth() - 24)
        viewport.setViewPosition(viewX + 24, 0);
}

void ModularFXAudioProcessorEditor::dragEnd(int)
{
    juce::Desktop::getInstance().getAnimator().cancelAllAnimations(false);
    if (! dragOrder.empty())
        audioProcessor.setRackOrder(dragOrder);
    dragOrder.clear();
    layoutRack();
}
