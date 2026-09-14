#pragma once

#include <JuceHeader.h>
#include "dsp/SaturationEngine.h"

/**
    NeoSatAudioProcessor
    --------------------
    Thin host-facing wrapper.  All the audio maths lives in SaturationEngine;
    this class owns the parameter tree (AudioProcessorValueTreeState), forwards
    parameter values to the engine once per block, and reports latency.
*/
class NeoSatAudioProcessor : public juce::AudioProcessor
{
public:
    NeoSatAudioProcessor();
    ~NeoSatAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // The editor reads these for its visualisers.
    SaturationEngine& getEngine() { return engine; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter IDs - shared with the editor.
    struct PID
    {
        static constexpr const char* drive      = "drive";
        static constexpr const char* output     = "output";
        static constexpr const char* tone       = "tone";
        static constexpr const char* mix        = "mix";
        static constexpr const char* mode       = "mode";
        static constexpr const char* tonePos    = "tonepos";
        static constexpr const char* oversample = "oversample";
        static constexpr const char* autoGain   = "autogain";
    };

private:
    SaturationEngine engine;

    std::atomic<float>* pDrive      = nullptr;
    std::atomic<float>* pOutput     = nullptr;
    std::atomic<float>* pTone       = nullptr;
    std::atomic<float>* pMix        = nullptr;
    std::atomic<float>* pMode       = nullptr;
    std::atomic<float>* pTonePos    = nullptr;
    std::atomic<float>* pOversample = nullptr;
    std::atomic<float>* pAutoGain   = nullptr;

    int lastReportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeoSatAudioProcessor)
};
