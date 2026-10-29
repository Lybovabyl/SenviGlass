#include "PluginEditor.h"

namespace
{
void setLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffd8d8dc));
}
}

SenviGlassAudioProcessorEditor::SenviGlassAudioProcessorEditor (SenviGlassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (760, 440);
    setResizable (true, true);
    setResizeLimits (660, 380, 1100, 700);

    for (auto* s : { &input, &color, &glue, &density, &shadow, &output })
    {
        styleKnob (*s);
        addAndMakeVisible (*s);
    }

    setLabel (inputLabel, "INPUT");
    setLabel (colorLabel, "COLOR");
    setLabel (glueLabel, "GLUE");
    setLabel (densityLabel, "DENSITY");
    setLabel (shadowLabel, "SHADOW");
    setLabel (outputLabel, "OUTPUT");

    for (auto* l : { &inputLabel, &colorLabel, &glueLabel, &densityLabel, &shadowLabel, &outputLabel })
        addAndMakeVisible (*l);

    for (auto* c : { &mode, &comp, &clip, &stereo })
    {
        styleCombo (*c);
        addAndMakeVisible (*c);
    }

    mode.addItemList ({ "LYRIC", "PUNCH", "NIGHT" }, 1);
    comp.addItemList ({ "SLOW", "AUTO", "FAST" }, 1);
    clip.addItemList ({ "CLEAN", "SOFT", "DRIVEN" }, 1);
    stereo.addItemList ({ "SAFE", "WIDE" }, 1);

    for (auto* b : { &levelMatch, &delta })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff24252a));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff7a512b));
        addAndMakeVisible (*b);
    }

    meters.setJustificationType (juce::Justification::centred);
    meters.setColour (juce::Label::textColourId, juce::Colour (0xffaeb2ba));
    addAndMakeVisible (meters);

    auto& apvts = processor.parameters;
    inputA = std::make_unique<SliderAttachment> (apvts, "input", input);
    colorA = std::make_unique<SliderAttachment> (apvts, "color", color);
    glueA = std::make_unique<SliderAttachment> (apvts, "glue", glue);
    densityA = std::make_unique<SliderAttachment> (apvts, "density", density);
    shadowA = std::make_unique<SliderAttachment> (apvts, "shadow", shadow);
    outputA = std::make_unique<SliderAttachment> (apvts, "output", output);
    modeA = std::make_unique<ComboAttachment> (apvts, "mode", mode);
    compA = std::make_unique<ComboAttachment> (apvts, "comp", comp);
    clipA = std::make_unique<ComboAttachment> (apvts, "clip", clip);
    stereoA = std::make_unique<ComboAttachment> (apvts, "stereo", stereo);
    levelMatchA = std::make_unique<ButtonAttachment> (apvts, "levelMatch", levelMatch);
    deltaA = std::make_unique<ButtonAttachment> (apvts, "delta", delta);

    startTimerHz (20);
}

void SenviGlassAudioProcessorEditor::styleKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffbc7440));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff30333a));
    s.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe0a16c));
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe7e7ea));
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff15161a));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void SenviGlassAudioProcessorEditor::styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202127));
    c.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe7e7ea));
    c.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff34363e));
    c.setJustificationType (juce::Justification::centred);
}

void SenviGlassAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff111216), 0, 0,
                             juce::Colour (0xff171b22), (float) getWidth(), (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (juce::Colour (0xff252932));
    g.fillRoundedRectangle (18.0f, 18.0f, (float) getWidth() - 36.0f, (float) getHeight() - 36.0f, 18.0f);

    g.setColour (juce::Colour (0xfff1f1f2));
    g.setFont (juce::Font (juce::FontOptions (24.0f)));
    g.drawText ("SENVI GLASS", 30, 24, getWidth() - 60, 34, juce::Justification::centred);

    g.setColour (juce::Colour (0xff888d98));
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("ADAPTIVE MASTER COLOR / PRE-LIMITER", 30, 55, getWidth() - 60, 22, juce::Justification::centred);

    const auto center = juce::Point<float> ((float) getWidth() * 0.5f, (float) getHeight() * 0.54f);
    const float radius = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.19f;
    g.setColour (juce::Colour (0x223a6ea8));
    g.fillEllipse (center.x - radius * 1.55f, center.y - radius, radius * 3.1f, radius * 2.0f);
    g.setColour (juce::Colour (0x22c57a43));
    g.fillEllipse (center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
}

void SenviGlassAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (30);
    area.removeFromTop (55);

    auto top = area.removeFromTop (38);
    const int comboW = 112;
    mode.setBounds (top.removeFromLeft (comboW));
    top.removeFromLeft (8);
    comp.setBounds (top.removeFromLeft (comboW));
    top.removeFromLeft (8);
    clip.setBounds (top.removeFromLeft (comboW));
    top.removeFromLeft (8);
    stereo.setBounds (top.removeFromLeft (comboW));
    top.removeFromLeft (12);
    levelMatch.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (8);
    delta.setBounds (top.removeFromLeft (80));

    area.removeFromTop (14);
    auto knobs = area.removeFromTop (235);
    const int sideW = juce::jmax (90, knobs.getWidth() / 6);

    auto place = [] (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l)
    {
        l.setBounds (cell.removeFromTop (22));
        s.setBounds (cell.reduced (4));
    };

    auto c1 = knobs.removeFromLeft (sideW);
    auto c2 = knobs.removeFromLeft (sideW);
    auto c3 = knobs.removeFromLeft (sideW);
    auto c4 = knobs.removeFromLeft (sideW);
    auto c5 = knobs.removeFromLeft (sideW);
    auto c6 = knobs;

    place (c1, input, inputLabel);
    place (c2, glue, glueLabel);
    place (c3, color, colorLabel);
    place (c4, density, densityLabel);
    place (c5, shadow, shadowLabel);
    place (c6, output, outputLabel);

    area.removeFromTop (4);
    meters.setBounds (area.removeFromTop (34));
}

void SenviGlassAudioProcessorEditor::timerCallback()
{
    meters.setText ("GR " + juce::String (processor.getGlueGR(), 2) + " dB     "
                    "PEAK SHAVE " + juce::String (processor.getPeakShave(), 2) + " dB     "
                    "CREST Δ " + juce::String (processor.getCrestDelta(), 2) + " dB     "
                    "CORR " + juce::String (processor.getCorrelation(), 2),
                    juce::dontSendNotification);
}
