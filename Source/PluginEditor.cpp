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
        l.setFont(juce::FontOptions(11.5f));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffd5c4b1));
        addAndMakeVisible(l);
    };

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

    // 3. Fluid Acoustic Cavity
    addAndMakeVisible(bodyTypeBox);
    bodyTypeBox.addItemList({
        "Dreadnought (D-28)",
        "Jumbo (J-200)",
        "Orchestra (000/OM)",
        "Grand Auditorium",
        "Parlor (Single-0)",
        "Classical Nylon",
        "Acoustic Archtop",
        "Selmer Maccaferri"
    }, 1);
    bodyTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "bodyType", bodyTypeBox);

    addAndMakeVisible(visualizer);

    setupRotary(bodySizeSlider, bodySizeLabel, "BODY SIZE", "x");
    bodySizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodySize", bodySizeSlider);

    setupRotary(bodyShapeSlider, bodyShapeLabel, "SHAPE (SIGMA)", "");
    bodyShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodyShape", bodyShapeSlider);

    setupRotary(bodyDepthSlider, bodyDepthLabel, "CAVITY DEPTH", "%");
    bodyDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bodyDepth", bodyDepthSlider);

    // 4. Fluid Reverb Space (From Euphoria Amp)
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
    addAndMakeVisible(afd1Toggle);
    afd1Toggle.setButtonText("AIR NOTCH (A0)");
    afd1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "feedbackKill1", afd1Toggle);

    setupRotary(afdFreq1Slider, afdFreq1Label, "A0 NOTCH FREQ", " Hz");
    afdFreq1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "feedbackFreq1", afdFreq1Slider);

    addAndMakeVisible(afd2Toggle);
    afd2Toggle.setButtonText("BODY NOTCH (T1)");
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

    setSize(960, 600);
    setResizable(true, true);
    setResizeLimits(800, 520, 1500, 950);

    startTimerHz(30);
}

CyberAcousticEditor::~CyberAcousticEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CyberAcousticEditor::timerCallback()
{
    int bodyType = bodyTypeBox.getSelectedItemIndex();
    float bodySize = (float)bodySizeSlider.getValue();
    float bodyShape = (float)bodyShapeSlider.getValue();
    float audioLevel = audioProcessor.getCurrentAudioLevel();

    visualizer.setParameters(bodyType, bodySize, bodyShape, audioLevel);
}

