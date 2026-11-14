#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Artwork.h"

// Draws a fader as just its porcelain cap; the slot and scale are part of the faceplate.
class FaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FaderLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return art::kCapH / 2; }
};

// The column of lights above a compressor fader: click a name to pick that model.
class ModelSelector : public juce::Component, public juce::TooltipClient
{
public:
    ModelSelector (int stripIndex, juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override { hoverRow = rowAt (e.y); }
    juce::String getTooltip() override;
    void setLit (bool shouldBeLit) { if (lit != shouldBeLit) { lit = shouldBeLit; repaint(); } }

private:
    int rowAt (int y) const;
    int strip, selected = 0, hoverRow = 0;
    bool lit = true;
    juce::ParameterAttachment attach;
};

// The small round In button above a compressor's slot.
class InButton : public juce::Button
{
public:
    InButton() : juce::Button ("In") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool, bool down) override
    {
        art::drawTopButton (g, (float) getWidth() * 0.5f, (float) getHeight() * 0.5f, getToggleState(), down);
    }
};

class RoundButton : public juce::Button
{
public:
    RoundButton() : juce::Button ("Relearn") {}
    void paintButton (juce::Graphics& g, bool, bool down) override
    {
        art::drawRoundButton (g, getLocalBounds().toFloat(), juce::Colour (0xffc22a1e), down);
    }
};

class MeterView : public juce::Component
{
public:
    enum class Kind { level, levelWithTarget, gainReduction };
    explicit MeterView (Kind k) : kind (k) { setInterceptsMouseClicks (false, false); }
    void setValue (float v) { if (std::abs (v - value) > 0.05f) { value = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        if (kind == Kind::gainReduction) art::drawGrMeter (g, getLocalBounds().toFloat(), value);
        else art::drawLevelMeter (g, getLocalBounds().toFloat(), value, kind == Kind::levelWithTarget);
    }

private:
    Kind kind;
    float value = -100.0f;
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
    struct Fader
    {
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };
    struct Comp
    {
        Comp (int stripIndex, juce::RangedAudioParameter& model) : selector (stripIndex, model) {}
        ModelSelector selector;
        InButton in;
        MeterView gr { MeterView::Kind::gainReduction };
        std::unique_ptr<APVTS::ButtonAttachment> inAttach;
        float shown = 0.0f;
    };

    void timerCallback() override;
    void setupFader (Fader&, const char* paramId, const juce::String& tip);
    void setupComp (Comp&, const char* onId);

    ToiletVocalsProcessor& proc;
    FaderLookAndFeel lnf;
    juce::Image faceplate;
    Fader input, amount1, amount2, output;
    Comp comp1, comp2;
    MeterView inMeter { MeterView::Kind::levelWithTarget }, outMeter { MeterView::Kind::level };
    RoundButton relearn;
    juce::Label levelStatus;
    juce::TooltipWindow tooltips { this, 600 };
    float outShown = -100.0f;
    bool learnedShown = false, ledOn = false;
    int blink = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToiletVocalsEditor)
};
