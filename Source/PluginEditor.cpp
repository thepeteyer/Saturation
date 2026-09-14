#include "PluginEditor.h"
#include "gui/NeonTheme.h"

//==============================================================================
NeoSatAudioProcessorEditor::NeoSatAudioProcessorEditor (NeoSatAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // --- knobs ----------------------------------------------------------
    setupKnob (driveKnob,  NeoSatAudioProcessor::PID::drive,  driveAtt);
    setupKnob (toneKnob,   NeoSatAudioProcessor::PID::tone,   toneAtt);
    setupKnob (mixKnob,    NeoSatAudioProcessor::PID::mix,    mixAtt);
    setupKnob (outputKnob, NeoSatAudioProcessor::PID::output, outputAtt);

    // --- mode selector -------------------------------------------------
    modeBox.addItemList ({ "Soft Clip", "Tube", "Tape", "Wavefolder" }, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<ComboAttach> (proc.apvts,
                                             NeoSatAudioProcessor::PID::mode, modeBox);
    modeBox.onChange = [this]
    {
        const juce::StringArray blurb {
            "SOFT CLIP - smooth symmetric limiting. Warm, thick, mostly odd harmonics.",
            "TUBE - asymmetric valve drive. Adds singing even harmonics, gentle bloom.",
            "TAPE - soft compressive shoulder. Rounds transients, glues the sound.",
            "WAVEFOLDER - reflects peaks back down. Bright, metallic, synth-style." };
        const int i = juce::jlimit (0, 3, modeBox.getSelectedItemIndex());
        showHint (blurb[i]);
    };

    modeCaption.setJustificationType (juce::Justification::centredLeft);
    modeCaption.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    modeCaption.setColour (juce::Label::textColourId, neo::textDim);
    addAndMakeVisible (modeCaption);

    // --- footer utilities -------------------------------------------
    autoGainButton.setClickingTogglesState (true);
    addAndMakeVisible (autoGainButton);
    autoGainAtt = std::make_unique<ButtonAttach> (proc.apvts,
                                                  NeoSatAudioProcessor::PID::autoGain, autoGainButton);
    autoGainButton.onClick = [this]
    {
        showHint (autoGainButton.getToggleState()
                    ? "AUTO GAIN on - output level is compensated so louder isn't mistaken for better."
                    : "AUTO GAIN off - set the Output knob yourself.");
    };

    tonePosBox.addItemList ({ "Pre", "Post" }, 1);
    addAndMakeVisible (tonePosBox);
    tonePosAtt = std::make_unique<ComboAttach> (proc.apvts,
                                                NeoSatAudioProcessor::PID::tonePos, tonePosBox);
    tonePosBox.onChange = [this]
    {
        showHint (tonePosBox.getSelectedItemIndex() == 0
                    ? "TONE = Pre - filter shapes the signal BEFORE it hits the saturator."
                    : "TONE = Post - filter shapes the sound AFTER saturation.");
    };

    oversampleBox.addItemList ({ "OS Off", "OS 2x", "OS 4x" }, 1);
    addAndMakeVisible (oversampleBox);
    oversampleAtt = std::make_unique<ComboAttach> (proc.apvts,
                                                   NeoSatAudioProcessor::PID::oversample, oversampleBox);
    oversampleBox.onChange = [this]
    {
        showHint ("OVERSAMPLING runs the saturator at a higher rate to suppress "
                  "aliasing at high Drive. 4x is cleanest, Off is lightest on CPU.");
    };

    for (auto* c : { &tonePosCaption, &oversampleCaption })
    {
        c->setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        c->setColour (juce::Label::textColourId, neo::textDim);
        c->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (c);
    }

    // --- hint line -------------------------------------------------
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    hintLabel.setFont (juce::Font (juce::FontOptions (12.5f)));
    hintLabel.setColour (juce::Label::textColourId, neo::cyan.withAlpha (0.85f));
    hintLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (hintLabel);
    showHint ("Hover a control to learn what it does. Turn Drive up and watch the curve bend.");

    // --- visualisers ---------------------------------------------------
    addAndMakeVisible (curve);
    addAndMakeVisible (scope);

    // --- resize behaviour: fixed 1.6 : 1 aspect ----------------------
    setResizable (true, true);
    setResizeLimits (neo::minWidth,
                     juce::roundToInt (neo::minWidth / neo::aspectRatio),
                     neo::maxWidth,
                     juce::roundToInt (neo::maxWidth / neo::aspectRatio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (neo::aspectRatio);

    setSize (neo::defaultWidth, neo::defaultHeight);
}

NeoSatAudioProcessorEditor::~NeoSatAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void NeoSatAudioProcessorEditor::setupKnob (GlowKnob& k, const char* paramID,
                                            std::unique_ptr<SliderAttach>& attach)
{
    addAndMakeVisible (k);
    attach = std::make_unique<SliderAttach> (proc.apvts, paramID, k.getSlider());
    k.onHintShow = [this, &k] { showHint (k.getHint()); };
}

void NeoSatAudioProcessorEditor::showHint (const juce::String& text)
{
    hintLabel.setText (text, juce::dontSendNotification);
}

//==============================================================================
void NeoSatAudioProcessorEditor::paint (juce::Graphics& g)
{
    // vertical gradient ground, near-black
    juce::ColourGradient bg (neo::background, 0, 0,
                             neo::background.brighter (0.03f), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    const float scale = (float) getWidth() / (float) neo::defaultWidth;
    auto r = getLocalBounds().toFloat();

    // --- header wordmark ------------------------------------------------
    auto header = r.removeFromTop (44.0f * scale).reduced (16.0f * scale, 0.0f);

    g.setFont (juce::Font (juce::FontOptions (juce::jmax (18.0f, 21.0f * scale))).boldened());
    // glow
    g.setColour (neo::cyan.withAlpha (0.25f));
    g.drawText ("NEOSAT", header.translated (0, 0.5f), juce::Justification::centredLeft, false);
    g.setColour (neo::textHi);
    g.drawText ("NEOSAT", header, juce::Justification::centredLeft, false);

    g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, 10.0f * scale))));
    g.setColour (neo::textDim);
    g.drawText ("SATURATION", header.withTrimmedLeft (juce::jmax (78.0f, 92.0f * scale)),
                juce::Justification::centredLeft, false);

    // thin neon divider under header
    g.setColour (neo::cyan.withAlpha (0.20f));
    g.fillRect (juce::Rectangle<float> (0.0f, 44.0f * scale, (float) getWidth(), 1.0f));
}

