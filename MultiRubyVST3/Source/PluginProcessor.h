#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxInfo.h"
#include "Effects.h"

#include <array>
#include <atomic>
#include <memory>

class MultiRubyAudioProcessor : public juce::AudioProcessor
{
public:
    MultiRubyAudioProcessor();
    ~MultiRubyAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // message thread: exchange the complete contents of two slots (used by the up/down buttons)
    void swapSlots (int a, int b);
    // message thread: put a slot's parameters to the defaults of its current effect
    void applyTypeDefaults (int slot);

    static juce::String slotId (int slot, const juce::String& name)
    {
        return "s" + juce::String (slot + 1) + "_" + name;
    }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };   // peak since last read

private:
    struct SlotParams
    {
        std::atomic<float>* type = nullptr;
        std::atomic<float>* on = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* p[Fx::numParams] = {};
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<SlotParams, Fx::numSlots> sp;
    std::array<std::array<std::unique_ptr<Fx::Base>, Fx::NumTypes>, Fx::numSlots> fx;
    std::array<int, Fx::numSlots> lastType {};

    std::atomic<float>* pIn = nullptr;
    std::atomic<float>* pOut = nullptr;
    float lastInGain = 1.0f, lastOutGain = 1.0f;

    static constexpr int kChunk = 512;
    float workL[kChunk] = {}, workR[kChunk] = {}, dryL[kChunk] = {}, dryR[kChunk] = {};

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiRubyAudioProcessor)
};
