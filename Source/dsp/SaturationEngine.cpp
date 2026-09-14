#include "dsp/SaturationEngine.h"
#include "dsp/Saturators.h"

using juce::dsp::Oversampling;

//==============================================================================
void SaturationEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    baseSampleRate = spec.sampleRate;
    maxBlock       = spec.maximumBlockSize;
    numChannels    = (int) spec.numChannels;

    juce::dsp::ProcessSpec toneSpec { spec.sampleRate,
                                      spec.maximumBlockSize,
                                      spec.numChannels };

    toneFilter.prepare (toneSpec);
    toneFilter.reset();

    dryBuffer.setSize (numChannels, (int) maxBlock, false, false, true);

    driveGain  .reset (spec.sampleRate, 0.02);
    outputGain .reset (spec.sampleRate, 0.02);
    mixSmoothed.reset (spec.sampleRate, 0.02);
    autoGainSm .reset (spec.sampleRate, 0.05);

    // Build both oversampling chains now (no allocation later).
    const size_t osCh = (size_t) juce::jmax (1, numChannels);
    for (int i = 0; i < 2; ++i)
    {
        const size_t stages = (i == 1) ? 2 : 1;           // 0 => 2x, 1 => 4x
        oversamplers[(size_t) i] = std::make_unique<Oversampling<float>> (
            osCh, stages,
            Oversampling<float>::filterHalfBandPolyphaseIIR, // low CPU, steep
            true,   // max quality
            true);  // integer latency for sample-accurate host PDC
        oversamplers[(size_t) i]->initProcessing ((size_t) maxBlock);
        oversamplers[(size_t) i]->reset();
    }

    current  = pending.load();
    activeOs = nullptr;
    latencySamples = 0;

    updateToneFilter (spec.sampleRate, current.toneHz, current.toneAmt);
}

void SaturationEngine::reset()
{
    toneFilter.reset();
    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();
    driveGain.setCurrentAndTargetValue (driveGain.getTargetValue());
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
    mixSmoothed.setCurrentAndTargetValue (mixSmoothed.getTargetValue());
}

//==============================================================================
void SaturationEngine::updateToneFilter (double sr, float hz, float amt)
{
    // amt < 0  -> low-pass, corner slides down as amt -> -1  (darker)
    // amt ~ 0  -> effectively bypassed (very high LP corner)
    // amt > 0  -> we still use the SVF as a LP but open it wide; the "bright"
    //             tilt is done by mixing a high-shelf-like emphasis below.
    //
    // To keep it to a single cheap filter we map the bipolar Tone knob to a
    // single low-pass corner:
    //     amt = -1  -> corner = 250 Hz     (dark, obvious)
    //     amt =  0  -> corner = hz          (user setpoint, ~4 kHz default)
    //     amt = +1  -> corner = 20 kHz      (open / no LP)
    // and apply a gentle broadband tilt via the SVF resonance for the bright
    // half so it audibly "lifts" rather than just "un-darkens".

    float corner;
    float q = 0.5f;

    if (amt <= 0.0f)
    {
        // interpolate 250 Hz .. setpoint on a log scale
        const float t = juce::jlimit (0.0f, 1.0f, amt + 1.0f); // 0..1
        corner = juce::jmap (t, std::log (250.0f), std::log (hz));
        corner = std::exp (corner);
        q = 0.5f;
    }
    else
    {
        const float t = juce::jlimit (0.0f, 1.0f, amt); // 0..1
        corner = juce::jmap (t, std::log (hz), std::log (18000.0f));
        corner = std::exp (corner);
        // add a mild resonant lift near the corner for "air"
        q = juce::jmap (t, 0.5f, 1.1f);
    }

    corner = juce::jlimit (30.0f, (float) (sr * 0.45), corner);

    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.setCutoffFrequency (corner);
    toneFilter.setResonance (q);
}

