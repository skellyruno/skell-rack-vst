#pragma once
#include <JuceHeader.h>

class SeigaihaDrawer
{
public:
    static void drawPattern(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour waveColor)
    {
        if (bounds.isEmpty() || bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f) return;

        g.setColour(waveColor.withAlpha(0.25f));
        juce::Path wavePattern;
        const float radius = 24.0f;
        const float startY = bounds.getBottom();

        for (float y = startY; y > bounds.getY() + 30.0f; y -= 24.0f)
        {
            for (float x = bounds.getX(); x < bounds.getRight() + 40.0f; x += 48.0f)
            {
                wavePattern.addCentredArc(x, y, radius, radius, 0.0f,
                                          -juce::MathConstants<float>::halfPi,
                                          juce::MathConstants<float>::halfPi, true);
            }
        }

        if (!wavePattern.isEmpty())
            g.strokePath(wavePattern, juce::PathStrokeType(1.5f));
    }
};

class TeardropKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        if (width <= 0 || height <= 0) return;

        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                            static_cast<float>(y),
                                            static_cast<float>(width),
                                            static_cast<float>(height)).reduced(4.0f);
        if (bounds.isEmpty()) return;

        auto center = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        if (radius <= 3.0f) return;

        float pos = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
        auto angle = rotaryStartAngle + pos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillEllipse(bounds.translated(0.0f, 2.0f));

        juce::Path teardrop;
        float topY = center.y - radius;
        float bulbRadius = radius * 0.65f;

        teardrop.startNewSubPath(center.x, topY);
        teardrop.cubicTo(center.x + radius, center.y - bulbRadius,
                         center.x + bulbRadius, center.y + radius,
                         center.x, center.y + radius);
        teardrop.cubicTo(center.x - bulbRadius, center.y + radius,
                         center.x - radius, center.y - bulbRadius,
                         center.x, topY);
        teardrop.closeSubPath();

        teardrop.applyTransform(juce::AffineTransform::rotation(angle, center.x, center.y));

        g.setColour(juce::Colours::white);
        g.fillPath(teardrop);

        g.setColour(juce::Colour(0xFF7A9BB0).withMultipliedAlpha(0.6f));
        g.strokePath(teardrop, juce::PathStrokeType(1.2f));
    }
};

class PurpleArcKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        if (width <= 0 || height <= 0) return;

        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                            static_cast<float>(y),
                                            static_cast<float>(width),
                                            static_cast<float>(height)).reduced(6.0f);
        if (bounds.isEmpty()) return;

        auto center = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        if (radius <= 3.0f) return;

        float pos = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
        auto angle = rotaryStartAngle + pos * (rotaryEndAngle - rotaryStartAngle);

        juce::Path trackPath;
        trackPath.addCentredArc(center.x, center.y, radius + 4.0f, radius + 4.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.strokePath(trackPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (angle > rotaryStartAngle + 0.01f)
        {
            juce::Path arcPath;
            arcPath.addCentredArc(center.x, center.y, radius + 4.0f, radius + 4.0f, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xFFA020F0));
            g.strokePath(arcPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colours::white);
        g.fillEllipse(bounds);

        juce::Path pointer;
        pointer.startNewSubPath(center);
        pointer.lineTo(center.x + std::sin(angle) * (radius * 0.65f),
                       center.y - std::cos(angle) * (radius * 0.65f));
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// Generic knob used by the cards that are built from the FX registry.
// Draws an accent-coloured value arc around a dark knob body.
class AccentKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AccentKnobLookAndFeel(juce::Colour accentColour, juce::Colour bodyColour)
        : accent(accentColour), body(bodyColour)
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.85f));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, accentColour.withAlpha(0.4f));

        setColour(juce::ComboBox::backgroundColourId, bodyColour.darker(0.3f));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::outlineColourId, accentColour.withAlpha(0.5f));
        setColour(juce::ComboBox::arrowColourId, accentColour);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF1E1E1E));
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha(0.6f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour(juce::TextButton::buttonColourId, bodyColour.darker(0.3f));
        setColour(juce::TextButton::buttonOnColourId, accentColour);
        setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);

        setColour(juce::ToggleButton::tickColourId, accentColour);
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
        setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    }

    juce::Font getLabelFont(juce::Label&) override { return juce::Font(juce::FontOptions(12.0f)); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        if (width <= 0 || height <= 0) return;

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(3.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        if (radius <= 6.0f) return;

        const auto centre = bounds.getCentre();
        const float alpha = slider.isEnabled() ? 1.0f : 0.35f;
        const float pos = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
        const float angle = rotaryStartAngle + pos * (rotaryEndAngle - rotaryStartAngle);
        const float arcR = radius - 2.0f;

        // "ticks" knobs (Input / Output / Frequency) show a ring of small dots and - / + marks instead of a value arc
        const bool ticks = (bool) slider.getProperties()["ticks"];
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        if (! ticks)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * alpha));
            g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.45f * alpha));
            for (int i = 0; i < 9; ++i)
            {
                const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float) i / 8.0f;
                g.fillEllipse(centre.x + std::sin(a) * (arcR - 0.5f) - 1.0f, centre.y - std::cos(a) * (arcR - 0.5f) - 1.0f, 2.0f, 2.0f);
            }
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText("-", juce::Rectangle<float>(centre.x - radius - 6.0f, centre.y + radius * 0.35f, 14.0f, 14.0f), juce::Justification::centred);
            g.drawText("+", juce::Rectangle<float>(centre.x + radius - 8.0f, centre.y + radius * 0.35f, 14.0f, 14.0f), juce::Justification::centred);
        }

        // fill from the left, or from the centre for bipolar controls
        // (slot parameters are plain 0..1 values, so bipolar controls say where their zero sits with a property)
        const auto zeroProp = slider.getProperties()["zeroNorm"];
        const bool bipolar = ! zeroProp.isVoid();
        const float zeroPos = bipolar ? static_cast<float>(slider.valueToProportionOfLength(static_cast<double>(zeroProp))) : 0.0f;
        const float zeroAngle = rotaryStartAngle + zeroPos * (rotaryEndAngle - rotaryStartAngle);
        const float from = juce::jmin(zeroAngle, angle), to = juce::jmax(zeroAngle, angle);
        if (! ticks && to - from > 0.01f)
        {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, from, to, true);
            g.setColour(accent.withAlpha(alpha));
            g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float bodyR = radius - 7.0f;
        g.setColour(juce::Colours::black.withAlpha(0.25f * alpha));
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR + 2.0f, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour(body.withAlpha(alpha));
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.15f * alpha));
        g.drawEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.startNewSubPath(centre.x + std::sin(angle) * bodyR * 0.35f, centre.y - std::cos(angle) * bodyR * 0.35f);
        pointer.lineTo(centre.x + std::sin(angle) * bodyR * 0.9f, centre.y - std::cos(angle) * bodyR * 0.9f);
        g.setColour((body.getBrightness() > 0.7f ? juce::Colour(0xFF14161A) : juce::Colours::white).withAlpha(alpha));
        g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Combo boxes look like BandLab's: a rounded dark field, a small accent caption on top, a chevron on the right.
    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
        g.setColour(juce::Colour(0xFF1E2126).withAlpha(box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle(r, 12.0f);

        const juce::String caption = box.getProperties()["caption"].toString();
        if (caption.isNotEmpty())
        {
            g.setColour(accent);
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(caption, juce::Rectangle<int>(14, 4, width - 40, 14), juce::Justification::centredLeft);
        }

        juce::Path chevron;
        const float cx = (float) width - 20.0f, cy = (float) height * 0.5f;
        chevron.startNewSubPath(cx - 5.0f, cy - 2.5f); chevron.lineTo(cx, cy + 2.5f); chevron.lineTo(cx + 5.0f, cy - 2.5f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        const bool hasCaption = box.getProperties()["caption"].toString().isNotEmpty();
        label.setBounds(10, hasCaption ? 15 : 1, box.getWidth() - 40, box.getHeight() - (hasCaption ? 19 : 2));
        label.setFont(juce::Font(juce::FontOptions(hasCaption ? 16.0f : 14.0f)));
    }

private:
    juce::Colour accent, body;
};
