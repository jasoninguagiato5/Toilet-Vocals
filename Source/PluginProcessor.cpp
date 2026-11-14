#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

Attr pct() { return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                          .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; }); }
Attr tone() { return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1); }); }
Attr hz()  { return Attr().withStringFromValueFunction ([] (float v, int) { return v <= tvox::kLowCutOffHz + 0.5f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " Hz"; })
                          .withValueFromStringFunction ([] (const juce::String& s) { return s.containsIgnoreCase ("off") ? tvox::kLowCutOffHz : s.getFloatValue(); }); }
Attr amount() { return Attr().withStringFromValueFunction ([] (float v, int) { return v < 0.05f ? juce::String ("Off") : juce::String (v, 1); })
                             .withValueFromStringFunction ([] (const juce::String& s) { return s.containsIgnoreCase ("off") ? 0.0f : s.getFloatValue(); }); }
Attr db()  { return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }).withLabel ("dB"); }
}

juce::AudioProcessorValueTreeState::ParameterLayout ToiletVocalsProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pid = [] (const char* s) { return juce::ParameterID { s, 1 }; };
    const tvox::Params d;

    juce::StringArray models;
    for (int i = 0; i < tvox::kNumModels; ++i) models.add (tvox::modelInfo (i).name);

    l.add (std::make_unique<APF> (pid (ID::input), "Input", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), d.inputDb, db()));
    l.add (std::make_unique<juce::AudioParameterChoice> (pid (ID::c1Model), "Comp 1 Model", models, d.comp1.model));
    l.add (std::make_unique<APF> (pid (ID::c1Amount), "Comp 1", juce::NormalisableRange<float> (0.0f, 1.0f), d.comp1.amount, pct()));
    l.add (std::make_unique<juce::AudioParameterBool> (pid (ID::c1On), "Comp 1 On", d.comp1.on));
    l.add (std::make_unique<juce::AudioParameterChoice> (pid (ID::c2Model), "Comp 2 Model", models, d.comp2.model));
    l.add (std::make_unique<APF> (pid (ID::c2Amount), "Comp 2", juce::NormalisableRange<float> (0.0f, 1.0f), d.comp2.amount, pct()));
    l.add (std::make_unique<juce::AudioParameterBool> (pid (ID::c2On), "Comp 2 On", d.comp2.on));
    l.add (std::make_unique<APF> (pid (ID::output), "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), d.outputDb, db()));

    juce::StringArray lowModes, highModels;
    for (int i = 0; i < tvox::kNumLowModes; ++i) lowModes.add (tvox::lowModeInfo (i).name);
    for (int i = 0; i < tvox::kNumHighModels; ++i) highModels.add (tvox::highModelInfo (i).name);
    juce::NormalisableRange<float> cutRange (tvox::kLowCutOffHz, tvox::kLowCutMaxHz, 1.0f);
    cutRange.setSkewForCentre (90.0f);
    l.add (std::make_unique<APF> (pid (ID::lowCut), "Low Cut", cutRange, d.lowCutHz, hz()));
    l.add (std::make_unique<juce::AudioParameterChoice> (pid (ID::lowMode), "Low Mode", lowModes, d.lowMode));
    l.add (std::make_unique<APF> (pid (ID::low), "Low", juce::NormalisableRange<float> (-10.0f, 10.0f, 0.1f), d.low * 10.0f, tone()));
    l.add (std::make_unique<juce::AudioParameterChoice> (pid (ID::highModel), "High Model", highModels, d.highModel));
    l.add (std::make_unique<APF> (pid (ID::high), "High", juce::NormalisableRange<float> (-10.0f, 10.0f, 0.1f), d.high * 10.0f, tone()));
    l.add (std::make_unique<APF> (pid (ID::deEss), "De-Ess", juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), d.deEss * 10.0f, amount()));
    l.add (std::make_unique<juce::AudioParameterChoice> (pid (ID::deEssRange), "De-Ess Range", juce::StringArray { "Low", "High" }, d.deEssHigh ? 1 : 0));
    return l;
}

ToiletVocalsProcessor::ToiletVocalsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ToiletVocals", createLayout())
{
    const char* const order[kNumParams] = { ID::input, ID::c1Model, ID::c1Amount, ID::c1On, ID::c2Model, ID::c2Amount, ID::c2On, ID::output,
                                            ID::lowCut, ID::lowMode, ID::low, ID::highModel, ID::high, ID::deEss, ID::deEssRange };
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[i] = apvts.getRawParameterValue (order[i]);
        jassert (raw[i] != nullptr);
    }
}

