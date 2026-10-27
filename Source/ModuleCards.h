#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "FxCatalog.h"
#include "Modules/NewEffects.h"     // VisualEq::makeCoeffs, so the EQ curve matches the audio exactly

constexpr int kCardW = 280;
constexpr int kCardH = 340;
constexpr int kHeaderH = 40;

using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// ---- bindings between on-screen controls (real values) and slot parameters (plain 0..1 host values) ----
class SlotSliderAttachment : private juce::Slider::Listener
{
public:
    SlotSliderAttachment(juce::AudioProcessorValueTreeState& apvts, int slot, const FxInfo& info,
                         const juce::String& specId, juce::Slider& s)
        : slider(s),
          spec(info.params[(size_t) specIndex(info, specId)]),
          attach(*apvts.getParameter(slotParamId(slot, specIndex(info, specId))), [this](float v) { fromParam(v); })
    {
        juce::NormalisableRange<double> r(spec.minV, spec.maxV, spec.step);
        if (spec.skewCentre > 0.0f) r.setSkewForCentre(spec.skewCentre);
        slider.setNormalisableRange(r);
        slider.setDoubleClickReturnValue(true, spec.defV);
        slider.textFromValueFunction = [sp = spec](double v) { return formatSpecValue(sp, (float) v); };
        slider.valueFromTextFunction = [sp = spec](const juce::String& t) { return (double) parseSpecValue(sp, t); };
        attach.sendInitialUpdate();
        slider.updateText();
        slider.addListener(this);
    }

    ~SlotSliderAttachment() override { slider.removeListener(this); }

private:
    void fromParam(float norm)
    {
        ignore = true;
        slider.setValue(realFromNorm(spec, norm), juce::sendNotificationSync);
        ignore = false;
    }

    void sliderValueChanged(juce::Slider*) override
    {
        if (ignore) return;
        const float n = normFromReal(spec, (float) slider.getValue());
        if (slider.isMouseButtonDown()) attach.setValueAsPartOfGesture(n);
        else                            attach.setValueAsCompleteGesture(n);
    }
    void sliderDragStarted(juce::Slider*) override { attach.beginGesture(); }
    void sliderDragEnded(juce::Slider*) override   { attach.endGesture(); }

    juce::Slider& slider;
    ParamSpec spec;
    bool ignore = false;
    juce::ParameterAttachment attach;
};

class SlotComboAttachment : private juce::ComboBox::Listener
{
public:
    SlotComboAttachment(juce::AudioProcessorValueTreeState& apvts, int slot, const FxInfo& info,
                        const juce::String& specId, juce::ComboBox& c)
        : combo(c),
          spec(info.params[(size_t) specIndex(info, specId)]),
          attach(*apvts.getParameter(slotParamId(slot, specIndex(info, specId))), [this](float v) { fromParam(v); })
    {
        attach.sendInitialUpdate();
        combo.addListener(this);
    }

    ~SlotComboAttachment() override { combo.removeListener(this); }

private:
    void fromParam(float norm)
    {
        ignore = true;
        combo.setSelectedId((int) realFromNorm(spec, norm) + 1, juce::dontSendNotification);
        ignore = false;
    }
    void comboBoxChanged(juce::ComboBox*) override
    {
        if (ignore) return;
        attach.setValueAsCompleteGesture(normFromReal(spec, (float) (combo.getSelectedId() - 1)));
    }

    juce::ComboBox& combo;
    ParamSpec spec;
    bool ignore = false;
    juce::ParameterAttachment attach;
};

// Full-card text panel shown by the "i" button.
class InfoOverlay : public juce::Component
{
public:
    void setContent(const juce::String& t, const juce::String& body, juce::Colour accentColour)
    {
        title = t; text = body; accent = accentColour; repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.93f));
        auto r = getLocalBounds().reduced(16, 14);
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        g.drawText(title, r.removeFromTop(24), juce::Justification::centredLeft);
        r.removeFromTop(6);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(juce::FontOptions(13.5f)));
        g.drawFittedText(text, r.removeFromTop(r.getHeight() - 22), juce::Justification::topLeft, 12);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("click to close", r, juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent&) override { setVisible(false); }

private:
    juce::String title, text;
    juce::Colour accent { juce::Colours::white };
};

// Small flat header buttons in BandLab's style: power switch, info circle and close cross.
class HeaderButton : public juce::Button
{
public:
    enum Kind { Power, Info, Close };
    explicit HeaderButton(Kind k) : juce::Button({}), kind(k) { if (k == Power) setClickingTogglesState(true); setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    juce::Colour ink { juce::Colours::white };

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const float a = down ? 0.6f : (over ? 1.0f : 0.88f);
        g.setColour(ink.withAlpha(a));
        if (kind == Power)
        {
            auto pill = r.withSizeKeepingCentre(34.0f, 18.0f);
            const bool on = getToggleState();
            if (on) g.fillRoundedRectangle(pill, 9.0f);
            else    g.drawRoundedRectangle(pill.reduced(0.75f), 8.5f, 1.5f);
            const float kx = on ? pill.getRight() - 9.5f : pill.getX() + 9.5f;
            g.setColour(on ? ink.contrasting(1.0f).withAlpha(0.95f) : ink.withAlpha(a));
            g.fillEllipse(kx - 5.5f, pill.getCentreY() - 5.5f, 11.0f, 11.0f);
        }
        else if (kind == Info)
        {
            auto c = r.withSizeKeepingCentre(19.0f, 19.0f);
            g.drawEllipse(c, 1.5f);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText("i", c, juce::Justification::centred);
        }
        else
        {
            auto c = r.withSizeKeepingCentre(12.0f, 12.0f);
            g.drawLine(c.getX(), c.getY(), c.getRight(), c.getBottom(), 1.8f);
            g.drawLine(c.getX(), c.getBottom(), c.getRight(), c.getY(), 1.8f);
        }
    }

private:
    Kind kind;
};

// Header (power, info, move, remove), "i" overlay and bypass dimming shared by every card.
class ModuleCardBase : public juce::Component
{
public:
    ModuleCardBase(ModularFXAudioProcessor& p, int slotIndex, int type)
        : processor(p), slot(slotIndex), moduleId(type), info(getFxInfo(type))
    {
        jassert(info != nullptr);

        bypassButton.setClickingTogglesState(true);
        bypassButton.setTooltip("On / off");
        bypassButton.onStateChange = [this] { repaint(); };
        addAndMakeVisible(bypassButton);
        if (info != nullptr)
            bypassAttach = std::make_unique<ButtonAttachment>(p.apvts, slotOnId(slot), bypassButton);

        infoButton.onClick = [this] { showInfo(); };
        addAndMakeVisible(infoButton);

        moveLeftButton.setButtonText("<");
        moveRightButton.setButtonText(">");
        addChildComponent(moveLeftButton);          // kept for completeness; cards are moved by dragging them
        addChildComponent(moveRightButton);
        addAndMakeVisible(removeButton);

        moveLeftButton.onClick  = [this] { if (onMoveRequested)   onMoveRequested(slot, -1); };
        moveRightButton.onClick = [this] { if (onMoveRequested)   onMoveRequested(slot, 1); };
        removeButton.onClick    = [this] { if (onRemoveRequested) onRemoveRequested(slot); };

        overlay.setVisible(false);
        addChildComponent(overlay);
    }

    ~ModuleCardBase() override = default;

    std::function<void(int, int)> onMoveRequested;
    std::function<void(int)> onRemoveRequested;
    std::function<void(int)> onDragStart;              // grab the header to drag a card along the rack
    std::function<void(int, int)> onDragMove;          // (id, new x in rack coordinates)
    std::function<void(int)> onDragEnd;

    int getSlot() const { return slot; }
    int getType() const { return moduleId; }

