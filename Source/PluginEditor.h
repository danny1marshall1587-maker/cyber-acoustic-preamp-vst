#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CustomAcousticLookAndFeel.h"
#include "GuitarBodyVisualizer.h"

namespace CyberAcoustic {

class AcousticInputMeter : public juce::Component {
public:
    void setLevels(float rmsDb, float peakDb) {
        currentRmsDb = rmsDb;
        currentPeakDb = peakDb;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        // Background track
        g.setColour(juce::Colour(0xff12100f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff332b26));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        auto meterBarArea = bounds.reduced(3.0f);
        float w = meterBarArea.getWidth();
        float h = meterBarArea.getHeight();

        // Range mapping: -48 dBFS to 0 dBFS
        auto dbToX = [w](float db) {
            float norm = std::clamp((db + 48.0f) / 48.0f, 0.0f, 1.0f);
            return norm * w;
        };

        // Zone background shading:
        // Low: -48 to -24 dB
        // Good: -24 to -12 dB (emerald target sweet spot)
        // Hot: -12 to -3 dB (amber warning)
        // Clip: -3 to 0 dB (red warning)
        float xLow = dbToX(-24.0f);
        float xGood = dbToX(-12.0f);
        float xHot = dbToX(-3.0f);

        g.setColour(juce::Colour(0x2210b981)); // Green zone
        g.fillRect(meterBarArea.getX() + xLow, meterBarArea.getY(), xGood - xLow, h);
        g.setColour(juce::Colour(0x22f59e0b)); // Amber zone
        g.fillRect(meterBarArea.getX() + xGood, meterBarArea.getY(), xHot - xGood, h);
        g.setColour(juce::Colour(0x33ef4444)); // Red zone
        g.fillRect(meterBarArea.getX() + xHot, meterBarArea.getY(), w - xHot, h);

        // Active RMS Bar fill
        float rmsX = dbToX(currentRmsDb);
        if (rmsX > 1.0f) {
            juce::Colour fillColour;
            if (currentRmsDb < -24.0f) fillColour = juce::Colour(0xff3b82f6); // Cyan/Blue
            else if (currentRmsDb <= -12.0f) fillColour = juce::Colour(0xff10b981); // Emerald Green GOOD
            else if (currentRmsDb <= -3.0f) fillColour = juce::Colour(0xfff59e0b);  // Amber HOT
            else fillColour = juce::Colour(0xffef4444); // Red CLIP

            g.setColour(fillColour.withAlpha(0.85f));
            g.fillRoundedRectangle(meterBarArea.getX(), meterBarArea.getY(), rmsX, h, 2.0f);
        }

        // Peak needle line
        float peakX = dbToX(currentPeakDb);
        if (peakX > 2.0f) {
            g.setColour(juce::Colour(0xffffffff));
            g.fillRect(meterBarArea.getX() + peakX - 1.0f, meterBarArea.getY(), 2.0f, h);
        }

        // Zone Tick Lines
        g.setColour(juce::Colour(0xff6e6155));
        g.drawVerticalLine((int)(meterBarArea.getX() + xLow), meterBarArea.getY(), meterBarArea.getBottom());
        g.drawVerticalLine((int)(meterBarArea.getX() + xGood), meterBarArea.getY(), meterBarArea.getBottom());
        g.drawVerticalLine((int)(meterBarArea.getX() + xHot), meterBarArea.getY(), meterBarArea.getBottom());

        // Status Label Text in Center
        g.setFont(juce::FontOptions(9.5f));
        if (currentRmsDb >= -24.0f && currentRmsDb <= -12.0f) {
            g.setColour(juce::Colour(0xff34d399)); // Glowing bright emerald
            g.drawText("● OPTIMAL SIGNAL (-24..-12dB)", bounds, juce::Justification::centred, false);
        } else if (currentRmsDb > -12.0f) {
            g.setColour(juce::Colour(0xfffca5a5));
            g.drawText(currentRmsDb > -3.0f ? "▲ SIGNAL CLIPPING" : "▲ SIGNAL HOT", bounds, juce::Justification::centred, false);
        } else if (currentRmsDb > -45.0f) {
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText("▼ SIGNAL LOW (BOOST GAIN)", bounds, juce::Justification::centred, false);
        } else {
            g.setColour(juce::Colour(0xff665a50));
            g.drawText("INPUT RMS METER", bounds, juce::Justification::centred, false);
        }
    }

private:
    float currentRmsDb = -100.0f;
    float currentPeakDb = -100.0f;
};

} // namespace CyberAcoustic

class CyberAcousticEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CyberAcousticEditor(CyberAcousticProcessor&);
    ~CyberAcousticEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void loadPreset(int presetIndex);

    CyberAcousticProcessor& audioProcessor;
    CyberAcoustic::CustomAcousticLookAndFeel lookAndFeel;
    CyberAcoustic::GuitarBodyVisualizer visualizer;
    CyberAcoustic::AcousticInputMeter inputMeter;

    // Header Staging & A/B Compare
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;

    juce::ToggleButton masterBypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAttachment;

    // Presets Bar & Input Selector
    juce::ComboBox presetSelector;
    juce::ComboBox inputSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputAttachment;

    // Controls: 10-Band Cyber-Denoiser PRO
    juce::Slider denoiseAmountSlider;
    juce::Label denoiseAmountLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> denoiseAmountAttachment;

    juce::Slider denoiseThreshSlider;
    juce::Label denoiseThreshLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> denoiseThreshAttachment;

    juce::ToggleButton denoiseShieldToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> denoiseShieldAttachment;

    juce::ToggleButton denoiseListenToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> denoiseListenAttachment;

    // Controls: Transducer & Anti-Quack
    juce::Slider quackSlider;
    juce::Label quackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> quackAttachment;

    juce::Slider deFizzSlider;
    juce::Label deFizzLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> deFizzAttachment;

    juce::Slider airShimmerSlider;
    juce::Label airShimmerLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> airShimmerAttachment;

    // Controls: Fluid Acoustic Cavity
    juce::ComboBox bodyTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bodyTypeAttachment;

    juce::Slider bodySizeSlider;
    juce::Label bodySizeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodySizeAttachment;

    juce::Slider bodyShapeSlider;
    juce::Label bodyShapeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodyShapeAttachment;

    juce::Slider bodyDepthSlider;
    juce::Label bodyDepthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodyDepthAttachment;

    // Controls: Dual Body Stereo Split & Widening Trick
    juce::ToggleButton dualModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dualModeAttachment;

    juce::ToggleButton doublerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> doublerAttachment;

    juce::ToggleButton phaseInvertButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseInvertAttachment;

    juce::ComboBox bodyTypeBBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bodyTypeBAttachment;

    juce::Slider bodySizeBSlider;
    juce::Label bodySizeBLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodySizeBAttachment;

    juce::Slider bodyShapeBSlider;
    juce::Label bodyShapeBLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodyShapeBAttachment;

    juce::Slider bodySpreadSlider;
    juce::Label bodySpreadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodySpreadAttachment;

    juce::Slider doublerSlopSlider;
    juce::Label doublerSlopLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> doublerSlopAttachment;

    juce::Slider doublerCouplingSlider;
    juce::Label doublerCouplingLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> doublerCouplingAttachment;

    // Controls: Fluid Air & Close-Mic Radiation (Simulates Close Mic vs Pickup Openness)
    juce::Slider micOpennessSlider;
    juce::Label micOpennessLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micOpennessAttachment;

    juce::Slider micAirSheenSlider;
    juce::Label micAirSheenLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micAirSheenAttachment;

    juce::Slider micProximitySlider;
    juce::Label micProximityLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micProximityAttachment;

    // Controls: Studio 4-Band Acoustic Parametric EQ (Active 3D Tone Matrix)
    juce::Slider eqLowGainSlider;
    juce::Label eqLowGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqLowGainAttachment;

    juce::Slider eqLowMidFreqSlider;
    juce::Label eqLowMidFreqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqLowMidFreqAttachment;

    juce::Slider eqLowMidGainSlider;
    juce::Label eqLowMidGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqLowMidGainAttachment;

    juce::Slider eqHighMidFreqSlider;
    juce::Label eqHighMidFreqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqHighMidFreqAttachment;

    juce::Slider eqHighMidGainSlider;
    juce::Label eqHighMidGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqHighMidGainAttachment;

    juce::Slider eqHighGainSlider;
    juce::Label eqHighGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqHighGainAttachment;

    // Controls: Pure Fluid Dynamics Reverb Space (From Euphoria Amp)
    juce::ComboBox reverbTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbTypeAttachment;

    juce::Slider reverbMixSlider;
    juce::Label reverbMixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttachment;

    juce::Slider reverbDecaySlider;
    juce::Label reverbDecayLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbDecayAttachment;

    juce::Slider reverbSizeSlider;
    juce::Label reverbSizeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttachment;

    juce::Slider reverbToneSlider;
    juce::Label reverbToneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbToneAttachment;

    // Controls: Stage Master & Feedback Killer
    juce::ToggleButton afd1Toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> afd1Attachment;
    juce::Slider afdFreq1Slider;
    juce::Label afdFreq1Label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> afdFreq1Attachment;

    juce::ToggleButton afd2Toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> afd2Attachment;
    juce::Slider afdFreq2Slider;
    juce::Label afdFreq2Label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> afdFreq2Attachment;

    juce::Slider stereoWidthSlider;
    juce::Label stereoWidthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoWidthAttachment;

    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberAcousticEditor)
};
