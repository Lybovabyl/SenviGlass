#pragma once

#include <JuceHeader.h>

class SenviGlassAudioProcessor final : public juce::AudioProcessor
{
public:
    SenviGlassAudioProcessor();
    ~SenviGlassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;

    float getGlueGR() const noexcept      { return glueGR.load(); }
    float getPeakShave() const noexcept   { return peakShave.load(); }
    float getCrestDelta() const noexcept  { return crestDelta.load(); }
    float getCorrelation() const noexcept { return correlation.load(); }

private:
    static APVTS::ParameterLayout createParameterLayout();

    struct OnePole
    {
        void prepare (double sr) { sampleRate = sr; z = 0.0f; }
        void setCutoff (float hz)
        {
            const auto x = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
            a = x;
        }
        float lowpass (float x)
        {
            z = (1.0f - a) * x + a * z;
            return z;
        }
        double sampleRate = 44100.0;
        float a = 0.0f, z = 0.0f;
    };

    double currentSampleRate = 44100.0;
    std::array<OnePole, 2> lowBandFilters;
    OnePole harshFilterL, harshFilterR;
    OnePole sideLowFilter;

    juce::dsp::Compressor<float> glueCompressor;
    juce::LinearSmoothedValue<float> levelMatchGain;

    float fastEnv = 0.0f;
    float mediumEnv = 0.0f;
    float slowEnv = 0.0f;
    float lowEnv = 0.0f;
    float harshEnv = 0.0f;

    std::atomic<float> glueGR { 0.0f };
    std::atomic<float> peakShave { 0.0f };
    std::atomic<float> crestDelta { 0.0f };
    std::atomic<float> correlation { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SenviGlassAudioProcessor)
};