    // The whole card is a drag handle. Knobs, sliders, buttons and menus take their own mouse clicks, so grabbing the
    // card anywhere else (background, labels, header, graphs) moves it without touching a setting.
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragging = false;
        pressed = ! e.mods.isPopupMenu();
        if (pressed)
            grabOffset = e.getEventRelativeTo(getParentComponent()).position.x - (float) getX();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! pressed) return;
        if (! dragging)
        {
            if (e.getDistanceFromDragStart() < 5) return;          // a plain click does nothing
            dragging = true;
            if (onDragStart) onDragStart(slot);
        }
        if (onDragMove)
            onDragMove(slot, (int) (e.getEventRelativeTo(getParentComponent()).position.x - grabOffset));
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging && onDragEnd) onDragEnd(slot);
        dragging = false;
        pressed = false;
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (! bypassButton.getToggleState())
        {
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(getLocalBounds().withTrimmedTop(kHeaderH));
        }
    }

protected:
    void paintHeader(juce::Graphics& g, juce::Colour textColour)
    {
        if (getWidth() <= 0 || getHeight() <= 0 || info == nullptr) return;
        g.setColour(textColour);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        juce::String title = info->name;
        if (moduleId == ID_OneKnob)                                   // the One Knobs card is named after its style
        {
            const auto& style = oneKnobStyleName((int) realParam("ONE_MODE"));
            if (style.isNotEmpty()) title = style;
        }
        for (auto* b : { &bypassButton, &infoButton, &removeButton })
            if (b->ink != textColour) { b->ink = textColour; b->repaint(); }
        g.drawText(title, 84, 6, getWidth() - 84 - 44, 24, juce::Justification::centred);

    }

    void resizedHeader()
    {
        bypassButton.setBounds(12, 6, 38, 22);
        infoButton.setBounds(54, 6, 24, 22);
        removeButton.setBounds(getWidth() - 32, 6, 24, 22);
        overlay.setBounds(getLocalBounds().withTrimmedTop(kHeaderH));
    }

    void showInfo()
    {
        if (info == nullptr) return;
        overlay.setContent(info->name, info->blurb, info->accent);
        overlay.setVisible(true);
        overlay.toFront(false);
    }

    // host parameter id of one of this effect's controls, and its current real value
    juce::String paramId(const juce::String& specId) const { return slotParamId(slot, specIndex(*info, specId)); }
    float realParam(const juce::String& specId) const
    {
        const int i = specIndex(*info, specId);
        if (i < 0) return 0.0f;
        if (auto* p = processor.apvts.getRawParameterValue(slotParamId(slot, i)))
            return realFromNorm(info->params[(size_t) i], p->load());
        return 0.0f;
    }

    ModularFXAudioProcessor& processor;
    const int slot;
    const int moduleId;     // effect type
    const FxInfo* info;

    HeaderButton bypassButton { HeaderButton::Power }, infoButton { HeaderButton::Info }, removeButton { HeaderButton::Close };
    juce::TextButton moveLeftButton, moveRightButton;
    InfoOverlay overlay;
    std::unique_ptr<ButtonAttachment> bypassAttach;
    bool dragging = false, pressed = false;
    float grabOffset = 0.0f;
};

// =============================================================================== generic card
// Built entirely from the FxRegistry: knobs, toggles and drop-downs for every parameter.
class GenericCard : public ModuleCardBase, private juce::Timer
{
public:
    GenericCard(ModularFXAudioProcessor& p, int slotIndex, int type)
        : ModuleCardBase(p, slotIndex, type),
          lnf(juce::Colour(0xFF22C7DB), juce::Colours::white)
    {
        const juce::Colour labelColour = juce::Colours::white.withAlpha(0.65f);

        for (const auto& spec : info->params)
        {
            auto c = std::make_unique<Control>();
            c->spec = &spec;

            c->label = std::make_unique<juce::Label>();
            c->label->setText(spec.label, juce::dontSendNotification);
            c->label->setJustificationType(juce::Justification::centred);
            c->label->setColour(juce::Label::textColourId, labelColour);
            c->label->setLookAndFeel(&lnf);
            c->label->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*c->label);

            if (spec.kind == ParamKind::Knob)
            {
                c->slider = std::make_unique<juce::Slider>();
                c->slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
                c->slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                c->slider->setScrollWheelEnabled(false);
                if (spec.minV < 0.0f && spec.maxV > 0.0f) c->slider->getProperties().set("zeroNorm", 0.0);
                c->slider->setLookAndFeel(&lnf);
                addAndMakeVisible(*c->slider);
                c->sliderAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, spec.id, *c->slider);
                if (spec.id == "FET_INPUT" || spec.id == "FET_OUTPUT" || spec.id == "MF_FREQ" || spec.id == "EZ_FREQ")
                    c->slider->getProperties().set("ticks", true);
                c->hover = std::make_unique<ValueHover>(*c->slider, *c->label, spec.label);      // the name below the knob shows the value while it is touched
            }
            else if (spec.kind == ParamKind::Toggle)
            {
                c->label->setVisible(false);
                c->button = std::make_unique<juce::TextButton>(spec.label);
                c->button->setClickingTogglesState(true);
                c->button->setLookAndFeel(&lnf);
                addAndMakeVisible(*c->button);
                c->buttonAttach = std::make_unique<ButtonAttachment>(p.apvts, paramId(spec.id), *c->button);
            }
            else
            {
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList(spec.choices, 1);
                c->combo->setScrollWheelEnabled(false);
                c->combo->getProperties().set("caption", spec.label);
                c->combo->setLookAndFeel(&lnf);
                c->label->setVisible(false);
                addAndMakeVisible(*c->combo);
                c->comboAttach = std::make_unique<SlotComboAttachment>(p.apvts, slot, *info, spec.id, *c->combo);
            }
            controls.push_back(std::move(c));
        }

        // Controls that only matter when a toggle is in a given state (e.g. Delay time vs. synced note)
        for (auto& c : controls)
        {
            if (c->spec->dependsOn.isEmpty()) continue;
            for (auto& t : controls)
            {
                if (t->spec->id == c->spec->dependsOn && t->button != nullptr)
                {
                    auto* toggle = t->button.get();
                    auto previous = toggle->onStateChange;
                    toggle->onStateChange = [this, previous] { if (previous) previous(); updateDependencies(); };
                }
            }
        }
        updateDependencies();

        if (moduleId == ID_MultiFilter)      // Gain only matters for Peak / shelf shapes
        {
            Control *gainC = nullptr, *typeC = nullptr;
            for (auto& c : controls) { if (c->spec->id == "MF_GAIN") gainC = c.get(); if (c->spec->id == "MF_TYPE") typeC = c.get(); }
            if (gainC != nullptr && typeC != nullptr && typeC->combo != nullptr)
            {
                dimSource = typeC->combo.get();
                gainDimmer.fn = [gainC, typeC]
                {
                    const bool on = typeC->combo->getSelectedItemIndex() >= 4;
                    gainC->slider->setEnabled(on);
                    gainC->slider->setAlpha(on ? 1.0f : 0.5f);
                    gainC->label->setAlpha(on ? 1.0f : 0.35f);
                };
                dimSource->addListener(&gainDimmer);
                gainDimmer.fn();
            }
        }

        if (info->display != DisplayKind::None)
            startTimerHz(30);
    }

    ~GenericCard() override
    {
        stopTimer();
        if (dimSource != nullptr) dimSource->removeListener(&gainDimmer);
        for (auto& c : controls)
        {
            c->hover.reset();
            if (c->slider != nullptr) c->slider->setLookAndFeel(nullptr);
            if (c->button != nullptr) c->button->setLookAndFeel(nullptr);
            if (c->combo  != nullptr) c->combo->setLookAndFeel(nullptr);
            c->label->setLookAndFeel(nullptr);
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF121316));
        paintHeader(g, juce::Colours::white);

        if (moduleId == ID_EzEq && ! ezCells[0].isEmpty()) paintEzEqGlyphs(g);

        if (! displayRect.isEmpty())
        {
            if (info->display == DisplayKind::EqCurve)       paintEqCurve(g, displayRect.toFloat());
            else if (info->display == DisplayKind::GainReduction) paintGainReduction(g, displayRect.toFloat());
        }
    }

    void resized() override
    {
        resizedHeader();

        auto area = getLocalBounds().withTrimmedTop(kHeaderH).reduced(10, 4);

        displayRect = {};
        if (info->display == DisplayKind::EqCurve)           { displayRect = area.removeFromTop(92); area.removeFromTop(8); }
        else if (info->display == DisplayKind::GainReduction) { displayRect = area.removeFromTop(24); area.removeFromTop(10); }

        std::vector<Control*> knobs, extras;
        for (auto& c : controls) (c->spec->kind == ParamKind::Knob ? knobs : extras).push_back(c.get());

        // some cards keep their drop-down above the knobs (Input Type), the rest keep it underneath
        const bool comboOnTop = moduleId == ID_MultiShifter || moduleId == ID_PitchShifter;

        if (! extras.empty() && comboOnTop)
        {
            auto top = area.removeFromTop(58).reduced(juce::jmax(10, area.getWidth() / 6), 4);
            for (auto* c : extras) if (c->combo != nullptr) { c->combo->setBounds(top); break; }
            area.removeFromTop(6);
        }
        else if (! extras.empty())
        {
            auto strip = area.removeFromBottom(54);
            area.removeFromBottom(4);
            const int cellW = strip.getWidth() / (int) extras.size();
            for (auto* c : extras)
            {
                auto cell = strip.removeFromLeft(cellW).reduced(4, 0);
                if (c->combo != nullptr)
                    c->combo->setBounds(cell.withSizeKeepingCentre(extras.size() == 1 ? juce::jmin(cell.getWidth(), 170) : cell.getWidth(), 48));
                else
                {
                    cell.removeFromTop(16);
                    c->button->setBounds(cell.removeFromTop(26));
                }
            }
        }

        const int nk = (int) knobs.size();
        if (nk == 0) return;

        auto placeKnob = [] (Control* c, juce::Rectangle<int> cell, int maxSize = 1000)
        {
            c->label->setBounds(cell.removeFromBottom(18));
            const int sz = juce::jmax(10, juce::jmin(juce::jmin(cell.getWidth() - 4, cell.getHeight() - 2), maxSize));
            c->slider->setBounds(cell.getCentreX() - sz / 2, cell.getBottom() - sz, sz, sz);     // the caption sits right under the knob
        };

        if (moduleId == ID_PitchShifter && nk == 2)                  // big Shift knob with a smaller Mix underneath
        {
            auto top = area.removeFromTop((int) (area.getHeight() * 0.62f));
            placeKnob(knobs[0], top);
            placeKnob(knobs[1], area.reduced(area.getWidth() / 5, 0));
            return;
        }
        if (moduleId == ID_MultiShifter && nk == 7)                  // 3 x 2 harmony knobs, with the Dry knob larger on the right
        {
            auto right = area.removeFromRight(area.getWidth() * 24 / 100);
            placeKnob(knobs[6], right.withSizeKeepingCentre(right.getWidth(), juce::jmin(right.getHeight(), 112)));
            const int cw = area.getWidth() / 3, ch = juce::jmin(area.getHeight() / 2, 112);
            const int y0 = area.getY() + (area.getHeight() - ch * 2) / 2;
            for (int i = 0; i < 6; ++i)
                placeKnob(knobs[(size_t) i], { area.getX() + (i % 3) * cw, y0 + (i / 3) * ch, cw, ch });
            return;
        }

        if ((moduleId == ID_Pumper || moduleId == ID_MultiFilter) && nk == 3)      // one big knob with two smaller ones underneath
        {
            auto top = area.removeFromTop((int) (area.getHeight() * 0.50f));
            placeKnob(knobs[0], top, 86);
            const int hw = area.getWidth() / 2;
            placeKnob(knobs[1], area.removeFromLeft(hw).withTrimmedBottom(2), 62);
            placeKnob(knobs[2], area.withTrimmedBottom(2), 62);
            return;
        }
        if (moduleId == ID_EzEq && nk == 5)                           // high-pass | gain, frequency, width | low-pass
        {
            static const float w[5] = { 1.25f, 1.0f, 1.25f, 1.0f, 1.25f };
            const float total = 5.5f;
            int x = area.getX();
            const int cy = area.getY() + area.getHeight() / 2 + 10;
            for (int i = 0; i < 5; ++i)
            {
                const int cw = (int) (area.getWidth() * w[i] / total);
                const int ch = (i % 2 == 0) ? 112 : 84;
                juce::Rectangle<int> cell(x, cy - ch / 2, cw, ch);
                ezCells[(size_t) i] = cell;
                placeKnob(knobs[(size_t) i], cell);
                x += cw;
            }
            return;
        }

        const int cols = (moduleId == ID_Eq3M && nk == 4) ? 3 : (nk <= 4 ? nk : (nk <= 6 ? 3 : 4));
        const int rows = (nk + cols - 1) / cols;
        const int cellW = area.getWidth() / cols;
        const int cellH = juce::jmin(area.getHeight() / rows, 124);
        const int top = area.getY() + (area.getHeight() - cellH * rows) / 2;

        for (int i = 0; i < nk; ++i)
        {
            const int row = i / cols, col = i % cols;
            const int inRow = juce::jmin(cols, nk - row * cols);
            const int xOffset = (cols - inRow) * cellW / 2;          // centre a short last row
            placeKnob(knobs[(size_t) i], { area.getX() + xOffset + col * cellW, top + row * cellH, cellW, cellH });
        }
    }

