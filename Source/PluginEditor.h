#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/NeonLookAndFeel.h"
#include "gui/GlowKnob.h"
#include "gui/TransferCurveDisplay.h"
#include "gui/WaveScope.h"

/**
    NeoSatAudioProcessorEditor
    --------------------------
    Fixed-aspect, resizable neon UI.

    Layout (top -> bottom):
        - header      : wordmark + Mode selector
        - visualisers : transfer curve (large) + IN/OUT scope
        - knob row    : Drive | Tone | Mix | Output  (large GlowKnobs)
        - footer      : rolling hint text + Auto Gain / Tone Pos / Oversample

    Only four large knobs and one Mode switch are "primary" controls; the three
    footer widgets are small utility toggles, keeping the main surface uncluttered.
*/
class NeoSatAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NeoSatAudioProcessorEditor (NeoSatAudioProcessor&);
    ~NeoSatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupKnob (GlowKnob& k, const char* paramID,
                    std::unique_ptr<SliderAttach>& attach);
    void showHint (const juce::String& text);

    NeoSatAudioProcessor& proc;

    NeonLookAndFeel lnf; // shared by combos / buttons / labels

    // primary controls
    GlowKnob driveKnob  { "Drive",  "How hard you push the signal into the saturator. More drive = more harmonics + grit." };
    GlowKnob toneKnob   { "Tone",   "Tilts the sound dark (left) or bright (right) around the distortion." };
    GlowKnob mixKnob    { "Mix",    "Blend between the clean signal (left) and the saturated signal (right)." };
    GlowKnob outputKnob { "Output", "Level trim after processing. Use with Auto Gain off to match loudness by ear." };

    juce::ComboBox modeBox;
    juce::Label    modeCaption { {}, "MODE" };

    // footer utilities
    juce::TextButton autoGainButton { "AUTO GAIN" };
    juce::ComboBox   tonePosBox;
    juce::ComboBox   oversampleBox;
    juce::Label      tonePosCaption    { {}, "TONE" };
    juce::Label      oversampleCaption { {}, "OVERSAMPLE" };

    juce::Label hintLabel;

    // visualisers
    TransferCurveDisplay curve { proc.getEngine() };
    WaveScope            scope { proc.getEngine() };

    // attachments
    std::unique_ptr<SliderAttach> driveAtt, toneAtt, mixAtt, outputAtt;
    std::unique_ptr<ComboAttach>  modeAtt, tonePosAtt, oversampleAtt;
    std::unique_ptr<ButtonAttach> autoGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeoSatAudioProcessorEditor)
};
