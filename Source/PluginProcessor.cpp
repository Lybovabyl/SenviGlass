#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }
float gainToDb (float g)  { return juce::Decibels::gainToDecibels (juce::jmax (g, 1.0e-9f)); }

float softClip (float x, float drive)
{
    const float d = juce::jmax (1.0f, drive);
    return std::tanh (x * d) / std::tanh (d);
}
}

SenviGlassAudioProcessor::SenviGlassAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

SenviGlassAudioProcessor::APVTS::ParameterLayout SenviGlassAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("input", "Input", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("color", "Color", 0.0f, 100.0f, 35.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("glue", "Glue", 0.0f, 100.0f, 25.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density", "Density", 0.0f, 100.0f, 30.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("shadow", "Shadow", 0.0f, 100.0f, 20.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output", "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));

    layout.add (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode", juce::StringArray { "LYRIC", "PUNCH", "NIGHT" }, 2));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("comp", "Comp", juce::StringArray { "SLOW", "AUTO", "FAST" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("clip", "Clip", juce::StringArray { "CLEAN", "SOFT", "DRIVEN" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("stereo", "Stereo", juce::StringArray { "SAFE", "WIDE" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> ("levelMatch", "Level Match", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("delta", "Delta", false));

    return layout;
}

void SenviGlassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& f : lowBandFilters)
    {
        f.prepare (sampleRate);
        f.setCutoff (160.0f);
    }

    harshFilterL.prepare (sampleRate); harshFilterL.setCutoff (2500.0f);
    harshFilterR.prepare (sampleRate); harshFilterR.setCutoff (2500.0f);
    sideLowFilter.prepare (sampleRate); sideLowFilter.setCutoff (150.0f);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    glueCompressor.prepare (spec);
    glueCompressor.reset();

    levelMatchGain.reset (sampleRate, 0.35);
    levelMatchGain.setCurrentAndTargetValue (1.0f);

    fastEnv = mediumEnv = slowEnv = lowEnv = harshEnv = 0.0f;
}

bool SenviGlassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SenviGlassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2)
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    const float inputDb = parameters.getRawParameterValue ("input")->load();
    const float outputDb = parameters.getRawParameterValue ("output")->load();
    const float color = parameters.getRawParameterValue ("color")->load() * 0.01f;
    const float glue = parameters.getRawParameterValue ("glue")->load() * 0.01f;
    const float density = parameters.getRawParameterValue ("density")->load() * 0.01f;
    const float shadow = parameters.getRawParameterValue ("shadow")->load() * 0.01f;
    const int mode = (int) parameters.getRawParameterValue ("mode")->load();
    const int compMode = (int) parameters.getRawParameterValue ("comp")->load();
    const int clipMode = (int) parameters.getRawParameterValue ("clip")->load();
    const int stereoMode = (int) parameters.getRawParameterValue ("stereo")->load();
    const bool levelMatch = parameters.getRawParameterValue ("levelMatch")->load() > 0.5f;
    const bool delta = parameters.getRawParameterValue ("delta")->load() > 0.5f;

    buffer.applyGain (dbToGain (inputDb));

    const int n = buffer.getNumSamples();
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    double inSq = 0.0;
    float inPeak = 0.0f;

    const float fastA = std::exp (-1.0f / (0.010f * (float) currentSampleRate));
    const float medA  = std::exp (-1.0f / (0.120f * (float) currentSampleRate));
    const float slowA = std::exp (-1.0f / (1.200f * (float) currentSampleRate));

    // Adaptive analysis + low-end control + COLOR.
    for (int i = 0; i < n; ++i)
    {
        float L = l[i];
        float R = r[i];
        const float mono = 0.5f * (L + R);
        const float amp = juce::jmax (std::abs (L), std::abs (R));

        fastEnv   = fastA * fastEnv   + (1.0f - fastA) * amp;
        mediumEnv = medA  * mediumEnv + (1.0f - medA)  * amp;
        slowEnv   = slowA * slowEnv   + (1.0f - slowA) * amp;

        inSq += 0.5 * ((double) L * L + (double) R * R);
        inPeak = juce::jmax (inPeak, amp);

        float lowL = lowBandFilters[0].lowpass (L);
        float lowR = lowBandFilters[1].lowpass (R);
        const float lowAmp = juce::jmax (std::abs (lowL), std::abs (lowR));
        lowEnv = medA * lowEnv + (1.0f - medA) * lowAmp;

        float lowStrength = density * (mode == 1 ? 0.35f : mode == 2 ? 0.24f : 0.16f);
        const float lowExcess = juce::jlimit (0.0f, 1.0f, (lowEnv - 0.22f) * 2.8f);
        const float lowGain = 1.0f - lowStrength * lowExcess;
        L += lowL * (lowGain - 1.0f);
        R += lowR * (lowGain - 1.0f);

        const float modeColor = mode == 0 ? 0.82f : mode == 1 ? 1.08f : 1.0f;
        const float drive = 1.0f + color * (1.25f * modeColor);
        const float wet = juce::jlimit (0.0f, 0.70f, color * (mode == 0 ? 0.32f : mode == 1 ? 0.46f : 0.42f));

        const float satL = softClip (L, drive);
        const float satR = softClip (R, drive);
        L = juce::jmap (wet, L, satL);
        R = juce::jmap (wet, R, satR);

        // Very subtle asymmetric body for even-harmonic flavour.
        const float body = color * (mode == 2 ? 0.025f : 0.018f);
        L += body * (L * L) * (L >= 0.0f ? 1.0f : -0.35f);
        R += body * (R * R) * (R >= 0.0f ? 1.0f : -0.35f);

        l[i] = L;
        r[i] = R;
        juce::ignoreUnused (mono);
    }

    // GLUE compressor. Timing family is selectable, depth is intentionally bounded.
    const float ratio = 1.15f + glue * (mode == 1 ? 1.35f : 1.0f);
    float attack = compMode == 0 ? 35.0f : compMode == 2 ? 5.0f : (mode == 1 ? 12.0f : 22.0f);
    float release = compMode == 0 ? 260.0f : compMode == 2 ? 75.0f : (mode == 2 ? 180.0f : 120.0f);
    const float threshold = -2.0f - glue * (mode == 1 ? 9.0f : 7.0f);

    glueCompressor.setRatio (ratio);
    glueCompressor.setAttack (attack);
    glueCompressor.setRelease (release);
    glueCompressor.setThreshold (threshold);

    const float beforeCompPeak = buffer.getMagnitude (0, n);
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    glueCompressor.process (context);
    const float afterCompPeak = buffer.getMagnitude (0, n);
    glueGR.store (juce::jmax (0.0f, gainToDb (beforeCompPeak + 1.0e-9f) - gainToDb (afterCompPeak + 1.0e-9f)));

    // DENSITY + adaptive harshness + SHADOW.
    double outSqPreClip = 0.0;
    float corrNum = 0.0f, corrDenL = 0.0f, corrDenR = 0.0f;
    float maxPre = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float L = l[i];
        float R = r[i];

        // Density reacts more when fast peaks exceed the slower body.
        const float transientExcess = juce::jlimit (0.0f, 1.0f, (fastEnv - mediumEnv) * 5.0f);
        const float densDrive = 1.0f + density * (0.55f + transientExcess * (mode == 1 ? 1.25f : 0.75f));
        const float densWet = density * (mode == 0 ? 0.20f : mode == 1 ? 0.42f : 0.32f);
        L = juce::jmap (densWet, L, softClip (L, densDrive));
        R = juce::jmap (densWet, R, softClip (R, densDrive));

        // Approximate harshness detector: extract upper-mid/high component and tame only on excess.
        const float lowForHarshL = harshFilterL.lowpass (L);
        const float lowForHarshR = harshFilterR.lowpass (R);
        const float highL = L - lowForHarshL;
        const float highR = R - lowForHarshR;
        const float hAmp = juce::jmax (std::abs (highL), std::abs (highR));
        harshEnv = medA * harshEnv + (1.0f - medA) * hAmp;
        const float harshExcess = juce::jlimit (0.0f, 1.0f, (harshEnv - 0.12f) * 4.0f);
        const float harshDepth = (0.025f + 0.035f * color + 0.025f * density) * (mode == 2 ? 1.25f : 1.0f);
        L -= highL * harshExcess * harshDepth;
        R -= highR * harshExcess * harshDepth;

        // SHADOW: keep low side stable; bloom upper side only when correlation allows it.
        float mid = 0.5f * (L + R);
        float side = 0.5f * (L - R);
        float sideLow = sideLowFilter.lowpass (side);
        float sideHigh = side - sideLow;
        const float widthCeiling = stereoMode == 0 ? 0.16f : 0.30f;
        const float adaptiveWidth = shadow * widthCeiling * (mode == 2 ? 1.20f : 1.0f);
        side = sideLow + sideHigh * (1.0f + adaptiveWidth);
        mid *= 1.0f + shadow * 0.018f;
        L = mid + side;
        R = mid - side;

        l[i] = L;
        r[i] = R;
        maxPre = juce::jmax (maxPre, juce::jmax (std::abs (L), std::abs (R)));
        outSqPreClip += 0.5 * ((double) L * L + (double) R * R);
        corrNum += L * R;
        corrDenL += L * L;
        corrDenR += R * R;
    }

    // Final soft peak shaving. 4x-style oversampling is intentionally omitted from this first build
    // to keep the project dependency-free and stable; the transfer remains bounded and conservative.
    const float clipBase = clipMode == 0 ? 1.10f : clipMode == 1 ? 1.35f : 1.75f;
    const float clipDrive = 1.0f + density * clipBase;
    const float clipWet = density * (clipMode == 0 ? 0.18f : clipMode == 1 ? 0.30f : 0.42f);
    float maxPost = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float L = l[i];
        float R = r[i];
        L = juce::jmap (clipWet, L, softClip (L, clipDrive));
        R = juce::jmap (clipWet, R, softClip (R, clipDrive));
        l[i] = L;
        r[i] = R;
        maxPost = juce::jmax (maxPost, juce::jmax (std::abs (L), std::abs (R)));
    }

    peakShave.store (juce::jmax (0.0f, gainToDb (maxPre + 1.0e-9f) - gainToDb (maxPost + 1.0e-9f)));

    const float inRms = std::sqrt ((float) (inSq / juce::jmax (1, n)));
    const float preClipRms = std::sqrt ((float) (outSqPreClip / juce::jmax (1, n)));
    const float inCrest = gainToDb ((inPeak + 1.0e-9f) / (inRms + 1.0e-9f));
    const float outCrest = gainToDb ((maxPre + 1.0e-9f) / (preClipRms + 1.0e-9f));
    crestDelta.store (inCrest - outCrest);

    const float denom = std::sqrt (juce::jmax (1.0e-12f, corrDenL * corrDenR));
    correlation.store (juce::jlimit (-1.0f, 1.0f, corrNum / denom));

    // Slow RMS level matching for honest A/B.
    if (levelMatch)
    {
        double wetSq = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* p = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                wetSq += (double) p[i] * p[i];
        }
        const float wetRms = std::sqrt ((float) (wetSq / juce::jmax (1, n * 2)));
        const float dryRms = juce::jmax (1.0e-6f, inRms);
        const float target = juce::jlimit (0.25f, 4.0f, dryRms / juce::jmax (1.0e-6f, wetRms));
        levelMatchGain.setTargetValue (target);
    }
    else
    {
        levelMatchGain.setTargetValue (1.0f);
    }

    for (int i = 0; i < n; ++i)
    {
        const float g = levelMatchGain.getNextValue();
        l[i] *= g;
        r[i] *= g;
    }

    buffer.applyGain (dbToGain (outputDb));

    if (delta)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* original = dry.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                wet[i] -= original[i];
        }
    }
}

juce::AudioProcessorEditor* SenviGlassAudioProcessor::createEditor()
{
    return new SenviGlassAudioProcessorEditor (*this);
}

void SenviGlassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SenviGlassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SenviGlassAudioProcessor();
}
