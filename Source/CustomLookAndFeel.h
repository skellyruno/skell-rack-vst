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
