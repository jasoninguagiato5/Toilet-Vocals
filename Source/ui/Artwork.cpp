#include "Artwork.h"

namespace art
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;
using Just = juce::Justification;

const Colour kPorcelain (0xffece6da), kPorcelainHi (0xfff8f4ec), kPorcelainLo (0xffd4ccbc), kGap (0xff2b2927),
             kChromeHi (0xfff2f2f0), kChromeLo (0xff7d7f82), kCream (0xfff3ead2);

const Strip kStrips[kNumMid] = {
    { "LOW",     { "SUB", "LOWER", "UPPER", "" },            3, Kind::eq,          true },
    { "HIGH",    { "BITE", "SHEEN", "SILK", "AIR" },         4, Kind::eq,          true },
    { "COMP 1",  { "#1 76", "LA #2A", "CL #1b", "IBSsor" },  4, Kind::comp,        false },
    { "COMP 2",  { "#1 76", "LA #2A", "CL #1b", "IBSsor" },  4, Kind::comp,        false },
    { "REVERB",  { "TIGHT", "LARGE", "CHAMBER", "" },        3, Kind::placeholder, false },
    { "DELAY",   { "SLAP", "EIGHT", "QUARTER", "" },         3, Kind::placeholder, false },
    { "WIDTH",   { "WIDE", "STEREO", "SPREADER", "" },       3, Kind::placeholder, false },
    { "DIRT",    { "GRIT", "MASH", "GAS", "" },              3, Kind::placeholder, false } };

void porcelainPanel (Graphics& g, Rectangle<float> r, float radius)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), radius);
    g.setGradientFill (juce::ColourGradient (kPorcelainHi, r.getX(), r.getY(), kPorcelainLo, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    // glazed edge: bright rim on top, soft shade at the bottom
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f), r.getX(), r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + 14.0f, false));
    g.fillRoundedRectangle (r.reduced (2.0f), radius - 1.0f);
    g.setColour (Colour (0xffa59d8c));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void chromeFill (Graphics& g, Rectangle<float> r)
{
    juce::ColourGradient cg (kChromeHi, r.getX(), r.getY(), kChromeLo, r.getX(), r.getBottom(), false);
    cg.addColour (0.45, Colour (0xffc9cbcd));
    cg.addColour (0.55, Colour (0xff8e9093));
    cg.addColour (0.8, Colour (0xffd9dadb));
    g.setGradientFill (cg);
}

void screw (Graphics& g, float cx, float cy, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r, cy - r + 1.0f, 2.0f * r, 2.0f * r);
    g.setGradientFill (juce::ColourGradient (Colour (0xff6b6d70), cx - r, cy - r, Colour (0xff1c1c1d), cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
    g.setColour (Colour (0xffb9bbbd));
    g.drawEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r, 1.0f);
    g.setColour (Colour (0xff0c0c0c));
    g.fillEllipse (cx - r * 0.42f, cy - r * 0.42f, r * 0.84f, r * 0.84f);
}

void checker (Graphics& g, Rectangle<int> area, int cell)
{
    g.setColour (Colour (0xfff4f1ea));
    g.fillRect (area);
    g.setColour (Colour (0xff151413));
    for (int row = 0, y = area.getY(); y < area.getBottom(); y += cell, ++row)
        for (int col = 0, x = area.getX(); x < area.getRight(); x += cell, ++col)
            if ((row + col) % 2 == 0)
                g.fillRect (Rectangle<int> (x, y, cell, cell).getIntersection (area));
}

void slot (Graphics& g, float cx)
{
    const Rectangle<float> r (cx - 4.5f, (float) kSlotT, 9.0f, (float) (kSlotB - kSlotT));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRoundedRectangle (r.translated (1.0f, 1.0f), 4.5f);
    g.setColour (Colour (0xff111110));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (Colour (0xff3a3937));
    g.fillRoundedRectangle (r.withWidth (2.5f).translated (5.5f, 0.0f).reduced (0.0f, 4.0f), 1.2f);
    screw (g, cx, (float) kSlotB + 6.0f, 3.5f);
}

