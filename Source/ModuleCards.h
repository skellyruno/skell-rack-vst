#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
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

        infoButton.setButtonText("i");
        infoButton.onClick = [this] { showInfo(); };
        addAndMakeVisible(infoButton);

        moveLeftButton.setButtonText("<");
        moveRightButton.setButtonText(">");
        removeButton.setButtonText("X");
        addAndMakeVisible(moveLeftButton);
        addAndMakeVisible(moveRightButton);
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

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragging = false;
        if (e.y < kHeaderH && ! e.mods.isPopupMenu())
        {
            dragging = true;
            grabOffset = e.getEventRelativeTo(getParentComponent()).position.x - (float) getX();
            if (onDragStart) onDragStart(slot);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragging && onDragMove)
            onDragMove(slot, (int) (e.getEventRelativeTo(getParentComponent()).position.x - grabOffset));
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging && onDragEnd) onDragEnd(slot);
        dragging = false;
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
        g.drawText(info->name, 80, 6, getWidth() - 80 - 84, 24, juce::Justification::centredLeft);

        g.setColour(textColour.withAlpha(0.3f));
        const int w = getWidth(), h = getHeight();
        g.fillRect(3, 3, 6, 6);
        g.fillRect(w - 9, 3, 6, 6);
        g.fillRect(3, h - 9, 6, 6);
        g.fillRect(w - 9, h - 9, 6, 6);
    }

    void resizedHeader()
    {
        bypassButton.setBounds(12, 6, 36, 22);
        infoButton.setBounds(52, 6, 22, 22);
        moveLeftButton.setBounds(getWidth() - 78, 6, 22, 22);
        moveRightButton.setBounds(getWidth() - 53, 6, 22, 22);
        removeButton.setBounds(getWidth() - 28, 6, 22, 22);
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

    juce::ToggleButton bypassButton;
    juce::TextButton infoButton, moveLeftButton, moveRightButton, removeButton;
    InfoOverlay overlay;
    std::unique_ptr<ButtonAttachment> bypassAttach;
    bool dragging = false;
    float grabOffset = 0.0f;
};

// =============================================================================== generic card
// Built entirely from the FxRegistry: knobs, toggles and drop-downs for every parameter.
class GenericCard : public ModuleCardBase, private juce::Timer
{
public:
    GenericCard(ModularFXAudioProcessor& p, int slotIndex, int type)
        : ModuleCardBase(p, slotIndex, type),
          lnf(info->accent, info->bg.brighter(0.25f))
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
                c->slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
                c->slider->setScrollWheelEnabled(false);
                if (spec.minV < 0.0f && spec.maxV > 0.0f) c->slider->getProperties().set("zeroNorm", 0.0);
                c->slider->setLookAndFeel(&lnf);
                addAndMakeVisible(*c->slider);
                c->sliderAttach = std::make_unique<SlotSliderAttachment>(p.apvts, slot, *info, spec.id, *c->slider);
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
                c->combo->setLookAndFeel(&lnf);
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

        if (info->display != DisplayKind::None)
            startTimerHz(30);
    }

    ~GenericCard() override
    {
        stopTimer();
        for (auto& c : controls)
        {
            if (c->slider != nullptr) c->slider->setLookAndFeel(nullptr);
            if (c->button != nullptr) c->button->setLookAndFeel(nullptr);
            if (c->combo  != nullptr) c->combo->setLookAndFeel(nullptr);
            c->label->setLookAndFeel(nullptr);
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        g.fillAll(info->bg);
        paintHeader(g, info->text);

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

        if (! extras.empty())
        {
            auto strip = area.removeFromBottom(46);
            area.removeFromBottom(4);
            const int cellW = strip.getWidth() / (int) extras.size();
            for (auto* c : extras)
            {
                auto cell = strip.removeFromLeft(cellW).reduced(4, 0);
                if (c->combo != nullptr)
                {
                    c->label->setBounds(cell.removeFromTop(16));
                    c->combo->setBounds(cell.removeFromTop(26));
                }
                else
                {
                    cell.removeFromTop(16);
                    c->button->setBounds(cell.removeFromTop(26));
                }
            }
        }

        const int nk = (int) knobs.size();
        if (nk == 0) return;
        const int cols = nk <= 4 ? nk : (nk <= 6 ? 3 : 4);
        const int rows = (nk + cols - 1) / cols;
        const int cellW = area.getWidth() / cols;
        const int cellH = juce::jmin(area.getHeight() / rows, 130);
        const int top = area.getY() + (area.getHeight() - cellH * rows) / 2;

        for (int i = 0; i < nk; ++i)
        {
            const int row = i / cols, col = i % cols;
            const int inRow = juce::jmin(cols, nk - row * cols);
            const int xOffset = (cols - inRow) * cellW / 2;          // centre a short last row
            juce::Rectangle<int> cell(area.getX() + xOffset + col * cellW, top + row * cellH, cellW, cellH);
            cell = cell.reduced(2, 0);
            cell.removeFromBottom(4);

            knobs[(size_t) i]->label->setBounds(cell.removeFromTop(16));
            knobs[(size_t) i]->slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, juce::jmax(40, cell.getWidth() - 4), 16);
            knobs[(size_t) i]->slider->setBounds(cell);
        }
    }

private:
    struct Control
    {
        const ParamSpec* spec = nullptr;
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
        ModuleCardBase::mouseDown(e);
        if (thresholdPillBounds.contains(e.position))
            isDraggingPill = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        ModuleCardBase::mouseDrag(e);
        if (! isDraggingPill) return;
        const auto scope = getScopeArea();
        const float norm = juce::jlimit(0.0f, 1.0f, (e.position.y - scope.getY()) / scope.getHeight());
        threshSlider.setValue(juce::jmap(norm, 1.0f, 0.0f, -60.0f, 0.0f));
    }

    void mouseUp(const juce::MouseEvent& e) override { ModuleCardBase::mouseUp(e); isDraggingPill = false; }

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

// =============================================================================== factory
inline std::unique_ptr<ModuleCardBase> createCard(ModularFXAudioProcessor& p, int slot, int type)
{
    switch (type)
    {
        case ID_Reverser:  return std::make_unique<ReverserCard>(p, slot);
        case ID_NoiseGate: return std::make_unique<NoiseGateCard>(p, slot);
        case ID_Chorus:    return std::make_unique<ChorusCard>(p, slot);
        default:           return std::make_unique<GenericCard>(p, slot, type);
    }
}
