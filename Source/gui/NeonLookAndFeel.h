#pragma once

#include <JuceHeader.h>
#include "gui/NeonTheme.h"

/**
    NeonLookAndFeel
    ---------------
    Draws rotary sliders as large ring knobs with a glow that grows with the
    value, plus themed combo boxes, buttons and labels.  The "energy ties to
    position" idea is implemented in drawRotarySlider(): the pointer, ring and
    outer bloom all brighten / thicken / shift toward lime as the value rises.
*/
class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont (juce::Label&) override;

    // Per-knob extra glow driven by hover/drag micro-animation (0..1).
    // The GlowKnob component sets this each frame before repainting.
    void setKnobHeat (float h) noexcept { knobHeat = juce::jlimit (0.0f, 1.0f, h); }

private:
    float knobHeat = 0.0f;
    juce::Font baseFont { juce::FontOptions (15.0f) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonLookAndFeel)
};