// Scale marks next to a slot. `side` is -1 for left, +1 for right.
void scale (Graphics& g, float cx, int side, std::initializer_list<std::pair<const char*, float>> marks)
{
    g.setFont (labelFont (11.5f));
    for (const auto& m : marks)
    {
        const float y = faderY (m.second);
        g.setColour (kInk.withAlpha (0.8f));
        g.fillRect (side < 0 ? cx - 22.0f : cx + 13.0f, y - 0.6f, 9.0f, 1.2f);
        if (m.first[0] != 0)
        {
            const Rectangle<float> t (side < 0 ? cx - 56.0f : cx + 25.0f, y - 8.0f, 31.0f, 16.0f);
            g.setColour (kInk);
            g.drawText (m.first, t, side < 0 ? Just::centredRight : Just::centredLeft, false);
        }
    }
}

void stripLabel (Graphics& g, const char* text, float cx, float width)
{
    g.setColour (kInk);
    g.setFont (labelFont (15.5f));
    g.drawText (text, Rectangle<float> (cx - width * 0.5f, (float) kLabelY, width, (float) kLabelH), Just::centred, false);
}

void grille (Graphics& g, Rectangle<float> r)
{
    chromeFill (g, r);
    g.fillRoundedRectangle (r, 5.0f);
    const auto in = r.reduced (3.0f);
    g.setColour (Colour (0xff2a2a2a));
    g.fillRoundedRectangle (in, 3.0f);
    g.setColour (Colour (0xff8a8c8e));
    for (float y = in.getY() + 3.0f; y < in.getBottom() - 2.0f; y += 4.0f)
        for (float x = in.getX() + 3.0f + (((int) ((y - in.getY()) / 4.0f)) % 2 ? 2.0f : 0.0f); x < in.getRight() - 2.0f; x += 4.0f)
            g.fillEllipse (x, y, 1.6f, 1.6f);
}

