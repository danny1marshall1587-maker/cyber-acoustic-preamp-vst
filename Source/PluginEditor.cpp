#include "PluginEditor.h"

CyberAcousticEditor::CyberAcousticEditor(CyberAcousticProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    // 1. Preset Selector
    addAndMakeVisible(presetSelector);
    presetSelector.addItem("Preset: Dreadnought Studio Flatpicker", 1);
    presetSelector.addItem("Preset: Modern Fingerstyle 000/OM", 2);
    presetSelector.addItem("Preset: Cavernous J-200 Strumming", 3);
    presetSelector.addItem("Preset: Delta Blues Boxy Parlor", 4);
    presetSelector.addItem("Preset: Concert Classical Warm Nylon", 5);
    presetSelector.addItem("Preset: Acoustic Jazz Archtop Comp", 6);
    presetSelector.addItem("Preset: Gypsy Django Selmer Lead", 7);
    presetSelector.addItem("Preset: Takamine Piezo Rescue", 8);
    presetSelector.addItem("Preset: Martin D-45 Deep Air", 9);
    presetSelector.addItem("Preset: Bluegrass F-5 Mandolin Chop", 10);
    presetSelector.addItem("Preset: Dual Stereo - Dread & OM Split", 11);
    presetSelector.addItem("Preset: Dual Stereo - Jumbo & Mandolin", 12);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this]() {
        loadPreset(presetSelector.getSelectedId());
    };

    // 1b. Input Routing Selector
    addAndMakeVisible(inputSelector);
    inputSelector.addItem("Input: Mono (In 1 / Left)", 1);
    inputSelector.addItem("Input: Mono (In 2 / Right)", 2);
    inputSelector.addItem("Input: Auto-Detect Active", 3);
    inputSelector.addItem("Input: Stereo Sum (-3dB)", 4);
    inputSelector.addItem("Input: Stereo Discrete", 5);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "inputRouting", inputSelector);

    // Helper for configuring sliders
    auto setupRotary = [this](juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
        s.setTextValueSuffix(suffix);
        addAndMakeVisible(s);

        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::FontOptions(11.0f));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffd5c4b1));
        addAndMakeVisible(l);
    };

    auto setupToggle = [this](juce::ToggleButton& b, const juce::String& text) {
        b.setButtonText(text);
        b.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffedd9be));
        b.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffd4974f));
        b.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff55483d));
        addAndMakeVisible(b);
    };

    // 0. Header Staging & Level-Matched A/B Bypass
    setupRotary(inputGainSlider, inputGainLabel, "IN GAIN", " dB");
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "inputGain", inputGainSlider);

    addAndMakeVisible(inputMeter);

    setupToggle(masterBypassButton, "A/B MATCHED BYPASS");
    masterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "masterBypass", masterBypassButton);

    // 1c. 10-Band Cyber-Denoiser PRO
    setupRotary(denoiseAmountSlider, denoiseAmountLabel, "D-NOISE", "%");
    denoiseAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "denoiseAmount", denoiseAmountSlider);

    setupRotary(denoiseThreshSlider, denoiseThreshLabel, "THRESHOLD", " dB");
    denoiseThreshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "denoiseThreshold", denoiseThreshSlider);

    setupToggle(denoiseShieldToggle, "HARMONIC SHIELD");
    denoiseShieldAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "denoiseHarmonicShield", denoiseShieldToggle);

    setupToggle(denoiseListenToggle, "LISTEN NOISE");
    denoiseListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "denoiseListen", denoiseListenToggle);

    // 2. Anti-Quack & De-Fizz Sliders
    setupRotary(quackSlider, quackLabel, "ANTI-QUACK", "%");
    quackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "quackReduction", quackSlider);

    setupRotary(deFizzSlider, deFizzLabel, "SMART DE-FIZZ", "%");
    deFizzAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "deFizz", deFizzSlider);

    setupRotary(airShimmerSlider, airShimmerLabel, "AIR SHIMMER", "%");
    airShimmerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "airShimmer", airShimmerSlider);

    // 3. Fluid Acoustic Cavity & Dual Stereo Controls
    juce::StringArray bodyNames = {
        "Dreadnought (D-28)",
        "Jumbo (J-200)",
        "Orchestra (000/OM)",
        "Grand Auditorium",
        "Parlor (Single-0)",
        "Classical Nylon",
        "Acoustic Archtop",
        "Selmer Maccaferri",
        "F-Style Mandolin (F-5)"
    };

    addAndMakeVisible(bodyTypeBox);
    bodyTypeBox.addItemList(bodyNames, 1);
    bodyTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "bodyType", bodyTypeBox);

    setupToggle(dualModeButton, "DUAL STEREO");
    dualModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bodyDualMode", dualModeButton);

    setupToggle(phaseInvertButton, "Φ INV R");
    phaseInvertAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bodyPhaseInvertB", phaseInvertButton);

    addAndMakeVisible(bodyTypeBBox);
    bodyTypeBBox.addItemList(bodyNames, 1);
    bodyTypeBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "bodyTypeB", bodyTypeBBox);

    addAndMakeVisible(visualizer);

    setupRotary(bodySizeSlider, bodySizeLabel, "SIZE A", "x");
    bodySizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodySize", bodySizeSlider);

    setupRotary(bodyShapeSlider, bodyShapeLabel, "SHAPE A", "");
    bodyShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodyShape", bodyShapeSlider);

    setupRotary(bodyDepthSlider, bodyDepthLabel, "DEPTH", "%");
    bodyDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodyDepth", bodyDepthSlider);

    setupRotary(bodySizeBSlider, bodySizeBLabel, "SIZE B", "x");
    bodySizeBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodySizeB", bodySizeBSlider);

    setupRotary(bodyShapeBSlider, bodyShapeBLabel, "SHAPE B", "");
    bodyShapeBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodyShapeB", bodyShapeBSlider);

    setupRotary(bodySpreadSlider, bodySpreadLabel, "SPREAD", "%");
    bodySpreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodySpread", bodySpreadSlider);

    // 3B. Fluid Air & Close-Mic Radiation (Simulates Close Mic vs Pickup Openness)
    setupRotary(micOpennessSlider, micOpennessLabel, "MIC OPENNESS", "%");
    micOpennessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "micOpenness", micOpennessSlider);

    setupRotary(micAirSheenSlider, micAirSheenLabel, "AIR SHEEN", "%");
    micAirSheenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "micAirSheen", micAirSheenSlider);

    setupRotary(micProximitySlider, micProximityLabel, "PROXIMITY", "%");
    micProximityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "micProximity", micProximitySlider);

    // 3C. Studio 4-Band Acoustic Parametric EQ (Active 3D Tone Matrix)
    setupRotary(eqLowGainSlider, eqLowGainLabel, "BODY (110Hz)", " dB");
    eqLowGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqLowGain", eqLowGainSlider);

    setupRotary(eqLowMidFreqSlider, eqLowMidFreqLabel, "LO-MID FREQ", " Hz");
    eqLowMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqLowMidFreq", eqLowMidFreqSlider);

    setupRotary(eqLowMidGainSlider, eqLowMidGainLabel, "LO-MID CUT", " dB");
    eqLowMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqLowMidGain", eqLowMidGainSlider);

    setupRotary(eqHighMidFreqSlider, eqHighMidFreqLabel, "HI-MID FREQ", " Hz");
    eqHighMidFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqHighMidFreq", eqHighMidFreqSlider);

    setupRotary(eqHighMidGainSlider, eqHighMidGainLabel, "HI-MID GAIN", " dB");
    eqHighMidGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqHighMidGain", eqHighMidGainSlider);

    setupRotary(eqHighGainSlider, eqHighGainLabel, "AIR (9.5kHz)", " dB");
    eqHighGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "eqHighGain", eqHighGainSlider);

    // 4. Fluid Reverb Space (From Euphoria Amp)
    addAndMakeVisible(reverbTypeBox);
    reverbTypeBox.addItemList({ "Studio Room", "Concert Hall", "Vintage Plate" }, 1);
    reverbTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "reverbType", reverbTypeBox);

    setupRotary(reverbMixSlider, reverbMixLabel, "REVERB MIX", "%");
    reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "reverbMix", reverbMixSlider);

    setupRotary(reverbDecaySlider, reverbDecayLabel, "REVERB DECAY", "%");
    reverbDecayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "reverbDecay", reverbDecaySlider);

    setupRotary(reverbSizeSlider, reverbSizeLabel, "ROOM SIZE", "x");
    reverbSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "reverbSize", reverbSizeSlider);

    setupRotary(reverbToneSlider, reverbToneLabel, "REVERB TONE", " Hz");
    reverbToneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "reverbTone", reverbToneSlider);

    // 5. Feedback Defense & Stage Master
    setupToggle(afd1Toggle, "AIR NOTCH (A0)");
    afd1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "feedbackKill1", afd1Toggle);

    setupRotary(afdFreq1Slider, afdFreq1Label, "A0 NOTCH FREQ", " Hz");
    afdFreq1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "feedbackFreq1", afdFreq1Slider);

    setupToggle(afd2Toggle, "BODY NOTCH (T1)");
    afd2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "feedbackKill2", afd2Toggle);

    setupRotary(afdFreq2Slider, afdFreq2Label, "T1 NOTCH FREQ", " Hz");
    afdFreq2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "feedbackFreq2", afdFreq2Slider);

    setupRotary(stereoWidthSlider, stereoWidthLabel, "STEREO IMAGER", "%");
    stereoWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "stereoWidth", stereoWidthSlider);

    setupRotary(outputGainSlider, outputGainLabel, "OUTPUT LEVEL", " dB");
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "outputGain", outputGainSlider);

    setSize(1120, 680);
    setResizable(true, true);
    setResizeLimits(960, 580, 1600, 1000);

    startTimerHz(30);
}

