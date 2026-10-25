#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CyberAcoustic {

class GuitarBodyVisualizer : public juce::Component, private juce::Timer {
public:
    GuitarBodyVisualizer() {
        startTimerHz(30); // 30 FPS gentle animation
    }

    ~GuitarBodyVisualizer() override {
        stopTimer();
    }

    void setParameters(int bodyType, float bodySize, float bodyShape, float audioLevel) {
        currentBodyType = bodyType;
        currentBodySize = bodySize;
        currentBodyShape = bodyShape;
        currentLevel = std::max(currentLevel * 0.85f, audioLevel); // Decay envelope
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(8.0f);
        auto center = bounds.getCentre();

        // 1. Dark wood grain chassis background
        g.setColour(juce::Colour(0xff141211));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xff2d2825));
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

        // 2. Draw Guitar Silhouette based on body type
        float scale = std::clamp(currentBodySize, 0.7f, 1.4f);
        float w = (bounds.getWidth() * 0.42f) * scale;
        float h = (bounds.getHeight() * 0.75f) * scale;

        juce::Path guitarBody;
        createGuitarPath(guitarBody, center.x, center.y, w, h, currentBodyType, currentBodyShape);

        // Fill with warm glowing spruce / mahogany gradient
        juce::ColourGradient woodFill(juce::Colour(0xff573b22), center.x, center.y - h * 0.4f,
                                     juce::Colour(0xff24170d), center.x, center.y + h * 0.4f, false);
        g.setGradientFill(woodFill);
        g.fillPath(guitarBody);

        // Subtle binding rim
        g.setColour(juce::Colour(0xffedd9be));
        g.strokePath(guitarBody, juce::PathStrokeType(1.8f));

        // 3. Soundhole / F-holes & Rosette
        if (currentBodyType == 6) {
            // Archtop with twin f-holes
            drawFHole(g, center.x - w * 0.28f, center.y - h * 0.05f, h * 0.35f, false);
            drawFHole(g, center.x + w * 0.28f, center.y - h * 0.05f, h * 0.35f, true);
        } else {
            // Round / Oval Soundhole
            float holeRadius = (currentBodyType == 7) ? w * 0.16f : w * 0.19f;
            float holeY = center.y - h * 0.12f;

            // Animated acoustic pressure ripples
            if (currentLevel > 0.01f) {
                float rippleRadius = holeRadius + currentLevel * 45.0f * (1.0f + animationPhase);
                g.setColour(juce::Colour(0xffd4974f).withAlpha(std::clamp((1.0f - animationPhase) * currentLevel * 0.8f, 0.0f, 1.0f)));
                g.drawEllipse(center.x - rippleRadius, holeY - rippleRadius * 0.9f, rippleRadius * 2.0f, rippleRadius * 1.8f, 1.5f);
            }

            // Rosette concentric rings
            g.setColour(juce::Colour(0xff916839));
            g.drawEllipse(center.x - (holeRadius + 6.0f), holeY - (holeRadius + 6.0f), (holeRadius + 6.0f) * 2.0f, (holeRadius + 6.0f) * 2.0f, 2.0f);
            g.setColour(juce::Colour(0xffeeddbb));
            g.drawEllipse(center.x - (holeRadius + 3.0f), holeY - (holeRadius + 3.0f), (holeRadius + 3.0f) * 2.0f, (holeRadius + 3.0f) * 2.0f, 1.0f);

            // Inner dark soundhole cavity
            g.setColour(juce::Colour(0xff0a0908));
            if (currentBodyType == 7) {
                // Selmer Petite Bouche Oval
                g.fillEllipse(center.x - holeRadius * 1.25f, holeY - holeRadius * 0.75f, holeRadius * 2.5f, holeRadius * 1.5f);
            } else {
                g.fillEllipse(center.x - holeRadius, holeY - holeRadius, holeRadius * 2.0f, holeRadius * 2.0f);
            }

            // Bridge & Saddle Assembly
            float bridgeW = w * 0.45f;
            float bridgeH = h * 0.08f;
            float bridgeY = center.y + h * 0.22f;
            g.setColour(juce::Colour(0xff120d09)); // Dark Rosewood / Ebony
            g.fillRoundedRectangle(center.x - bridgeW * 0.5f, bridgeY, bridgeW, bridgeH, 3.0f);
            // White Bone Saddle
            g.setColour(juce::Colour(0xfff0eee6));
            g.fillRoundedRectangle(center.x - bridgeW * 0.35f, bridgeY + bridgeH * 0.3f, bridgeW * 0.70f, 2.5f, 1.0f);
        }

        // 4. Header Label & Acoustic Resonance Spec
        g.setColour(juce::Colour(0xffedd9be));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(getBodyName(currentBodyType), bounds.removeFromTop(22.0f), juce::Justification::centred, false);

        g.setColour(juce::Colour(0xffa19284));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(getBodySpec(currentBodyType), bounds.removeFromBottom(20.0f), juce::Justification::centred, false);
    }

