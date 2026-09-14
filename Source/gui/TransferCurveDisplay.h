#pragma once

#include <JuceHeader.h>
#include <array>
#include "dsp/SaturationEngine.h"

/**
    TransferCurveDisplay
    --------------------
    The centrepiece visualiser.  It plots the plugin's input -> output transfer
    function (what a saturator actually *is*): the horizontal axis is input
    amplitude -1..+1, the vertical axis is the shaped output.  A straight 45'
    line would mean "no effect"; the more the neon curve bows away from that
    line, the harder the signal is being saturated - so turning Drive visibly
    bends the curve, and switching Mode visibly changes its shape.

    A pulsing dot rides the curve at the current input level, tying the live
    signal to the maths.  Everything is drawn with stacked translucent strokes
    to fake a bloom without needing a blur shader.
*/
class TransferCurveDisplay : public juce::Component,
                             private juce::Timer
{
public:
    explicit TransferCurveDisplay (SaturationEngine& e);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SaturationEngine& engine;

    static constexpr int kPoints = 160;
    std::array<float, kPoints> curveY {};   // cached output values, -1..1
    float liveIn = 0.0f, liveOut = 0.0f;
    float dotPulse = 0.0f;

    juce::Path buildCurvePath (juce::Rectangle<float> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferCurveDisplay)
};
