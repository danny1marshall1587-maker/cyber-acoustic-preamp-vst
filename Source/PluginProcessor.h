#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CyberAcousticEngine.hpp"

class CyberAcousticProcessor : public juce::AudioProcessor {
public:
    CyberAcousticProcessor();
    ~CyberAcousticProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    float getCurrentAudioLevel() const { return currentAudioLevel.load(); }
    float getInputRmsDb() const { return currentInputRms.load(); }
    float getInputPeakDb() const { return currentInputPeak.load(); }
    float getMatchedBypassGain() const { return currentMatchedGain.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    CyberAcoustic::CyberAcousticEngine dspEngineLeft;
    CyberAcoustic::CyberAcousticEngine dspEngineRight;
    CyberAcoustic::FluidStereoDoublerDSP doublerDSP;

    std::atomic<float> currentAudioLevel { 0.0f };
    std::atomic<float> currentInputRms { -100.0f };
    std::atomic<float> currentInputPeak { -100.0f };
    std::atomic<float> currentMatchedGain { 1.0f };

    float dryRmsFiltered = 0.0f;
    float wetRmsFiltered = 0.0f;
    float targetMatchedGain = 1.0f;

    float smoothedSpread = 1.0f;
    float currentPhaseB = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberAcousticProcessor)
};
