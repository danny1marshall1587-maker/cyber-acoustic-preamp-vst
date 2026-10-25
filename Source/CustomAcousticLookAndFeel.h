#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CyberAcoustic {

class CustomAcousticLookAndFeel : public juce::LookAndFeel_V4 {
public:
    CustomAcousticLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff181615));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd4974f));      // Warm amber/spruce
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2d2a28));   // Dark chassis
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffe8c382));                 // Gold indicator
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff242120));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff4a433f));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffedd9be));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1e1c1b));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffedd9be));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffd4974f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff121110));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto center = bounds.getCentre();

        // 1. Outer Dark Shadow Ring
        g.setColour(juce::Colour(0xff0e0d0c));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // 2. Arc Track Background
        auto arcRadius = radius - 4.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f,
                                   rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff2d2a28));
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Colored Arc
        juce::Path valueArc;
        valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, toAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Metallic Knob Rotor Body
        auto knobRadius = radius - 10.0f;
        if (knobRadius > 4.0f) {
            juce::ColourGradient knobGrad(juce::Colour(0xff3c3632), center.x - knobRadius, center.y - knobRadius,
                                         juce::Colour(0xff1f1c1a), center.x + knobRadius, center.y + knobRadius, false);
            g.setGradientFill(knobGrad);
            g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

            // Subtle bezel border
            g.setColour(juce::Colour(0xff524a45));
            g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

            // 5. Dial Indicator Needle / Dot
            juce::Path p;
            auto pointerLength = knobRadius * 0.72f;
            auto pointerThickness = 2.5f;
            p.addRoundedRectangle(-pointerThickness * 0.5f, -knobRadius, pointerThickness, pointerLength, 1.2f);
            p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));

            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillPath(p);

            // Center Brass/Gold Cap Accent
            auto capRadius = knobRadius * 0.28f;
            g.setColour(juce::Colour(0xff4a3f32));
            g.fillEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
            g.setColour(juce::Colour(0xff9e7a46));
            g.drawEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);
        }
    }
};

} // namespace CyberAcoustic
