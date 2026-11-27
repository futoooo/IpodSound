#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// Egy "hangzás" beállításai
struct PodMode
{
    const char* name;
    float hp, lp;  bool steep;          // felüláteresztő, aluláteresztő (steep = 24 dB/okt)
    float peakF, peakQ, peakDb;         // középsáv kiemelés/vágás
    float lowF, lowDb;                  // mély polc
    float highF, highDb;                // magas polc
    float width;                        // 0 = mono, 1 = eredeti sztereó
    float drive;                        // telítés (1 = nincs)
    float reverb;                       // visszhang mennyisége (0 = nincs)
    float gainDb;                       // kimeneti szint
};

inline const std::array<PodMode, 7> podModes {{
    //  name             hp    lp     steep  peakF  Q    dB    lowF  dB   highF  dB   width drive rev   gain
    { "Clean",           20,   20000, false, 1000,  1.0f, 0,   100,  0,   8000,  0,   1.0f, 1.0f, 0.0f,  0 },
    { "Earbuds",         110,  14000, false, 3500,  1.0f, 3,   100, -3,   9000,  2,   0.75f,1.3f, 0.0f,  0 },
    { "Next Room",       60,   450,   true,  180,   0.8f, 4,   100,  0,   8000,  0,   0.3f, 1.0f, 0.25f,-3 },
    { "Car",             30,   9000,  false, 2500,  0.7f,-2,   90,   6,   6000, -3,   0.85f,1.2f, 0.0f, -2 },
    { "Phone Speaker",   450,  7000,  true,  1800,  1.5f, 5,   100,  0,   8000,  0,   0.0f, 2.0f, 0.0f,  2 },
    { "Old Radio",       300,  3500,  true,  1000,  2.0f, 4,   100,  0,   8000,  0,   0.0f, 2.5f, 0.0f,  3 },
    { "Club Outside",    40,   250,   true,  80,    0.8f, 5,   60,   0,   8000,  0,   0.5f, 1.0f, 0.15f,-2 },
}};

class PodProcessor : public juce::AudioProcessor
{
public:
    PodProcessor();
    ~PodProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "iPod Sound"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (int modeIndex);

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using Biquad = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, Coeffs>;

    juce::dsp::StateVariableTPTFilter<float> hp, lpA, lpB;
    Biquad lowShelf, peak, highShelf;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> dry;

    juce::AudioParameterChoice* modeParam = nullptr;
    juce::AudioParameterFloat*  mixParam = nullptr;
    juce::AudioParameterBool*   bypassParam = nullptr;

    double sr = 44100.0;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PodProcessor)
};