CyberAcousticEditor::~CyberAcousticEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CyberAcousticEditor::timerCallback()
{
    bool dual = dualModeButton.getToggleState();
    bodyTypeBBox.setEnabled(dual);
    phaseInvertButton.setEnabled(dual);
    bodySizeBSlider.setEnabled(dual);
    bodyShapeBSlider.setEnabled(dual);
    bodySpreadSlider.setEnabled(dual);

    juce::Colour activeLabelCol(0xffd5c4b1);
    juce::Colour dimmedLabelCol(0xff55483d);
    bodySizeBLabel.setColour(juce::Label::textColourId, dual ? activeLabelCol : dimmedLabelCol);
    bodyShapeBLabel.setColour(juce::Label::textColourId, dual ? activeLabelCol : dimmedLabelCol);
    bodySpreadLabel.setColour(juce::Label::textColourId, dual ? activeLabelCol : dimmedLabelCol);

    int bodyType = bodyTypeBox.getSelectedItemIndex();
    float bodySize = (float)bodySizeSlider.getValue();
    float bodyShape = (float)bodyShapeSlider.getValue();

    int bodyTypeB = bodyTypeBBox.getSelectedItemIndex();
    float bodySizeB = (float)bodySizeBSlider.getValue();
    float bodyShapeB = (float)bodyShapeBSlider.getValue();
    float spread = (float)bodySpreadSlider.getValue();
    bool phaseInvertB = phaseInvertButton.getToggleState();

    float audioLevel = audioProcessor.getCurrentAudioLevel();

    visualizer.setParameters(bodyType, bodySize, bodyShape,
                             dual, bodyTypeB, bodySizeB, bodyShapeB,
                             spread, phaseInvertB, audioLevel);
    inputMeter.setLevels(audioProcessor.getInputRmsDb(), audioProcessor.getInputPeakDb());
}

