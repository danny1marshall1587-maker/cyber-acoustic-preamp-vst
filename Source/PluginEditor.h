#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "CustomAcousticLookAndFeel.h"
#include "GuitarBodyVisualizer.h"

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
