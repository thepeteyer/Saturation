# NeoSat

A neon-styled **VST3 saturation plugin** built with [JUCE](https://juce.com) (C++).
Four saturation algorithms, oversampling, a pre/post tone filter, dry/wet mix and
auto-gain — wrapped in a dark, glow-heavy UI whose visuals are wired directly to
what the DSP is doing.

See [`docs/layout.txt`](docs/layout.txt) for an ASCII sketch of the window.

---

## Folder structure

```
NeoSat/
├── CMakeLists.txt            # single-command build; fetches JUCE 8 by default
├── scripts/
│   ├── build.ps1             # Windows / MSVC helper
│   └── build.sh              # macOS / Linux helper
└── Source/
    ├── PluginProcessor.h/.cpp   # host plumbing + parameter tree (APVTS)
    ├── PluginEditor.h/.cpp      # neon UI, layout, fixed-aspect resizing
    ├── dsp/
    │   ├── Saturators.h         # the waveshaping math, one function per mode
    │   │                        #   (fully commented — see "Saturation math" below)
    │   └── SaturationEngine.h/.cpp  # the full signal chain:
    │                            #   drive -> tone(pre) -> oversample -> shape
    │                            #   -> downsample -> tone(post) -> auto-gain
    │                            #   -> output -> dry/wet mix
    └── gui/
        ├── NeonTheme.h          # palette + layout constants (one source of truth)
        ├── NeonLookAndFeel.h/.cpp   # glowing knobs / combos / buttons
        ├── GlowKnob.h/.cpp          # knob + caption + value + hover-pulse anim
        ├── TransferCurveDisplay.h/.cpp  # the input->output transfer curve
        └── WaveScope.h/.cpp         # slim scrolling IN/OUT level history
```

**Two classes do the real work**, exactly as requested:

| Concern            | Class                    | File                          |
|--------------------|--------------------------|-------------------------------|
| DSP / processing   | `SaturationEngine`       | `Source/dsp/SaturationEngine.*` |
| Editor / UI        | `NeoSatAudioProcessorEditor` | `Source/PluginEditor.*`     |

`NeoSatAudioProcessor` is a thin shell: it owns the parameters and forwards
them to `SaturationEngine` once per block.

---

## Building

### Prerequisites
- **CMake ≥ 3.22**
- A C++17 compiler: **Visual Studio 2022** (Windows), **Xcode / Command Line
  Tools** (macOS), or **gcc/clang** + ALSA/JACK dev packages (Linux)
- Internet on the first configure (JUCE is pulled via `FetchContent`), or a
  local JUCE checkout passed with `-DNEOSAT_JUCE_PATH=/path/to/JUCE`

### Quick start

```bash
# Windows (PowerShell)
pwsh scripts/build.ps1

# macOS / Linux
./scripts/build.sh
```

### Manual

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The VST3 is written to
`build/NeoSat_artefacts/Release/VST3/NeoSat.vst3`. Copy it into your VST3
folder to use it in a DAW:

| OS      | VST3 folder |
|---------|-------------|
| Windows | `C:\Program Files\Common Files\VST3` |
| macOS   | `~/Library/Audio/Plug-Ins/VST3` |
| Linux   | `~/.vst3` |

(Or flip `COPY_PLUGIN_AFTER_BUILD` to `TRUE` in `CMakeLists.txt` to auto-install.)
A **Standalone** app is also built for quick auditioning.

---

## Controls

Only **four large knobs + a Mode switch** live on the main surface. Three small
utility widgets sit in the footer.

| Control        | Range            | What it does |
|----------------|------------------|--------------|
| **Drive**      | −6…+36 dB        | Input gain into the shaper. More drive = more harmonics + grit. The transfer curve visibly bends as you turn it. |
| **Tone**       | dark ↔ bright    | One bipolar filter. Left ⇒ low-pass sweeps down to 250 Hz (darker). Right ⇒ opens up + adds a resonant "air" lift. Centre ⇒ flat. |
| **Mix**        | 0…100 %          | Dry/wet blend. Parallel saturation at low values. |
| **Output**     | −24…+24 dB       | Post-processing trim. |
| **Mode**       | 4 choices        | Soft Clip · Tube · Tape · Wavefolder. |
| Auto Gain      | on/off           | Compensates output level per mode/drive so "louder" isn't mistaken for "better" when A/B-ing. |
| Tone           | Pre / Post       | Filter position relative to the shaper. |
| Oversample     | Off / 2x / 4x    | Aliasing suppression at high drive (reports latency to the host). |

Every control shows a one-line plain-language hint in the footer on hover, and
selecting a Mode prints a one-sentence description of its character.

---

## Saturation math

All shapers live in `Source/dsp/Saturators.h` as pure, stateless functions of a
single sample `x` (already scaled by Drive). Each is normalised so a small
signal passes at ≈ unity gain, which keeps the visualiser honest.

### Mode 0 — Soft Clip  `tanh`
```
f(x) = tanh(k·x) / tanh(k)
```
The canonical soft knee. Near zero `tanh(x) ≈ x − x³/3`, so quiet passages are
untouched; as `|x|` grows the curve bends smoothly toward ±1. It is
**odd-symmetric**, so it adds mostly **odd** harmonics (3rd, 5th…) → "warm,
thick". Dividing by `tanh(k)` pins the curve to ±1 at the extremes regardless of
the knee sharpness `k`.

### Mode 1 — Tube / Valve  (asymmetric)
A triode conducts differently on each half-cycle, so its transfer curve is **not**
odd-symmetric — and that asymmetry is what produces **even** harmonics (2nd,
4th…), the "singing" valve quality. Modelled as:
```
add DC bias b, then
f(x) = (1 − e^(−a·x)) / a          for x ≥ 0   (soft, expands)
f(x) = −(1 − e^( a·x)) / (a·s)     for x < 0   (firmer, s > 1)
subtract f(b)                       (re-centre — remove the DC the bias added)
```
The leftover asymmetry in the *shape* is the point; the DC is removed so it
doesn't thump the mix bus.

### Mode 2 — Tape
```
f(x) = x / (1 + |x|^p)^(1/p)   ·  norm      (p = 2 ⇒ x / √(1+x²))
```
A rational sigmoid with a longer, softer shoulder than `tanh` — the gradual
record-level compression tape is known for. It rounds transients and "glues" a
mix. The **spectral** side of tape (head bump, HF loss) is intentionally left to
NeoSat's Tone filter so this file stays a pure memoryless curve.

### Mode 3 — Wavefolder  (triangle fold)
Instead of flattening peaks it **reflects** them back down when they exceed a
threshold. Each fold injects a fresh burst of high harmonics, so pushing Drive
sweeps through metallic, bell-like timbres (a west-coast synth staple).
Implemented with the exact closed-form triangle fold:
```
y = 4·| x/4 + 1/4 − floor(x/4 + 3/4) | − 1
```
followed by a mild `tanh` to soften the fold corners before oversampling.

### Auto-gain
`autoGainFor(mode, drive)` returns an empirical `drive^(−e)` make-up factor per
mode (the wavefolder loses the most level, so it gets the strongest
compensation). These are loudness ballpark fits, not exact RMS matches.

---

## Oversampling

`SaturationEngine` builds a **2×** (1 stage) and a **4×** (2 stage)
`juce::dsp::Oversampling` chain up-front in `prepare()`
(`filterHalfBandPolyphaseIIR`, max quality, integer latency). Switching ratio at
run time is a **pointer swap only — no allocation on the audio thread**. The
active chain's latency is reported to the host via `setLatencySamples()` so
delay compensation stays sample-accurate.

Only the waveshaper runs oversampled; Drive, the tone filter and the mix run at
base rate.

---

## UI / UX design

- **Palette** (`gui/NeonTheme.h`): near-black ground, one dominant accent
  (electric cyan) with magenta for the live signal dot and lime reserved for
  "hot" — it only appears as a knob nears maximum.
- **Energy ties to position**: `NeonLookAndFeel::drawRotarySlider` thickens the
  value arc, brightens the pointer and grows the outer bloom as the value rises,
  and shifts cyan → lime past ~60 %. You can read the drive amount from the glow
  alone.
- **Micro-animations**: each `GlowKnob` eases a "heat" value toward 1 on
  hover/drag with a subtle breathing pulse, fed into its own `LookAndFeel`
  instance so the effect is per-knob.
- **Transfer-curve visualiser** (`TransferCurveDisplay`): plots input → output.
  A dashed 45° line is "no effect"; the neon curve's bow away from it is the
  saturation amount. A magenta dot rides the curve at the live input level. The
  glow colour/intensity follows the curve's peak deviation from unity. Bloom is
  faked with stacked translucent strokes (no shader needed).
- **Scope** (`WaveScope`): a quiet secondary readout — scrolling input (dim) vs
  post-saturation output (bright) peak history with a 0 dBFS reference line.
- **Resizing**: `setResizeLimits()` + `constrainer->setFixedAspectRatio(1.6)`.
  The window keeps a locked **8 : 5** ratio between 520×325 and 1200×750; all
  layout math scales from `getWidth() / 720`.

---

## Notes / limitations

- Built and code-reviewed against the JUCE 8.0.4 API; it was **not compiled in
  the authoring environment** (no CMake/JUCE/MSVC there). Run `scripts/build.*`
  to produce the binary.
- `std::atomic<Params>` is used for the message→audio parameter hand-off. It is
  not lock-free for a struct that size, but the critical section is a few
  instructions and read once per block — fine in practice; swap to per-field
  atomics if you want a hard guarantee.
- Mono and stereo layouts are supported.
