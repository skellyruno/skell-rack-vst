#pragma once
#include <JuceHeader.h>
#include <functional>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

namespace masterui
{
    const juce::Colour kBg     { 0xFF111216 };
    const juce::Colour kBorder { 0xFF24262B };
    const juce::Colour kField  { 0xFF1D1F24 };
    const juce::Colour kGrey   { 0xFF8B93A1 };
    const juce::Colour kBlue   { 0xFF3B82F6 };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // A button whose look is a callback, so every icon in the side panels can be drawn in a few lines.
    class PaintedButton : public juce::Button
    {
    public:
        PaintedButton() : juce::Button ({}) {}
        std::function<void (juce::Graphics&, juce::Rectangle<float>, bool over, bool down, bool on)> painter;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            if (painter) painter (g, getLocalBounds().toFloat(), over, down, getToggleState());
        }
    };
}

// ---------------------------------------------------------------------------- left: presets + blend
class PresetPanel : public juce::Component
{
public:
    explicit PresetPanel (ModularFXAudioProcessor& p) : proc (p)
    {
        using namespace masterui;

        prevButton.painter = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool) { chevron (g, r, true, over); };
        nextButton.painter = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool) { chevron (g, r, false, over); };
        prevButton.onClick = [this] { step (-1); };
        nextButton.onClick = [this] { step (1); };
        addAndMakeVisible (prevButton);
        addAndMakeVisible (nextButton);

        saveButton.painter = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down, bool)
        {
            g.setColour (down ? juce::Colour (0xFF383C45) : over ? juce::Colour (0xFF30333B) : juce::Colour (0xFF2A2D34));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
            g.drawText ("Save", r, juce::Justification::centred);
        };
        saveButton.onClick = [this] { askToSave(); };
        addAndMakeVisible (saveButton);

        moreButton.painter = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool)
        {
            g.setColour (juce::Colours::white.withAlpha (over ? 0.9f : 0.55f));
            for (int i = -1; i <= 1; ++i) g.fillEllipse (r.getCentreX() - 1.8f, r.getCentreY() + (float) i * 6.0f - 1.8f, 3.6f, 3.6f);
        };
        moreButton.onClick = [this] { showMoreMenu(); };
        addAndMakeVisible (moreButton);

        bypassButton.setClickingTogglesState (true);
        bypassButton.painter = [] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool, bool on)
        {
            g.setColour (on ? masterui::kBlue : juce::Colour (0xFF2A2D34).brighter (over ? 0.15f : 0.0f));
            g.fillEllipse (r);
            g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.7f));
            const auto c = r.getCentre();
            juce::Path fx;                                          // a small "fx" with a slash through it
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText ("fx", r, juce::Justification::centred);
            g.drawLine (c.x - 10.0f, c.y + 10.0f, c.x + 10.0f, c.y - 10.0f, 1.8f);
        };
        addAndMakeVisible (bypassButton);
        bypassAtt = std::make_unique<masterui::ButtonAtt> (proc.apvts, "M_BYP", bypassButton);

        blend.setSliderStyle (juce::Slider::LinearHorizontal);
        blend.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        blend.setColour (juce::Slider::trackColourId, masterui::kBlue);
        blend.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF2A2D34));
        blend.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        blend.setScrollWheelEnabled (false);
        blend.setDoubleClickReturnValue (true, 1.0);
        blend.onValueChange = [this] { blendValue.setText (juce::String (juce::roundToInt (blend.getValue() * 100.0)) + " %", juce::dontSendNotification); };
        addAndMakeVisible (blend);
        blendAtt = std::make_unique<masterui::SliderAtt> (proc.apvts, "M_BLEND", blend);

        blendValue.setJustificationType (juce::Justification::centred);
        blendValue.setColour (juce::Label::textColourId, juce::Colours::white);
        blendValue.setColour (juce::Label::backgroundColourId, masterui::kField);
        blendValue.setFont (juce::Font (juce::FontOptions (16.0f)));
        blendValue.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (blendValue);
        blend.onValueChange();
    }

    void refresh() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace masterui;
        g.fillAll (kBg);
        g.setColour (kBorder);
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
        g.drawText ("Presets", 0, 14, getWidth(), 26, juce::Justification::centred);

        // thumbnail: the first four effects of the rack as coloured tiles
        const auto t = juce::Rectangle<float> ((float) getWidth() * 0.5f - 36.0f, 48.0f, 72.0f, 72.0f);
        g.setColour (juce::Colour (0xFF08090B));
        g.fillRoundedRectangle (t, 10.0f);
        const auto rack = proc.getRack();
        for (int i = 0; i < 4; ++i)
        {
            const auto cell = juce::Rectangle<float> (t.getX() + 6.0f + (float) (i % 2) * 31.0f, t.getY() + 6.0f + (float) (i / 2) * 31.0f, 29.0f, 29.0f);
            if (i < (int) rack.size())
            {
                if (const FxInfo* info = getFxInfo (rack[(size_t) i].type))
                {
                    g.setColour (info->accent.withAlpha (0.9f));
                    g.fillRoundedRectangle (cell, 5.0f);
                    g.setColour (info->bg.withAlpha (0.55f));
                    g.fillEllipse (cell.reduced (8.0f));
                }
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRoundedRectangle (cell, 5.0f);
            }
        }

        // preset name field
        const auto f = nameBounds().toFloat();
        g.setColour (kField);
        g.fillRoundedRectangle (f, 9.0f);
        const auto name = proc.getPresetName();
        g.setColour (name.isEmpty() ? kGrey : juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (17.0f)));
        g.drawText (name.isEmpty() ? "None" : name, f.reduced (34.0f, 0.0f), juce::Justification::centred, true);

        g.setColour (kBorder);
        g.drawHorizontalLine (250, 14.0f, (float) getWidth() - 14.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
        g.drawText ("Blend", 18, 262, 80, 28, juce::Justification::centredLeft);
    }

    void resized() override
    {
        const int w = getWidth();
        const auto nb = nameBounds();
        prevButton.setBounds (nb.getX(), nb.getY(), 30, nb.getHeight());
        nextButton.setBounds (nb.getRight() - 30, nb.getY(), 30, nb.getHeight());
        saveButton.setBounds (14, 186, 80, 34);
        moreButton.setBounds (108, 186, 22, 34);
        bypassButton.setBounds (w - 14 - 40 - 6, 183, 40, 40);
        blendValue.setBounds (w - 18 - 76, 262, 76, 30);
        blend.setBounds (14, 306, w - 28, 24);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto nb = nameBounds().reduced (30, 0);
        if (nb.contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown()) showPresetMenu();
    }

