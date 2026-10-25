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

    // Presets Bar
    juce::ComboBox presetSelector;

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
