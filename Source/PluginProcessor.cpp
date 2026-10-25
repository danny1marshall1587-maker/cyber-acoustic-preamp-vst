#include "PluginProcessor.h"
#include "PluginEditor.h"

CyberAcousticProcessor::CyberAcousticProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::mono(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

CyberAcousticProcessor::~CyberAcousticProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CyberAcousticProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray inputModes = {
        "Mono In (Input 1 / Left)",
        "Mono In (Input 2 / Right)",
        "Auto-Detect Active Mono",
        "Stereo Sum (-3dB)",
        "Stereo Discrete"
    };

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("inputRouting", 1), "Input Routing", inputModes, 0));

    juce::StringArray bodyTypes = {
        "Dreadnought (D-28)",
        "Jumbo (J-200)",
        "Orchestra (000/OM)",
        "Grand Auditorium",
        "Parlor (Single-0)",
        "Classical Nylon",
        "Acoustic Archtop",
        "Selmer Maccaferri"
    };

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("bodyType", 1), "Body Type", bodyTypes, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bodySize", 1), "Body Size", 0.5f, 2.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bodyShape", 1), "Body Shape (Sigma)", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("quackReduction", 1), "Anti-Quack Inertia", 0.0f, 100.0f, 60.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bodyDepth", 1), "Fluid Cavity Depth", 0.0f, 100.0f, 65.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("deFizz", 1), "Smart De-Fizz", 0.0f, 100.0f, 55.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("airShimmer", 1), "Air & Shimmer", 0.0f, 100.0f, 35.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoWidth", 1), "Stereo Width", 0.0f, 200.0f, 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain", -24.0f, 12.0f, 0.0f));

    // Feedback Notches
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("feedbackKill1", 1), "AFD Air Notch 1", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("feedbackFreq1", 1), "AFD Freq 1", 70.0f, 160.0f, 98.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("feedbackDepth1", 1), "AFD Depth 1", 0.0f, 100.0f, 75.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("feedbackKill2", 1), "AFD Body Notch 2", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("feedbackFreq2", 1), "AFD Freq 2", 160.0f, 320.0f, 195.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("feedbackDepth2", 1), "AFD Depth 2", 0.0f, 100.0f, 75.0f));

    return { params.begin(), params.end() };
}

const juce::String CyberAcousticProcessor::getName() const
{
    return "CyberAcousticPreamp";
}

bool CyberAcousticProcessor::acceptsMidi() const { return false; }
bool CyberAcousticProcessor::producesMidi() const { return false; }
bool CyberAcousticProcessor::isMidiEffect() const { return false; }
double CyberAcousticProcessor::getTailLengthSeconds() const { return 0.0; }

int CyberAcousticProcessor::getNumPrograms() { return 1; }
int CyberAcousticProcessor::getCurrentProgram() { return 0; }
void CyberAcousticProcessor::setCurrentProgram(int) {}
const juce::String CyberAcousticProcessor::getProgramName(int) { return {}; }
void CyberAcousticProcessor::changeProgramName(int, const juce::String&) {}

void CyberAcousticProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    dspEngineLeft.init((float)sampleRate);
    dspEngineRight.init((float)sampleRate);
}

void CyberAcousticProcessor::releaseResources()
{
    dspEngineLeft.reset();
    dspEngineRight.reset();
}

