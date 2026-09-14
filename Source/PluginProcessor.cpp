#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NeoSatAudioProcessor::NeoSatAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pDrive      = apvts.getRawParameterValue (PID::drive);
    pOutput     = apvts.getRawParameterValue (PID::output);
    pTone       = apvts.getRawParameterValue (PID::tone);
    pMix        = apvts.getRawParameterValue (PID::mix);
    pMode       = apvts.getRawParameterValue (PID::mode);
    pTonePos    = apvts.getRawParameterValue (PID::tonePos);
    pOversample = apvts.getRawParameterValue (PID::oversample);
    pAutoGain   = apvts.getRawParameterValue (PID::autoGain);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
NeoSatAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // DRIVE - how hard we push the signal into the shaper. Skew so the useful
    // low end of the range has more knob travel.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { PID::drive, 1 }, "Drive",
        NormalisableRange<float> (-6.0f, 36.0f, 0.01f, 0.6f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    // OUTPUT - post trim.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { PID::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    // TONE - bipolar. -1 dark, 0 flat, +1 bright.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { PID::tone, 1 }, "Tone",
        NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    // MIX - dry/wet.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { PID::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f)); })));

    // MODE - which saturation algorithm.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { PID::mode, 1 }, "Mode",
        StringArray { "Soft Clip", "Tube", "Tape", "Wavefolder" }, 0));

    // TONE POSITION - filter before or after the shaper.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { PID::tonePos, 1 }, "Tone Pos",
        StringArray { "Pre", "Post" }, 1));

    // OVERSAMPLING.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { PID::oversample, 1 }, "Oversample",
        StringArray { "Off", "2x", "4x" }, 1));

    // AUTO GAIN.
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { PID::autoGain, 1 }, "Auto Gain", true));

    return layout;
}

//==============================================================================
void NeoSatAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    spec.numChannels      = (juce::uint32) juce::jmax (getTotalNumInputChannels(),
                                                       getTotalNumOutputChannels());
    engine.prepare (spec);

    lastReportedLatency = -1;
}

bool NeoSatAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out) return false;
    if (in.isDisabled()) return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void NeoSatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    SaturationEngine::Params p;
    p.driveDb    = pDrive->load();
    p.outputDb   = pOutput->load();
    p.toneHz     = 4000.0f;                    // internal setpoint; Tone knob is the tilt
    p.toneAmt    = pTone->load();
    p.mix        = pMix->load();
    p.mode       = (int) pMode->load();
    p.tonePos    = (int) pTonePos->load();
    p.oversample = (int) pOversample->load();
    p.autoGain   = pAutoGain->load() > 0.5f;
    engine.setParams (p);

    engine.process (buffer);

    // Report oversampling latency to the host when it changes.
    const int lat = engine.getLatencySamples();
    if (lat != lastReportedLatency)
    {
        lastReportedLatency = lat;
        setLatencySamples (lat);
    }
}

//==============================================================================
juce::AudioProcessorEditor* NeoSatAudioProcessor::createEditor()
{
    return new NeoSatAudioProcessorEditor (*this);
}

//==============================================================================
void NeoSatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void NeoSatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeoSatAudioProcessor();
}
