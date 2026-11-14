#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour ruby (0xffe0245e);

    juce::Colour categoryColour (int cat)
    {
        switch (cat)
        {
            case Fx::CatDynamics:   return juce::Colour (0xfff2c14e);
            case Fx::CatDrive:      return juce::Colour (0xffe8504a);
            case Fx::CatTone:       return juce::Colour (0xffa066e8);
            case Fx::CatModulation: return juce::Colour (0xff4aa8e8);
            case Fx::CatSpace:      return juce::Colour (0xff4ac98a);
            case Fx::CatSpecial:    return juce::Colour (0xffe88a4a);
            default:                return juce::Colour (0xff6b7280);
        }
    }
}

//==============================================================================
// SlotStrip
//==============================================================================
SlotStrip::SlotStrip (MultiRubyAudioProcessor& p, int slotIndex, std::function<void (int, int)> moveCallback)
    : proc (p), slot (slotIndex), onMove (std::move (moveCallback))
{
    auto& apvts = proc.apvts;

    addAndMakeVisible (onButton);
    onAttachment = std::make_unique<APVTS::ButtonAttachment> (apvts, MultiRubyAudioProcessor::slotId (slot, "on"), onButton);

    // The type box is driven manually (not by an attachment) so that picking an effect can load its defaults
    typeBox.addItemList (Fx::typeNames(), 1);
    addAndMakeVisible (typeBox);
    typeBox.onChange = [this] { userChangedType(); };

    upButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb2"));
    downButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbc"));
    upButton.setTooltip ("Move this effect up in the chain");
    downButton.setTooltip ("Move this effect down in the chain");
    upButton.onClick   = [this] { onMove (slot, slot - 1); };
    downButton.onClick = [this] { onMove (slot, slot + 1); };
    addAndMakeVisible (upButton);
    addAndMakeVisible (downButton);

    for (int i = 0; i < Fx::numParams; ++i)
        knobs[i].init (*this, apvts, MultiRubyAudioProcessor::slotId (slot, "p" + juce::String (i + 1)), "");
    mixKnob.init (*this, apvts, MultiRubyAudioProcessor::slotId (slot, "mix"), "Mix");

    syncFromParameters (true);
}

void SlotStrip::userChangedType()
{
    const int idx = typeBox.getSelectedId() - 1;
    if (idx < 0) return;

    if (auto* prm = proc.apvts.getParameter (MultiRubyAudioProcessor::slotId (slot, "type")))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        prm->endChangeGesture();
    }
    proc.applyTypeDefaults (slot);
    syncFromParameters (true);
}

void SlotStrip::syncFromParameters (bool force)
{
    const int type = juce::jlimit (0, (int) Fx::NumTypes - 1,
                                   (int) std::lround (proc.apvts.getRawParameterValue (MultiRubyAudioProcessor::slotId (slot, "type"))->load()));

    if (typeBox.getSelectedId() - 1 != type)
        typeBox.setSelectedId (type + 1, juce::dontSendNotification);

    if (! force && type == shownType)
        return;

    shownType = type;
    const auto& td = Fx::typeDef (type);

    for (int i = 0; i < Fx::numParams; ++i)
    {
        const bool used = td.p[i].name != nullptr;
        knobs[i].setVisible (used);
        if (used)
        {
            knobs[i].label.setText (td.p[i].name, juce::dontSendNotification);
            knobs[i].slider.updateText();
        }
    }
    mixKnob.setVisible (type != Fx::Empty);
    mixKnob.slider.updateText();
    repaint();
}

void SlotStrip::paint (juce::Graphics& g)
{
    const auto col = categoryColour (Fx::typeDef (shownType < 0 ? 0 : shownType).category);
    const bool active = shownType > 0;
    auto r = getLocalBounds().toFloat().reduced (2.0f, 2.0f);

    g.setColour (juce::Colour (0xff262a33));
    g.fillRoundedRectangle (r, 7.0f);

    g.setColour (active ? col : col.withAlpha (0.35f));
    g.fillRoundedRectangle (r.withWidth (6.0f), 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText (juce::String (slot + 1), 12, 0, 22, getHeight(), juce::Justification::centred);
}

void SlotStrip::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    r.removeFromLeft (36);                                 // stripe + number

    onButton.setBounds (r.removeFromLeft (52).reduced (0, 8));
    typeBox.setBounds (r.removeFromLeft (150).withSizeKeepingCentre (150, juce::jmin (26, r.getHeight())));
    r.removeFromLeft (6);

    auto arrows = r.removeFromLeft (28);
    upButton.setBounds (arrows.removeFromTop (arrows.getHeight() / 2).reduced (1));
    downButton.setBounds (arrows.reduced (1));
    r.removeFromLeft (8);

    const int w = r.getWidth() / (Fx::numParams + 1);
    for (int i = 0; i < Fx::numParams; ++i)
        knobs[i].setBounds (r.removeFromLeft (w));
    mixKnob.setBounds (r);
}