bool CyberAcousticProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void CyberAcousticProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Fetch normalized parameter values
    int inputMode = (int)apvts.getRawParameterValue("inputRouting")->load();
    int bodyType = (int)apvts.getRawParameterValue("bodyType")->load();
    float bodySize = apvts.getRawParameterValue("bodySize")->load();
    float bodyShape = apvts.getRawParameterValue("bodyShape")->load();
    float quack = apvts.getRawParameterValue("quackReduction")->load() / 100.0f;
    float depth = apvts.getRawParameterValue("bodyDepth")->load() / 100.0f;
    float defizz = apvts.getRawParameterValue("deFizz")->load() / 100.0f;
    float shimmer = apvts.getRawParameterValue("airShimmer")->load() / 100.0f;
    float width = apvts.getRawParameterValue("stereoWidth")->load() / 100.0f;
    float gainDb = apvts.getRawParameterValue("outputGain")->load();
    float gainLin = std::pow(10.0f, gainDb / 20.0f);

    bool fbKill1 = apvts.getRawParameterValue("feedbackKill1")->load() > 0.5f;
    float fbFreq1 = apvts.getRawParameterValue("feedbackFreq1")->load();
    float fbDepth1 = apvts.getRawParameterValue("feedbackDepth1")->load() / 100.0f;

    bool fbKill2 = apvts.getRawParameterValue("feedbackKill2")->load() > 0.5f;
    float fbFreq2 = apvts.getRawParameterValue("feedbackFreq2")->load();
    float fbDepth2 = apvts.getRawParameterValue("feedbackDepth2")->load() / 100.0f;

    dspEngineLeft.feedback_killer.setNotch(0, fbKill1, fbFreq1, fbDepth1);
    dspEngineLeft.feedback_killer.setNotch(1, fbKill2, fbFreq2, fbDepth2);
    dspEngineRight.feedback_killer.setNotch(0, fbKill1, fbFreq1, fbDepth1);
    dspEngineRight.feedback_killer.setNotch(1, fbKill2, fbFreq2, fbDepth2);

    auto numSamples = buffer.getNumSamples();
    float maxAbs = 0.0f;

    const float* inL = (totalNumInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer(1) : nullptr;

    float* outL = (totalNumOutputChannels > 0) ? buffer.getWritePointer(0) : nullptr;
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    if (inputMode == 4 && totalNumInputChannels >= 2) {
        // Mode 4: Stereo Discrete Processing
        for (int n = 0; n < numSamples; ++n) {
            float sL = inL ? inL[n] : 0.0f;
            float sR = inR ? inR[n] : 0.0f;
            float dummy1 = 0.0f, dummy2 = 0.0f;

            float l = 0.0f, r = 0.0f;
            dspEngineLeft.process(sL, l, dummy1, quack, depth, defizz, shimmer, width,
                                  bodyType, bodySize, bodyShape, gainLin);
            dspEngineRight.process(sR, dummy2, r, quack, depth, defizz, shimmer, width,
                                   bodyType, bodySize, bodyShape, gainLin);

            float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r) * width;
            if (outL) outL[n] = mid + side;
            if (outR) outR[n] = mid - side;

            maxAbs = std::max(maxAbs, std::max(std::abs(l), std::abs(r)));
        }
    } else {
        // Mono Acoustic Guitar Modes (Mode 0: In 1, Mode 1: In 2, Mode 2: Auto, Mode 3: Sum)
        for (int n = 0; n < numSamples; ++n) {
            float guitarMono = 0.0f;

            if (totalNumInputChannels == 1) {
                guitarMono = inL ? inL[n] : 0.0f;
            } else if (totalNumInputChannels >= 2) {
                float sL = inL ? inL[n] : 0.0f;
                float sR = inR ? inR[n] : 0.0f;

                switch (inputMode) {
                    case 0: // Mono In (Input 1 / Left / Inst 1)
                        guitarMono = sL;
                        break;
                    case 1: // Mono In (Input 2 / Right / Inst 2)
                        guitarMono = sR;
                        break;
                    case 2: // Auto-Detect Active Mono
                        if (std::abs(sL) > 1e-4f && std::abs(sR) < 1e-5f) {
                            guitarMono = sL;
                        } else if (std::abs(sR) > 1e-4f && std::abs(sL) < 1e-5f) {
                            guitarMono = sR;
                        } else {
                            guitarMono = (sL + sR) * 0.7071f;
                        }
                        break;
                    case 3: // Stereo Sum (-3dB)
                    default:
                        guitarMono = (sL + sR) * 0.7071f;
                        break;
                }
            }

            // Process mono acoustic guitar through fluid space cavity & stereo mic imager
            float l = 0.0f, r = 0.0f;
            dspEngineLeft.process(guitarMono, l, r, quack, depth, defizz, shimmer, width,
                                  bodyType, bodySize, bodyShape, gainLin);

            if (totalNumOutputChannels >= 2) {
                if (outL) outL[n] = l;
                if (outR) outR[n] = r;
            } else if (totalNumOutputChannels == 1) {
                if (outL) outL[n] = 0.5f * (l + r);
            }

            maxAbs = std::max(maxAbs, std::max(std::abs(l), std::abs(r)));
        }
    }

    currentAudioLevel.store(maxAbs);
}

bool CyberAcousticProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* CyberAcousticProcessor::createEditor()
{
    return new CyberAcousticEditor(*this);
}

void CyberAcousticProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CyberAcousticProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CyberAcousticProcessor();
}