private:
    int currentBodyType = 0;
    float currentBodySize = 1.0f;
    float currentBodyShape = 0.5f;
    float currentLevel = 0.0f;
    float animationPhase = 0.0f;

    void timerCallback() override {
        animationPhase += 0.06f;
        if (animationPhase > 1.0f) animationPhase = 0.0f;
        if (currentLevel > 0.005f) repaint();
    }

    void createGuitarPath(juce::Path& p, float cx, float cy, float w, float h, int type, float shape) {
        float halfW = w * 0.5f;
        float halfH = h * 0.5f;
        float waistWidth = halfW * (0.68f - 0.15f * shape);
        float upperWidth = halfW * (0.82f + 0.10f * shape);
        float lowerWidth = halfW;

        if (type == 1) { // Jumbo
            waistWidth = halfW * 0.58f;
            lowerWidth = halfW * 1.15f;
        } else if (type == 4) { // Parlor
            waistWidth = halfW * 0.72f;
            lowerWidth = halfW * 0.85f;
            upperWidth = halfW * 0.78f;
        }

        p.startNewSubPath(cx, cy - halfH); // Top neck block
        // Right Upper Bout
        p.cubicTo(cx + upperWidth, cy - halfH * 0.85f, cx + upperWidth, cy - halfH * 0.35f, cx + waistWidth, cy - halfH * 0.05f);
        // Right Waist & Lower Bout
        p.cubicTo(cx + waistWidth * 0.85f, cy + halfH * 0.12f, cx + lowerWidth, cy + halfH * 0.35f, cx + lowerWidth * 0.85f, cy + halfH * 0.85f);
        // Bottom End Block
        p.cubicTo(cx + lowerWidth * 0.50f, cy + halfH, cx + halfW * 0.2f, cy + halfH, cx, cy + halfH);
        // Left Lower Bout
        p.cubicTo(cx - halfW * 0.2f, cy + halfH, cx - lowerWidth * 0.50f, cy + halfH, cx - lowerWidth * 0.85f, cy + halfH * 0.85f);
        p.cubicTo(cx - lowerWidth, cy + halfH * 0.35f, cx - waistWidth * 0.85f, cy + halfH * 0.12f, cx - waistWidth, cy - halfH * 0.05f);
        // Left Upper Bout
        p.cubicTo(cx - upperWidth, cy - halfH * 0.35f, cx - upperWidth, cy - halfH * 0.85f, cx, cy - halfH);
        p.closeSubPath();
    }

    void drawFHole(juce::Graphics& g, float x, float y, float len, bool mirrored) {
        g.setColour(juce::Colour(0xff0a0908));
        float dir = mirrored ? -1.0f : 1.0f;
        juce::Path f;
        f.startNewSubPath(x, y - len * 0.5f);
        f.cubicTo(x + 8.0f * dir, y - len * 0.25f, x - 6.0f * dir, y + len * 0.25f, x + 4.0f * dir, y + len * 0.5f);
        g.strokePath(f, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(x - 3.0f, y - len * 0.5f - 3.0f, 6.0f, 6.0f);
        g.fillEllipse(x + 4.0f * dir - 3.0f, y + len * 0.5f - 3.0f, 6.0f, 6.0f);
    }

    static juce::String getBodyName(int type) {
        switch (type) {
            case 0: return "Dreadnought (D-28)";
            case 1: return "Jumbo (J-200)";
            case 2: return "Orchestra (000 / OM)";
            case 3: return "Grand Auditorium";
            case 4: return "Parlor (Single-0)";
            case 5: return "Classical Nylon";
            case 6: return "Acoustic Jazz Archtop";
            case 7: return "Selmer Maccaferri";
            default: return "Custom Acoustic";
        }
    }

    static juce::String getBodySpec(int type) {
        switch (type) {
            case 0: return "Cavity: 23.5L | A0: 98Hz | T1: 195Hz";
            case 1: return "Cavity: 28.0L | A0: 88Hz | T1: 182Hz";
            case 2: return "Cavity: 17.2L | A0: 108Hz | T1: 210Hz";
            case 3: return "Cavity: 20.8L | A0: 102Hz | T1: 202Hz";
            case 4: return "Cavity: 12.5L | A0: 132Hz | T1: 238Hz";
            case 5: return "Cavity: 16.8L | A0: 102Hz | T1: 218Hz";
            case 6: return "Cavity: 19.0L | A0: 138Hz | T1: 225Hz";
            case 7: return "Cavity: 18.5L | A0: 118Hz | T1: 215Hz";
            default: return "Fluid Space Physical Model";
        }
    }
};

} // namespace CyberAcoustic