bool ToiletVocalsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    if (out == mono) return in == mono;
    return out == stereo && (in == mono || in == stereo);
}

void ToiletVocalsProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples()); // the 2x oversampling filters
}

tvox::Params ToiletVocalsProcessor::readParams() const
{
    auto v = [this] (int i) { return raw[i]->load (std::memory_order_relaxed); };
    auto model = [&v] (int i) { return juce::jlimit (0, tvox::kNumModels - 1, juce::roundToInt (v (i))); };
    tvox::Params p;
    p.inputDb = v (0);
    p.comp1 = { model (1), v (2), v (3) > 0.5f };
    p.comp2 = { model (4), v (5), v (6) > 0.5f };
    p.outputDb = v (7);
    p.lowCutHz = v (8);
    p.lowMode = juce::jlimit (0, tvox::kNumLowModes - 1, juce::roundToInt (v (9)));
    p.low = v (10) * 0.1f;
    p.highModel = juce::jlimit (0, tvox::kNumHighModels - 1, juce::roundToInt (v (11)));
    p.high = v (12) * 0.1f;
    p.deEss = v (13) * 0.1f;
    p.deEssHigh = v (14) > 0.5f;
    return p;
}

void ToiletVocalsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0) return;

    switch (pendingCmd.exchange (cmdNone, std::memory_order_relaxed))
    {
        case cmdRelearn: engine.relearn(); break;
        case cmdRestore: engine.restoreLearn (restoreLearned.load(), restoreDb.load()); break;
        default: break;
    }

    // mono track on a stereo output: feed both sides
    if (getTotalNumInputChannels() == 1 && numCh > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    const auto p = readParams();
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    constexpr int chunk = 64; // parameters are re-read this often
    float scratch[chunk];
    float gr1 = 0.0f, gr2 = 0.0f, deEss = 0.0f;
    for (int off = 0; off < n; off += chunk)
    {
        const int cn = juce::jmin (chunk, n - off);
        float* r = R != nullptr ? R + off : scratch;
        if (R == nullptr) std::copy (L + off, L + off + cn, scratch);
        engine.setParams (p);
        engine.process (L + off, r, cn);
        gr1 = juce::jmax (gr1, engine.gr1Db());
        gr2 = juce::jmax (gr2, engine.gr2Db());
        deEss = juce::jmax (deEss, engine.deEssDb());
    }

    float peak = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, n));

    auto hold = [] (std::atomic<float>& a, float v) { a.store (juce::jmax (v, a.load (std::memory_order_relaxed)), std::memory_order_relaxed); };
    hold (uiOutPeak, peak);
    hold (uiGr1, gr1);
    hold (uiGr2, gr2);
    hold (uiDeEss, deEss);
    uiInputDb.store (engine.inputLevelDb(), std::memory_order_relaxed);
    uiLearned.store (engine.isLearned(), std::memory_order_relaxed);
    uiLearnProgress.store (engine.learnProgress(), std::memory_order_relaxed);
    uiAutoGainDb.store (engine.normGainDb(), std::memory_order_relaxed);
}

// The learned input level is saved with the session, so a reopened project (and
// every bounce) uses exactly the same gain instead of listening again.
void ToiletVocalsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const bool pendingRestore = pendingCmd.load() == cmdRestore; // restored but audio hasn't run yet
    const bool learned = pendingRestore ? restoreLearned.load() : uiLearned.load();
    state.setProperty ("learned", learned, nullptr);
    state.setProperty ("learnDb", (double) (pendingRestore ? restoreDb.load() : uiAutoGainDb.load()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ToiletVocalsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const bool learned = tree.getProperty ("learned", false);
            const float gainDb = juce::jlimit (-24.0f, 30.0f, (float) (double) tree.getProperty ("learnDb", 0.0));
            apvts.replaceState (tree);
            restoreLearned.store (learned);
            restoreDb.store (gainDb);
            uiLearned.store (learned);
            uiAutoGainDb.store (gainDb);
            uiLearnProgress.store (learned ? 1.0f : 0.0f);
            pendingCmd.store (cmdRestore);
        }
}

juce::AudioProcessorEditor* ToiletVocalsProcessor::createEditor() { return new ToiletVocalsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ToiletVocalsProcessor(); }
