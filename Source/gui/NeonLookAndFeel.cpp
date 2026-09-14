#include "gui/NeonLookAndFeel.h"

//==============================================================================
NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, neo::background);
    setColour (juce::Slider::textBoxTextColourId,        neo::textHi);
    setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId,       neo::panel);
    setColour (juce::ComboBox::textColourId,             neo::textHi);
    setColour (juce::ComboBox::outlineColourId,          neo::panelEdge);
    setColour (juce::ComboBox::arrowColourId,            neo::cyan);
    setColour (juce::PopupMenu::backgroundColourId,      neo::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, neo::cyan.withAlpha (0.18f));
    setColour (juce::PopupMenu::textColourId,            neo::textHi);
    setColour (juce::Label::textColourId,                neo::textHi);
}

//==============================================================================
void NeonLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // "energy" = how far the knob is turned, plus the transient hover/drag heat.
    const float energy = juce::jlimit (0.0f, 1.0f, sliderPos);
    const float heat   = juce::jlimit (0.0f, 1.0f, energy * 0.75f + knobHeat * 0.6f);
    const juce::Colour accent = neo::energyColour (energy);

    const float ringR = radius * 0.78f;

    // --- outer bloom: soft filled circle, alpha rises with heat -------------
    {
        const float bloomR = radius * (0.95f + 0.15f * heat);
        juce::ColourGradient grad (accent.withAlpha (0.28f * heat), cx, cy,
                                   accent.withAlpha (0.0f), cx, cy - bloomR, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - bloomR, cy - bloomR, bloomR * 2.0f, bloomR * 2.0f);
    }

    // --- knob body --------------------------------------------------------
    g.setColour (neo::panel);
    g.fillEllipse (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);
    g.setColour (neo::panelEdge);
    g.drawEllipse (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, 1.5f);

    // --- track (unlit part of the arc) --------------------------------------
    const float trackR = radius * 0.90f;
    juce::Path track;
    track.addCentredArc (cx, cy, trackR, trackR, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (neo::panelEdge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // --- lit value arc: thicker + brighter with energy, drawn twice for glow
    juce::Path value;
    value.addCentredArc (cx, cy, trackR, trackR, 0.0f,
                         rotaryStartAngle, angle, true);

    const float coreW = 3.0f + 3.0f * energy;
    // glow underlay
    g.setColour (accent.withAlpha (0.25f + 0.45f * heat));
    g.strokePath (value, juce::PathStrokeType (coreW + 8.0f * (0.4f + heat),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    // bright core
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (coreW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // --- pointer --------------------------------------------------------
    const float tipR  = ringR * 0.92f;
    const float baseR = ringR * 0.30f;
    juce::Point<float> tip  { cx + tipR  * std::cos (angle - juce::MathConstants<float>::halfPi),
                              cy + tipR  * std::sin (angle - juce::MathConstants<float>::halfPi) };
    juce::Point<float> root { cx + baseR * std::cos (angle - juce::MathConstants<float>::halfPi),
                              cy + baseR * std::sin (angle - juce::MathConstants<float>::halfPi) };

    g.setColour (accent.withAlpha (0.35f + 0.4f * heat));
    g.drawLine ({ root, tip }, coreW + 6.0f);
    g.setColour (juce::Colours::white.interpolatedWith (accent, 0.5f + 0.5f * energy));
    g.drawLine ({ root, tip }, 2.0f + 1.5f * energy);

    // --- centre cap dot -------------------------------------------------
    g.setColour (accent.withAlpha (0.9f));
    g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

    juce::ignoreUnused (slider);
}

//==============================================================================
void NeonLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                    bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);

    g.setColour (neo::panel);
    g.fillRoundedRectangle (r, 7.0f);

    const bool active = box.isPopupActive() || box.isMouseOver (true);
    g.setColour (active ? neo::cyan : neo::panelEdge);
    g.drawRoundedRectangle (r, 7.0f, active ? 1.6f : 1.0f);

    if (active)
    {
        g.setColour (neo::cyan.withAlpha (0.15f));
        g.drawRoundedRectangle (r.expanded (1.5f), 8.0f, 2.5f);
    }

    // arrow
    const float ah = 4.0f;
    const float ax = (float) width - 16.0f;
    const float ay = (float) height * 0.5f;
    juce::Path p;
    p.startNewSubPath (ax - ah, ay - ah * 0.5f);
    p.lineTo (ax,       ay + ah * 0.6f);
    p.lineTo (ax + ah,  ay - ah * 0.5f);
    g.setColour (neo::cyan);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void NeonLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font NeonLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return baseFont.withHeight (14.0f);
}

//==============================================================================
void NeonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                            const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (neo::panel);
    g.fillRoundedRectangle (r, 7.0f);

    const juce::Colour edge = on ? neo::cyan
                                 : (highlighted ? neo::cyan.withAlpha (0.6f) : neo::panelEdge);
    g.setColour (edge);
    g.drawRoundedRectangle (r, 7.0f, on ? 1.6f : 1.0f);

    if (on || highlighted)
    {
        g.setColour (neo::cyan.withAlpha (on ? 0.22f : 0.10f));
        g.drawRoundedRectangle (r.expanded (1.5f), 8.5f, 2.5f);
    }
    juce::ignoreUnused (down);
}

void NeonLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                      bool, bool)
{
    g.setFont (baseFont.withHeight (13.0f));
    g.setColour (b.getToggleState() ? neo::cyan : neo::textDim);
    g.drawText (b.getButtonText(), b.getLocalBounds(),
                juce::Justification::centred, false);
}

//==============================================================================
juce::Font NeonLookAndFeel::getLabelFont (juce::Label& l)
{
    return baseFont.withHeight (l.getFont().getHeight());
}
