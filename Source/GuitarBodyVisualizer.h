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

    void setParameters(int bodyType, float bodySize, float bodyShape,
                       bool dualMode, int bodyTypeB, float bodySizeB, float bodyShapeB,
                       float spread, bool phaseInvertB, bool doublerActive, float audioLevel) {
        currentBodyType = bodyType;
        currentBodySize = bodySize;
        currentBodyShape = bodyShape;
        isDualMode = dualMode;
        currentBodyTypeB = bodyTypeB;
        currentBodySizeB = bodySizeB;
        currentBodyShapeB = bodyShapeB;
        currentSpread = spread;
        currentPhaseInvertB = phaseInvertB;
        isDoublerActive = doublerActive;
        currentLevel = std::max(currentLevel * 0.85f, audioLevel); // Decay envelope
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(6.0f);
        auto center = bounds.getCentre();

        // 1. Dark wood grain chassis background
        g.setColour(juce::Colour(0xff141211));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xff2d2825));
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

        if (!isDualMode) {
            // ================================================================
            // Single Mono Body Emulation View
            // ================================================================
            float scale = std::clamp(currentBodySize, 0.7f, 1.4f);
            float w = (bounds.getWidth() * 0.42f) * scale;
            float h = (bounds.getHeight() * 0.72f) * scale;

            drawGuitarInstance(g, center.x, center.y, w, h, currentBodyType, currentBodyShape,
                               juce::Colour(0xff573b22), juce::Colour(0xff24170d),
                               juce::Colour(0xffedd9be), false);

            // Header & Footer Labels
            g.setColour(juce::Colour(0xffedd9be));
            g.setFont(juce::FontOptions(13.0f));
            g.drawText(getBodyName(currentBodyType), bounds.removeFromTop(20.0f), juce::Justification::centred, false);

            g.setColour(juce::Colour(0xffa19284));
            g.setFont(juce::FontOptions(10.5f));
            g.drawText(getBodySpec(currentBodyType), bounds.removeFromBottom(18.0f), juce::Justification::centred, false);
        } else {
            // ================================================================
            // Dual Stereo Body Split View (Left: Body A, Right: Body B)
            // ================================================================
            float cxA = bounds.getX() + bounds.getWidth() * 0.27f;
            float cxB = bounds.getX() + bounds.getWidth() * 0.73f;

            float scaleA = std::clamp(currentBodySize, 0.7f, 1.3f);
            float scaleB = std::clamp(currentBodySizeB, 0.7f, 1.3f);

            float wA = (bounds.getWidth() * 0.29f) * scaleA;
            float hA = (bounds.getHeight() * 0.62f) * scaleA;

            float wB = (bounds.getWidth() * 0.29f) * scaleB;
            float hB = (bounds.getHeight() * 0.62f) * scaleB;

            float bodyY = center.y + 2.0f;

            // Draw Body A (Left - Spruce Tone)
            drawGuitarInstance(g, cxA, bodyY, wA, hA, currentBodyType, currentBodyShape,
                               juce::Colour(0xff573b22), juce::Colour(0xff24170d),
                               juce::Colour(0xffedd9be), false);

            // Draw Body B (Right - Deep Rosewood Tone with Phase Pill)
            drawGuitarInstance(g, cxB, bodyY, wB, hB, currentBodyTypeB, currentBodyShapeB,
                               juce::Colour(0xff4a1f1a), juce::Colour(0xff1c0d0a),
                               juce::Colour(0xffe29d72), currentPhaseInvertB);

            // Center Stereo Spread Gauge
            auto midArea = juce::Rectangle<float>(cxA + wA * 0.5f + 2.0f, center.y - 14.0f,
                                                 (cxB - wB * 0.5f) - (cxA + wA * 0.5f) - 4.0f, 28.0f);
            if (midArea.getWidth() > 20.0f) {
                g.setColour(juce::Colour(0x33d4974f));
                g.fillRoundedRectangle(midArea, 4.0f);
                g.setColour(juce::Colour(0xffd4974f));
                g.setFont(juce::FontOptions(9.5f));
                g.drawText("SPREAD", midArea.removeFromTop(12.0f), juce::Justification::centred, false);
                g.drawText(juce::String((int)currentSpread) + "%", midArea, juce::Justification::centred, false);
            }

            // Top Header Labels (A / B)
            auto topArea = bounds.removeFromTop(20.0f);
            int halfW = (int)(topArea.getWidth() / 2);
            g.setFont(juce::FontOptions(11.5f));

            g.setColour(juce::Colour(0xffedd9be));
            g.drawText((isDoublerActive ? "PLAYER 1 (L): " : "L: ") + getShortBodyName(currentBodyType), topArea.removeFromLeft(halfW), juce::Justification::centredLeft, false);

            g.setColour(juce::Colour(0xffe29d72));
            g.drawText((isDoublerActive ? "PLAYER 2 (R): " : "R: ") + getShortBodyName(currentBodyTypeB), topArea, juce::Justification::centredRight, false);

            // Bottom Footer Status
            auto bottomArea = bounds.removeFromBottom(18.0f);
            g.setFont(juce::FontOptions(10.0f));
            if (isDoublerActive) {
                g.setColour(juce::Colour(0xff38bdf8));
                if (currentPhaseInvertB) {
                    g.drawText("DUAL STEREO DOUBLER \u2022 DRIFT & AIR BLEED \u2022 \u03A6 INV R", bottomArea, juce::Justification::centred, false);
                } else {
                    g.drawText("DUAL STEREO DOUBLER \u2022 DUAL-TRACKED SEPARATION (11-22ms DRIFT)", bottomArea, juce::Justification::centred, false);
                }
            } else if (currentPhaseInvertB) {
                g.setColour(juce::Colour(0xfff59e0b));
                g.drawText("DUAL STEREO \u2022 RIGHT POLARITY INVERTED [WIDE ACOUSTIC TRICK]", bottomArea, juce::Justification::centred, false);
            } else {
                g.setColour(juce::Colour(0xffa19284));
                g.drawText("DUAL STEREO \u2022 DUAL DISCRETE ACOUSTIC CAVITIES", bottomArea, juce::Justification::centred, false);
            }
        }
    }