void jack (Graphics& g, float cx, float cy)
{
    chromeFill (g, { cx - 11.0f, cy - 11.0f, 22.0f, 22.0f });
    g.fillEllipse (cx - 11.0f, cy - 11.0f, 22.0f, 22.0f);
    g.setColour (Colour (0xff141414));
    g.fillEllipse (cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (Colour (0xff3d3d3d));
    g.drawEllipse (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f, 1.5f);
}

void toilet (Graphics& g, float x, float y)
{
    const Colour line (0xff1f1d1a);
    auto body = [&] (const Path& p) {
        g.setColour (kPorcelainHi); g.fillPath (p);
        g.setColour (line); g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    Path tank; tank.addRoundedRectangle (x + 2.0f, y + 8.0f, 46.0f, 40.0f, 5.0f); body (tank);
    Path lid; lid.addRoundedRectangle (x - 2.0f, y, 54.0f, 11.0f, 4.0f); body (lid);
    g.setColour (line); g.fillRoundedRectangle (x + 36.0f, y + 16.0f, 8.0f, 4.0f, 2.0f); // flush handle
    Path bowl;
    bowl.startNewSubPath (x + 14.0f, y + 56.0f);
    bowl.quadraticTo (x + 16.0f, y + 88.0f, x + 38.0f, y + 94.0f);
    bowl.lineTo (x + 36.0f, y + 110.0f);
    bowl.lineTo (x + 70.0f, y + 110.0f);
    bowl.lineTo (x + 68.0f, y + 94.0f);
    bowl.quadraticTo (x + 84.0f, y + 84.0f, x + 86.0f, y + 58.0f);
    bowl.closeSubPath();
    body (bowl);
    Path seat; seat.addEllipse (x + 10.0f, y + 44.0f, 80.0f, 26.0f); body (seat);
    g.setColour (Colour (0xffbdb6a6)); g.fillEllipse (x + 22.0f, y + 50.0f, 56.0f, 14.0f);
    g.setColour (line); g.drawEllipse (x + 22.0f, y + 50.0f, 56.0f, 14.0f, 2.0f);
}

void lamp (Graphics& g, float cx)
{
    // glow on the wall
    juce::ColourGradient halo (Colour (0xffffc24a).withAlpha (0.55f), cx, 66.0f, Colour (0xffffc24a).withAlpha (0.0f), cx, 66.0f + 120.0f, true);
    g.setGradientFill (halo);
    g.fillEllipse (cx - 120.0f, 66.0f - 120.0f, 240.0f, 240.0f);
    // ceiling fitting
    chromeFill (g, { cx - 46.0f, 0.0f, 92.0f, 18.0f });
    g.fillRoundedRectangle (cx - 46.0f, -6.0f, 92.0f, 24.0f, 6.0f);
    chromeFill (g, { cx - 34.0f, 16.0f, 68.0f, 10.0f });
    g.fillRoundedRectangle (cx - 34.0f, 16.0f, 68.0f, 10.0f, 3.0f);
    // glass
    const float gy = 70.0f, rx = 41.0f, ry = 48.0f;
    juce::ColourGradient glow (Colour (0xfffffbe6), cx, gy - 6.0f, Colour (0xffe8930c), cx, gy + ry + 8.0f, true);
    glow.addColour (0.5, Colour (0xffffd45e));
    g.setGradientFill (glow);
    g.fillEllipse (cx - rx, gy - ry, 2.0f * rx, 2.0f * ry);
    // cage
    g.setColour (Colour (0xffe2e3e4));
    const juce::PathStrokeType wire (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    for (float f : { -0.75f, -0.27f, 0.27f, 0.75f })
    {
        Path p; p.startNewSubPath (cx + f * 26.0f, gy - ry + 2.0f);
        p.quadraticTo (cx + f * rx * 1.75f, gy + 4.0f, cx + f * 14.0f, gy + ry + 2.0f);
        g.strokePath (p, wire);
    }
    for (float yy : { gy - 24.0f, gy + 2.0f, gy + 27.0f })
    {
        const float half = rx * std::sqrt (1.0f - (yy - gy) * (yy - gy) / (ry * ry)) + 2.0f;
        Path p; p.startNewSubPath (cx - half, yy);
        p.quadraticTo (cx, yy + 9.0f, cx + half, yy);
        g.strokePath (p, wire);
    }
    chromeFill (g, { cx - 7.0f, gy + ry - 2.0f, 14.0f, 11.0f });
    g.fillEllipse (cx - 7.0f, gy + ry - 2.0f, 14.0f, 11.0f);
}

void cleanDirty (Graphics& g, Rectangle<float> r)
{
    chromeFill (g, r);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    const auto in = r.reduced (7.0f);
    g.setColour (Colour (0xff151413));
    g.fillRoundedRectangle (in, in.getHeight() * 0.5f);
    auto left = in.withWidth (in.getWidth() * 0.5f).reduced (3.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xfffff8e2), left.getX(), left.getY(), Colour (0xffe9d9ad), left.getX(), left.getBottom(), false));
    g.fillRoundedRectangle (left, left.getHeight() * 0.5f);
    g.setFont (labelFont (21.0f).withExtraKerningFactor (0.08f));
    g.setColour (kInk);
    g.drawText ("CLEAN", left, Just::centred, false);
    g.setColour (kCream);
    g.drawText ("DIRTY", in.withTrimmedLeft (in.getWidth() * 0.5f), Just::centred, false);
}

void vent (Graphics& g, float cx, float cy, float r)
{
    chromeFill (g, { cx - r, cy - r, 2.0f * r, 2.0f * r });
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
    const float ri = r - 7.0f;
    Path hole; hole.addEllipse (cx - ri, cy - ri, 2.0f * ri, 2.0f * ri);
    g.setColour (Colour (0xff171615));
    g.fillPath (hole);
    g.saveState();
    g.reduceClipRegion (hole);
    for (float y = cy - ri + 5.0f; y < cy + ri; y += 9.0f)
    {
        g.setGradientFill (juce::ColourGradient (Colour (0xfff5f1e8), cx, y, Colour (0xffb5ae9f), cx, y + 5.0f, false));
        g.fillRect (cx - ri, y, 2.0f * ri, 5.0f);
    }
    g.restoreState();
}

void rollHolder (Graphics& g)
{
    // two wall mounts and the bar between them; the roll itself is drawn by drawRoll()
    chromeFill (g, { 40.0f, 588.0f, 172.0f, 10.0f });
    g.fillRect (40.0f, 588.0f, 172.0f, 10.0f);
    for (float x : { 44.0f, 208.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (x - 15.0f, 580.0f, 32.0f, 32.0f);
        chromeFill (g, { x - 16.0f, 577.0f, 32.0f, 32.0f });
        g.fillEllipse (x - 16.0f, 577.0f, 32.0f, 32.0f);
    }
    g.setColour (kInk);
    g.setFont (labelFont (13.0f));
    g.drawText ("LOW CUT", Rectangle<float> ((float) kRollTextX, (float) kRollTextY - 16.0f, 90.0f, 14.0f), Just::centredLeft, false);
}

void brandPlate (Graphics& g, Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 9.0f);
    juce::ColourGradient metal (Colour (0xffe6e6e4), r.getX(), r.getY(), Colour (0xff9c9d9e), r.getX(), r.getBottom(), false);
    metal.addColour (0.5, Colour (0xffcfcfcd));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f)); // brushed lines
    for (float y = r.getY() + 3.0f; y < r.getBottom() - 2.0f; y += 3.0f)
        g.fillRect (r.getX() + 6.0f, y, r.getWidth() - 12.0f, 1.0f);
    g.setColour (Colour (0xff6f7072));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
    for (float sx : { r.getX() + 12.0f, r.getRight() - 12.0f })
        for (float sy : { r.getY() + 12.0f, r.getBottom() - 12.0f })
            screw (g, sx, sy, 4.0f);
    // globe mark
    const float cx = r.getCentreX(), cy = r.getY() + 25.0f;
    g.setColour (kInk);
    g.drawEllipse (cx - 26.0f, cy - 12.0f, 52.0f, 24.0f, 1.8f);
    g.drawEllipse (cx - 12.0f, cy - 12.0f, 24.0f, 24.0f, 1.4f);
    g.fillRect (cx - 26.0f, cy - 0.7f, 52.0f, 1.4f);
    g.fillRect (cx - 0.7f, cy - 12.0f, 1.4f, 24.0f);
    g.drawEllipse (cx - 22.0f, cy - 6.0f, 44.0f, 12.0f, 1.2f);
    g.setFont (labelFont (16.0f).withHorizontalScale (1.0f).withExtraKerningFactor (0.16f));
    g.drawText ("PORCELAIN AUDIO", r.withTrimmedTop (42.0f).withTrimmedBottom (8.0f), Just::centred, false);
}

