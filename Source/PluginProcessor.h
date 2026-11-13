#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ToiletVocalsEngine.h"

class ToiletVocalsProcessor : public juce::AudioProcessor
{
public:
    ToiletVocalsProcessor();
    ~ToiletVocalsProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Toilet Vocals"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    struct ID
    {
        static constexpr const char* input = "input";
        static constexpr const char* c1Model = "c1model";
        static constexpr const char* c1Amount = "c1amount";
        static constexpr const char* c1On = "c1on";
        static constexpr const char* c2Model = "c2model";
        static constexpr const char* c2Amount = "c2amount";
        static constexpr const char* c2On = "c2on";
        static constexpr const char* output = "output";
    };

    // Throws away the learned input level and listens again.
    void relearn() { pendingCmd.store (cmdRelearn, std::memory_order_relaxed); }

    // ---- read by the editor
    std::atomic<float> uiInputDb { -100.0f };   // level going into the compressors (RMS)
    std::atomic<float> uiGr1 { 0.0f }, uiGr2 { 0.0f };
    std::atomic<float> uiOutPeak { 0.0f };
    std::atomic<bool> uiLearned { false };
    std::atomic<float> uiLearnProgress { 0.0f };
    std::atomic<float> uiAutoGainDb { 0.0f };

private:
    enum { cmdNone = 0, cmdRelearn = 1, cmdRestore = 2 };
    tvox::Params readParams() const;

    tvox::Engine engine;
    std::atomic<float>* raw[8] {};
    std::atomic<int> pendingCmd { cmdNone };
    std::atomic<bool> restoreLearned { false };
    std::atomic<float> restoreDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToiletVocalsProcessor)
};
