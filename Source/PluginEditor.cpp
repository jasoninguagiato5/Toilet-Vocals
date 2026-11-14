#include "PluginEditor.h"

using PID = ToiletVocalsProcessor::ID;

// ------------------------------------------------------------ look and feel
FaderLookAndFeel::FaderLookAndFeel()
{
    const juce::Colour cream (0xfff6f1e6);
    setColour (juce::BubbleComponent::backgroundColourId, cream);
    setColour (juce::BubbleComponent::outlineColourId, art::kInk.withAlpha (0.5f));
    setColour (juce::TooltipWindow::backgroundColourId, cream);
    setColour (juce::TooltipWindow::textColourId, art::kInk);
    setColour (juce::TooltipWindow::outlineColourId, art::kInk.withAlpha (0.5f));
    setColour (juce::Label::textColourId, art::kInk);
}

void FaderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int, int width, int, float sliderPos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s)
{
    art::drawCap (g, (float) x + (float) width * 0.5f, sliderPos, s.isEnabled());
}

// ----------------------------------------------------------- model selector
ModelSelector::ModelSelector (int stripIndex, juce::RangedAudioParameter& p)
    : strip (stripIndex),
      attach (p, [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attach.sendInitialUpdate();
}

void ModelSelector::paint (juce::Graphics& g)
{
    g.setOrigin (-getX(), -getY()); // the artwork draws in faceplate coordinates
    art::drawModes (g, strip, selected, lit);
}

int ModelSelector::rowAt (int y) const
{
    const int n = art::strip (strip).numModes;
    const float first = art::modeRowY (n, 0), step = art::modeRowY (n, 1) - first;
    return juce::jlimit (0, n - 1, (int) std::floor (((float) (y + getY()) - first) / step + 0.5f));
}

void ModelSelector::mouseDown (const juce::MouseEvent& e) { attach.setValueAsCompleteGesture ((float) rowAt (e.y)); }

juce::String ModelSelector::getTooltip() { return tvox::modelInfo (hoverRow).blurb; }

// ------------------------------------------------------------------- editor
ToiletVocalsEditor::ToiletVocalsEditor (ToiletVocalsProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      comp1 (art::kComp1, *p.apvts.getParameter (PID::c1Model)),
      comp2 (art::kComp2, *p.apvts.getParameter (PID::c2Model))
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    // The faceplate never changes, so it is drawn once (at 2x for sharp displays).
    faceplate = juce::Image (juce::Image::RGB, art::kW * 2, art::kH * 2, false);
    {
        juce::Graphics g (faceplate);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        art::drawBackground (g);
    }

    setupFader (input, PID::input, "Input: pushes the vocal harder into the chain, or backs it off. The level is set automatically; this is an offset on top.");
    setupFader (amount1, PID::c1Amount, "Compressor 1: from its gentlest setting at the bottom to its most extreme at the top.");
    setupFader (amount2, PID::c2Amount, "Compressor 2: from its gentlest setting at the bottom to its most extreme at the top.");
    setupFader (output, PID::output, "Output level.");
    setupComp (comp1, PID::c1On);
    setupComp (comp2, PID::c2On);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    relearn.onClick = [this] { proc.relearn(); };
    relearn.setTooltip ("Measure the vocal level again. Press it, then play the loudest section.");
    addAndMakeVisible (relearn);

    levelStatus.setFont (art::labelFont (12.0f));
    levelStatus.setJustificationType (juce::Justification::centred);
    levelStatus.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (levelStatus);

    setSize (art::kW, art::kH);
    startTimerHz (30);
}

ToiletVocalsEditor::~ToiletVocalsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ToiletVocalsEditor::setupFader (Fader& f, const char* paramId, const juce::String& tip)
{
    f.slider.setSliderStyle (juce::Slider::LinearVertical);
    f.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    f.slider.setSliderSnapsToMousePosition (false);
    f.slider.setPopupDisplayEnabled (true, false, this);
    f.slider.setTooltip (tip);
    addAndMakeVisible (f.slider);
    f.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, f.slider);
    if (auto* param = proc.apvts.getParameter (paramId))
        f.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void ToiletVocalsEditor::setupComp (Comp& c, const char* onId)
{
    addAndMakeVisible (c.selector);
    addAndMakeVisible (c.gr);
    c.in.setTooltip ("Compressor in / out.");
    addAndMakeVisible (c.in);
    c.inAttach = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, onId, c.in);
}

void ToiletVocalsEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (faceplate, getLocalBounds().toFloat());
    art::drawLed (g, art::kInputFaderX, art::kTopDotY, 5.5f, ledOn, learnedShown ? art::kGreen : art::kAmber);
    g.setColour (art::kInk.withAlpha (0.55f));
    g.setFont (art::labelFont (11.0f));
    g.drawText ("v" TOILET_VOCALS_VERSION, art::kW / 2 - 60, art::kFooterB - 19, 120, 12, juce::Justification::centred);
}

void ToiletVocalsEditor::resized()
{
    auto faderBounds = [] (float cx) { return juce::Rectangle<int> ((int) cx - 22, art::kSlotT, 44, art::kSlotB - art::kSlotT); };
    input.slider.setBounds (faderBounds (art::kInputFaderX));
    amount1.slider.setBounds (faderBounds (art::midX (art::kComp1)));
    amount2.slider.setBounds (faderBounds (art::midX (art::kComp2)));
    output.slider.setBounds (faderBounds (art::kOutputFaderX));

    Comp* comps[2] = { &comp1, &comp2 };
    const int strips[2] = { art::kComp1, art::kComp2 };
    for (int i = 0; i < 2; ++i)
    {
        const float cx = art::midX (strips[i]);
        comps[i]->selector.setBounds (art::modeArea (strips[i]).toNearestInt());
        comps[i]->in.setBounds (juce::Rectangle<float> (cx - 11.0f, art::kTopDotY - 11.0f, 22.0f, 22.0f).toNearestInt());
        comps[i]->gr.setBounds (juce::Rectangle<float> (cx + 24.0f, (float) art::kTravelT - 6.0f, 10.0f, (float) (art::kTravelB - art::kTravelT) + 8.0f).toNearestInt());
    }

    inMeter.setBounds ((int) art::kInputMeterX, art::kMeterT, art::kMeterW, art::kMeterB - art::kMeterT);
    outMeter.setBounds ((int) art::kOutputMeterX, art::kMeterT, art::kMeterW, art::kMeterB - art::kMeterT);
    relearn.setBounds ((int) art::kInputMeterX + 15, 490, 20, 20);
    levelStatus.setBounds (8, 229, art::kInputW - 16, 15);
}

void ToiletVocalsEditor::timerCallback()
{
    inMeter.setValue (proc.uiInputDb.load());

    // peak-hold meters: fast up, eased back
    auto ease = [] (float shown, float target, float fall) { return target > shown ? target : juce::jmax (target, shown - fall); };
    comp1.shown = ease (comp1.shown, proc.uiGr1.exchange (0.0f), 0.6f);
    comp2.shown = ease (comp2.shown, proc.uiGr2.exchange (0.0f), 0.6f);
    outShown = ease (outShown, juce::Decibels::gainToDecibels (proc.uiOutPeak.exchange (0.0f), -100.0f), 1.2f);
    comp1.gr.setValue (comp1.in.getToggleState() ? comp1.shown : 0.0f);
    comp2.gr.setValue (comp2.in.getToggleState() ? comp2.shown : 0.0f);
    outMeter.setValue (outShown);

    // input level: steady green once set, blinking amber while it listens
    const bool learned = proc.uiLearned.load();
    blink = (blink + 1) % 20;
    const bool led = learned || blink < 10;
    if (led != ledOn || learned != learnedShown)
    {
        ledOn = led; learnedShown = learned;
        repaint (juce::Rectangle<float> (art::kInputFaderX - 16.0f, art::kTopDotY - 16.0f, 32.0f, 32.0f).toNearestInt());
    }
    const float gain = proc.uiAutoGainDb.load();
    levelStatus.setText (learned ? "LEVEL SET  " + juce::String (gain > 0.0f ? "+" : "") + juce::String (gain, 1) + " dB"
                                 : "LISTENING  " + juce::String (juce::roundToInt (proc.uiLearnProgress.load() * 100.0f)) + "%",
                         juce::dontSendNotification);

    for (auto* c : { &comp1, &comp2 })
        c->selector.setLit (c->in.getToggleState());
    amount1.slider.setEnabled (comp1.in.getToggleState());
    amount2.slider.setEnabled (comp2.in.getToggleState());
}
