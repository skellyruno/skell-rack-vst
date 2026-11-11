#include "PluginEditor.h"

namespace { constexpr int kTopBar = 46; constexpr int kPad = 10; constexpr int kLeftW = 200; constexpr int kRightW = 150; constexpr int kCentreW = 880; }

namespace { constexpr int kNativeW = kLeftW + kCentreW + kRightW; constexpr int kNativeH = kTopBar + kCardH + 2 * kPad + 12; }

ModularFXAudioProcessorEditor::ModularFXAudioProcessorEditor(ModularFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), meters(p), presetPanel(p), outputPanel(p), presetBrowser(p)
{
    content.onPaint = [this](juce::Graphics& g) { paintNative(g); };
    addAndMakeVisible(content);

    content.addAndMakeVisible(meters);
    helpTour.setButtonText("?");
    helpTour.setTooltip("Help / Show Tutorial");
    helpTour.onClick=[this]{showGuidedTour(false);};
    content.addAndMakeVisible(helpTour);
    auto arrow = [](bool undo) { return [undo](juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool) {
        auto c = r.reduced(4.0f);
        g.setColour(juce::Colour(over ? 0xFF30333B : 0xFF23262C)); g.fillRoundedRectangle(r, 8.0f);
        juce::Path p; const float cx = r.getCentreX(), cy = r.getCentreY(), s = undo ? -1.0f : 1.0f;
        p.startNewSubPath(cx - 6.0f * s, cy + 3.0f); p.cubicTo(cx - 6.0f * s, cy - 6.0f, cx + 6.0f * s, cy - 6.0f, cx + 6.0f * s, cy + 3.0f);
        p.startNewSubPath(cx + 6.0f * s, cy + 3.0f); p.lineTo(cx + 6.0f * s, cy - 2.0f); p.startNewSubPath(cx + 6.0f * s, cy + 3.0f); p.lineTo(cx + 1.5f * s, cy + 3.0f);
        (void) c; g.setColour(juce::Colours::white); g.strokePath(p, juce::PathStrokeType(1.6f)); }; };
    undoButton.painter = arrow(true);  redoButton.painter = arrow(false);
    auto abPaint = [this](int slot) { return [this, slot](juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool) {
        const bool active = audioProcessor.getActiveAB() == slot;
        g.setColour(active ? juce::Colour(0xFF22C7DB) : juce::Colour(over ? 0xFF30333B : 0xFF23262C)); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(active ? juce::Colour(0xFF0B1114) : juce::Colours::white.withAlpha(audioProcessor.abHasData(slot) || slot == 0 ? 0.95f : 0.55f));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(slot == 0 ? "A" : "B", r, juce::Justification::centred); }; };
    abButtonA.painter = abPaint(0);  abButtonB.painter = abPaint(1);
    undoButton.setTooltip("Undo (Ctrl+Z)");  redoButton.setTooltip("Redo (Ctrl+Y)");
    abButtonA.setTooltip("Sound A");  abButtonB.setTooltip("Sound B: tweak, then flip between A and B to compare");
    undoButton.onClick = [this] { audioProcessor.undo(); refreshHistoryButtons(); };
    redoButton.onClick = [this] { audioProcessor.redo(); refreshHistoryButtons(); };
    abButtonA.onClick  = [this] { audioProcessor.switchAB(0); refreshHistoryButtons(); };
    abButtonB.onClick  = [this] { audioProcessor.switchAB(1); refreshHistoryButtons(); };
    for (auto* b : { &abButtonA, &abButtonB, &undoButton, &redoButton }) content.addAndMakeVisible(*b);
    setWantsKeyboardFocus(true);
    content.addAndMakeVisible(presetPanel);
    content.addAndMakeVisible(outputPanel);
    viewport.setViewedComponent(&rack, false);
    viewport.setScrollBarsShown(false, true);
    viewport.setScrollBarThickness(10);
    content.addAndMakeVisible(viewport);

    rack.addChildComponent(addTile);
    addTile.onClick = [this] { showAddMenu(); };

    browser.onPick  = [this](const FxCatalogEntry& entry) { addModule(entry); };
    browser.onClose = [this] { closeBrowser(); };
    content.addChildComponent(browser);
    presetPanel.onBrowse = [this] { presetBrowser.setBounds(0, 0, kNativeW, kNativeH); presetBrowser.open(); };
    presetBrowser.onClose = [this] { presetPanel.refresh(); repaint(); };
    content.addChildComponent(presetBrowser);

    layoutNative();
    audioProcessor.rackChanged.addChangeListener(this);
    audioProcessor.updateHistory();
    startTimer(100);

    // resizable, always keeping the shape of the design; starts 25% smaller than the full-size layout
    const float scale = juce::jlimit(0.5f, 1.5f, audioProcessor.uiScale.load());
    setResizable(true, true);
    setResizeLimits(kNativeW / 2, kNativeH / 2, kNativeW * 3 / 2, kNativeH * 3 / 2);
    if (auto* cb = getConstrainer()) cb->setFixedAspectRatio((double) kNativeW / (double) kNativeH);
    setSize(juce::roundToInt((float) kNativeW * scale), juce::roundToInt((float) kNativeH * scale));
    layoutRack();
    showGuidedTour(true);
}