private:
    // While a knob is hovered or dragged, the caption underneath it shows the value instead of the name.
    struct ValueHover : public juce::MouseListener, public juce::Slider::Listener
    {
        ValueHover(juce::Slider& sl, juce::Label& lb, const juce::String& n) : s(sl), l(lb), name(n)
        {
            s.addMouseListener(this, false);
            s.addListener(this);
        }
        ~ValueHover() override { s.removeMouseListener(this); s.removeListener(this); }

        void refresh()
        {
            const bool show = inside || down;
            l.setText(show ? s.getTextFromValue(s.getValue()) : name, juce::dontSendNotification);
            l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(show ? 0.95f : 0.65f));
        }
        void mouseEnter(const juce::MouseEvent&) override { inside = true;  refresh(); }
        void mouseExit(const juce::MouseEvent&) override  { inside = false; refresh(); }
        void mouseDown(const juce::MouseEvent&) override  { down = true;    refresh(); }
        void mouseUp(const juce::MouseEvent&) override    { down = false;   refresh(); }
        void sliderValueChanged(juce::Slider*) override   { if (inside || down) refresh(); }

        juce::Slider& s; juce::Label& l; juce::String name;
        bool inside = false, down = false;
    };

    struct Control
    {
        const ParamSpec* spec = nullptr;
        std::unique_ptr<ValueHover> hover;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::ComboBox> combo;
        // attachments are declared last so they are destroyed before the components they point at
        std::unique_ptr<SlotSliderAttachment> sliderAttach;
        std::unique_ptr<ButtonAttachment> buttonAttach;
        std::unique_ptr<SlotComboAttachment> comboAttach;
    };

    void updateDependencies()
    {
        for (auto& c : controls)
        {
            if (c->spec->dependsOn.isEmpty()) continue;
            bool toggleState = false;
            for (auto& t : controls)
                if (t->spec->id == c->spec->dependsOn && t->button != nullptr)
                    toggleState = t->button->getToggleState();

            const bool active = (toggleState == c->spec->dependsValue);
            if (c->slider != nullptr) c->slider->setEnabled(active);
            if (c->combo  != nullptr) c->combo->setEnabled(active);
            c->label->setAlpha(active ? 1.0f : 0.35f);
            if (c->combo != nullptr) c->combo->setAlpha(active ? 1.0f : 0.45f);
            if (c->slider != nullptr) c->slider->setAlpha(active ? 1.0f : 0.5f);
        }
    }

    void timerCallback() override
    {
        if (! isShowing() || displayRect.isEmpty()) return;

        if (info->display == DisplayKind::GainReduction)
        {
            const float now = processor.getFx(slot) != nullptr ? processor.getFx(slot)->getMeter() : 0.0f;
            meterValue = juce::jmax(now, meterValue * 0.88f);
        }
        repaint(displayRect.expanded(2));
    }

    float getParam(const char* id) const { return realParam(id); }

    void paintEzEqGlyphs(juce::Graphics& g) const
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        const float y = (float) ezCells[2].getY() - 16.0f;
        auto glyph = [&](float cx, int kind)
        {
            juce::Path p;
            if (kind == 0)      { p.startNewSubPath(cx - 7, y + 6); p.lineTo(cx - 3, y - 6); p.lineTo(cx + 7, y - 6); }       // high-pass
            else if (kind == 1) { p.startNewSubPath(cx - 8, y + 6); p.cubicTo(cx - 4, y + 6, cx - 4, y - 7, cx, y - 7); p.cubicTo(cx + 4, y - 7, cx + 4, y + 6, cx + 8, y + 6); }   // bell
            else                { p.startNewSubPath(cx - 7, y - 6); p.lineTo(cx + 3, y - 6); p.lineTo(cx + 7, y + 6); }       // low-pass
            g.strokePath(p, juce::PathStrokeType(1.5f));
        };
        glyph((float) ezCells[0].getCentreX(), 0);
        glyph((float) ezCells[2].getCentreX(), 1);
        glyph((float) ezCells[4].getCentreX(), 2);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        const float lx = (float) ezCells[1].getCentreX() - 22.0f, rx = (float) ezCells[3].getCentreX() + 22.0f;
        g.drawLine(lx, y + 8.0f, lx, y + 14.0f); g.drawLine(rx, y + 8.0f, rx, y + 14.0f);
        g.drawLine(lx, y + 8.0f, (float) ezCells[2].getCentreX() - 14.0f, y + 8.0f);
        g.drawLine((float) ezCells[2].getCentreX() + 14.0f, y + 8.0f, rx, y + 8.0f);
    }

    void paintEqCurve(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(r, 6.0f);

        const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
        auto xFor = [&](double f) { return r.getX() + r.getWidth() * (float) (std::log(f / 20.0) / std::log(1000.0)); };
        constexpr float range = 18.0f;
        auto yFor = [&](float db) { return r.getCentreY() - juce::jlimit(-range, range, db) / range * (r.getHeight() * 0.5f - 4.0f); };

        // grid
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            g.drawVerticalLine((int) xFor(f), r.getY(), r.getBottom());
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawHorizontalLine((int) yFor(0.0f), r.getX(), r.getRight());

        fx::VisualEq::Settings s { getParam("EQ_HPF"), getParam("EQ_LF"), getParam("EQ_LG"), getParam("EQ_MF"),
                                   getParam("EQ_MG"), getParam("EQ_MQ"), getParam("EQ_HF"), getParam("EQ_HG") };
        std::array<fx::BiquadCoeffs, 4> co;
        bool hpfOn = false;
        fx::VisualEq::makeCoeffs(sr, s, co, hpfOn);

        juce::Path curve;
        const int points = 160;
        for (int i = 0; i <= points; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, (double) i / points);
            float mag = co[1].magnitude(f, sr) * co[2].magnitude(f, sr) * co[3].magnitude(f, sr);
            if (hpfOn) mag *= co[0].magnitude(f, sr);
            const float x = xFor(f), y = yFor(fx::gainToDb(mag));
            if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
        }

        juce::Path fill(curve);
        fill.lineTo(r.getRight(), yFor(0.0f));
        fill.lineTo(r.getX(), yFor(0.0f));
        fill.closeSubPath();
        g.setColour(info->accent.withAlpha(0.18f));
        g.fillPath(fill);
        g.setColour(info->accent);
        g.strokePath(curve, juce::PathStrokeType(2.0f));

        // band handles
        auto dot = [&](double f, float db)
        {
            g.setColour(juce::Colours::white);
            g.fillEllipse(xFor(f) - 3.5f, yFor(db) - 3.5f, 7.0f, 7.0f);
        };
        dot(s.lf, s.lg);  dot(s.mf, s.mg);  dot(s.hf, s.hg);
    }

    void paintGainReduction(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(r, 6.0f);

        auto bar = r.reduced(6.0f, 7.0f);
        const float maxDb = 24.0f;
        const float w = bar.getWidth() * juce::jlimit(0.0f, 1.0f, meterValue / maxDb);
        g.setColour(info->accent);
        g.fillRoundedRectangle(bar.withWidth(juce::jmax(0.0f, w)), 3.0f);

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("GAIN REDUCTION", r.reduced(10.0f, 0.0f), juce::Justification::centredLeft);
        g.drawText("-" + juce::String(meterValue, 1) + " dB", r.reduced(10.0f, 0.0f), juce::Justification::centredRight);
    }

    AccentKnobLookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;
    std::array<juce::Rectangle<int>, 5> ezCells;
    struct GainDimmer : juce::ComboBox::Listener
    {
        std::function<void()> fn;
        void comboBoxChanged(juce::ComboBox*) override { if (fn) fn(); }
    } gainDimmer;
    juce::ComboBox* dimSource = nullptr;
    juce::Rectangle<int> displayRect;
    float meterValue = 0.0f;
};

