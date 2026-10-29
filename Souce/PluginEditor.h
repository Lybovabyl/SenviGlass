#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SenviGlassAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit SenviGlassAudioProcessorEditor (SenviGlassAudioProcessor&);
    ~SenviGlassAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static void styleKnob (juce::Slider&);
    static void styleCombo (juce::ComboBox&);

    SenviGlassAudioProcessor& processor;

    juce::Slider input, color, glue, density, shadow, output;
    juce::Label inputLabel, colorLabel, glueLabel, densityLabel, shadowLabel, outputLabel;
    juce::ComboBox mode, comp, clip, stereo;
    juce::TextButton levelMatch { "LEVEL MATCH" }, delta { "DELTA" };
    juce::Label meters;

    using SliderAttachment = SenviGlassAudioProcessor::APVTS::SliderAttachment;
    using ComboAttachment  = SenviGlassAudioProcessor::APVTS::ComboBoxAttachment;
    using ButtonAttachment = SenviGlassAudioProcessor::APVTS::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputA, colorA, glueA, densityA, shadowA, outputA;
    std::unique_ptr<ComboAttachment> modeA, compA, clipA, stereoA;
    std::unique_ptr<ButtonAttachment> levelMatchA, deltaA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SenviGlassAudioProcessorEditor)
};