void CyberAcousticEditor::loadPreset(int id)
{
    auto setVal = [this](const juce::String& paramId, float value) {
        if (auto* param = audioProcessor.apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    switch (id) {
        case 1: // Dreadnought Studio Flatpicker
            setVal("bodyType", 0); setVal("bodySize", 1.0f); setVal("bodyShape", 0.82f);
            setVal("quackReduction", 65.0f); setVal("bodyDepth", 70.0f); setVal("deFizz", 60.0f);
            setVal("airShimmer", 35.0f); setVal("stereoWidth", 110.0f);
            setVal("reverbMix", 18.0f); setVal("reverbDecay", 35.0f); setVal("reverbSize", 1.0f); setVal("reverbTone", 6000.0f);
            break;
        case 2: // Modern Fingerstyle OM
            setVal("bodyType", 2); setVal("bodySize", 0.95f); setVal("bodyShape", 0.42f);
            setVal("quackReduction", 55.0f); setVal("bodyDepth", 60.0f); setVal("deFizz", 50.0f);
            setVal("airShimmer", 45.0f); setVal("stereoWidth", 130.0f);
            setVal("reverbMix", 26.0f); setVal("reverbDecay", 55.0f); setVal("reverbSize", 1.25f); setVal("reverbTone", 7500.0f);
            break;
        case 3: // Cavernous J-200 Strumming
            setVal("bodyType", 1); setVal("bodySize", 1.25f); setVal("bodyShape", 0.65f);
            setVal("quackReduction", 75.0f); setVal("bodyDepth", 85.0f); setVal("deFizz", 65.0f);
            setVal("airShimmer", 40.0f); setVal("stereoWidth", 140.0f);
            setVal("reverbMix", 32.0f); setVal("reverbDecay", 70.0f); setVal("reverbSize", 1.8f); setVal("reverbTone", 5000.0f);
            break;
        case 4: // Delta Blues Parlor
            setVal("bodyType", 4); setVal("bodySize", 0.80f); setVal("bodyShape", 0.25f);
            setVal("quackReduction", 45.0f); setVal("bodyDepth", 50.0f); setVal("deFizz", 40.0f);
            setVal("airShimmer", 20.0f); setVal("stereoWidth", 75.0f);
            setVal("reverbMix", 12.0f); setVal("reverbDecay", 22.0f); setVal("reverbSize", 0.7f); setVal("reverbTone", 4500.0f);
            break;
        case 5: // Concert Classical Nylon
            setVal("bodyType", 5); setVal("bodySize", 0.92f); setVal("bodyShape", 0.35f);
            setVal("quackReduction", 40.0f); setVal("bodyDepth", 65.0f); setVal("deFizz", 45.0f);
            setVal("airShimmer", 25.0f); setVal("stereoWidth", 100.0f);
            setVal("reverbMix", 28.0f); setVal("reverbDecay", 60.0f); setVal("reverbSize", 1.4f); setVal("reverbTone", 6500.0f);
            break;
        case 6: // Acoustic Jazz Archtop
            setVal("bodyType", 6); setVal("bodySize", 1.05f); setVal("bodyShape", 0.18f);
            setVal("quackReduction", 50.0f); setVal("bodyDepth", 45.0f); setVal("deFizz", 55.0f);
            setVal("airShimmer", 30.0f); setVal("stereoWidth", 80.0f);
            setVal("reverbMix", 16.0f); setVal("reverbDecay", 30.0f); setVal("reverbSize", 0.9f); setVal("reverbTone", 5500.0f);
            break;
        case 7: // Gypsy Django Selmer
            setVal("bodyType", 7); setVal("bodySize", 1.0f); setVal("bodyShape", 0.12f);
            setVal("quackReduction", 60.0f); setVal("bodyDepth", 55.0f); setVal("deFizz", 60.0f);
            setVal("airShimmer", 50.0f); setVal("stereoWidth", 90.0f);
            setVal("reverbMix", 20.0f); setVal("reverbDecay", 38.0f); setVal("reverbSize", 1.0f); setVal("reverbTone", 7000.0f);
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
    auto headerArea = getLocalBounds().removeFromTop(56);
    g.setColour(juce::Colour(0xff1f1c1a));
    g.fillRect(headerArea);
    g.setColour(juce::Colour(0xff38322e));
    g.drawHorizontalLine(55, 0, (float)getWidth());

    // Title & Subtitle
    g.setColour(juce::Colour(0xffedd9be));
    g.setFont(juce::Font(juce::FontOptions(18.0f)).withExtraKerningFactor(0.08f));
    g.drawText("CYBER ACOUSTIC PREAMP", 20, 10, 320, 22, juce::Justification::left, false);

    g.setColour(juce::Colour(0xffa19284));
    g.setFont(juce::FontOptions(10.5f));
    g.drawText("Fluid Space Acoustic Cavity Physics & Anti-Quack Engine", 22, 32, 360, 16, juce::Justification::left, false);

    // Section Dividing Lines
    auto mainArea = getLocalBounds().reduced(16).withTrimmedTop(54);
    float colW = mainArea.getWidth() / 3.0f;

    g.setColour(juce::Colour(0xff2d2825));
    g.drawVerticalLine((int)(mainArea.getX() + colW), (float)mainArea.getY() + 10, (float)mainArea.getBottom() - 10);
    g.drawVerticalLine((int)(mainArea.getX() + colW * 2.0f), (float)mainArea.getY() + 10, (float)mainArea.getBottom() - 10);

    // Section Titles
    g.setColour(juce::Colour(0xffd4974f));
    g.setFont(juce::FontOptions(12.5f));
    g.drawText("TRANSDUCER CONDITIONING", (int)mainArea.getX(), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
    g.drawText("FLUID ACOUSTIC CAVITY", (int)(mainArea.getX() + colW), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
    g.drawText("FLUID REVERB & STAGE MASTER", (int)(mainArea.getX() + colW * 2.0f), mainArea.getY() + 4, (int)colW, 20, juce::Justification::centred, false);
}

void CyberAcousticEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(56);

    // Header controls: Input Selector & Preset Selector
    presetSelector.setBounds(header.getRight() - 300, 14, 280, 28);
    inputSelector.setBounds(header.getRight() - 530, 14, 215, 28);

    auto mainArea = area.reduced(16);
    int colW = mainArea.getWidth() / 3;

    // --- Column 1: Transducer & Anti-Quack ---
    auto col1 = mainArea.removeFromLeft(colW).reduced(10);
    col1.removeFromTop(28); // Header offset

    int knobH = (col1.getHeight() - 40) / 3;

    auto qArea = col1.removeFromTop(knobH);
    quackLabel.setBounds(qArea.removeFromBottom(20));
    quackSlider.setBounds(qArea.reduced(10));

    auto dfArea = col1.removeFromTop(knobH);
    deFizzLabel.setBounds(dfArea.removeFromBottom(20));
    deFizzSlider.setBounds(dfArea.reduced(10));

    auto asArea = col1.removeFromTop(knobH);
    airShimmerLabel.setBounds(asArea.removeFromBottom(20));
    airShimmerSlider.setBounds(asArea.reduced(10));

    // --- Column 2: Fluid Cavity & Guitar Visualizer ---
    auto col2 = mainArea.removeFromLeft(colW).reduced(10);
    col2.removeFromTop(28);

    bodyTypeBox.setBounds(col2.removeFromTop(28));
    col2.removeFromTop(8);

    // Guitar visualizer occupies middle 45%
    int visH = (int)(col2.getHeight() * 0.52f);
    visualizer.setBounds(col2.removeFromTop(visH));
    col2.removeFromTop(10);

    // Bottom row: Body Size, Shape, Cavity Depth
    auto bottomCavityRow = col2;
    int subKnobW = bottomCavityRow.getWidth() / 3;

    auto szArea = bottomCavityRow.removeFromLeft(subKnobW);
    bodySizeLabel.setBounds(szArea.removeFromBottom(20));
    bodySizeSlider.setBounds(szArea);

    auto shArea = bottomCavityRow.removeFromLeft(subKnobW);
    bodyShapeLabel.setBounds(shArea.removeFromBottom(20));
    bodyShapeSlider.setBounds(shArea);

    auto dpArea = bottomCavityRow;
    bodyDepthLabel.setBounds(dpArea.removeFromBottom(20));
    bodyDepthSlider.setBounds(dpArea);

    // --- Column 3: Fluid Reverb Space & Stage Master ---
    auto col3 = mainArea.reduced(10);
    col3.removeFromTop(28);

    // Part A: Fluid Reverb Space (2x2 Grid)
    auto revArea = col3.removeFromTop(150);
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

    col3.removeFromTop(10);

    // Part B: Dual AFD Surgical Notches
    auto afdRow1 = col3.removeFromTop(60);
    afd1Toggle.setBounds(afdRow1.removeFromLeft(125).reduced(2));
    afdFreq1Label.setBounds(afdRow1.removeFromBottom(16));
    afdFreq1Slider.setBounds(afdRow1);

    col3.removeFromTop(4);

    auto afdRow2 = col3.removeFromTop(60);
    afd2Toggle.setBounds(afdRow2.removeFromLeft(125).reduced(2));
    afdFreq2Label.setBounds(afdRow2.removeFromBottom(16));
    afdFreq2Slider.setBounds(afdRow2);

    col3.removeFromTop(10);

    // Part C: Stereo Width & Output Gain
    auto masterRow = col3;
    int mKnobW = masterRow.getWidth() / 2;

    auto swArea = masterRow.removeFromLeft(mKnobW);
    stereoWidthLabel.setBounds(swArea.removeFromBottom(18));
    stereoWidthSlider.setBounds(swArea.reduced(6));

    auto outArea = masterRow;
    outputGainLabel.setBounds(outArea.removeFromBottom(18));
    outputGainSlider.setBounds(outArea.reduced(6));
}