// =============================================================================== Reverser (custom)
class ReverserCard : public ModuleCardBase
{
public:
    ReverserCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_Reverser)
    {
        mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        mixSlider.setScrollWheelEnabled(false);
        mixSlider.setLookAndFeel(&purpleStyle);
        addAndMakeVisible(mixSlider);

        timeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        timeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        timeSlider.setScrollWheelEnabled(false);
        timeSlider.setLookAndFeel(&purpleStyle);
        addAndMakeVisible(timeSlider);

        beatButton.setButtonText("beat");
        beatButton.setClickingTogglesState(true);
        beatButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFA020F0));
        addAndMakeVisible(beatButton);

        mixAttach  = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "REV_MIX", mixSlider);
        timeAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "REV_TIME", timeSlider);
        beatAttach = std::make_unique<ButtonAttachment>(p.apvts, paramId("REV_BEAT"), beatButton);
    }

    ~ReverserCard() override
    {
        mixSlider.setLookAndFeel(nullptr);
        timeSlider.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF121212));
        paintHeader(g, juce::Colours::white);

        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("Mix", mixSlider.getX(), mixSlider.getBottom() + 4, mixSlider.getWidth(), 18, juce::Justification::centred);
        g.drawText("Time", timeSlider.getX() - 50, timeSlider.getBounds().getCentreY() - 10, 44, 20, juce::Justification::right);
    }

    void resized() override
    {
        resizedHeader();
        mixSlider.setBounds(getWidth() / 2 - 50, 50, 100, 100);
        timeSlider.setBounds(getWidth() / 2 - 25, 195, 50, 50);
        beatButton.setBounds(getWidth() / 2 + 40, 205, 50, 28);
    }

private:
    PurpleArcKnobLookAndFeel purpleStyle;
    juce::Slider mixSlider, timeSlider;
    juce::TextButton beatButton;
    std::unique_ptr<SlotSliderAttachment> mixAttach, timeAttach;
    std::unique_ptr<ButtonAttachment> beatAttach;
};

