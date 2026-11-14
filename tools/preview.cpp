// Renders the faceplate to a PNG (no plugin host needed). Links juce_core,
// juce_events and juce_graphics only.
//   preview out.png [scale]
#include "../Source/ui/Artwork.h"

int main (int argc, char** argv)
{
    if (argc < 2) { std::puts ("usage: preview out.png [scale]"); return 2; }
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 2.0f;
    juce::Image img (juce::Image::ARGB, (int) ((float) art::kW * scale), (int) ((float) art::kH * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        art::drawBackground (g);

        // the moving parts, at example positions
        art::drawLed (g, art::kInputFaderX, art::kTopDotY, 5.5f, true, art::kGreen);
        art::drawCap (g, art::kInputFaderX, art::faderY (0.5f), true);
        art::drawLevelMeter (g, { art::kInputMeterX, (float) art::kMeterT, (float) art::kMeterW, (float) (art::kMeterB - art::kMeterT) }, -17.0f, true);
        art::drawRoundButton (g, { art::kInputMeterX + 15.0f, 490.0f, 20.0f, 20.0f }, juce::Colour (0xffc22a1e), false);
        const float amounts[2] = { 0.35f, 0.6f }; const int models[2] = { 0, 1 }; const float gr[2] = { 7.0f, 4.0f };
        for (int c = 0; c < 2; ++c)
        {
            const int i = c == 0 ? art::kComp1 : art::kComp2;
            art::drawModes (g, i, models[c], true);
            art::drawTopButton (g, art::midX (i), art::kTopDotY, true, false);
            art::drawGrMeter (g, { art::midX (i) + 24.0f, (float) art::kTravelT - 6.0f, 10.0f, (float) (art::kTravelB - art::kTravelT) + 8.0f }, gr[c]);
            art::drawCap (g, art::midX (i), art::faderY (amounts[c]), true);
        }
        art::drawLevelMeter (g, { art::kOutputMeterX, (float) art::kMeterT, (float) art::kMeterW, (float) (art::kMeterB - art::kMeterT) }, -6.0f, false);
        art::drawCap (g, art::kOutputFaderX, art::faderY (24.0f / 36.0f), true);
    }
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    return os.openedOk() && juce::PNGImageFormat().writeImageToStream (img, os) ? 0 : 1;
}