//==============================================================================
// Editor
//==============================================================================
MultiRubyAudioProcessorEditor::MultiRubyAudioProcessorEditor (MultiRubyAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    laf.setColour (juce::Slider::rotarySliderFillColourId, ruby);
    laf.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff5f5f5));
    laf.setColour (juce::ToggleButton::tickColourId, ruby);
    setLookAndFeel (&laf);

    title.setText ("MULTI RUBY", juce::dontSendNotification);
    title.setFont (juce::Font (26.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, ruby);
    addAndMakeVisible (title);

    hint.setText ("Signal flows from slot 1 to slot 10. Pick an effect for each slot, reorder with the arrows.",
                  juce::dontSendNotification);
    hint.setFont (juce::Font (12.5f));
    hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    addAndMakeVisible (hint);

    inKnob.init  (*this, proc.apvts, "inGain",  "Input");
    outKnob.init (*this, proc.apvts, "outGain", "Output");

    for (int s = 0; s < Fx::numSlots; ++s)
    {
        strips[(size_t) s] = std::make_unique<SlotStrip> (proc, s, [this] (int a, int b)
        {
            proc.swapSlots (a, b);
            for (auto& st : strips) st->syncFromParameters (true);
        });
        content.addAndMakeVisible (*strips[(size_t) s]);
    }

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (900, 400, 1700, 1100);
    setSize (1100, 800);
    startTimerHz (20);
}

MultiRubyAudioProcessorEditor::~MultiRubyAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MultiRubyAudioProcessorEditor::timerCallback()
{
    for (auto& st : strips)
        st->syncFromParameters();

    inShown  = juce::jmax (proc.inLevel.exchange (0.0f),  inShown * 0.85f);
    outShown = juce::jmax (proc.outLevel.exchange (0.0f), outShown * 0.85f);
    repaint (inMeterArea.getUnion (outMeterArea));
}

void MultiRubyAudioProcessorEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float level, const juce::String& name)
{
    g.setColour (juce::Colour (0xff15171c));
    g.fillRoundedRectangle (area.toFloat(), 3.0f);

    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float pos = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -1.0f ? juce::Colour (0xffff3b3b) : db > -12.0f ? juce::Colour (0xfff2c14e) : juce::Colour (0xff4ac98a));
    g.fillRoundedRectangle (area.toFloat().withWidth ((float) area.getWidth() * pos), 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (11.0f));
    g.drawText (name, area.reduced (4, 0), juce::Justification::centredLeft);
}

void MultiRubyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d23));
    drawMeter (g, inMeterArea,  inShown,  "IN");
    drawMeter (g, outMeterArea, outShown, "OUT");
}

void MultiRubyAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);

    auto top = r.removeFromTop (88);
    auto left = top.removeFromLeft (300);
    title.setBounds (left.removeFromTop (36));
    hint.setBounds (left.withWidth (getWidth() - 40));

    auto right = top.removeFromRight (400);
    outKnob.setBounds (right.removeFromRight (86));
    inKnob.setBounds  (right.removeFromRight (86));
    right.removeFromRight (10);
    outMeterArea = right.removeFromBottom (14).reduced (0, 0);
    right.removeFromBottom (4);
    inMeterArea  = right.removeFromBottom (14);

    hint.setBounds (10, 44, getWidth() - 420, 20);

    r.removeFromTop (4);
    viewport.setBounds (r);
    const int rowH = 78;                                   // tall enough for properly sized knobs
    content.setSize (r.getWidth() - viewport.getScrollBarThickness(), rowH * Fx::numSlots);
    for (int s = 0; s < Fx::numSlots; ++s)
        strips[(size_t) s]->setBounds (0, s * rowH, content.getWidth(), rowH);
}