//==============================================================================
void SaturationEngine::process (juce::AudioBuffer<float>& buffer)
{
    current = pending.load();

    const int numCh  = juce::jmin (buffer.getNumChannels(), numChannels);
    const int numSm  = buffer.getNumSamples();

    if (numCh == 0 || numSm == 0)
        return;

    // Select the pre-built oversampling chain (pointer swap only).
    juce::dsp::Oversampling<float>* wantOs = nullptr;
    if      (current.oversample == 1) wantOs = oversamplers[0].get(); // 2x
    else if (current.oversample >= 2) wantOs = oversamplers[1].get(); // 4x

    if (wantOs != activeOs)
    {
        if (wantOs != nullptr) wantOs->reset();
        activeOs = wantOs;
        latencySamples = (activeOs != nullptr)
                           ? (int) activeOs->getLatencyInSamples()
                           : 0;
    }

    updateToneFilter (baseSampleRate, current.toneHz, current.toneAmt);

    // --- target gains -------------------------------------------------------
    const float driveLin  = juce::Decibels::decibelsToGain (current.driveDb);
    const float outLin     = juce::Decibels::decibelsToGain (current.outputDb);
    driveGain .setTargetValue (driveLin);
    outputGain.setTargetValue (outLin);
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, current.mix));

    const float autoGain = current.autoGain
                             ? neosat::sat::autoGainFor (current.mode, driveLin)
                             : 1.0f;
    autoGainSm.setTargetValue (autoGain);

    // --- keep a dry copy for the Mix blend --------------------------------
    dryBuffer.setSize (numCh, numSm, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSm);

    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSm));
    inLevel.store (inPeak);

    juce::dsp::AudioBlock<float> block (buffer);
    block = block.getSubBlock (0, (size_t) numSm).getSubsetChannelBlock (0, (size_t) numCh);

    // --- 1. Drive (applied at base rate) ---------------------------------
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        auto  g = driveGain;                 // copy so each channel ramps the same
        for (int n = 0; n < numSm; ++n)
            d[n] *= g.getNextValue();
    }
    driveGain.skip (numSm);

    // --- 2. Tone filter PRE --------------------------------------------------
    if ((TonePos) current.tonePos == TonePos::pre)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        toneFilter.process (ctx);
    }

    // --- 3. Oversample -> waveshape -> downsample ------------------------
    const int mode = current.mode;

    auto shapeBlock = [mode] (juce::dsp::AudioBlock<float>& b)
    {
        const size_t chs = b.getNumChannels();
        const size_t len = b.getNumSamples();
        for (size_t ch = 0; ch < chs; ++ch)
        {
            auto* s = b.getChannelPointer (ch);
            for (size_t n = 0; n < len; ++n)
                s[n] = neosat::sat::process (mode, s[n]);
        }
    };

    if (activeOs != nullptr)
    {
        auto up = activeOs->processSamplesUp (block);
        shapeBlock (up);
        activeOs->processSamplesDown (block);
    }
    else
    {
        shapeBlock (block);
    }

    // --- 4. Tone filter POST ---------------------------------------------
    if ((TonePos) current.tonePos == TonePos::post)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        toneFilter.process (ctx);
    }

    // --- 5. Auto-gain + Output trim ------------------------------------
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d  = buffer.getWritePointer (ch);
        auto  ag = autoGainSm;
        auto  og = outputGain;
        for (int n = 0; n < numSm; ++n)
            d[n] *= ag.getNextValue() * og.getNextValue();
    }
    autoGainSm.skip (numSm);
    outputGain.skip (numSm);

    // --- 6. Dry / wet mix ---------------------------------------------------
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        auto  m   = mixSmoothed;
        for (int n = 0; n < numSm; ++n)
        {
            const float mm = m.getNextValue();
            wet[n] = dry[n] * (1.0f - mm) + wet[n] * mm;
        }
    }
    mixSmoothed.skip (numSm);

    float outPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSm));
    outLevel.store (outPeak);
}

//==============================================================================
float SaturationEngine::evaluateCurve (float x) const
{
    const Params p = pending.load();
    const float driveLin = juce::Decibels::decibelsToGain (p.driveDb);
    const float outLin    = juce::Decibels::decibelsToGain (p.outputDb);
    const float ag = p.autoGain ? neosat::sat::autoGainFor (p.mode, driveLin) : 1.0f;

    float y = neosat::sat::process (p.mode, x * driveLin);
    y *= ag * outLin;
    return y;
}
