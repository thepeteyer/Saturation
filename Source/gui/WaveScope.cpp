#include "gui/WaveScope.h"
#include "gui/NeonTheme.h"

//==============================================================================
WaveScope::WaveScope (SaturationEngine& e) : engine (e)
{
    inHist .assign (kHistory, 0.0f);
    outHist.assign (kHistory, 0.0f);
    startTimerHz (30);
}

void WaveScope::timerCallback()
{
    inHist [(size_t) writePos] = juce::jlimit (0.0f, 1.4f, engine.getInputLevel());
    outHist[(size_t) writePos] = juce::jlimit (0.0f, 1.4f, engine.getOutputLevel());
    writePos = (writePos + 1) % kHistory;
    repaint();
}

//==============================================================================
void WaveScope::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    g.setColour (neo::panel);
    g.fillRoundedRectangle (full, neo::cornerRadius);
    g.setColour (neo::panelEdge);
    g.drawRoundedRectangle (full.reduced (0.5f), neo::cornerRadius, 1.0f);

    auto area = full.reduced (10.0f, 8.0f);
    const float mid = area.getBottom();

    auto pathFrom = [&] (const std::vector<float>& hist) -> juce::Path
    {
        juce::Path p;
        for (int i = 0; i < kHistory; ++i)
        {
            const int idx = (writePos + i) % kHistory;
            const float nx = (float) i / (float) (kHistory - 1);
            const float v  = juce::jlimit (0.0f, 1.2f, hist[(size_t) idx]);
            const float x  = area.getX() + nx * area.getWidth();
            const float y  = mid - v / 1.2f * area.getHeight();
            if (i == 0) p.startNewSubPath (x, mid);
            p.lineTo (x, y);
            if (i == kHistory - 1) p.lineTo (x, mid);
        }
        p.closeSubPath();
        return p;
    };

    // input: dim fill
    g.setColour (neo::cyan.withAlpha (0.12f));
    g.fillPath (pathFrom (inHist));

    // output: energy-coloured, brighter, with a glow stroke
    const float e = juce::jlimit (0.0f, 1.0f, engine.getOutputLevel() / 1.1f);
    const juce::Colour accent = neo::energyColour (e);
    const auto outPath = pathFrom (outHist);
    g.setColour (accent.withAlpha (0.18f));
    g.fillPath (outPath);
    g.setColour (accent.withAlpha (0.5f));
    g.strokePath (outPath, juce::PathStrokeType (2.5f));
    g.setColour (accent);
    g.strokePath (outPath, juce::PathStrokeType (1.0f));

    // 0 dBFS reference
    const float refY = mid - (1.0f / 1.2f) * area.getHeight();
    g.setColour (neo::magenta.withAlpha (0.35f));
    g.drawHorizontalLine ((int) refY, area.getX(), area.getRight());

    g.setColour (neo::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("IN / OUT", area.removeFromTop (11), juce::Justification::topLeft, false);
}