private:
    int currentBodyType = 0;
    float currentBodySize = 1.0f;
    float currentBodyShape = 0.5f;

    bool isDualMode = false;
    bool isDoublerActive = true;
    int currentBodyTypeB = 2;
    float currentBodySizeB = 1.0f;
    float currentBodyShapeB = 0.5f;
    float currentSpread = 100.0f;
    bool currentPhaseInvertB = false;

    float currentLevel = 0.0f;
    float animationPhase = 0.0f;

    void timerCallback() override {
        animationPhase += 0.06f;
        if (animationPhase > 1.0f) animationPhase = 0.0f;
        if (currentLevel > 0.005f) repaint();
    }

    void drawGuitarInstance(juce::Graphics& g, float cx, float cy, float w, float h,
                            int type, float shape, juce::Colour topCol, juce::Colour botCol,
                            juce::Colour rimCol, bool showPhaseBadge) {
        juce::Path guitarBody;
        createGuitarPath(guitarBody, cx, cy, w, h, type, shape);

        // Fill with wood gradient
        juce::ColourGradient woodFill(topCol, cx, cy - h * 0.4f, botCol, cx, cy + h * 0.4f, false);
        g.setGradientFill(woodFill);
        g.fillPath(guitarBody);

        // Binding rim
        g.setColour(rimCol);
        g.strokePath(guitarBody, juce::PathStrokeType(1.5f));

        if (type == 6 || type == 8) {
            // Archtop & F-Style Mandolin with twin f-holes
            float fLen = (type == 8) ? h * 0.28f : h * 0.35f;
            float fOffset = (type == 8) ? w * 0.24f : w * 0.28f;
            drawFHole(g, cx - fOffset, cy - h * 0.05f, fLen, false);
            drawFHole(g, cx + fOffset, cy - h * 0.05f, fLen, true);

            // Floating Bridge Assembly
            float bridgeW = (type == 8) ? w * 0.32f : w * 0.45f;
            float bridgeH = h * 0.065f;
            float bridgeY = cy + h * 0.18f;
            g.setColour(juce::Colour(0xff120d09)); // Ebony bridge
            g.fillRoundedRectangle(cx - bridgeW * 0.5f, bridgeY, bridgeW, bridgeH, 2.5f);
            // Thumbwheels
            g.setColour(juce::Colour(0xffd4974f));
            g.fillEllipse(cx - bridgeW * 0.35f, bridgeY + 2.0f, 3.5f, 3.5f);
            g.fillEllipse(cx + bridgeW * 0.35f - 3.5f, bridgeY + 2.0f, 3.5f, 3.5f);
        } else {
            // Round / Oval Soundhole
            float holeRadius = (type == 7) ? w * 0.16f : w * 0.19f;
            float holeY = cy - h * 0.12f;

            // Animated acoustic pressure ripples
            if (currentLevel > 0.01f) {
                float rippleRadius = holeRadius + currentLevel * 35.0f * (1.0f + animationPhase);
                g.setColour(rimCol.withAlpha(std::clamp((1.0f - animationPhase) * currentLevel * 0.8f, 0.0f, 1.0f)));
                g.drawEllipse(cx - rippleRadius, holeY - rippleRadius * 0.9f, rippleRadius * 2.0f, rippleRadius * 1.8f, 1.2f);
            }

            // Rosette concentric rings
            g.setColour(juce::Colour(0xff916839));
            g.drawEllipse(cx - (holeRadius + 5.0f), holeY - (holeRadius + 5.0f), (holeRadius + 5.0f) * 2.0f, (holeRadius + 5.0f) * 2.0f, 1.5f);
            g.setColour(rimCol);
            g.drawEllipse(cx - (holeRadius + 2.5f), holeY - (holeRadius + 2.5f), (holeRadius + 2.5f) * 2.0f, (holeRadius + 2.5f) * 2.0f, 1.0f);

            // Inner dark soundhole cavity
            g.setColour(juce::Colour(0xff0a0908));
            if (type == 7) {
                g.fillEllipse(cx - holeRadius * 1.25f, holeY - holeRadius * 0.75f, holeRadius * 2.5f, holeRadius * 1.5f);
            } else {
                g.fillEllipse(cx - holeRadius, holeY - holeRadius, holeRadius * 2.0f, holeRadius * 2.0f);
            }

            // Bridge & Saddle Assembly
            float bridgeW = w * 0.45f;
            float bridgeH = h * 0.08f;
            float bridgeY = cy + h * 0.22f;
            g.setColour(juce::Colour(0xff120d09)); // Dark Rosewood / Ebony
            g.fillRoundedRectangle(cx - bridgeW * 0.5f, bridgeY, bridgeW, bridgeH, 3.0f);
            // White Bone Saddle
            g.setColour(juce::Colour(0xfff0eee6));
            g.fillRoundedRectangle(cx - bridgeW * 0.35f, bridgeY + bridgeH * 0.3f, bridgeW * 0.70f, 2.0f, 1.0f);
        }

        // Phase Inversion Badge (on Right Channel)
        if (showPhaseBadge) {
            float badgeW = 48.0f;
            float badgeH = 15.0f;
            auto badgeArea = juce::Rectangle<float>(cx - badgeW * 0.5f, cy + h * 0.38f, badgeW, badgeH);
            g.setColour(juce::Colour(0xddb45309)); // Glowing dark amber
            g.fillRoundedRectangle(badgeArea, 3.0f);
            g.setColour(juce::Colour(0xfffef08a));
            g.drawRoundedRectangle(badgeArea, 3.0f, 1.0f);
            g.setFont(juce::FontOptions(9.0f));
            g.drawText("Φ 180° INV", badgeArea, juce::Justification::centred, false);
        }
    }

    void createGuitarPath(juce::Path& p, float cx, float cy, float w, float h, int type, float shape) {
        float halfW = w * 0.5f;
        float halfH = h * 0.5f;

        if (type == 8) { // F-Style Mandolin (Compact body with upper scroll horn and points)
            float mHalfW = halfW * 0.85f;
            float mHalfH = halfH * 0.85f;
            p.startNewSubPath(cx, cy - mHalfH);
            p.lineTo(cx + mHalfW * 0.65f, cy - mHalfH * 0.95f);
            p.lineTo(cx + mHalfW * 0.92f, cy - mHalfH * 0.70f); // Upper Point
            p.cubicTo(cx + mHalfW * 0.80f, cy - mHalfH * 0.35f, cx + mHalfW, cy + mHalfH * 0.15f, cx + mHalfW * 0.90f, cy + mHalfH * 0.75f);
            p.cubicTo(cx + mHalfW * 0.50f, cy + mHalfH, cx + mHalfW * 0.20f, cy + mHalfH, cx, cy + mHalfH); // Bottom teardrop
            p.cubicTo(cx - mHalfW * 0.20f, cy + mHalfH, cx - mHalfW * 0.50f, cy + mHalfH, cx - mHalfW * 0.90f, cy + mHalfH * 0.75f);
            p.cubicTo(cx - mHalfW, cy + mHalfH * 0.15f, cx - mHalfW * 0.80f, cy - mHalfH * 0.35f, cx - mHalfW * 1.15f, cy - mHalfH * 0.70f); // Left Scroll Horn
            p.cubicTo(cx - mHalfW * 1.05f, cy - mHalfH * 0.90f, cx - mHalfW * 0.55f, cy - mHalfH * 1.05f, cx, cy - mHalfH);
            p.closeSubPath();
            return;
        }

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
        p.cubicTo(cx + upperWidth, cy - halfH * 0.85f, cx + upperWidth, cy - halfH * 0.35f, cx + waistWidth, cy - halfH * 0.05f);
        p.cubicTo(cx + waistWidth * 0.85f, cy + halfH * 0.12f, cx + lowerWidth, cy + halfH * 0.35f, cx + lowerWidth * 0.85f, cy + halfH * 0.85f);
        p.cubicTo(cx + lowerWidth * 0.50f, cy + halfH, cx + halfW * 0.2f, cy + halfH, cx, cy + halfH);
        p.cubicTo(cx - halfW * 0.2f, cy + halfH, cx - lowerWidth * 0.50f, cy + halfH, cx - lowerWidth * 0.85f, cy + halfH * 0.85f);
        p.cubicTo(cx - lowerWidth, cy + halfH * 0.35f, cx - waistWidth * 0.85f, cy + halfH * 0.12f, cx - waistWidth, cy - halfH * 0.05f);
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
            case 8: return "F-Style Mandolin (F-5)";
            default: return "Custom Acoustic";
        }
    }

    static juce::String getShortBodyName(int type) {
        switch (type) {
            case 0: return "Dreadnought";
            case 1: return "J-200 Jumbo";
            case 2: return "000 / OM";
            case 3: return "Auditorium";
            case 4: return "Parlor";
            case 5: return "Classical";
            case 6: return "Archtop";
            case 7: return "Selmer";
            case 8: return "F-5 Mandolin";
            default: return "Custom";
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
            case 8: return "Cavity: 2.2L | A0: 285Hz | T1: 350Hz";
            default: return "Fluid Space Physical Model";
        }
    }
};

} // namespace CyberAcoustic
