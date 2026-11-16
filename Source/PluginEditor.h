#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace Look
{
    inline const juce::Colour bg      { 0xff1f1f23 };   // very dark grey (not black)
    inline const juce::Colour panel   { 0xff26262b };
    inline const juce::Colour purple  { 0xff9b27ff };   // vibrant dark purple
    inline const juce::Colour black   { 0xff09090b };
    inline float time = 0.0f;                           // animation clock
}

class MeltingKnobLAF : public juce::LookAndFeel_V4
{
public:
    MeltingKnobLAF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class VioletTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VioletTuneEditor (VioletTuneProcessor&);
    ~VioletTuneEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { Look::time += 1.0f / 60.0f; repaint(); }
    void paintHeat (juce::Graphics&, juce::Rectangle<int>, bool input);

    VioletTuneProcessor& proc;
    MeltingKnobLAF laf;
    juce::Slider speed, amount, sustain, stab, dbl;
    juce::Label speedL, amountL, sustainL, stabL, dblL, keyL, scaleL;
    juce::ComboBox key, scale;
    juce::Rectangle<int> heatIn, heatOut;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> speedA, amountA, sustainA, stabA, dblA;
    std::unique_ptr<CA> keyA, scaleA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VioletTuneEditor)
};
