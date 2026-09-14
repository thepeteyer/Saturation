#pragma once

#include <JuceHeader.h>
#include "gui/NeonLookAndFeel.h"

/**
    GlowKnob
    --------
    A self-contained control: a large rotary Slider, a caption above it and a
    live value readout below.  It runs a small timer that eases a "heat" value
    toward 1 while the pointer is over or dragging the knob and back to 0
    afterwards - that heat feeds the LookAndFeel so the glow pulses on
    interaction.  Each GlowKnob owns its own LookAndFeel instance so the heat is
    per-knob, not global.

    A one-line hint string (set by the editor) is shown under the value on hover
    so a first-time user learns what the knob does within seconds.
*/
class GlowKnob : public juce::Component,
                 private juce::Timer
{
public:
    GlowKnob (const juce::String& caption, const juce::String& hint);
    ~GlowKnob() override;

    juce::Slider& getSlider() noexcept { return slider; }

    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onHintShow; // editor uses this to surface the hint text
    juce::String getHint() const { return hintText; }

private:
    void timerCallback() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    NeonLookAndFeel lnf;
    juce::Slider slider;
    juce::String captionText, hintText;

    float heat = 0.0f;
    float heatTarget = 0.0f;
    float pulsePhase = 0.0f;
    bool  wasHot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowKnob)
};