// =============================================================================== Noise Gate (custom)
class NoiseGateCard : public ModuleCardBase, private juce::Timer
{
public:
    NoiseGateCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_NoiseGate)
    {
        for (auto* s : { &attackSlider, &releaseSlider })
        {
            s->setSliderStyle(juce::Slider::LinearHorizontal);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s->setScrollWheelEnabled(false);
            s->setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFFB703));
            s->setColour(juce::Slider::trackColourId, juce::Colour(0xFFFFB703).withAlpha(0.6f));
            s->setColour(juce::Slider::backgroundColourId, juce::Colours::white.withAlpha(0.12f));
            addAndMakeVisible(*s);
        }

        // The threshold is edited by dragging the pill, so this slider only carries the value.
        threshSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        threshSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        attackAttach  = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GATE_ATTACK", attackSlider);
        releaseAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GATE_RELEASE", releaseSlider);
        threshAttach  = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GATE_THRESH", threshSlider);

        attackSlider.onValueChange = releaseSlider.onValueChange = [this] { repaint(); };
        threshSlider.onValueChange = [this] { repaint(); };
        startTimerHz(30);
    }

    ~NoiseGateCard() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF0F0F0F));
        paintHeader(g, juce::Colours::white);

        const auto scopeArea = getScopeArea();
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRect(scopeArea);

        juce::Path wave;
        wave.startNewSubPath(scopeArea.getX(), scopeArea.getCentreY());
        for (float x = scopeArea.getX(); x < scopeArea.getRight(); x += 5.0f)
            wave.lineTo(x, scopeArea.getCentreY() + std::sin(x * 0.05f) * 35.0f);
        g.setColour(juce::Colour(0xFFFFB703).withAlpha(0.3f + 0.7f * gateOpen));      // brighter while the gate is open
        g.strokePath(wave, juce::PathStrokeType(1.5f));

        const float threshVal = static_cast<float>(threshSlider.getValue());
        const float y = juce::jmap(threshVal, -60.0f, 0.0f, scopeArea.getBottom(), scopeArea.getY());

        g.setColour(juce::Colours::white.withAlpha(0.6f));
        const float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(scopeArea.getX(), y, scopeArea.getRight(), y), dashes, 2, 1.0f);

        thresholdPillBounds = juce::Rectangle<float>(scopeArea.getCentreX() - 55.0f, y - 14.0f, 110.0f, 28.0f);
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(thresholdPillBounds, 14.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(thresholdPillBounds, 14.0f, 1.2f);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("Threshold", thresholdPillBounds, juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(juce::String(threshVal, 1) + " dB", juce::Rectangle<float>(scopeArea.getX() + 6.0f, y - 20.0f, 80.0f, 18.0f),
                   juce::Justification::left);

        const int half = (getWidth() - 30 - 10) / 2;
        auto drawParam = [&](const juce::String& name, juce::Slider& s, int x)
        {
            g.setColour(juce::Colours::grey);
            g.drawText(name, x, 185, half, 18, juce::Justification::left);
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(static_cast<int>(s.getValue())) + " ms", x, 185, half, 18, juce::Justification::right);
        };
        drawParam("Attack", attackSlider, 15);
        drawParam("Release", releaseSlider, 15 + half + 10);
    }

    void resized() override
    {
        resizedHeader();
        const int half = (getWidth() - 30 - 10) / 2;
        attackSlider.setBounds(15, 206, half, 22);
        releaseSlider.setBounds(15 + half + 10, 206, half, 22);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (thresholdPillBounds.contains(e.position))
            isDraggingPill = true;              // the pill sets the threshold, it does not move the card
        else
            ModuleCardBase::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! isDraggingPill) { ModuleCardBase::mouseDrag(e); return; }
        const auto scope = getScopeArea();
        const float norm = juce::jlimit(0.0f, 1.0f, (e.position.y - scope.getY()) / scope.getHeight());
        threshSlider.setValue(juce::jmap(norm, 1.0f, 0.0f, -60.0f, 0.0f));
    }

    void mouseUp(const juce::MouseEvent& e) override { if (! isDraggingPill) ModuleCardBase::mouseUp(e); isDraggingPill = false; }

private:
    juce::Rectangle<float> getScopeArea() const
    {
        return { 15.0f, 52.0f, (float) getWidth() - 30.0f, 110.0f };
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        const float now = processor.getFx(slot) != nullptr ? processor.getFx(slot)->getMeter() : 0.0f;
        if (std::abs(now - gateOpen) > 0.02f) { gateOpen = now; repaint(getScopeArea().toNearestInt()); }
    }

    juce::Slider attackSlider, releaseSlider, threshSlider;
    std::unique_ptr<SlotSliderAttachment> attackAttach, releaseAttach, threshAttach;
    juce::Rectangle<float> thresholdPillBounds;
    bool isDraggingPill = false;
    float gateOpen = 0.0f;
};

// =============================================================================== Chorus Ensemble (custom)
class ChorusCard : public ModuleCardBase
{
public:
    ChorusCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_Chorus)
    {
        auto setupKnob = [this](juce::Slider& slider)
        {
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setScrollWheelEnabled(false);
            slider.setLookAndFeel(&teardropStyle);
            addAndMakeVisible(slider);
        };
        setupKnob(depthSlider); setupKnob(amountSlider); setupKnob(speedSlider); setupKnob(bassSlider); setupKnob(trebleSlider);

        depthAttach  = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "CHO_DEPTH", depthSlider);
        amountAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "CHO_AMOUNT", amountSlider);
        speedAttach  = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "CHO_SPEED", speedSlider);
        bassAttach   = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "CHO_BASS", bassSlider);
        trebleAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "CHO_TREBLE", trebleSlider);
    }

    ~ChorusCard() override
    {
        for (auto* s : { &depthSlider, &amountSlider, &speedSlider, &bassSlider, &trebleSlider })
            s->setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF6B93AC));
        SeigaihaDrawer::drawPattern(g, getLocalBounds().toFloat(), juce::Colour(0xFF3B627C));
        paintHeader(g, juce::Colours::white);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        auto label = [&](const juce::String& t, juce::Slider& s) { g.drawText(t, s.getX() - 10, s.getBottom() + 4, s.getWidth() + 20, 18, juce::Justification::centred); };
        label("Depth", depthSlider);  label("Amount", amountSlider);
        label("Speed", speedSlider);  label("Bass", bassSlider);  label("Treble", trebleSlider);
    }

    void resized() override
    {
        resizedHeader();
        depthSlider.setBounds(getWidth() / 4 - 32, 60, 64, 64);
        amountSlider.setBounds((getWidth() * 3) / 4 - 32, 60, 64, 64);

        const int third = getWidth() / 3;
        speedSlider.setBounds(third / 2 - 28, 190, 56, 56);
        bassSlider.setBounds(third + third / 2 - 28, 190, 56, 56);
        trebleSlider.setBounds(third * 2 + third / 2 - 28, 190, 56, 56);
    }

private:
    TeardropKnobLookAndFeel teardropStyle;
    juce::Slider depthSlider, amountSlider, speedSlider, bassSlider, trebleSlider;
    std::unique_ptr<SlotSliderAttachment> depthAttach, amountAttach, speedAttach, bassAttach, trebleAttach;
};