void towels (Graphics& g)
{
    for (int i = 0; i < 3; ++i)
    {
        const Rectangle<float> t (1008.0f, 612.0f + 24.0f * (float) i, 130.0f, 28.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (t.translated (0.0f, 3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xffffffff), t.getX(), t.getY(), Colour (0xffdedbd2), t.getX(), t.getBottom(), false));
        g.fillRoundedRectangle (t, 12.0f);
    }
    g.setColour (kInk);
    g.setFont (labelFont (13.0f));
    g.drawText ("DE-ESS", Rectangle<float> ((float) kSoapTextR - 90.0f, (float) kSoapTextY - 16.0f, 90.0f, 14.0f), Just::centredRight, false);
}
} // namespace

// ------------------------------------------------------------------ public
const Strip& strip (int i) { return kStrips[juce::jlimit (0, kNumMid - 1, i)]; }

int modeForRow (int stripIndex, int row)
{
    const auto& s = strip (stripIndex);
    return s.reversed ? s.numModes - 1 - row : row;
}

float modeRowY (int numModes, int row)
{
    return numModes >= 4 ? 188.0f + 16.5f * (float) row : 190.0f + 22.0f * (float) row;
}

juce::Rectangle<float> modeArea (int i)
{
    const auto& s = strip (i);
    const float top = modeRowY (s.numModes, 0) - 12.0f, bottom = modeRowY (s.numModes, s.numModes - 1) + 9.0f;
    return { midLeft (i) + 4.0f, top, midW() - 8.0f, bottom - top };
}

juce::Font labelFont (float height)
{
    return juce::Font (juce::FontOptions (height).withStyle ("Bold").withHorizontalScale (0.84f));
}

