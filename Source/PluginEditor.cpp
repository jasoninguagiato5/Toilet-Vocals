#include "PluginEditor.h"

namespace
{
const juce::Colour kPorcelain (0xfff4f6f5), kTile (0xffdfe6e8), kGrout (0xffc9d3d6), kInk (0xff22313a),
                   kWater (0xff3c8dbc), kChrome (0xff9aa7ad);

juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h).withStyle (bold ? "Bold" : "Regular"));
}
constexpr int kW = 720, kH = 480, kHeader = 64, kColW = 170, kPad = 20;
}

// ------------------------------------------------------------ look and feel
PorcelainLookAndFeel::PorcelainLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kInk);
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kChrome);
    setColour (juce::ComboBox::arrowColourId, kInk);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kWater);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, kInk);
    setColour (juce::ToggleButton::tickColourId, kWater);
    setColour (juce::ToggleButton::tickDisabledColourId, kChrome);
    setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, kInk);
}

void PorcelainLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                             float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cx = (float) x + (float) width * 0.5f;
    const auto rail = juce::Rectangle<float> (cx - 3.0f, (float) y, 6.0f, (float) height);
    g.setColour (kInk.withAlpha (0.75f));
    g.fillRoundedRectangle (rail, 3.0f);
    g.setColour (kWater.withAlpha (s.isEnabled() ? 0.9f : 0.3f)); // filled up to the cap
    g.fillRoundedRectangle (rail.withTop (sliderPos), 3.0f);

    // tick marks
    g.setColour (kChrome);
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = (float) y + (float) height * (float) i / 10.0f;
        g.fillRect (cx - 22.0f, ty - 0.5f, 10.0f, 1.0f);
        g.fillRect (cx + 12.0f, ty - 0.5f, 10.0f, 1.0f);
    }

    // porcelain cap
    const auto cap = juce::Rectangle<float> (cx - 19.0f, sliderPos - 13.0f, 38.0f, 26.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cap.getX(), cap.getY(), juce::Colour (0xffd5dee2), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 6.0f);
    g.setColour (kChrome);
    g.drawRoundedRectangle (cap, 6.0f, 1.0f);
    g.setColour (kWater);
    g.fillRect (cap.getX() + 5.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 10.0f, 2.0f);
}

// -------------------------------------------------------------------- meter
void BarMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (kInk.withAlpha (0.85f));
    g.fillRoundedRectangle (b, 3.0f);
    auto yFor = [&] (float v) {
        const float t = juce::jlimit (0.0f, 1.0f, (v - minV) / (maxV - minV));
        return fromTop ? b.getY() + t * b.getHeight() : b.getBottom() - t * b.getHeight();
    };
    if (hasZone)
    {
        g.setColour (juce::Colour (0xff58c27d).withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (b.getX(), yFor (zoneHi), b.getRight(), yFor (zoneLo)));
    }
    const float yv = yFor (value);
    g.setColour (colour);
    if (fromTop) g.fillRoundedRectangle (b.withBottom (yv).reduced (2.0f, 0.0f), 2.0f);
    else g.fillRoundedRectangle (b.withTop (yv).reduced (2.0f, 0.0f), 2.0f);
}

// ------------------------------------------------------------------- editor
ToiletVocalsEditor::ToiletVocalsEditor (ToiletVocalsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setupStrip (input, ToiletVocalsProcessor::ID::input, "INPUT");
    setupStrip (comp1, ToiletVocalsProcessor::ID::c1Amount, "COMP 1");
    setupStrip (comp2, ToiletVocalsProcessor::ID::c2Amount, "COMP 2");
    setupStrip (output, ToiletVocalsProcessor::ID::output, "OUTPUT");
    input.fader.setDoubleClickReturnValue (true, 0.0);
    output.fader.setDoubleClickReturnValue (true, 0.0);
    setupComp (extras1, ToiletVocalsProcessor::ID::c1Model, ToiletVocalsProcessor::ID::c1On);
    setupComp (extras2, ToiletVocalsProcessor::ID::c2Model, ToiletVocalsProcessor::ID::c2On);

    inMeter.setZone (-21.0f, -15.0f); // where the compressors expect the vocal to sit
    for (auto* m : { &inMeter, &gr1Meter, &gr2Meter, &outMeter }) addAndMakeVisible (*m);

    relearn.onClick = [this] { proc.relearn(); };
    relearn.setTooltip ("Forget the measured vocal level and listen again. Play the loudest section while it listens.");
    addAndMakeVisible (relearn);
    levelStatus.setFont (uiFont (11.5f));
    levelStatus.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (levelStatus);

    setSize (kW, kH);
    startTimerHz (30);
}

ToiletVocalsEditor::~ToiletVocalsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ToiletVocalsEditor::setupStrip (Strip& s, const char* paramId, const juce::String& name)
{
    s.fader.setSliderStyle (juce::Slider::LinearVertical);
    s.fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (s.fader);
    s.attach = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, s.fader);
    s.title.setText (name, juce::dontSendNotification);
    s.title.setFont (uiFont (14.0f, true));
    s.title.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (s.title);
}

void ToiletVocalsEditor::setupComp (CompExtras& c, const char* modelId, const char* onId)
{
    for (int i = 0; i < tvox::kNumModels; ++i) c.model.addItem (tvox::modelInfo (i).name, i + 1);
    addAndMakeVisible (c.model);
    c.modelAttach = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, modelId, c.model);
    addAndMakeVisible (c.on);
    c.onAttach = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, onId, c.on);
    c.blurb.setFont (uiFont (11.0f));
    c.blurb.setJustificationType (juce::Justification::centredTop);
    c.blurb.setColour (juce::Label::textColourId, kInk.withAlpha (0.7f));
    addAndMakeVisible (c.blurb);
}

void ToiletVocalsEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPorcelain);
    // tiled wall
    g.setColour (kTile);
    g.fillRect (0, kHeader, getWidth(), getHeight() - kHeader);
    g.setColour (kGrout);
    for (int x = 0; x < getWidth(); x += 40) g.drawVerticalLine (x, (float) kHeader, (float) getHeight());
    for (int y = kHeader; y < getHeight(); y += 40) g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // header
    g.setColour (kInk);
    g.setFont (uiFont (28.0f, true));
    g.drawText ("TOILET VOCALS", kPad, 8, 400, 34, juce::Justification::centredLeft);
    g.setFont (uiFont (12.0f));
    g.drawText ("Porcelain Audio", kPad + 2, 40, 300, 16, juce::Justification::centredLeft);
    g.drawText ("v" TOILET_VOCALS_VERSION, getWidth() - 120 - kPad, 24, 120, 16, juce::Justification::centredRight);
    g.setColour (kWater);
    g.fillRect (0, kHeader - 3, getWidth(), 3);

    // a porcelain panel behind each strip
    for (int i = 0; i < 4; ++i)
    {
        const auto panel = juce::Rectangle<float> ((float) (kPad + i * kColW), (float) (kHeader + 14), (float) (kColW - 10), (float) (getHeight() - kHeader - 28));
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (panel.translated (0.0f, 3.0f), 12.0f);
        g.setColour (kPorcelain);
        g.fillRoundedRectangle (panel, 12.0f);
        g.setColour (kChrome.withAlpha (0.6f));
        g.drawRoundedRectangle (panel, 12.0f, 1.0f);
    }

    g.setColour (kInk.withAlpha (0.6f));
    g.setFont (uiFont (10.0f));
    g.drawText ("GR", gr1Meter.getX() - 8, gr1Meter.getBottom() + 2, gr1Meter.getWidth() + 16, 12, juce::Justification::centred);
    g.drawText ("GR", gr2Meter.getX() - 8, gr2Meter.getBottom() + 2, gr2Meter.getWidth() + 16, 12, juce::Justification::centred);
}

void ToiletVocalsEditor::resized()
{
    Strip* strips[4] = { &input, &comp1, &comp2, &output };
    BarMeter* meters[4] = { &inMeter, &gr1Meter, &gr2Meter, &outMeter };
    CompExtras* extras[4] = { nullptr, &extras1, &extras2, nullptr };

    for (int i = 0; i < 4; ++i)
    {
        auto col = juce::Rectangle<int> (kPad + i * kColW, kHeader + 14, kColW - 10, getHeight() - kHeader - 28).reduced (10, 8);
        strips[i]->title.setBounds (col.removeFromTop (22));
        auto top = col.removeFromTop (64);    // model picker + blurb, or the level controls
        auto bottom = col.removeFromBottom (26);
        if (auto* e = extras[i])
        {
            e->model.setBounds (top.removeFromTop (24));
            e->blurb.setBounds (top.reduced (0, 4));
            e->on.setBounds (bottom.withSizeKeepingCentre (56, 22));
        }
        else if (i == 0)
        {
            relearn.setBounds (top.removeFromTop (24).reduced (8, 0));
            levelStatus.setBounds (top.reduced (0, 4));
        }
        col.removeFromTop (4);
        auto meterArea = col.removeFromRight (16);
        meters[i]->setBounds (meterArea.withTrimmedTop (12).withTrimmedBottom (34));
        strips[i]->fader.setBounds (col);
    }
}

void ToiletVocalsEditor::timerCallback()
{
    inMeter.setValue (proc.uiInputDb.load());

    // peak-hold meters: fast up, eased back
    auto ease = [] (float shown, float target, float fall) { return target > shown ? target : juce::jmax (target, shown - fall); };
    gr1Shown = ease (gr1Shown, proc.uiGr1.exchange (0.0f), 0.6f);
    gr2Shown = ease (gr2Shown, proc.uiGr2.exchange (0.0f), 0.6f);
    outShown = ease (outShown, juce::Decibels::gainToDecibels (proc.uiOutPeak.exchange (0.0f), -100.0f), 1.2f);
    gr1Meter.setValue (gr1Shown);
    gr2Meter.setValue (gr2Shown);
    outMeter.setValue (outShown);

    const float gain = proc.uiAutoGainDb.load();
    const auto gainText = (gain > 0.0f ? "+" : "") + juce::String (gain, 1) + " dB";
    levelStatus.setText (proc.uiLearned.load() ? "Level set: " + gainText
                                               : "Listening... " + juce::String (juce::roundToInt (proc.uiLearnProgress.load() * 100.0f)) + " %",
                         juce::dontSendNotification);

    extras1.blurb.setText (tvox::modelInfo (extras1.model.getSelectedItemIndex()).blurb, juce::dontSendNotification);
    extras2.blurb.setText (tvox::modelInfo (extras2.model.getSelectedItemIndex()).blurb, juce::dontSendNotification);
    comp1.fader.setEnabled (extras1.on.getToggleState());
    comp2.fader.setEnabled (extras2.on.getToggleState());
}
