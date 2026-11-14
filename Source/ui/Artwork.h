// All drawing for the Toilet Vocals faceplate. Depends on juce_graphics only,
// so tools/preview.cpp can render the interface to a PNG without a plugin host.
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace art
{
// ---- layout (design pixels)
constexpr int kW = 1120, kH = 692;
constexpr int kHeaderB = 147, kBandT = 149, kBandB = 171, kStripT = 173, kStripB = 548, kFooterT = 552, kFooterB = 674;
constexpr int kInputW = 190, kOutputW = 180, kNumMid = 8;
constexpr int kMidL = kInputW, kMidR = kW - kOutputW;
constexpr int kSlotT = 268, kSlotB = 502;       // fader slot
constexpr int kCapW = 38, kCapH = 52;           // fader cap; its centre travels kTravelT..kTravelB
constexpr int kTravelT = kSlotT + kCapH / 2, kTravelB = kSlotB - kCapH / 2;
constexpr int kLabelY = 514, kLabelH = 24;
constexpr int kMeterT = 272, kMeterB = 486, kMeterW = 50;
constexpr float kInputFaderX = 66.0f, kInputMeterX = 112.0f;
constexpr float kOutputMeterX = (float) kMidR + 22.0f, kOutputFaderX = (float) kW - 64.0f;
constexpr float kTopDotY = 254.0f;              // the little round fitting above every slot

inline float midW() { return (float) (kMidR - kMidL) / (float) kNumMid; }
inline float midLeft (int i) { return (float) kMidL + midW() * (float) i; }
inline float midX (int i) { return midLeft (i) + midW() * 0.5f; }
inline float faderY (float norm) { return (float) kTravelB - norm * (float) (kTravelB - kTravelT); }

struct Strip
{
    const char* name;
    const char* modes[4];
    int numModes;
    bool active;
};
const Strip& strip (int i);                      // the eight middle strips, left to right
constexpr int kComp1 = 2, kComp2 = 3;            // which of them are the compressors
juce::Rectangle<float> modeArea (int i);         // where strip i's mode lights sit
float modeRowY (int numModes, int row);          // centre y of one mode row

// ---- drawing
juce::Font labelFont (float height);
void drawBackground (juce::Graphics&);                                          // everything that never moves
void drawCap (juce::Graphics&, float cx, float cy, bool enabled);               // fader cap
void drawModes (juce::Graphics&, int stripIndex, int selected, bool enabled);   // mode lights + names
void drawLed (juce::Graphics&, float cx, float cy, float r, bool on, juce::Colour onColour);
void drawTopButton (juce::Graphics&, float cx, float cy, bool lit, bool down);  // a strip's In button
void drawRoundButton (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool down);
void drawLevelMeter (juce::Graphics&, juce::Rectangle<float>, float db, bool markTarget); // 0 .. -27 dB
void drawGrMeter (juce::Graphics&, juce::Rectangle<float>, float grDb);                  // 0 .. 24 dB, grows downwards

const juce::Colour kAmber (0xffffb428), kGreen (0xff58d64a), kInk (0xff1f1d1a);
}
