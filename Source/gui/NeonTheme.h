#pragma once

#include <JuceHeader.h>

/**
    NeonTheme
    ---------
    One place for the NeoSat palette and shared layout constants so the whole
    UI stays visually consistent: near-black ground, a single dominant neon
    accent (electric cyan) with a magenta secondary for contrast, and a lime
    "hot" colour that only appears at high energy.
*/
namespace neo
{
    // --- palette ---------------------------------------------------------------
    const juce::Colour background   { 0xff07090c }; // almost black, slight blue
    const juce::Colour panel        { 0xff0d1116 }; // raised surfaces
    const juce::Colour panelEdge    { 0xff161c24 };

    const juce::Colour textHi       { 0xfff2f6ff }; // high-contrast labels
    const juce::Colour textDim      { 0xff6b7684 }; // secondary labels

    const juce::Colour cyan         { 0xff00e5ff }; // primary accent / glow
    const juce::Colour magenta      { 0xffff2fb3 }; // secondary accent
    const juce::Colour lime         { 0xffb6ff3b }; // "hot" - only near max drive

    // Interp helper: accent colour for a normalised energy value 0..1.
    // Low energy = cool cyan, high energy = it shifts toward lime = "hot".
    inline juce::Colour energyColour (float norm) noexcept
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        if (norm < 0.6f)
            return cyan;
        const float t = (norm - 0.6f) / 0.4f;
        return cyan.interpolatedWith (lime, t);
    }

    // --- layout --------------------------------------------------------------
    constexpr float aspectRatio   = 1.6f;   // width : height, locked by the editor
    constexpr int   defaultWidth  = 720;
    constexpr int   defaultHeight = 450;    // 720 / 1.6
    constexpr int   minWidth      = 520;
    constexpr int   maxWidth      = 1200;

    constexpr float cornerRadius  = 10.0f;
}