// =============================================================================== DeEsser (custom)
// A scope of drifting particles with a glowing band at the sibilance frequency; the Threshold pill is dragged up and down.
class DeEsserCard : public ModuleCardBase, private juce::Timer
{
public:
    DeEsserCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_DeEsser)
    {
        freqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        freqSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        freqSlider.setScrollWheelEnabled(false);
        freqSlider.setColour(juce::Slider::thumbColourId, orange);
        freqSlider.setColour(juce::Slider::trackColourId, orange.withAlpha(0.6f));
        freqSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::white.withAlpha(0.12f));
        addAndMakeVisible(freqSlider);

        threshSlider.setSliderStyle(juce::Slider::LinearHorizontal);       // only carries the value; the pill edits it
        threshSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        freqAttach   = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "DS_FREQ", freqSlider);
        threshAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "DS_THRESH", threshSlider);
        freqSlider.onValueChange = threshSlider.onValueChange = [this] { repaint(); };

        juce::Random rng(7);
        for (auto& d : dots) d = { rng.nextFloat(), rng.nextFloat(), 1.0f + rng.nextFloat() * 3.5f, 0.05f + rng.nextFloat() * 0.18f };
        startTimerHz(30);
    }

    ~DeEsserCard() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF0F0F0F));
        paintHeader(g, juce::Colours::white);

        const auto scope = getScopeArea();
        g.saveState();
        g.reduceClipRegion(scope.toNearestInt());

        for (const auto& d : dots)
        {
            g.setColour(juce::Colours::white.withAlpha(d.a));
            g.fillEllipse(scope.getX() + d.x * scope.getWidth() - d.r, scope.getY() + d.y * scope.getHeight() - d.r, d.r * 2.0f, d.r * 2.0f);
        }

        const float threshVal = (float) threshSlider.getValue();
        const float y = juce::jmap(threshVal, -60.0f, 0.0f, scope.getBottom(), scope.getY());

        // the sibilance band: a column at the chosen frequency, brighter while the effect is working
        const float fNorm = (float) (std::log(freqSlider.getValue() / 3000.0) / std::log(4.0));
        const float cx = scope.getX() + 24.0f + fNorm * (scope.getWidth() - 48.0f);
        const juce::Rectangle<float> band(cx - 20.0f, scope.getY(), 40.0f, scope.getHeight());
        g.setColour(orange.withAlpha(0.10f + 0.20f * activity));
        g.fillRect(band.withTop(y));
        g.setColour(orange.withAlpha(0.55f));
        g.fillRect(band.getX(), band.getY(), 1.5f, band.getHeight());
        g.fillRect(band.getRight() - 1.5f, band.getY(), 1.5f, band.getHeight());
        for (int i = 0; i < 14; ++i)
        {
            const float t = std::fmod(phase * (0.35f + 0.05f * (float) (i % 5)) + (float) i * 0.37f, 1.0f);
            const float py = scope.getBottom() - t * (scope.getBottom() - y);
            const float px = band.getX() + 4.0f + std::fmod((float) i * 0.618f, 1.0f) * (band.getWidth() - 8.0f);
            const float r = 1.5f + (float) (i % 4);
            g.setColour(orange.withAlpha((0.35f + 0.6f * activity) * (1.0f - t)));
            g.fillEllipse(px - r, py - r, r * 2.0f, r * 2.0f);
        }
        g.restoreState();

        g.setColour(juce::Colours::white);
        const float dashes[] = { 4.0f, 4.0f };
        g.drawLine(scope.getX(), y, scope.getRight(), y, 1.4f);
        (void) dashes;

        pillBounds = juce::Rectangle<float>(scope.getCentreX() - 55.0f, y - 14.0f, 110.0f, 28.0f);
        g.setColour(juce::Colour(0xFF161616));
        g.fillRoundedRectangle(pillBounds, 14.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(pillBounds, 14.0f, 1.2f);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("Threshold", pillBounds.withTrimmedRight(18.0f).withTrimmedLeft(8.0f), juce::Justification::centred);
        const float ax = pillBounds.getRight() - 16.0f, ay = pillBounds.getCentreY();      // little up/down chevrons
        juce::Path chev;
        chev.startNewSubPath(ax - 3.0f, ay - 2.0f); chev.lineTo(ax, ay - 5.0f); chev.lineTo(ax + 3.0f, ay - 2.0f);
        chev.startNewSubPath(ax - 3.0f, ay + 2.0f); chev.lineTo(ax, ay + 5.0f); chev.lineTo(ax + 3.0f, ay + 2.0f);
        g.strokePath(chev, juce::PathStrokeType(1.3f));

        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(juce::String(threshVal, 1) + " dB", juce::Rectangle<float>(scope.getX() + 6.0f, y - 20.0f, 80.0f, 18.0f), juce::Justification::left);

        g.setColour(juce::Colours::grey);
        g.drawText("Frequency", 15, 185, getWidth() - 30, 18, juce::Justification::left);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(18.0f)));
        g.drawText(juce::String((int) freqSlider.getValue()) + " Hz", 15, 205, getWidth() - 30, 26, juce::Justification::left);
    }

    void resized() override
    {
        resizedHeader();
        freqSlider.setBounds(15, 238, getWidth() - 30, 22);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (pillBounds.contains(e.position)) draggingPill = true;
        else ModuleCardBase::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! draggingPill) { ModuleCardBase::mouseDrag(e); return; }
        const auto scope = getScopeArea();
        const float norm = juce::jlimit(0.0f, 1.0f, (e.position.y - scope.getY()) / scope.getHeight());
        threshSlider.setValue(juce::jmap(norm, 1.0f, 0.0f, -60.0f, 0.0f));
    }
    void mouseUp(const juce::MouseEvent& e) override { if (! draggingPill) ModuleCardBase::mouseUp(e); draggingPill = false; }

private:
    juce::Rectangle<float> getScopeArea() const { return { 0.0f, 44.0f, (float) getWidth(), 130.0f }; }

    void timerCallback() override
    {
        if (! isShowing()) return;
        const float gr = processor.getFx(slot) != nullptr ? processor.getFx(slot)->getMeter() : 0.0f;
        activity = juce::jmax(juce::jlimit(0.0f, 1.0f, gr / 12.0f), activity * 0.9f);
        phase += 0.012f;
        repaint(getScopeArea().toNearestInt());
    }

    struct Dot { float x, y, r, a; };
    std::array<Dot, 70> dots;
    const juce::Colour orange { 0xFFFFB703 };
    juce::Slider freqSlider, threshSlider;
    std::unique_ptr<SlotSliderAttachment> freqAttach, threshAttach;
    juce::Rectangle<float> pillBounds;
    bool draggingPill = false;
    float activity = 0.0f, phase = 0.0f;
};

// =============================================================================== readout (label + big value, drag to change)
class ParamReadout : public juce::Component
{
public:
    ParamReadout(const juce::String& labelText, juce::Slider* s, juce::ComboBox* c, juce::Colour accentColour, bool beatChip)
        : label(labelText), slider(s), combo(c), accent(accentColour), beat(beatChip)
    {
        if (slider != nullptr) slider->onValueChange = [this] { repaint(); };
        if (combo != nullptr)  combo->onChange = [this] { repaint(); };
    }

    void paint(juce::Graphics& g) override
    {
        const float norm = slider != nullptr ? (float) slider->valueToProportionOfLength(slider->getValue())
                                             : (combo != nullptr && combo->getNumItems() > 1 ? (float) combo->getSelectedItemIndex() / (float) (combo->getNumItems() - 1) : 0.0f);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawLine(3.0f, 4.0f, 3.0f, 26.0f, 1.2f);
        const float ty = 26.0f - norm * 22.0f;
        g.setColour(accent);
        g.fillRect(0.5f, ty - 1.0f, 6.0f, 2.0f);

        g.setColour(juce::Colour(0xFF9AA0AA));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(label, 16, 0, getWidth() - 16, 16, juce::Justification::centredLeft);

        const juce::String value = slider != nullptr ? slider->getTextFromValue(slider->getValue()) : (combo != nullptr ? combo->getText() : juce::String());
        g.setColour(juce::Colours::white);
        const juce::Font vf(juce::FontOptions(17.0f));
        g.setFont(vf);
        g.drawFittedText(value, 16, 15, getWidth() - 16, 22, juce::Justification::centredLeft, 1, 0.7f);
        if (beat && combo != nullptr)
        {
            juce::GlyphArrangement ga; ga.addLineOfText(vf, value, 0.0f, 0.0f);
            const float x = 16.0f + ga.getBoundingBox(0, -1, true).getWidth() + 8.0f;
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.fillRoundedRectangle(x, 19.0f, 40.0f, 17.0f, 5.0f);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(13.0f)));
            g.drawText("beat", juce::Rectangle<float>(x, 19.0f, 40.0f, 17.0f), juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (slider != nullptr) { startProp = slider->valueToProportionOfLength(slider->getValue()); slider->setValue(slider->getValue(), juce::sendNotificationSync); }
        moved = false;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (slider == nullptr) return;
        moved = true;
        const double delta = ((double) e.getDistanceFromDragStartX() - (double) e.getDistanceFromDragStartY()) / 160.0;
        slider->setValue(slider->proportionOfLengthToValue(juce::jlimit(0.0, 1.0, startProp + delta)));
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (combo == nullptr || e.getDistanceFromDragStart() > 4) return;
        juce::PopupMenu m;
        for (int i = 0; i < combo->getNumItems(); ++i) m.addItem(combo->getItemId(i), combo->getItemText(i), true, i == combo->getSelectedItemIndex());
        auto* c = combo;
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [c](int id) { if (id != 0) c->setSelectedId(id, juce::sendNotificationSync); });
    }

private:
    juce::String label;
    juce::Slider* slider;
    juce::ComboBox* combo;
    juce::Colour accent;
    bool beat;
    double startProp = 0.0;
    bool moved = false;
};

