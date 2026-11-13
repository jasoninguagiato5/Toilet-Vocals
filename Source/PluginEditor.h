#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Porcelain fader caps on chrome rails.
class PorcelainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PorcelainLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

// Vertical bar meter. `fromTop` meters (gain reduction) grow downwards.
class BarMeter : public juce::Component
{
public:
    BarMeter (float lo, float hi, bool fromTopIn, juce::Colour c) : minV (lo), maxV (hi), fromTop (fromTopIn), colour (c) {}
    void setValue (float v) { if (std::abs (v - value) > 0.02f) { value = v; repaint(); } }
    void setZone (float lo, float hi) { zoneLo = lo; zoneHi = hi; hasZone = true; }
    void paint (juce::Graphics&) override;

private:
    float minV, maxV, value = 0.0f, zoneLo = 0.0f, zoneHi = 0.0f;
    bool fromTop, hasZone = false;
    juce::Colour colour;
};

class ToiletVocalsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ToiletVocalsEditor (ToiletVocalsProcessor&);
    ~ToiletVocalsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Strip
    {
        juce::Slider fader;
        juce::Label title;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };
    struct CompExtras
    {
        juce::ComboBox model;
        juce::ToggleButton on { "In" };
        juce::Label blurb;
        std::unique_ptr<APVTS::ComboBoxAttachment> modelAttach;
        std::unique_ptr<APVTS::ButtonAttachment> onAttach;
    };

    void timerCallback() override;
    void setupStrip (Strip&, const char* paramId, const juce::String& name);
    void setupComp (CompExtras&, const char* modelId, const char* onId);

    ToiletVocalsProcessor& proc;
    PorcelainLookAndFeel lnf;
    Strip input, comp1, comp2, output;
    CompExtras extras1, extras2;
    BarMeter inMeter { -42.0f, 0.0f, false, juce::Colour (0xff3c8dbc) };
    BarMeter gr1Meter { 0.0f, 24.0f, true, juce::Colour (0xffd9822b) };
    BarMeter gr2Meter { 0.0f, 24.0f, true, juce::Colour (0xffd9822b) };
    BarMeter outMeter { -42.0f, 0.0f, false, juce::Colour (0xff3c8dbc) };
    juce::TextButton relearn { "Relearn level" };
    juce::Label levelStatus;
    juce::TooltipWindow tooltips { this };
    float gr1Shown = 0.0f, gr2Shown = 0.0f, outShown = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToiletVocalsEditor)
};
