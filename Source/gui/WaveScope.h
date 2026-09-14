#pragma once

#include <JuceHeader.h>
#include <vector>
#include "dsp/SaturationEngine.h"

/**
    WaveScope
    ---------
    A slim, secondary visualiser: a scrolling history of the input (dim cyan)
    and post-saturation output (bright, energy-coloured) peak levels.  It gives
    an at-a-glance read on how much the effect is adding / compressing without
    competing with the transfer curve for attention.
*/
class WaveScope : public juce::Component,
                  private juce::Timer
{
public:
    explicit WaveScope (SaturationEngine& e);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    SaturationEngine& engine;

    static constexpr int kHistory = 220;
    std::vector<float> inHist, outHist;
    int writePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveScope)
};