//==============================================================================
void NeoSatAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) neo::defaultWidth;
    auto r = getLocalBounds();

    const int header = juce::roundToInt (44.0f * scale);
    const int knobH  = juce::roundToInt (128.0f * scale);
    const int footer = juce::roundToInt (38.0f * scale);
    const int pad    = juce::roundToInt (14.0f * scale);

    auto head = r.removeFromTop (header).reduced (pad, juce::roundToInt (7.0f * scale));
    // Mode selector sits on the right of the header.
    {
        auto mb = head.removeFromRight (juce::roundToInt (200.0f * scale));
        modeCaption.setBounds (mb.removeFromLeft (juce::roundToInt (46.0f * scale)));
        modeBox.setBounds (mb);
    }

    auto foot = r.removeFromBottom (footer).reduced (pad, juce::roundToInt (5.0f * scale));
    {
        const int cbW  = juce::roundToInt (86.0f * scale);
        const int capW = juce::roundToInt (40.0f * scale);
        const int gap  = juce::roundToInt (8.0f * scale);

        oversampleBox.setBounds (foot.removeFromRight (cbW));
        foot.removeFromRight (gap);
        oversampleCaption.setBounds (foot.removeFromRight (juce::roundToInt (74.0f * scale)));
        foot.removeFromRight (gap * 2);

        tonePosBox.setBounds (foot.removeFromRight (juce::roundToInt (64.0f * scale)));
        foot.removeFromRight (gap);
        tonePosCaption.setBounds (foot.removeFromRight (capW));
        foot.removeFromRight (gap * 2);

        autoGainButton.setBounds (foot.removeFromRight (juce::roundToInt (96.0f * scale)));
        foot.removeFromRight (gap * 2);

        hintLabel.setBounds (foot); // takes the remaining left space
    }

    auto knobRow = r.removeFromBottom (knobH).reduced (pad, juce::roundToInt (6.0f * scale));
    {
        GlowKnob* knobs[] = { &driveKnob, &toneKnob, &mixKnob, &outputKnob };
        const int w = knobRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto cell = (i < 3) ? knobRow.removeFromLeft (w) : knobRow;
            knobs[i]->setBounds (cell.reduced (juce::roundToInt (6.0f * scale), 0));
        }
    }

    // --- visualiser area (whatever is left) -----------------------------
    auto vis = r.reduced (pad, juce::roundToInt (10.0f * scale));
    const int scopeW = juce::roundToInt (vis.getWidth() * 0.34f);
    scope.setBounds (vis.removeFromRight (scopeW));
    vis.removeFromRight (juce::roundToInt (10.0f * scale));
    curve.setBounds (vis);
}
