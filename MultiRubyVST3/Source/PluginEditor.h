#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

using APVTS = juce::AudioProcessorValueTreeState;

struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    void init (juce::Component& parent, APVTS& apvts, const juce::String& paramId, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 14);
        parent.addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (12.0f, juce::Font::bold));
        label.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (apvts, paramId, slider);
    }

    void setVisible (bool v) { slider.setVisible (v); label.setVisible (v); }

    void setBounds (juce::Rectangle<int> r)
    {
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }
};

//==============================================================================
// One row of the effect chain
class SlotStrip : public juce::Component
{
public:
    SlotStrip (MultiRubyAudioProcessor& p, int slotIndex, std::function<void (int, int)> moveCallback);

    void paint (juce::Graphics&) override;
    void resized() override;

    // keeps the type box and knob labels in step with the parameters
    void syncFromParameters (bool force = false);

private:
    void userChangedType();

    MultiRubyAudioProcessor& proc;
    const int slot;
    std::function<void (int, int)> onMove;

    juce::ToggleButton onButton { "On" };
    juce::ComboBox typeBox;
    juce::TextButton upButton, downButton;
    Knob knobs[Fx::numParams];
    Knob mixKnob;

    std::unique_ptr<APVTS::ButtonAttachment> onAttachment;
    int shownType = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotStrip)
};

//==============================================================================
class MultiRubyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit MultiRubyAudioProcessorEditor (MultiRubyAudioProcessor&);
    ~MultiRubyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<int> area, float level, const juce::String& name);

    juce::LookAndFeel_V4 laf;
    MultiRubyAudioProcessor& proc;

    juce::Label title, hint;
    Knob inKnob, outKnob;
    juce::Viewport viewport;      // the 10 slots scroll if the window is short
    juce::Component content;
    std::array<std::unique_ptr<SlotStrip>, Fx::numSlots> strips;

    juce::Rectangle<int> inMeterArea, outMeterArea;
    float inShown = 0.0f, outShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiRubyAudioProcessorEditor)
};