// =============================================================================== pill card (Filter Echo +, D-Delay +, Spring Reverb)
// A graphic with ripples that grow with the main control, a draggable pill that sets it, and a grid of readouts underneath.
class PillCard : public ModuleCardBase, private juce::Timer
{
public:
    enum Art { Rings, Spring, Arcs };
    struct Cell { const char* id; int row; };

    PillCard(ModularFXAudioProcessor& p, int slotIndex, int type, const char* pillId, const char* pillName, Art artKind, juce::Colour accentColour,
             std::vector<Cell> cells, int scopeHeight)
        : ModuleCardBase(p, slotIndex, type), art(artKind), accent(accentColour), pillText(pillName), cellDefs(std::move(cells)), scopeH(scopeHeight)
    {
        pillSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        pillSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pillAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, pillId, pillSlider);
        pillSlider.onValueChange = [this] { repaint(getScopeArea().toNearestInt().expanded(0, 24)); };

        for (const auto& d : cellDefs)
        {
            auto it = std::make_unique<Item>();
            const int idx = specIndex(*info, d.id);
            const auto& spec = info->params[(size_t) idx];
            if (spec.kind == ParamKind::Choice)
            {
                it->combo = std::make_unique<juce::ComboBox>();
                it->combo->addItemList(spec.choices, 1);
                it->comboAttach = std::make_unique<SlotComboAttachment>(p.apvts, slot, *info, spec.id, *it->combo);
            }
            else
            {
                it->slider = std::make_unique<juce::Slider>();
                it->slider->setSliderStyle(juce::Slider::LinearHorizontal);
                it->slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                it->sliderAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, spec.id, *it->slider);
            }
            const bool chip = spec.kind == ParamKind::Choice && (spec.label == "Time");
            it->readout = std::make_unique<ParamReadout>(spec.label, it->slider.get(), it->combo.get(), accent, chip);
            addAndMakeVisible(*it->readout);
            items.push_back(std::move(it));
        }
        startTimerHz(24);
    }

    ~PillCard() override { stopTimer(); for (auto& it : items) { if (it->slider) it->slider->onValueChange = nullptr; if (it->combo) it->combo->onChange = nullptr; } }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF121316));
        paintHeader(g, juce::Colours::white);

        const auto scope = getScopeArea();
        const float v = (float) pillSlider.valueToProportionOfLength(pillSlider.getValue());
        const float y = juce::jmap(v, scope.getBottom(), scope.getY() + 6.0f);

        g.saveState();
        g.reduceClipRegion(scope.toNearestInt());
        paintArt(g, scope, v);
        g.restoreState();

        g.setColour(juce::Colours::white);
        g.drawLine(0.0f, y, (float) getWidth(), y, 1.4f);
        pillBounds = juce::Rectangle<float>(scope.getCentreX() - 60.0f, y - 15.0f, 120.0f, 30.0f);
        g.setColour(juce::Colour(0xFF121316));
        g.fillRoundedRectangle(pillBounds, 15.0f);
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(pillBounds, 15.0f, 1.2f);
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::plain)));
        g.drawText(pillText, pillBounds.withTrimmedRight(20.0f).withTrimmedLeft(8.0f), juce::Justification::centred);
        const float ax = pillBounds.getRight() - 18.0f, ay = pillBounds.getCentreY();
        juce::Path chev;
        chev.startNewSubPath(ax - 3.5f, ay - 2.5f); chev.lineTo(ax, ay - 6.0f); chev.lineTo(ax + 3.5f, ay - 2.5f);
        chev.startNewSubPath(ax - 3.5f, ay + 2.5f); chev.lineTo(ax, ay + 6.0f); chev.lineTo(ax + 3.5f, ay + 2.5f);
        g.strokePath(chev, juce::PathStrokeType(1.4f));

        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(pillSlider.getTextFromValue(pillSlider.getValue()), juce::Rectangle<float>(8.0f, y - 21.0f, 90.0f, 18.0f), juce::Justification::left);
    }

    void resized() override
    {
        resizedHeader();
        // readouts are laid out in rows underneath the graphic
        int maxRow = 0; for (const auto& d : cellDefs) maxRow = juce::jmax(maxRow, d.row);
        const int top = 44 + scopeH + 14;
        const int rowH = 46;
        for (int r = 0; r <= maxRow; ++r)
        {
            std::vector<size_t> idxs;
            for (size_t i = 0; i < cellDefs.size(); ++i) if (cellDefs[i].row == r) idxs.push_back(i);
            if (idxs.empty()) continue;
            const int cw = (getWidth() - 36) / (int) idxs.size();
            int x = 18;
            for (auto i : idxs) { items[i]->readout->setBounds(x, top + r * rowH, cw - 4, 40); x += cw; }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (pillBounds.contains(e.position)) draggingPill = true;
        else ModuleCardBase::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! draggingPill) { ModuleCardBase::mouseDrag(e); return; }
        const auto s = getScopeArea();
        const float norm = juce::jlimit(0.0f, 1.0f, (s.getBottom() - e.position.y) / (s.getHeight() - 6.0f));
        pillSlider.setValue(pillSlider.proportionOfLengthToValue(norm));
    }
    void mouseUp(const juce::MouseEvent& e) override { if (! draggingPill) ModuleCardBase::mouseUp(e); draggingPill = false; }

private:
    struct Item
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<ParamReadout> readout;
        std::unique_ptr<SlotSliderAttachment> sliderAttach;
        std::unique_ptr<SlotComboAttachment> comboAttach;
    };

    juce::Rectangle<float> getScopeArea() const { return { 0.0f, 44.0f, (float) getWidth(), (float) scopeH }; }

    void paintArt(juce::Graphics& g, juce::Rectangle<float> s, float v) const
    {
        const float cx = s.getCentreX();
        if (art == Rings || art == Spring)
        {
            const bool spring = art == Spring;
            const float cy = spring ? s.getCentreY() - 6.0f : s.getCentreY() + 4.0f;
            const float sx = spring ? 1.9f : 1.0f;
            const int n = 1 + (int) std::round(v * (spring ? 7.0f : 3.0f));
            if (spring && v > 0.02f)
            {
                juce::ColourGradient glow(accent.withAlpha(0.28f * v), cx, cy, accent.withAlpha(0.0f), cx + 150.0f, cy, true);
                g.setGradientFill(glow);
                g.fillEllipse(cx - 160.0f, cy - 70.0f, 320.0f, 140.0f);
            }
            for (int i = 0; i < n; ++i)
            {
                const float r = 14.0f + (float) i * (spring ? 11.0f : 25.0f) + std::fmod(phase * 6.0f, 1.0f) * 0.0f;
                g.setColour(accent.withAlpha(juce::jmax(0.12f, 0.95f - (float) i * (spring ? 0.1f : 0.2f))));
                g.drawEllipse(cx - r * sx * 0.62f - (spring ? 0.0f : 0.0f), cy - r * 0.62f, r * sx * 1.24f, r * 1.24f, spring ? 1.3f : 1.6f);
            }
            if (! spring)
            {
                g.setColour(accent.withAlpha(0.12f));
                g.drawEllipse(cx - 130.0f, cy - 100.0f, 260.0f, 200.0f, 1.0f);
            }
        }
        else
        {
            const float cy = s.getBottom() + 10.0f;
            const int n = 4 + (int) (v * 14.0f);
            for (int i = 0; i < n; ++i)
            {
                const float r = 30.0f + (float) i * 14.0f;
                g.setColour(accent.withAlpha(0.14f * (1.0f - (float) i / (float) (n + 2))));
                g.drawEllipse(cx - r * 1.5f, cy - r, r * 3.0f, r * 2.0f, 1.0f);
            }
        }
    }

    void timerCallback() override { phase += 0.02f; }

    Art art;
    juce::Colour accent;
    juce::String pillText;
    std::vector<Cell> cellDefs;
    int scopeH;
    juce::Slider pillSlider;
    std::unique_ptr<SlotSliderAttachment> pillAttach;
    std::vector<std::unique_ptr<Item>> items;
    juce::Rectangle<float> pillBounds;
    bool draggingPill = false;
    float phase = 0.0f;
};