private:
    juce::Rectangle<int> nameBounds() const { return { 10, 134, getWidth() - 20, 40 }; }

    static void chevron (juce::Graphics& g, juce::Rectangle<float> r, bool left, bool over)
    {
        g.setColour (juce::Colours::white.withAlpha (over ? 1.0f : 0.6f));
        const auto c = r.getCentre();
        const float s = left ? -1.0f : 1.0f;
        juce::Path p;
        p.startNewSubPath (c.x - 3.5f * s, c.y - 7.0f);
        p.lineTo (c.x + 3.5f * s, c.y);
        p.lineTo (c.x - 3.5f * s, c.y + 7.0f);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void step (int dir)
    {
        const auto list = proc.getPresetList();
        if (list.empty()) return;
        int cur = -1;
        const auto name = proc.getPresetName();
        for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == name) cur = i;
        int next = cur < 0 ? (dir > 0 ? 0 : (int) list.size() - 1) : (cur + dir + (int) list.size()) % (int) list.size();
        proc.loadPreset (next);
        repaint();
    }

    void showPresetMenu()
    {
        const auto list = proc.getPresetList();
        juce::PopupMenu menu;
        menu.addSectionHeader ("Presets");
        bool sawUser = false;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            if (! list[(size_t) i].factory && ! sawUser) { menu.addSeparator(); menu.addSectionHeader ("My presets"); sawUser = true; }
            menu.addItem (i + 1, list[(size_t) i].name, true, list[(size_t) i].name == proc.getPresetName());
        }
        juce::Component::SafePointer<PresetPanel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (nameBounds())).withMinimumWidth (getWidth() - 20),
                            [safe] (int r) { if (safe != nullptr && r > 0) { safe->proc.loadPreset (r - 1); safe->repaint(); } });
    }

    void showMoreMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Clear rack");
        const auto list = proc.getPresetList();
        int userIndex = -1;
        for (int i = 0; i < (int) list.size(); ++i)
            if (! list[(size_t) i].factory && list[(size_t) i].name == proc.getPresetName()) userIndex = i;
        menu.addItem (2, "Delete this preset", userIndex >= 0);
        juce::Component::SafePointer<PresetPanel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                            [safe, userIndex] (int r)
                            {
                                if (safe == nullptr) return;
                                if (r == 1) safe->proc.clearRack();
                                if (r == 2 && userIndex >= 0) { safe->proc.deletePreset (userIndex); safe->proc.apvts.state.setProperty ("preset", "", nullptr); }
                                safe->repaint();
                            });
    }

    void askToSave()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.getPresetName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetPanel> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
        {
            if (r == 1 && safe != nullptr) { safe->proc.savePreset (w->getTextEditorContents ("name")); safe->repaint(); }
        }), true);
    }

    ModularFXAudioProcessor& proc;
    masterui::PaintedButton prevButton, nextButton, saveButton, moreButton, bypassButton;
    juce::Slider blend;
    juce::Label blendValue;
    std::unique_ptr<masterui::SliderAtt> blendAtt;
    std::unique_ptr<masterui::ButtonAtt> bypassAtt;
};

