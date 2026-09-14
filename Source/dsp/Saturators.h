#pragma once

#include <cmath>

/**
    Saturators.h
    ------------
    Pure, stateless waveshaping functions - one per NeoSat "mode".

    Every function takes a single sample `x` (already scaled by Drive) and
    returns the shaped sample, normalised so that a small-signal input passes
    through at roughly unity gain.  Keeping them stateless makes them trivial
    to call from inside the oversampled render loop and easy to reuse in the
    transfer-curve visualiser (we literally feed it -1..+1 and draw the result).

    The comments for each mode explain the math and *why* it sounds the way it
    does.
*/

namespace neosat::sat
{
    //==============================================================================
    /*  MODE 0 - SOFT CLIP  (odd-symmetric tanh)

        f(x) = tanh(k * x) / tanh(k)

        The hyperbolic tangent is the classic "soft" saturation curve.  Near
        zero it is almost linear (tanh(x) ~ x - x^3/3 ...), so quiet passages are
        untouched.  As |x| grows the curve bends smoothly toward +/-1, which
        rounds transients and adds mostly low-order ODD harmonics (3rd, 5th ...).
        Odd harmonics preserve the waveform's half-wave symmetry, so the result
        reads as "warm and thick" rather than "buzzy".

        `k` (drive shape) sets how hard the knee is.  We divide by tanh(k) so the
        curve still hits +/-1 at x = +/-1 regardless of k - that keeps the
        visualiser honest and the perceived level stable.
    */
    inline float softClip (float x, float k = 2.0f) noexcept
    {
        const float norm = 1.0f / std::tanh (k);
        return std::tanh (k * x) * norm;
    }

    //==============================================================================
    /*  MODE 1 - TUBE / VALVE  (asymmetric, biased)

        A real triode stage is asymmetric: the grid conducts differently on the
        positive and negative half-cycles, so the transfer curve is not
        odd-symmetric.  That asymmetry is what generates EVEN harmonics (2nd,
        4th ...), the "singing" quality people associate with valve gear.

        We model it in two steps:

          1. Add a small DC bias `b` so the signal sits off-centre on the curve.
          2. Shape with an asymmetric function: a gentler exponential soft-knee
             for x > 0 and a firmer one for x < 0.

             f(x) = (1 - exp(-a * x)) / a          for x >= 0   (soft, expands)
             f(x) = -(1 - exp( a * x)) / (a * s)    for x <  0   (harder, s > 1)

        Finally we subtract the DC that the bias introduced (measured once at
        construction) so the output stays centred and doesn't thump the mix bus.
        The leftover asymmetry in the *shape* is what we actually want.
    */
    inline float tube (float x, float bias = 0.15f) noexcept
    {
        constexpr float a = 1.8f;   // knee sharpness
        constexpr float s = 1.4f;   // negative-side firmness (s > 1 => harder)

        auto shape = [] (float v) noexcept
        {
            if (v >= 0.0f)
                return (1.0f - std::exp (-a * v)) / a;
            return -(1.0f - std::exp (a * v)) / (a * s);
        };

        // DC produced by the bias alone - computed here so output is centred.
        const float dc = shape (bias);
        return shape (x + bias) - dc;
    }

    //==============================================================================
    /*  MODE 2 - TAPE  (hysteresis-flavoured soft saturation)

        Magnetic tape has two audible fingerprints:

          * A smooth compressive saturation of the record level.  We use a
            rational sigmoid  x / (1 + |x|)^p  which is cheaper than tanh and has
            a slightly longer, softer shoulder - very "tape".

          * Frequency-dependent behaviour (head bump, HF loss).  The static
            shaper here only covers the amplitude non-linearity; NeoSat's Tone
            filter and the optional pre-emphasis in SaturationEngine provide the
            spectral tilt so this file stays a pure memoryless curve.

        f(x) = x / (1 + |x|^p) ^ (1/p)      then normalised to hit ~1 at x = 1

        With p = 2 this is the "fast sigmoid" x / sqrt(1 + x^2): almost linear
        for small x, very gradual compression up top, dominated by low-order odd
        harmonics with a gentle even component from the slight offset we add.
    */
    inline float tape (float x) noexcept
    {
        constexpr float p = 2.0f;
        const float shaped = x / std::pow (1.0f + std::pow (std::abs (x), p), 1.0f / p);

        // Re-normalise so full-scale in maps to ~full-scale out.
        constexpr float norm = 1.41421356f; // 1 / f(1) for p = 2  => sqrt(2)
        return shaped * norm;
    }

    //==============================================================================
    /*  MODE 3 - WAVEFOLDER  (triangle fold)

        Instead of flattening peaks, a wavefolder *reflects* them back down when
        they exceed a threshold.  Each fold injects a fresh burst of high
        harmonics, so pushing Drive sweeps through metallic, bell-like timbres -
        a west-coast synth staple.

        Implementation: scale the input, then repeatedly reflect it into the
        [-1, 1] band using the identity for a triangle wave:

            y = 4 * | (x/4 + 1/4) - floor(x/4 + 3/4) | - 1

        This is exact (no iteration needed) and folds as many times as the drive
        demands.  A final tanh softens the sharp fold corners just enough to keep
        the very top octave from aliasing before oversampling cleans up the rest.
    */
    inline float waveFold (float x) noexcept
    {
        constexpr float foldGain = 1.0f; // extra folding headroom on top of Drive
        float v = x * foldGain;

        // Exact triangle fold into [-1, 1].
        v = 4.0f * std::abs (0.25f * v + 0.25f - std::floor (0.25f * v + 0.75f)) - 1.0f;

        // Soften the corners a touch (pre-oversampling anti-alias help).
        return std::tanh (1.5f * v) / std::tanh (1.5f);
    }

    //==============================================================================
    /** Dispatch by mode index. Keep in sync with SaturationEngine::Mode. */
    inline float process (int mode, float x) noexcept
    {
        switch (mode)
        {
            case 0:  return softClip (x);
            case 1:  return tube (x);
            case 2:  return tape (x);
            case 3:  return waveFold (x);
            default: return softClip (x);
        }
    }

    /** Approximate make-up gain (linear) that roughly restores perceived level
        for a given mode at a given drive.  Used by the auto-gain option.  These
        are empirical fits, not exact RMS matches - they get you in the ballpark
        so A/B'ing the effect isn't dominated by loudness. */
    inline float autoGainFor (int mode, float driveLinear) noexcept
    {
        // As drive rises, each shaper compresses more, so we need to add level
        // back.  A gentle power curve of 1/drive^e per mode does the job.
        const float d = driveLinear < 1.0f ? 1.0f : driveLinear;
        switch (mode)
        {
            case 0:  return std::pow (d, -0.60f); // soft clip
            case 1:  return std::pow (d, -0.55f); // tube
            case 2:  return std::pow (d, -0.50f); // tape
            case 3:  return std::pow (d, -0.80f); // wavefolder loses more level
            default: return std::pow (d, -0.60f);
        }
    }
}