// =============================================================================== Graphic EQ (custom)
class GraphicEqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cx = (float) x + (float) width * 0.5f;
        juce::Rectangle<float> track(cx - 9.0f, (float) y, 18.0f, (float) height);
        g.setColour(juce::Colour(0xFF9A9082));
        g.fillRoundedRectangle(track, 9.0f);
        juce::Rectangle<float> thumb(cx - 15.0f, sliderPos - 14.0f, 30.0f, 28.0f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(thumb.translated(0.0f, 2.0f), 6.0f);
        g.setColour(juce::Colour(0xFFF4F2EF));
        g.fillRoundedRectangle(thumb, 6.0f);
        g.setColour(juce::Colour(0xFF4A453E));
        g.fillRect(cx - 8.0f, sliderPos - 1.0f, 16.0f, 2.0f);
    }
};

class GraphicEqCard : public ModuleCardBase
{
public:
    GraphicEqCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_GraphicEq)
    {
        static const char* ids[7] = { "GEQ_1", "GEQ_2", "GEQ_3", "GEQ_4", "GEQ_5", "GEQ_6", "GEQ_7" };
        for (int i = 0; i < 7; ++i)
        {
            auto& s = bands[(size_t) i];
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setScrollWheelEnabled(false);
            s.setLookAndFeel(&look);
            s.setDoubleClickReturnValue(true, 0.0);
            addAndMakeVisible(s);
            bandAttach[(size_t) i] = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, ids[i], s);
        }
        level.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        level.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        level.setScrollWheelEnabled(false);
        level.setLookAndFeel(&teardrop);
        addAndMakeVisible(level);
        levelAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GEQ_LEVEL", level);
    }
    ~GraphicEqCard() override { for (auto& s : bands) s.setLookAndFeel(nullptr); level.setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFFB5AB9D));
        // herringbone pattern along the bottom
        g.setColour(juce::Colour(0xFFC4BBAE));
        for (int x = -20; x < getWidth() + 40; x += 46)
        {
            juce::Path pth;
            pth.startNewSubPath((float) x, (float) getHeight()); pth.lineTo((float) x + 23.0f, (float) getHeight() - 30.0f);
            pth.lineTo((float) x + 46.0f, (float) getHeight()); pth.closeSubPath();
            g.fillPath(pth);
        }
        paintHeader(g, juce::Colour(0xFF2B2722));

        g.setColour(juce::Colour(0xFF3A342C));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("+15", 4, bands[0].getY() - 2, 36, 16, juce::Justification::centred);
        g.drawText("0", 4, bands[0].getBounds().getCentreY() - 8, 36, 16, juce::Justification::centred);
        g.drawText("-15", 4, bands[0].getBottom() - 14, 36, 16, juce::Justification::centred);
        for (int dy : { -26, 26 }) g.fillRect(14, bands[0].getBounds().getCentreY() + dy, 14, 2);
        for (int dy : { -52, 52 }) g.fillRect(14, bands[0].getBounds().getCentreY() + dy, 14, 2);

        static const char* names[7] = { "100", "200", "400", "800", "1.6k", "3.2k", "6.4k" };
        g.setColour(juce::Colour(0xFFD9D3CA));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        for (int i = 0; i < 7; ++i) g.drawText(names[i], bands[(size_t) i].getX() - 6, bands[(size_t) i].getBottom() + 4, bands[(size_t) i].getWidth() + 12, 18, juce::Justification::centred);
        g.setColour(juce::Colour(0xFF2B2722));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("Level", level.getX() - 10, level.getBottom() + 1, level.getWidth() + 20, 18, juce::Justification::centred);
    }

    void resized() override
    {
        resizedHeader();
        const int left = 44, right = getWidth() - 12;
        const int cw = (right - left) / 7;
        for (int i = 0; i < 7; ++i) bands[(size_t) i].setBounds(left + i * cw + cw / 2 - 16, 56, 32, 176);
        level.setBounds(getWidth() / 2 - 22, 266, 44, 44);
    }

private:
    GraphicEqLookAndFeel look;
    TeardropKnobLookAndFeel teardrop;
    std::array<juce::Slider, 7> bands;
    juce::Slider level;
    std::array<std::unique_ptr<SlotSliderAttachment>, 7> bandAttach;
    std::unique_ptr<SlotSliderAttachment> levelAttach;
};

// =============================================================================== GTR Squeeze (custom)
class GtrSqueezeCard : public ModuleCardBase
{
public:
    GtrSqueezeCard(ModularFXAudioProcessor& p, int slotIndex) : ModuleCardBase(p, slotIndex, ID_GtrSqueeze)
    {
        for (auto* s : { &sustain, &volume })
        {
            s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s->setScrollWheelEnabled(false);
            s->setLookAndFeel(&teardrop);
            addAndMakeVisible(*s);
        }
        sAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GSQ_SUSTAIN", sustain);
        vAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, "GSQ_VOL", volume);
    }
    ~GtrSqueezeCard() override { sustain.setLookAndFeel(nullptr); volume.setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(juce::Colour(0xFF4F5667));
        g.setColour(juce::Colour(0xFF2B2F3A));
        for (int row = 0; row < 4; ++row)
            for (int x = (row % 2) * 24 - 24; x < getWidth() + 24; x += 48)
            {
                const float y = (float) getHeight() - 26.0f - (float) row * 30.0f;
                g.drawEllipse((float) x - 14.0f, y - 14.0f, 28.0f, 28.0f, 1.6f);
                g.drawEllipse((float) x + 10.0f, y - 6.0f, 12.0f, 12.0f, 1.6f);
            }
        paintHeader(g, juce::Colours::white);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
        for (auto* s : { &sustain, &volume })
            g.drawText(s == &sustain ? "Sustain" : "Volume", s->getX() - 20, s->getBottom() + 6, s->getWidth() + 40, 22, juce::Justification::centred);
    }

    void resized() override
    {
        resizedHeader();
        sustain.setBounds(getWidth() / 4 - 38, 64, 76, 76);
        volume.setBounds(getWidth() * 3 / 4 - 38, 64, 76, 76);
    }

private:
    TeardropKnobLookAndFeel teardrop;
    juce::Slider sustain, volume;
    std::unique_ptr<SlotSliderAttachment> sAttach, vAttach;
};

// =============================================================================== factory
inline std::unique_ptr<ModuleCardBase> createCard(ModularFXAudioProcessor& p, int slot, int type)
{
    switch (type)
    {
        case ID_Reverser:  return std::make_unique<ReverserCard>(p, slot);
        case ID_NoiseGate: return std::make_unique<NoiseGateCard>(p, slot);
        case ID_Chorus:    return std::make_unique<ChorusCard>(p, slot);
        case ID_DeEsser:   return std::make_unique<DeEsserCard>(p, slot);
        case ID_GraphicEq: return std::make_unique<GraphicEqCard>(p, slot);
        case ID_GtrSqueeze: return std::make_unique<GtrSqueezeCard>(p, slot);
        case ID_FilterEcho:  return std::make_unique<PillCard>(p, slot, type, "FEC_FB", "Feedback", PillCard::Rings, juce::Colour(0xFFC026D3),
                                  std::vector<PillCard::Cell>{ { "FEC_DIV", 0 }, { "FEC_FREQ", 0 }, { "FEC_MIX", 0 } }, 200);
        case ID_SpringReverb: return std::make_unique<PillCard>(p, slot, type, "SPR_AMOUNT", "Amount", PillCard::Spring, juce::Colour(0xFFC026D3),
                                  std::vector<PillCard::Cell>{ { "SPR_TYPE", 0 } }, 200);
        case ID_LegacyDelay: return std::make_unique<PillCard>(p, slot, type, "DDX_FB", "Feedback", PillCard::Arcs, juce::Colour(0xFF2DD4BF),
                                  std::vector<PillCard::Cell>{ { "DDX_TIME", 0 }, { "DDX_HP", 0 }, { "DDX_LP", 0 },
                                                               { "DDX_SPREAD", 1 }, { "DDX_WET", 1 }, { "DDX_DRY", 1 }, { "DDX_VOL", 1 } }, 130);
        default:           return std::make_unique<GenericCard>(p, slot, type);
    }
}