void CyberAcousticEditor::loadPreset(int id)
{
    auto setVal = [this](const juce::String& paramId, float value) {
        if (auto* param = audioProcessor.apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    auto resetDual = [&]() {
        setVal("bodyDualMode", 0.0f);
        setVal("bodyTypeB", 2);
        setVal("bodySizeB", 1.0f);
        setVal("bodyShapeB", 0.5f);
        setVal("bodySpread", 100.0f);
        setVal("bodyPhaseInvertB", 0.0f);
    };

    switch (id) {
        case 1: // Dreadnought Studio Flatpicker
            resetDual();
            setVal("bodyType", 0); setVal("bodySize", 1.0f); setVal("bodyShape", 0.82f);
            setVal("denoiseAmount", 50.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 65.0f); setVal("bodyDepth", 70.0f); setVal("deFizz", 60.0f);
            setVal("airShimmer", 35.0f);
            setVal("micOpenness", 85.0f); setVal("micAirSheen", 80.0f); setVal("micProximity", 80.0f);
            setVal("eqLowGain", 1.0f); setVal("eqLowMidFreq", 450.0f); setVal("eqLowMidGain", -2.5f);
            setVal("eqHighMidFreq", 3200.0f); setVal("eqHighMidGain", 1.5f); setVal("eqHighGain", 2.0f);
            setVal("stereoWidth", 110.0f);
            setVal("reverbType", 0);
            setVal("reverbMix", 18.0f); setVal("reverbDecay", 35.0f); setVal("reverbSize", 1.0f); setVal("reverbTone", 6000.0f);
            break;
        case 2: // Modern Fingerstyle OM
            resetDual();
            setVal("bodyType", 2); setVal("bodySize", 0.95f); setVal("bodyShape", 0.42f);
            setVal("denoiseAmount", 55.0f); setVal("denoiseThreshold", -82.0f);
            setVal("quackReduction", 55.0f); setVal("bodyDepth", 60.0f); setVal("deFizz", 50.0f);
            setVal("airShimmer", 45.0f);
            setVal("micOpenness", 75.0f); setVal("micAirSheen", 60.0f); setVal("micProximity", 50.0f);
            setVal("eqLowGain", 0.5f); setVal("eqLowMidFreq", 500.0f); setVal("eqLowMidGain", -3.0f);
            setVal("eqHighMidFreq", 3800.0f); setVal("eqHighMidGain", 2.5f); setVal("eqHighGain", 3.5f);
            setVal("stereoWidth", 130.0f);
            setVal("reverbType", 0);
            setVal("reverbMix", 26.0f); setVal("reverbDecay", 55.0f); setVal("reverbSize", 1.25f); setVal("reverbTone", 7500.0f);
            break;
        case 3: // Cavernous J-200 Strumming
            resetDual();
            setVal("bodyType", 1); setVal("bodySize", 1.25f); setVal("bodyShape", 0.65f);
            setVal("denoiseAmount", 60.0f); setVal("denoiseThreshold", -78.0f);
            setVal("quackReduction", 75.0f); setVal("bodyDepth", 85.0f); setVal("deFizz", 65.0f);
            setVal("airShimmer", 40.0f);
            setVal("micOpenness", 80.0f); setVal("micAirSheen", 70.0f); setVal("micProximity", 75.0f);
            setVal("eqLowGain", 2.5f); setVal("eqLowMidFreq", 380.0f); setVal("eqLowMidGain", -2.0f);
            setVal("eqHighMidFreq", 2800.0f); setVal("eqHighMidGain", 1.0f); setVal("eqHighGain", 2.0f);
            setVal("stereoWidth", 140.0f);
            setVal("reverbType", 1); // Concert Hall
            setVal("reverbMix", 32.0f); setVal("reverbDecay", 70.0f); setVal("reverbSize", 1.8f); setVal("reverbTone", 5000.0f);
            break;
        case 4: // Delta Blues Parlor
            resetDual();
            setVal("bodyType", 4); setVal("bodySize", 0.80f); setVal("bodyShape", 0.25f);
            setVal("denoiseAmount", 40.0f); setVal("denoiseThreshold", -75.0f);
            setVal("quackReduction", 45.0f); setVal("bodyDepth", 50.0f); setVal("deFizz", 40.0f);
            setVal("airShimmer", 20.0f);
            setVal("micOpenness", 60.0f); setVal("micAirSheen", 40.0f); setVal("micProximity", 85.0f);
            setVal("eqLowGain", -2.0f); setVal("eqLowMidFreq", 650.0f); setVal("eqLowMidGain", 1.5f);
            setVal("eqHighMidFreq", 2400.0f); setVal("eqHighMidGain", 2.0f); setVal("eqHighGain", -1.0f);
            setVal("stereoWidth", 75.0f);
            setVal("reverbType", 0);
            setVal("reverbMix", 12.0f); setVal("reverbDecay", 22.0f); setVal("reverbSize", 0.7f); setVal("reverbTone", 4500.0f);
            break;
        case 5: // Concert Classical Nylon
            resetDual();
            setVal("bodyType", 5); setVal("bodySize", 0.92f); setVal("bodyShape", 0.35f);
            setVal("denoiseAmount", 45.0f); setVal("denoiseThreshold", -84.0f);
            setVal("quackReduction", 40.0f); setVal("bodyDepth", 65.0f); setVal("deFizz", 45.0f);
            setVal("airShimmer", 25.0f);
            setVal("micOpenness", 85.0f); setVal("micAirSheen", 50.0f); setVal("micProximity", 60.0f);
            setVal("eqLowGain", 1.5f); setVal("eqLowMidFreq", 400.0f); setVal("eqLowMidGain", -1.5f);
            setVal("eqHighMidFreq", 2600.0f); setVal("eqHighMidGain", 0.5f); setVal("eqHighGain", 1.0f);
            setVal("stereoWidth", 100.0f);
            setVal("reverbType", 1); // Concert Hall
            setVal("reverbMix", 28.0f); setVal("reverbDecay", 60.0f); setVal("reverbSize", 1.4f); setVal("reverbTone", 6500.0f);
            break;
        case 6: // Acoustic Jazz Archtop
            resetDual();
            setVal("bodyType", 6); setVal("bodySize", 1.05f); setVal("bodyShape", 0.18f);
            setVal("denoiseAmount", 50.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 50.0f); setVal("bodyDepth", 45.0f); setVal("deFizz", 55.0f);
            setVal("airShimmer", 30.0f);
            setVal("micOpenness", 55.0f); setVal("micAirSheen", 35.0f); setVal("micProximity", 65.0f);
            setVal("eqLowGain", 1.0f); setVal("eqLowMidFreq", 550.0f); setVal("eqLowMidGain", 1.0f);
            setVal("eqHighMidFreq", 2200.0f); setVal("eqHighMidGain", -1.0f); setVal("eqHighGain", -2.5f);
            setVal("stereoWidth", 80.0f);
            setVal("reverbType", 2); // Vintage Plate
            setVal("reverbMix", 16.0f); setVal("reverbDecay", 30.0f); setVal("reverbSize", 0.9f); setVal("reverbTone", 5500.0f);
            break;
        case 7: // Gypsy Django Selmer
            resetDual();
            setVal("bodyType", 7); setVal("bodySize", 1.0f); setVal("bodyShape", 0.12f);
            setVal("denoiseAmount", 55.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 60.0f); setVal("bodyDepth", 55.0f); setVal("deFizz", 60.0f);
            setVal("airShimmer", 50.0f);
            setVal("micOpenness", 70.0f); setVal("micAirSheen", 65.0f); setVal("micProximity", 45.0f);
            setVal("eqLowGain", -1.5f); setVal("eqLowMidFreq", 750.0f); setVal("eqLowMidGain", 2.5f);
            setVal("eqHighMidFreq", 3500.0f); setVal("eqHighMidGain", 3.0f); setVal("eqHighGain", 1.5f);
            setVal("stereoWidth", 90.0f);
            setVal("reverbType", 2); // Vintage Plate
            setVal("reverbMix", 20.0f); setVal("reverbDecay", 38.0f); setVal("reverbSize", 1.0f); setVal("reverbTone", 7000.0f);
            break;
        case 8: // Takamine Piezo Rescue (Maximum Boxiness Elimination & Breath)
            resetDual();
            setVal("bodyType", 0); setVal("bodySize", 1.05f); setVal("bodyShape", 0.85f);
            setVal("denoiseAmount", 55.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 75.0f); setVal("bodyDepth", 75.0f); setVal("deFizz", 65.0f);
            setVal("airShimmer", 40.0f);
            setVal("micOpenness", 90.0f); setVal("micAirSheen", 75.0f); setVal("micProximity", 65.0f);
            setVal("eqLowGain", 0.5f); setVal("eqLowMidFreq", 400.0f); setVal("eqLowMidGain", -3.5f);
            setVal("eqHighMidFreq", 3000.0f); setVal("eqHighMidGain", 1.0f); setVal("eqHighGain", 2.5f);
            setVal("stereoWidth", 120.0f);
            setVal("reverbType", 0);
            setVal("reverbMix", 22.0f); setVal("reverbDecay", 45.0f); setVal("reverbSize", 1.1f); setVal("reverbTone", 6500.0f);
            break;
        case 9: // Martin D-45 Deep Air (Huge Dreadnought Chest Warmth & Condenser Sparkle)
            resetDual();
            setVal("bodyType", 0); setVal("bodySize", 1.0f); setVal("bodyShape", 0.82f);
            setVal("denoiseAmount", 50.0f); setVal("denoiseThreshold", -82.0f);
            setVal("quackReduction", 60.0f); setVal("bodyDepth", 80.0f); setVal("deFizz", 55.0f);
            setVal("airShimmer", 45.0f);
            setVal("micOpenness", 88.0f); setVal("micAirSheen", 85.0f); setVal("micProximity", 80.0f);
            setVal("eqLowGain", 2.0f); setVal("eqLowMidFreq", 420.0f); setVal("eqLowMidGain", -2.0f);
            setVal("eqHighMidFreq", 3400.0f); setVal("eqHighMidGain", 2.0f); setVal("eqHighGain", 3.0f);
            setVal("stereoWidth", 135.0f);
            setVal("reverbType", 1); // Concert Hall
            setVal("reverbMix", 24.0f); setVal("reverbDecay", 50.0f); setVal("reverbSize", 1.3f); setVal("reverbTone", 7200.0f);
            break;
        case 10: // Bluegrass F-5 Mandolin Chop
            resetDual();
            setVal("bodyType", 8); setVal("bodySize", 0.75f); setVal("bodyShape", 0.15f);
            setVal("denoiseAmount", 50.0f); setVal("denoiseThreshold", -82.0f);
            setVal("quackReduction", 70.0f); setVal("bodyDepth", 50.0f); setVal("deFizz", 55.0f);
            setVal("airShimmer", 45.0f);
            setVal("micOpenness", 80.0f); setVal("micAirSheen", 45.0f); setVal("micProximity", 85.0f);
            setVal("eqLowGain", 1.5f); setVal("eqLowMidFreq", 350.0f); setVal("eqLowMidGain", 2.0f);
            setVal("eqHighMidFreq", 3200.0f); setVal("eqHighMidGain", 1.0f); setVal("eqHighGain", 0.5f);
            setVal("stereoWidth", 115.0f);
            setVal("reverbType", 0);
            setVal("reverbMix", 16.0f); setVal("reverbDecay", 28.0f); setVal("reverbSize", 0.85f); setVal("reverbTone", 6500.0f);
            break;
        case 11: // Dual Stereo - Dreadnought & OM Split (Wide Studio Acoustics)
            setVal("bodyDualMode", 1.0f);
            setVal("bodyType", 0); setVal("bodySize", 1.05f); setVal("bodyShape", 0.85f);
            setVal("bodyTypeB", 2); setVal("bodySizeB", 0.95f); setVal("bodyShapeB", 0.40f);
            setVal("bodySpread", 100.0f); setVal("bodyPhaseInvertB", 1.0f); // Polarity inverted for wide stereo illusion!
            setVal("denoiseAmount", 50.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 60.0f); setVal("bodyDepth", 70.0f); setVal("deFizz", 55.0f);
            setVal("airShimmer", 40.0f);
            setVal("micOpenness", 85.0f); setVal("micAirSheen", 75.0f); setVal("micProximity", 70.0f);
            setVal("eqLowGain", 1.0f); setVal("eqLowMidFreq", 450.0f); setVal("eqLowMidGain", -2.0f);
            setVal("eqHighMidFreq", 3200.0f); setVal("eqHighMidGain", 1.5f); setVal("eqHighGain", 2.0f);
            setVal("stereoWidth", 120.0f);
            setVal("reverbType", 1); // Concert Hall
            setVal("reverbMix", 24.0f); setVal("reverbDecay", 50.0f); setVal("reverbSize", 1.3f); setVal("reverbTone", 6500.0f);
            break;
        case 12: // Dual Stereo - Jumbo & Mandolin Split (Epic Acoustic Ensemble)
            setVal("bodyDualMode", 1.0f);
            setVal("bodyType", 1); setVal("bodySize", 1.20f); setVal("bodyShape", 0.65f); // Jumbo left
            setVal("bodyTypeB", 8); setVal("bodySizeB", 0.80f); setVal("bodyShapeB", 0.20f); // F-5 Mandolin right
            setVal("bodySpread", 95.0f); setVal("bodyPhaseInvertB", 0.0f);
            setVal("denoiseAmount", 55.0f); setVal("denoiseThreshold", -80.0f);
            setVal("quackReduction", 65.0f); setVal("bodyDepth", 75.0f); setVal("deFizz", 60.0f);
            setVal("airShimmer", 45.0f);
            setVal("micOpenness", 80.0f); setVal("micAirSheen", 70.0f); setVal("micProximity", 75.0f);
            setVal("eqLowGain", 1.5f); setVal("eqLowMidFreq", 400.0f); setVal("eqLowMidGain", -2.5f);
            setVal("eqHighMidFreq", 3000.0f); setVal("eqHighMidGain", 2.0f); setVal("eqHighGain", 2.5f);
            setVal("stereoWidth", 130.0f);
            setVal("reverbType", 0); // Studio Room
            setVal("reverbMix", 20.0f); setVal("reverbDecay", 40.0f); setVal("reverbSize", 1.1f); setVal("reverbTone", 7000.0f);
            break;
    }
}

void CyberAcousticEditor::paint(juce::Graphics& g)
{
    // Background gradient: dark aged mahogany & brushed aluminum trim
    juce::ColourGradient bg(juce::Colour(0xff181615), 0, 0,
                            juce::Colour(0xff100e0d), 0, (float)getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Top Header Banner
    auto headerArea = getLocalBounds().removeFromTop(58);
    g.setColour(juce::Colour(0xff1f1c1a));
    g.fillRect(headerArea);
    g.setColour(juce::Colour(0xff38322e));
    g.drawHorizontalLine(57, 0, (float)getWidth());

    // Title & Subtitle
    g.setColour(juce::Colour(0xffedd9be));
    g.setFont(juce::Font(juce::FontOptions(17.0f)).withExtraKerningFactor(0.06f));
    g.drawText("CYBER ACOUSTIC PREAMP", 18, 10, 240, 20, juce::Justification::left, false);

    g.setColour(juce::Colour(0xffa19284));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Fluid Space Acoustic Physics & Imager", 20, 30, 240, 16, juce::Justification::left, false);

    // Section Dividing Lines (4 Columns)
    auto mainArea = getLocalBounds().reduced(14).withTrimmedTop(56);
    float colW = mainArea.getWidth() / 4.0f;

    g.setColour(juce::Colour(0xff2d2825));
    for (int c = 1; c < 4; ++c) {
        g.drawVerticalLine((int)(mainArea.getX() + colW * (float)c), (float)mainArea.getY() + 10, (float)mainArea.getBottom() - 10);
    }

    // Section Titles
    g.setColour(juce::Colour(0xffd4974f));
    g.setFont(juce::FontOptions(11.5f));
    g.drawText("D-NOISE & TRANSDUCER", (int)mainArea.getX(), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
    g.drawText("FLUID CAVITY & DUAL STEREO", (int)(mainArea.getX() + colW), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
    g.drawText("STUDIO 4-BAND EQ", (int)(mainArea.getX() + colW * 2.0f), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
    g.drawText("FLUID REVERB & MASTER", (int)(mainArea.getX() + colW * 3.0f), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
}

void CyberAcousticEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(58);

    // Header controls layout from right to left
    int rightX = header.getRight() - 14;

    int presetW = std::clamp((int)(header.getWidth() * 0.18f), 160, 220);
    presetSelector.setBounds(rightX - presetW, 14, presetW, 28);
    rightX -= (presetW + 8);

    int inputW = std::clamp((int)(header.getWidth() * 0.14f), 130, 170);
    inputSelector.setBounds(rightX - inputW, 14, inputW, 28);
    rightX -= (inputW + 8);

    int bypassW = std::clamp((int)(header.getWidth() * 0.13f), 125, 150);
    masterBypassButton.setBounds(rightX - bypassW, 14, bypassW, 28);
    rightX -= (bypassW + 8);

    int meterW = std::clamp((int)(header.getWidth() * 0.14f), 130, 170);
    inputMeter.setBounds(rightX - meterW, 14, meterW, 28);
    rightX -= (meterW + 8);

    int inGainW = 55;
    auto inGainArea = juce::Rectangle<int>(rightX - inGainW, 4, inGainW, 50);
    inputGainLabel.setBounds(inGainArea.removeFromBottom(14));
    inputGainSlider.setBounds(inGainArea);

    auto mainArea = area.reduced(14);
    int colW = mainArea.getWidth() / 4;

    // =========================================================================
    // --- Column 1: D-NOISE & TRANSDUCER ---
    // =========================================================================
    auto col1 = mainArea.removeFromLeft(colW).reduced(6);
    col1.removeFromTop(26);

    // D-Noise Section
    auto dnRow1 = col1.removeFromTop(95);
    int dnHalfW = dnRow1.getWidth() / 2;
    auto dnR1L = dnRow1.removeFromLeft(dnHalfW);
    denoiseAmountLabel.setBounds(dnR1L.removeFromBottom(16));
    denoiseAmountSlider.setBounds(dnR1L);

    auto dnR1R = dnRow1;
    denoiseThreshLabel.setBounds(dnR1R.removeFromBottom(16));
    denoiseThreshSlider.setBounds(dnR1R);

    auto dnToggleRow = col1.removeFromTop(32);
    int tglHalfW = dnToggleRow.getWidth() / 2;
    denoiseShieldToggle.setBounds(dnToggleRow.removeFromLeft(tglHalfW).reduced(2));
    denoiseListenToggle.setBounds(dnToggleRow.reduced(2));

    col1.removeFromTop(10);

    // Transducer & Anti-Quack (3 Knobs)
    int transKnobH = (col1.getHeight() - 10) / 3;

    auto qArea = col1.removeFromTop(transKnobH);
    quackLabel.setBounds(qArea.removeFromBottom(16));
    quackSlider.setBounds(qArea.reduced(6));

    auto dfArea = col1.removeFromTop(transKnobH);
    deFizzLabel.setBounds(dfArea.removeFromBottom(16));
    deFizzSlider.setBounds(dfArea.reduced(6));

    auto asArea = col1;
    airShimmerLabel.setBounds(asArea.removeFromBottom(16));
    airShimmerSlider.setBounds(asArea.reduced(6));

    // =========================================================================
    // --- Column 2: FLUID ACOUSTIC CAVITY & DUAL STEREO SPLIT ---
    // =========================================================================
    auto col2 = mainArea.removeFromLeft(colW).reduced(6);
    col2.removeFromTop(26);

    // Row 1: Dual Mode & Phase Invert Toggles
    auto dualRow = col2.removeFromTop(24);
    int dHalfW = dualRow.getWidth() / 2;
    dualModeButton.setBounds(dualRow.removeFromLeft(dHalfW).reduced(2, 0));
    phaseInvertButton.setBounds(dualRow.reduced(2, 0));
    col2.removeFromTop(4);

    // Row 2: Body Dropdown Selectors (Left Body A / Right Body B)
    auto bodyBoxRow = col2.removeFromTop(26);
    int bHalfW = bodyBoxRow.getWidth() / 2;
    bodyTypeBox.setBounds(bodyBoxRow.removeFromLeft(bHalfW).reduced(2, 0));
    bodyTypeBBox.setBounds(bodyBoxRow.reduced(2, 0));
    col2.removeFromTop(6);

    // Visualizer (Dynamic Single / Dual Stereo Cavity)
    int visH = (int)(col2.getHeight() * 0.36f);
    visualizer.setBounds(col2.removeFromTop(visH));
    col2.removeFromTop(6);

    int cavityKnobH = (col2.getHeight() - 8) / 3;

    // Row 3: Cavity A & Depth (Size A, Shape A, Depth)
    auto rowA = col2.removeFromTop(cavityKnobH);
    int subKnobW = rowA.getWidth() / 3;

    auto szArea = rowA.removeFromLeft(subKnobW);
    bodySizeLabel.setBounds(szArea.removeFromBottom(16));
    bodySizeSlider.setBounds(szArea);

    auto shArea = rowA.removeFromLeft(subKnobW);
    bodyShapeLabel.setBounds(shArea.removeFromBottom(16));
    bodyShapeSlider.setBounds(shArea);

    auto dpArea = rowA;
    bodyDepthLabel.setBounds(dpArea.removeFromBottom(16));
    bodyDepthSlider.setBounds(dpArea);

    col2.removeFromTop(4);

    // Row 4: Cavity B & Stereo Spread (Size B, Shape B, Spread)
    auto rowB = col2.removeFromTop(cavityKnobH);

    auto szBArea = rowB.removeFromLeft(subKnobW);
    bodySizeBLabel.setBounds(szBArea.removeFromBottom(16));
    bodySizeBSlider.setBounds(szBArea);

    auto shBArea = rowB.removeFromLeft(subKnobW);
    bodyShapeBLabel.setBounds(shBArea.removeFromBottom(16));
    bodyShapeBSlider.setBounds(shBArea);

    auto spArea = rowB;
    bodySpreadLabel.setBounds(spArea.removeFromBottom(16));
    bodySpreadSlider.setBounds(spArea);

    col2.removeFromTop(4);

    // Row 5: Fluid Air & Close-Mic Radiation (Openness, Sheen, Proximity)
    auto rowAir = col2;

    auto opArea = rowAir.removeFromLeft(subKnobW);
    micOpennessLabel.setBounds(opArea.removeFromBottom(16));
    micOpennessSlider.setBounds(opArea);

    auto snArea = rowAir.removeFromLeft(subKnobW);
    micAirSheenLabel.setBounds(snArea.removeFromBottom(16));
    micAirSheenSlider.setBounds(snArea);

    auto pxArea = rowAir;
    micProximityLabel.setBounds(pxArea.removeFromBottom(16));
    micProximitySlider.setBounds(pxArea);

    // =========================================================================
    // --- Column 3: STUDIO 4-BAND ACOUSTIC EQ ---
    // =========================================================================
    auto col3 = mainArea.removeFromLeft(colW).reduced(6);
    col3.removeFromTop(26);

    // Band 1: Low Shelf Warmth
    auto eqRow1 = col3.removeFromTop(98);
    eqLowGainLabel.setBounds(eqRow1.removeFromBottom(16));
    eqLowGainSlider.setBounds(eqRow1.reduced(4));

    col3.removeFromTop(6);

    // Band 2: Low-Mid Freq & Cut (2 knobs side-by-side)
    auto eqRow2 = col3.removeFromTop(105);
    int eqHalfW = eqRow2.getWidth() / 2;
    auto eqR2L = eqRow2.removeFromLeft(eqHalfW);
    eqLowMidFreqLabel.setBounds(eqR2L.removeFromBottom(16));
    eqLowMidFreqSlider.setBounds(eqR2L);

    auto eqR2R = eqRow2;
    eqLowMidGainLabel.setBounds(eqR2R.removeFromBottom(16));
    eqLowMidGainSlider.setBounds(eqR2R);

    col3.removeFromTop(6);

    // Band 3: High-Mid Freq & Gain (2 knobs side-by-side)
    auto eqRow3 = col3.removeFromTop(105);
    auto eqR3L = eqRow3.removeFromLeft(eqHalfW);
    eqHighMidFreqLabel.setBounds(eqR3L.removeFromBottom(16));
    eqHighMidFreqSlider.setBounds(eqR3L);

    auto eqR3R = eqRow3;
    eqHighMidGainLabel.setBounds(eqR3R.removeFromBottom(16));
    eqHighMidGainSlider.setBounds(eqR3R);

    col3.removeFromTop(6);

    // Band 4: Air Glass High Shelf
    auto eqRow4 = col3;
    eqHighGainLabel.setBounds(eqRow4.removeFromBottom(16));
    eqHighGainSlider.setBounds(eqRow4.reduced(4));

    // =========================================================================
    // --- Column 4: FLUID REVERB & STAGE MASTER ---
    // =========================================================================
    auto col4 = mainArea.reduced(6);
    col4.removeFromTop(26);

    // Reverb Type Selector (Studio Room / Concert Hall / Vintage Plate)
    auto revTypeRow = col4.removeFromTop(26);
    reverbTypeBox.setBounds(revTypeRow);
    col4.removeFromTop(6);

    // Part A: Fluid Reverb Space (2x2 Grid)
    auto revArea = col4.removeFromTop(155);
    int revRowH = revArea.getHeight() / 2;
    auto revRow1 = revArea.removeFromTop(revRowH);
    auto revRow2 = revArea;

    int revHalfW = revRow1.getWidth() / 2;
    auto r1L = revRow1.removeFromLeft(revHalfW);
    reverbMixLabel.setBounds(r1L.removeFromBottom(16));
    reverbMixSlider.setBounds(r1L);

    auto r1R = revRow1;
    reverbDecayLabel.setBounds(r1R.removeFromBottom(16));
    reverbDecaySlider.setBounds(r1R);

    auto r2L = revRow2.removeFromLeft(revHalfW);
    reverbSizeLabel.setBounds(r2L.removeFromBottom(16));
    reverbSizeSlider.setBounds(r2L);

    auto r2R = revRow2;
    reverbToneLabel.setBounds(r2R.removeFromBottom(16));
    reverbToneSlider.setBounds(r2R);

    col4.removeFromTop(10);

    // Part B: Dual AFD Surgical Notches
    auto afdRow1 = col4.removeFromTop(54);
    afd1Toggle.setBounds(afdRow1.removeFromLeft(125).reduced(2));
    afdFreq1Label.setBounds(afdRow1.removeFromBottom(16));
    afdFreq1Slider.setBounds(afdRow1);

    col4.removeFromTop(4);

    auto afdRow2 = col4.removeFromTop(54);
    afd2Toggle.setBounds(afdRow2.removeFromLeft(125).reduced(2));
    afdFreq2Label.setBounds(afdRow2.removeFromBottom(16));
    afdFreq2Slider.setBounds(afdRow2);

    col4.removeFromTop(10);

    // Part C: Stereo Width & Output Gain
    auto masterRow = col4;
    int mKnobW = masterRow.getWidth() / 2;

    auto swArea = masterRow.removeFromLeft(mKnobW);
    stereoWidthLabel.setBounds(swArea.removeFromBottom(16));
    stereoWidthSlider.setBounds(swArea.reduced(4));

    auto outArea = masterRow;
    outputGainLabel.setBounds(outArea.removeFromBottom(16));
    outputGainSlider.setBounds(outArea.reduced(4));
}
