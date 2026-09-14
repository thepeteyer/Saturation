#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

/**
    SaturationEngine
    ----------------
    Owns the whole signal chain for one instance of NeoSat:

        input -> Drive gain -> [Tone filter if PRE] -> oversample up
              -> waveshaper (mode) -> oversample down
              -> [Tone filter if POST] -> auto-gain -> Output gain
              -> dry/wet Mix

    The processor (PluginProcessor) just pushes parameter values in and calls
    process().  Keeping the DSP here keeps PluginProcessor focused on the
    JUCE/host plumbing.
*/
class SaturationEngine
{
public:
    enum class Mode { softClip = 0, tube = 1, tape = 2, waveFolder = 3 };
    enum class TonePos { pre = 0, post = 1 };
    enum class OverSample { off = 0, x2 = 1, x4 = 2 };

    SaturationEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Push the current parameter state. Cheap; call once per block. */
    struct Params
    {
        float driveDb        = 0.0f;   // -+ input gain into the shaper
        float outputDb       = 0.0f;   // trim after shaping
        float toneHz         = 4000.0f;// filter corner
        float toneAmt        = 0.0f;   // -1 = dark (LP), 0 = flat, +1 = bright (HS)
        float mix            = 1.0f;   // 0 = dry, 1 = wet
        int   mode           = 0;
        int   tonePos        = 0;      // 0 = pre, 1 = post
        int   oversample     = 1;      // 0 off, 1 = 2x, 2 = 4x
        bool  autoGain       = true;
    };
    void setParams (const Params& p) { pending.store (p); }

    void process (juce::AudioBuffer<float>& buffer);

    /** Latency in samples introduced by the current oversampling setting. */
    int getLatencySamples() const { return latencySamples; }

    /** For the UI transfer-curve display: evaluate the current static curve
        (mode + drive + auto-gain + output) for an input in [-1, 1].  Thread
        safe-ish: it only reads the atomically published params. */
    float evaluateCurve (float x) const;

    /** Post-block peak of input / output, for the UI meters & scope. 0..~1+. */
    float getInputLevel()  const { return inLevel.load();  }
    float getOutputLevel() const { return outLevel.load(); }

private:
    void updateToneFilter (double sampleRate, float hz, float amt);

    // Params is trivially copyable; std::atomic gives us a torn-write-free
    // hand-off from the message thread. It is not lock-free for a struct this
    // size, but the internal critical section is a few instructions and never
    // contended for long - acceptable for a once-per-block read.
    std::atomic<Params> pending { Params{} };
    Params current;

    double baseSampleRate = 44100.0;
    juce::uint32 maxBlock = 512;
    int numChannels = 2;

    // Both oversampling chains are built up-front in prepare() so switching
    // ratio at run time is just a pointer change - no allocation on the audio
    // thread. Index 0 = 2x (1 stage), index 1 = 4x (2 stages).
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;
    juce::dsp::Oversampling<float>* activeOs = nullptr;
    int latencySamples = 0;

    // Tone: a state-variable filter used as LP (dark) or high-shelf (bright).
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
    float toneAmtSmoothedTarget = 0.0f;

    // Smoothed gains to avoid zipper noise.
    juce::SmoothedValue<float> driveGain   { 1.0f };
    juce::SmoothedValue<float> outputGain  { 1.0f };
    juce::SmoothedValue<float> mixSmoothed { 1.0f };
    juce::SmoothedValue<float> autoGainSm  { 1.0f };

    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float> inLevel  { 0.0f };
    std::atomic<float> outLevel { 0.0f };
};