ModularFXAudioProcessorEditor::~ModularFXAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.rackChanged.removeChangeListener(this);
}

void ModularFXAudioProcessorEditor::showGuidedTour(bool firstRun)
{
    std::vector<skellcore::ui::GuidedTourStep> steps;
    steps.emplace_back(&presetPanel,"START WITH A PRESET",
        "Load a preset or save your own effect-rack setup.");
    if(addTile.isVisible())
        steps.emplace_back(&addTile,"ADD AN EFFECT",
            "Use Add Effect to browse available modules and build your chain.");
    steps.emplace_back(&viewport,"YOUR EFFECT RACK",
        "Effect cards process audio in order. Drag cards to change their routing.");
    steps.emplace_back(&outputPanel,"FINAL OUTPUT",
        "Adjust overall output and check levels after your effects.");
    steps.emplace_back(&abButtonA,"COMPARE A AND B",
        "A and B keep two versions of your rack for fast comparison.",
        std::function<void()>{},&abButtonB);
    steps.emplace_back(&undoButton,"UNDO CHANGES",
        "Undo or redo edits if you want to try a different rack arrangement.",
        std::function<void()>{},&redoButton);
    guidedTour.setPalette({juce::Colour(0xff22c7db),juce::Colour(0xff181a1f),
        juce::Colours::white,juce::Colour(0xffa2aebd),juce::Colour(0xd9000000)});
    guidedTour.setSteps(std::move(steps));
    if(firstRun)guidedTour.startIfFirstRun();
    else guidedTour.replay();
}
void ModularFXAudioProcessorEditor::paintNative(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0F1013));
    g.setColour(juce::Colour(0xFF24262B));
    g.drawHorizontalLine(kTopBar - 1, 0.0f, (float) kNativeW);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText("Modular FX Suite", 16, 8, 220, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(juce::String((int) audioProcessor.getRack().size()) + " / " + juce::String(kMaxRack) + " in rack",
               240, 8, 120, 30, juce::Justification::centredLeft);
}

void ModularFXAudioProcessorEditor::layoutNative()
{
    content.setBounds(0, 0, kNativeW, kNativeH);
    meters.setBounds(400, 6, 400, 34);
    abButtonA.setBounds(826, 9, 30, 28);   abButtonB.setBounds(860, 9, 30, 28);
    undoButton.setBounds(906, 9, 34, 28);  redoButton.setBounds(944, 9, 34, 28);
    presetPanel.setBounds(0, kTopBar, kLeftW, kNativeH - kTopBar);
    outputPanel.setBounds(kNativeW - kRightW, kTopBar, kRightW, kNativeH - kTopBar);
    viewport.setBounds(kLeftW, kTopBar, kNativeW - kLeftW - kRightW, kNativeH - kTopBar);
    browser.setBounds(0, 0, kNativeW, kNativeH);
}

void ModularFXAudioProcessorEditor::refreshHistoryButtons()
{
    undoButton.setEnabled(audioProcessor.canUndo());  undoButton.setAlpha(audioProcessor.canUndo() ? 1.0f : 0.4f);
    redoButton.setEnabled(audioProcessor.canRedo());  redoButton.setAlpha(audioProcessor.canRedo() ? 1.0f : 0.4f);
    abButtonA.repaint();  abButtonB.repaint();
}

bool ModularFXAudioProcessorEditor::keyPressed(const juce::KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown();
    if (cmd && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        if (k.getModifiers().isShiftDown()) audioProcessor.redo(); else audioProcessor.undo();
        refreshHistoryButtons();  return true;
    }
    if (cmd && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { audioProcessor.redo(); refreshHistoryButtons(); return true; }
    return false;
}

void ModularFXAudioProcessorEditor::resized()
{
    if (getWidth() <= 0) return;
    const float scale = (float) getWidth() / (float) kNativeW;
    content.setTransform(juce::AffineTransform::scale(scale));
    audioProcessor.uiScale.store(scale);
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
    browser.setBounds(0, 0, kNativeW, kNativeH);
    browser.open();
}

void ModularFXAudioProcessorEditor::closeBrowser()
{
    browser.setVisible(false);
    repaint();
}

void ModularFXAudioProcessorEditor::addModule(const FxCatalogEntry& entry)
{
    const int slot = audioProcessor.addEffect(entry.id);
    if (slot < 0) return;
    for (const auto& v : entry.initial) audioProcessor.setSlotParam(slot, v.first, v.second);
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