void drawLed (Graphics& g, float cx, float cy, float r, bool on, Colour onColour)
{
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (onColour.withAlpha (0.55f), cx, cy, onColour.withAlpha (0.0f), cx, cy + r * 2.2f, true));
        g.fillEllipse (cx - r * 2.2f, cy - r * 2.2f, r * 4.4f, r * 4.4f);
    }
    g.setColour (Colour (0xff2c2b29));
    g.fillEllipse (cx - r - 1.5f, cy - r - 1.5f, 2.0f * r + 3.0f, 2.0f * r + 3.0f);
    if (on)
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - r * 0.3f, cy - r * 0.3f, onColour, cx + r, cy + r, true));
    else
        g.setGradientFill (juce::ColourGradient (Colour (0xffc9c4b8), cx - r * 0.3f, cy - r * 0.3f, Colour (0xff7f7a70), cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
}

void drawModes (Graphics& g, int stripIndex, int selected, bool enabled)
{
    const auto& s = strip (stripIndex);
    const float left = midLeft (stripIndex);
    g.setFont (labelFont (s.numModes >= 4 ? 13.5f : 14.5f));
    for (int row = 0; row < s.numModes; ++row)
    {
        const float y = modeRowY (s.numModes, row);
        const int mode = modeForRow (stripIndex, row);
        const bool on = mode == selected;
        drawLed (g, left + 17.0f, y, 5.0f, on && enabled, kAmber);
        g.setColour (kInk.withAlpha (enabled ? 1.0f : 0.55f));
        g.drawText (s.modes[mode], Rectangle<float> (left + 29.0f, y - 9.0f, midW() - 31.0f, 18.0f), Just::centredLeft, false);
    }
}

void drawCap (Graphics& g, float cx, float cy, bool enabled)
{
    const Rectangle<float> r (cx - (float) kCapW * 0.5f, cy - (float) kCapH * 0.5f, (float) kCapW, (float) kCapH);
    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (r.translated (2.0f, 4.0f), 5.0f);
    // upper and lower bevels meet at a finger groove
    const auto top = r.withBottom (cy), bottom = r.withTop (cy);
    g.setGradientFill (juce::ColourGradient (Colour (0xfffdfbf5), cx, r.getY(), Colour (0xffd6cfc0), cx, cy, false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xffe9e3d6), cx, cy, Colour (0xfff7f3ea), cx, r.getBottom(), false));
    g.fillRoundedRectangle (bottom.withTrimmedTop (1.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx, top.getY(), juce::Colours::white.withAlpha (0.0f), cx, top.getY() + 8.0f, false));
    g.fillRoundedRectangle (top.reduced (2.0f, 1.5f), 4.0f);
    g.setColour (Colour (0xff4a463e));
    g.fillRect (r.getX() + 1.0f, cy - 1.0f, r.getWidth() - 2.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRect (r.getX() + 1.0f, cy + 1.0f, r.getWidth() - 2.0f, 1.0f);
    g.setColour (Colour (0xff9f9787));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    if (! enabled)
    {
        g.setColour (kPorcelainLo.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

void drawTopButton (Graphics& g, float cx, float cy, bool lit, bool down)
{
    const float r = 8.0f;
    chromeFill (g, { cx - r, cy - r, 2.0f * r, 2.0f * r });
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
    g.setColour (Colour (0xff161514));
    g.fillEllipse (cx - r + 2.0f, cy - r + 2.0f, 2.0f * r - 4.0f, 2.0f * r - 4.0f);
    drawLed (g, cx, cy + (down ? 0.5f : 0.0f), 3.2f, lit, kAmber);
}

void drawRoundButton (Graphics& g, Rectangle<float> r, Colour c, bool down)
{
    chromeFill (g, r);
    g.fillEllipse (r);
    const auto in = r.reduced (2.5f);
    g.setGradientFill (juce::ColourGradient (c.brighter (down ? 0.0f : 0.5f), in.getX(), in.getY(), c.darker (0.6f), in.getRight(), in.getBottom(), true));
    g.fillEllipse (in);
    // a little "again" arc
    const auto ic = in.reduced (in.getWidth() * 0.27f);
    Path arc; arc.addArc (ic.getX(), ic.getY(), ic.getWidth(), ic.getHeight(), 0.6f, 5.6f, true);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (arc, juce::PathStrokeType (1.4f));
}

void drawRoll (Graphics& g, float unrolled)
{
    const Colour paper (0xfffbfaf6), shade (0xffd3cfc4);
    const float bottom = 630.0f + juce::jlimit (0.0f, 1.0f, unrolled) * 62.0f;
    // the sheet hanging off the front...
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (76.0f, 600.0f, 92.0f, bottom - 600.0f);
    g.setGradientFill (juce::ColourGradient (paper, 70.0f, 0.0f, shade, 162.0f, 0.0f, false));
    g.fillRect (70.0f, 596.0f, 92.0f, bottom - 596.0f);
    g.setColour (Colour (0xffc4c0b4));
    for (float y = bottom - 4.0f; y > 626.0f; y -= 30.0f) // perforations travel with the paper
        for (float x = 74.0f; x < 160.0f; x += 6.0f)
            g.fillRect (x, y, 3.0f, 1.0f);
    // ...then the roll over it
    g.setGradientFill (juce::ColourGradient (Colour (0xffffffff), 0.0f, 564.0f, shade, 0.0f, 624.0f, false));
    g.fillRoundedRectangle (70.0f, 564.0f, 112.0f, 58.0f, 26.0f);
    g.setColour (Colour (0xffe6e3da));
    g.fillEllipse (158.0f, 566.0f, 28.0f, 54.0f);
    g.setColour (Colour (0xffb9b5aa));
    g.drawEllipse (158.0f, 566.0f, 28.0f, 54.0f, 1.0f);
    g.setColour (Colour (0xff8f8b81));
    g.fillEllipse (167.0f, 583.0f, 10.0f, 20.0f);
}

void drawLevelMeter (Graphics& g, Rectangle<float> r, float db, bool markTarget)
{
    chromeFill (g, r);
    g.fillRoundedRectangle (r, 4.0f);
    const auto in = r.reduced (2.0f);
    g.setColour (Colour (0xff0d0d0c));
    g.fillRoundedRectangle (in, 3.0f);

    constexpr int segs = 18;
    constexpr float range = 27.0f;
    const float top = in.getY() + 5.0f, h = in.getHeight() - 10.0f, segH = h / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float segDb = -range * (float) (i + 1) / (float) segs; // lights when the level reaches this
        const bool lit = db >= segDb;
        const Colour c = i < 2 ? Colour (0xffff3b22) : i < 5 ? Colour (0xffffa51e) : i < 8 ? Colour (0xfff4e21d) : Colour (0xff7be21f);
        g.setColour (lit ? c : c.withMultipliedBrightness (0.22f));
        const float y = top + segH * (float) i;
        g.fillRoundedRectangle (in.getX() + 4.0f, y + 1.0f, 12.0f, segH - 2.0f, 1.5f);
        g.fillRoundedRectangle (in.getRight() - 16.0f, y + 1.0f, 12.0f, segH - 2.0f, 1.5f);
    }
    g.setFont (labelFont (12.5f));
    for (int d : { 0, 6, 12, 18, 24 })
    {
        const float y = top + h * ((float) d + 0.75f) / range;
        const bool target = markTarget && d == 18;
        g.setColour (target ? kGreen : Colour (0xfff1ede4));
        g.drawText (juce::String (d), Rectangle<float> (in.getX() + 16.0f, y - 8.0f, in.getWidth() - 32.0f, 16.0f), Just::centred, false);
    }
}

void drawSoap (Graphics& g, float level, bool pumpDown)
{
    const float dy = pumpDown ? 11.0f : 0.0f;
    // pump: spout and stem move, the collar stays
    chromeFill (g, { 922.0f, 566.0f + dy, 42.0f, 34.0f });
    g.fillRect (951.0f, 574.0f + dy, 8.0f, 30.0f - dy);
    g.fillRoundedRectangle (922.0f, 568.0f + dy, 42.0f, 8.0f, 4.0f);
    g.fillRect (922.0f, 572.0f + dy, 6.0f, 10.0f);
    chromeFill (g, { 942.0f, 598.0f, 26.0f, 24.0f });
    g.fillRoundedRectangle (942.0f, 600.0f, 26.0f, 20.0f, 3.0f);
    g.setColour (Colour (0xff6f7275)); // outlines, so the chrome reads against the porcelain
    g.drawRoundedRectangle (922.0f, 568.0f + dy, 42.0f, 8.0f, 4.0f, 1.0f);
    g.drawRect (951.0f, 576.0f + dy, 8.0f, 24.0f - dy, 1.0f);
    g.drawRoundedRectangle (942.0f, 600.0f, 26.0f, 20.0f, 3.0f, 1.0f);
    // bottle
    const Rectangle<float> b (918.0f, 618.0f, 74.0f, 72.0f);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillRoundedRectangle (b.translated (3.0f, 3.0f), 12.0f);
    g.setColour (Colour (0xffeef3f2).withAlpha (0.9f));
    g.fillRoundedRectangle (b, 12.0f);
    // soap: the fuller the bottle, the more de-essing
    const auto inner = b.reduced (4.0f, 3.0f);
    const float h = 5.0f + juce::jlimit (0.0f, 1.0f, level) * (inner.getHeight() - 9.0f);
    g.saveState();
    g.reduceClipRegion (inner.withTop (inner.getBottom() - h).toNearestInt());
    g.setGradientFill (juce::ColourGradient (Colour (0xfff5e08e), b.getX(), inner.getBottom() - h, Colour (0xffc9a640), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (inner, 9.0f);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillRoundedRectangle (b.getX() + 8.0f, b.getY() + 8.0f, 6.0f, 46.0f, 3.0f);
    g.setColour (Colour (0xff9aa3a3));
    g.drawRoundedRectangle (b, 12.0f, 1.2f);
}

void drawGrMeter (Graphics& g, Rectangle<float> r, float grDb, float dbPerLight)
{
    g.setColour (Colour (0xff0d0d0c));
    g.fillRoundedRectangle (r, 2.5f);
    constexpr int segs = 12;
    const float segH = (r.getHeight() - 4.0f) / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const bool lit = grDb >= dbPerLight * ((float) i + 0.5f);
        const Colour c = i < 6 ? kAmber : i < 9 ? Colour (0xffff7a1e) : Colour (0xffff3b22);
        g.setColour (lit ? c : c.withMultipliedBrightness (0.2f));
        g.fillRoundedRectangle (r.getX() + 2.0f, r.getY() + 2.0f + segH * (float) i + 0.8f, r.getWidth() - 4.0f, segH - 1.6f, 1.0f);
    }
}

void drawBackground (Graphics& g)
{
    g.fillAll (kGap);

    // ---- header
    porcelainPanel (g, { 2.0f, 2.0f, (float) kW - 4.0f, (float) kHeaderB - 2.0f }, 14.0f);
    lamp (g, (float) kW * 0.5f);
    toilet (g, 56.0f, 20.0f);
    {
        const auto title = juce::Font (juce::FontOptions (66.0f).withStyle ("Bold Italic").withHorizontalScale (0.92f));
        const Rectangle<float> area (150.0f, 22.0f, 340.0f, 76.0f);
        g.setFont (title);
        g.setColour (juce::Colours::white);
        g.drawText ("Toilet Vocals", area.translated (-1.0f, -1.5f), Just::centredLeft, false);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.drawText ("Toilet Vocals", area.translated (2.0f, 3.0f), Just::centredLeft, false);
        g.setColour (kInk);
        g.drawText ("Toilet Vocals", area, Just::centredLeft, false);
        Path swoosh;
        swoosh.startNewSubPath (156.0f, 110.0f);
        swoosh.quadraticTo (300.0f, 92.0f, 468.0f, 104.0f);
        swoosh.quadraticTo (310.0f, 100.0f, 156.0f, 116.0f);
        swoosh.closeSubPath();
        g.fillPath (swoosh);
    }
    {
        g.beginTransparencyLayer (0.6f); // not wired up yet
        cleanDirty (g, { 662.0f, 44.0f, 276.0f, 64.0f });
        g.endTransparencyLayer();
    }
    vent (g, (float) kW - 82.0f, 75.0f, 54.0f);
    for (float sx : { 26.0f, (float) kW - 26.0f })
        for (float sy : { 26.0f, (float) kHeaderB - 22.0f })
            screw (g, sx, sy, 6.0f);

    checker (g, { 0, kBandT, kW, kBandB - kBandT }, 11);

    // ---- strips
    porcelainPanel (g, { 2.0f, (float) kStripT, (float) kW - 4.0f, (float) (kStripB - kStripT) }, 10.0f);
    {
        const Rectangle<float> mid ((float) kMidL, (float) kStripT + 6.0f, (float) (kMidR - kMidL), (float) (kStripB - kStripT) - 8.0f);
        g.setGradientFill (juce::ColourGradient (kPorcelainHi, 0.0f, mid.getY(), Colour (0xffded7c8), 0.0f, mid.getBottom(), false));
        g.fillRoundedRectangle (mid, 9.0f);
        g.setColour (Colour (0xffb0a898));
        g.drawRoundedRectangle (mid, 9.0f, 1.0f);
        for (int i = 1; i < kNumMid; ++i)
        {
            const float x = midLeft (i);
            g.setColour (Colour (0xffb9b1a1));
            g.fillRect (x - 0.5f, mid.getY() + 6.0f, 1.0f, mid.getHeight() - 12.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (x + 0.5f, mid.getY() + 6.0f, 1.0f, mid.getHeight() - 12.0f);
        }
    }

    // input column
    grille (g, { 22.0f, 183.0f, 146.0f, 44.0f });
    slot (g, kInputFaderX);
    scale (g, kInputFaderX, -1, { { "+10", 22.0f / 24.0f }, { "", 17.0f / 24.0f }, { "0", 0.5f }, { "", 7.0f / 24.0f }, { "-10", 2.0f / 24.0f } });
    jack (g, kInputMeterX + (float) kMeterW * 0.5f, kTopDotY - 1.0f);
    stripLabel (g, "INPUT SENS", (float) kInputW * 0.5f, (float) kInputW);

    // the eight middle strips
    for (int i = 0; i < kNumMid; ++i)
    {
        const auto& s = strip (i);
        const float cx = midX (i);
        slot (g, cx);
        stripLabel (g, s.name, cx, midW());
        if (s.kind == Kind::comp)
        {
            scale (g, cx, -1, { { "10", 1.0f }, { "", 0.75f }, { "5", 0.5f }, { "", 0.25f }, { "0", 0.0f } });
            g.setColour (kInk.withAlpha (0.7f));
            g.setFont (labelFont (9.5f));
            g.drawText ("GR", Rectangle<float> (cx + 20.0f, (float) kTravelB + 4.0f, 18.0f, 10.0f), Just::centred, false);
        }
        else if (s.kind == Kind::eq)
        {
            scale (g, cx, -1, { { "+10", 1.0f }, { "", 0.75f }, { "0", 0.5f }, { "", 0.25f }, { "-10", 0.0f } });
            screw (g, cx, kTopDotY, 4.5f);
        }
        else // ghosted until the module exists
        {
            scale (g, cx, -1, { { "+10", 22.0f / 24.0f }, { "", 17.0f / 24.0f }, { "0", 0.5f }, { "", 7.0f / 24.0f }, { "-10", 2.0f / 24.0f } });
            screw (g, cx, kTopDotY, 4.5f);
            drawModes (g, i, -1, false);
            drawCap (g, cx, faderY (0.5f), true);
            g.setColour (Colour (0xffe6dfd1).withAlpha (0.42f));
            g.fillRect (Rectangle<float> (midLeft (i) + 2.0f, (float) kStripT + 10.0f, midW() - 4.0f, (float) (kStripB - kStripT) - 16.0f));
        }
    }

    // output column
    slot (g, kOutputFaderX);
    scale (g, kOutputFaderX, 1, { { "+10", 34.0f / 36.0f }, { "0", 24.0f / 36.0f }, { "-10", 14.0f / 36.0f }, { "-20", 4.0f / 36.0f } });
    jack (g, kOutputMeterX + (float) kMeterW * 0.5f, kTopDotY - 1.0f);
    screw (g, kOutputFaderX, kTopDotY, 4.5f);
    stripLabel (g, "OUTPUT", (float) kMidR + (float) kOutputW * 0.5f, (float) kOutputW);

    // ---- footer
    porcelainPanel (g, { 2.0f, (float) kFooterT, (float) kW - 4.0f, (float) (kFooterB - kFooterT) }, 12.0f);
    checker (g, { 0, kFooterB, kW, kH - kFooterB }, 18);
    brandPlate (g, { (float) kW * 0.5f - 135.0f, 578.0f, 270.0f, 74.0f });
    rollHolder (g);
    towels (g);
}
}
