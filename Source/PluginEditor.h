#pragma once
#include "PluginProcessor.h"

class PodEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PodEditor (PodProcessor&);
    ~PodEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override { repaint(); }
    int  currentMode() const;
    bool isBypassed() const;
    void stepMode (int dir);
    void toggleBypass();
    float angleOf (juce::Point<float> p) const;

    PodProcessor& proc;
    juce::Slider mixSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttachment;

    juce::Rectangle<float> screenR;
    juce::Point<float> wheelC;
    const float outerR = 115.0f;
    const float innerR = 44.0f;

    enum class Drag { none, ring, centre };
    Drag drag = Drag::none;
    float lastAngle = 0.0f, accum = 0.0f, wheelAccum = 0.0f;
    bool moved = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PodEditor)
};
