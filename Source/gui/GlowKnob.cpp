#include "gui/GlowKnob.h"
#include "gui/NeonTheme.h"

//==============================================================================
GlowKnob::GlowKnob (const juce::String& caption, const juce::String& hint)
    : captionText (caption), hintText (hint)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setLookAndFeel (&lnf);
    slider.setMouseDragSensitivity (280);
    slider.setVelocityBasedMode (false);
    slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);

    setInterceptsMouseClicks (false, true); // children still get mouse; we watch enter/exit
    startTimerHz (60);
}

GlowKnob::~GlowKnob()
{
    slider.setLookAndFeel (nullptr);
}

//==============================================================================
void GlowKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (18);      // caption
    r.removeFromBottom (20);   // value readout
    slider.setBounds (r);
}

void GlowKnob::paint (juce::Graphics& g)
{
    const float value01 = (float) slider.valueToProportionOfLength (slider.getValue());
    const juce::Colour accent = neo::energyColour (value01);

    // caption
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.setColour (neo::textDim.interpolatedWith (accent, 0.25f + 0.5f * heat));
    g.drawText (captionText.toUpperCase (),
                getLocalBounds().removeFromTop (16),
                juce::Justification::centred, false);

    // value readout
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (neo::textHi);
    g.drawText (slider.getTextFromValue (slider.getValue()),
                getLocalBounds().removeFromBottom (18),
                juce::Justification::centred, false);
}

//==============================================================================
void GlowKnob::timerCallback()
{
    const bool hot = isMouseOverOrDragging (true) || slider.isMouseButtonDown();

    // rising edge of hover -> ask the editor to show this knob's hint
    if (hot && ! wasHot && onHintShow)
        onHintShow();
    wasHot = hot;

    heatTarget = hot ? 1.0f : 0.0f;

    // critically-ish damped ease
    heat += (heatTarget - heat) * (hot ? 0.25f : 0.08f);

    // subtle breathing pulse while hot
    pulsePhase += 0.18f;
    const float pulse = hot ? 0.12f * (0.5f + 0.5f * std::sin (pulsePhase)) : 0.0f;

    lnf.setKnobHeat (juce::jlimit (0.0f, 1.0f, heat + pulse));

    if (heat > 0.001f || heatTarget > 0.0f || slider.isMouseButtonDown())
    {
        slider.repaint();
        repaint();
    }
}

void GlowKnob::mouseEnter (const juce::MouseEvent&) {}
void GlowKnob::mouseExit  (const juce::MouseEvent&) {}