// ---------------------------------------------------------------------------- right: volume, pan, reverb
class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (ModularFXAudioProcessor& p)
        : lnf (juce::Colours::white, juce::Colour (0xFF2A2D34))
    {
        using namespace masterui;
        volume.setSliderStyle (juce::Slider::LinearVertical);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setColour (juce::Slider::trackColourId, kBlue);
        volume.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF2A2D34));
        volume.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        volume.setScrollWheelEnabled (false);
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (volume);

        for (auto* k : { &pan, &reverb })
        {
            k->setSliderStyle (juce::Slider::RotaryVerticalDrag);
            k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            k->setScrollWheelEnabled (false);
            k->setLookAndFeel (&lnf);
            k->setPopupDisplayEnabled (true, true, nullptr);
            addAndMakeVisible (*k);
        }
        pan.getProperties().set ("zeroNorm", 0.0);
        pan.setDoubleClickReturnValue (true, 0.0);
        reverb.setDoubleClickReturnValue (true, 0.0);

        volAtt = std::make_unique<SliderAtt> (p.apvts, "M_VOL", volume);
        panAtt = std::make_unique<SliderAtt> (p.apvts, "M_PAN", pan);
        revAtt = std::make_unique<SliderAtt> (p.apvts, "M_REV", reverb);
    }

    ~OutputPanel() override { pan.setLookAndFeel (nullptr); reverb.setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        using namespace masterui;
        g.fillAll (kBg);
        g.setColour (kBorder);
        g.drawVerticalLine (0, 0.0f, (float) getHeight());

        // tick marks either side of the fader
        const auto v = volume.getBounds().toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        for (int i = 0; i < 9; ++i)
        {
            const float y = v.getY() + 12.0f + (v.getHeight() - 24.0f) * (float) i / 8.0f;
            g.drawLine (v.getCentreX() - 30.0f, y, v.getCentreX() - 14.0f, y, 1.5f);
            g.drawLine (v.getCentreX() + 14.0f, y, v.getCentreX() + 30.0f, y, 1.5f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText ("Volume", 0, volume.getBottom() + 2, getWidth(), 20, juce::Justification::centred);
        g.setColour (kGrey);
        g.drawText ("Pan", pan.getX() - 6, pan.getBottom() + 2, pan.getWidth() + 12, 20, juce::Justification::centred);
        g.drawText ("Reverb", reverb.getX() - 10, reverb.getBottom() + 2, reverb.getWidth() + 20, 20, juce::Justification::centred);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("L", pan.getX() - 8, pan.getBottom() - 16, 12, 14, juce::Justification::centred);
        g.drawText ("R", pan.getRight() - 4, pan.getBottom() - 16, 12, 14, juce::Justification::centred);
    }

    void resized() override
    {
        const int w = getWidth();
        volume.setBounds (w / 2 - 16, 10, 32, getHeight() - 150);
        const int ky = volume.getBottom() + 38;
        pan.setBounds (16, ky, 52, 52);
        reverb.setBounds (w - 16 - 52, ky, 52, 52);
    }

private:
    AccentKnobLookAndFeel lnf;
    juce::Slider volume, pan, reverb;
    std::unique_ptr<masterui::SliderAtt> volAtt, panAtt, revAtt;
};
