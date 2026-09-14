#include "gui/TransferCurveDisplay.h"
#include "gui/NeonTheme.h"

//==============================================================================
TransferCurveDisplay::TransferCurveDisplay (SaturationEngine& e) : engine (e)
{
    curveY.fill (0.0f);
    startTimerHz (30);
}

//==============================================================================
void TransferCurveDisplay::resized() {}

void TransferCurveDisplay::timerCallback()
{
    // Recompute the cached curve (cheap: kPoints static evaluations).
    for (int i = 0; i < kPoints; ++i)
    {
        const float x = juce::jmap ((float) i, 0.0f, (float) (kPoints - 1), -1.0f, 1.0f);
        curveY[(size_t) i] = juce::jlimit (-1.6f, 1.6f, engine.evaluateCurve (x));
    }

    // Live operating point from the engine's block meters.
    const float tgtIn  = juce::jlimit (0.0f, 1.0f, engine.getInputLevel());
    const float tgtOut = juce::jlimit (0.0f, 1.6f, engine.getOutputLevel());
    liveIn  += (tgtIn  - liveIn)  * 0.3f;
    liveOut += (tgtOut - liveOut) * 0.3f;

    dotPulse += 0.2f;

    repaint();
}

//==============================================================================
juce::Path TransferCurveDisplay::buildCurvePath (juce::Rectangle<float> area) const
{
    juce::Path p;
    for (int i = 0; i < kPoints; ++i)
    {
        const float nx = (float) i / (float) (kPoints - 1);        // 0..1
        const float ny = 0.5f - 0.5f * (curveY[(size_t) i] / 1.0f); // invert Y
        const juce::Point<float> pt { area.getX() + nx * area.getWidth(),
                                      juce::jlimit (area.getY(), area.getBottom(),
                                                    area.getY() + ny * area.getHeight()) };
        if (i == 0) p.startNewSubPath (pt);
        else        p.lineTo (pt);
    }
    return p;
}

void TransferCurveDisplay::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    // panel
    g.setColour (neo::panel);
    g.fillRoundedRectangle (full, neo::cornerRadius);
    g.setColour (neo::panelEdge);
    g.drawRoundedRectangle (full.reduced (0.5f), neo::cornerRadius, 1.0f);

    auto area = full.reduced (14.0f);

    // --- grid ------------------------------------------------------------
    g.setColour (neo::panelEdge.withAlpha (0.9f));
    for (int i = 1; i < 4; ++i)
    {
        const float fx = area.getX() + area.getWidth()  * i / 4.0f;
        const float fy = area.getY() + area.getHeight() * i / 4.0f;
        g.drawVerticalLine   ((int) fx, area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) fy, area.getX(), area.getRight());
    }

    // --- unity reference: the "no effect" 45' diagonal ----------------
    // A short-dashed line drawn by hand so the neon curve reads as a
    // deviation from unity.
    {
        g.setColour (neo::textDim.withAlpha (0.55f));
        const juce::Point<float> a = area.getBottomLeft();
        const juce::Point<float> b = area.getTopRight();
        const int segs = 32;
        for (int i = 0; i < segs; i += 2)
        {
            const float t0 = (float) i       / (float) segs;
            const float t1 = (float) (i + 1) / (float) segs;
            g.drawLine ({ a + (b - a) * t0, a + (b - a) * t1 }, 1.0f);
        }
    }

    // --- the transfer curve, stacked strokes for bloom ---------------
    const auto curve = buildCurvePath (area);

    // energy = peak deviation of the curve from unity -> drives colour/glow
    float dev = 0.0f;
    for (int i = 0; i < kPoints; ++i)
    {
        const float lin = juce::jmap ((float) i, 0.0f, (float) (kPoints - 1), -1.0f, 1.0f);
        dev = juce::jmax (dev, std::abs (curveY[(size_t) i] - lin));
    }
    const float energy = juce::jlimit (0.0f, 1.0f, dev * 1.4f);
    const juce::Colour accent = neo::energyColour (energy);

    struct Pass { float w; float a; };
    const Pass passes[] = { { 14.0f, 0.06f + 0.10f * energy },
                            {  8.0f, 0.10f + 0.14f * energy },
                            {  4.0f, 0.22f + 0.20f * energy },
                            {  2.0f, 1.0f } };
    for (auto& pass : passes)
    {
        g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, pass.a)));
        g.strokePath (curve, juce::PathStrokeType (pass.w, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // --- live operating-point dot -------------------------------------
    if (liveIn > 0.001f)
    {
        const float nx = juce::jlimit (0.0f, 1.0f, liveIn);
        const int   idx = juce::jlimit (0, kPoints - 1,
                                        juce::roundToInt (nx * (kPoints - 1)));
        const float ny  = 0.5f - 0.5f * curveY[(size_t) idx];

        const juce::Point<float> dot { area.getX() + nx * area.getWidth(),
                                       area.getY() + juce::jlimit (0.0f, 1.0f, ny) * area.getHeight() };
        const float r = 3.0f + 1.5f * (0.5f + 0.5f * std::sin (dotPulse));
        g.setColour (neo::magenta.withAlpha (0.25f));
        g.fillEllipse (dot.x - r * 2.5f, dot.y - r * 2.5f, r * 5.0f, r * 5.0f);
        g.setColour (neo::magenta);
        g.fillEllipse (dot.x - r, dot.y - r, r * 2.0f, r * 2.0f);
    }

    // --- axis captions ------------------------------------------------
    g.setColour (neo::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("INPUT",
                juce::Rectangle<float> (full.getCentreX() - 24.0f, full.getBottom() - 13.0f, 48.0f, 12.0f),
                juce::Justification::centred, false);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                     full.getX() + 8.0f, full.getCentreY()));
    g.drawText ("OUTPUT",
                juce::Rectangle<float> (full.getX() + 8.0f - 24.0f, full.getCentreY() - 6.0f, 48.0f, 12.0f),
                juce::Justification::centred, false);
    g.restoreState();
}
