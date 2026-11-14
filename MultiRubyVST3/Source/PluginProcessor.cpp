#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    // The chain you get when you first insert the plugin
    constexpr int defaultTypes[Fx::numSlots] =
    {
        Fx::NoiseGate, Fx::Compressor, Fx::Overdrive, Fx::EQ3, Fx::Chorus,
        Fx::Delay, Fx::Reverb, Fx::Empty, Fx::Empty, Fx::Empty
    };

    juce::String fmtPercent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    float parsePercent (const juce::String& t) { return t.getFloatValue() / 100.0f; }
}

//==============================================================================
MultiRubyAudioProcessor::MultiRubyAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int s = 0; s < Fx::numSlots; ++s)
    {
        sp[(size_t) s].type = apvts.getRawParameterValue (slotId (s, "type"));
        sp[(size_t) s].on   = apvts.getRawParameterValue (slotId (s, "on"));
        sp[(size_t) s].mix  = apvts.getRawParameterValue (slotId (s, "mix"));
        for (int i = 0; i < Fx::numParams; ++i)
            sp[(size_t) s].p[i] = apvts.getRawParameterValue (slotId (s, "p" + juce::String (i + 1)));
        lastType[(size_t) s] = -1;
    }
    pIn  = apvts.getRawParameterValue ("inGain");
    pOut = apvts.getRawParameterValue ("outGain");
}

MultiRubyAudioProcessor::~MultiRubyAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout MultiRubyAudioProcessor::createParameterLayout()
{
    using Attr = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbText = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "inGain", 1 }, "Input Gain",
                    juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, Attr().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "outGain", 1 }, "Output Gain",
                    juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, Attr().withStringFromValueFunction (dbText)));

    const auto names = Fx::typeNames();

    for (int s = 0; s < Fx::numSlots; ++s)
    {
        const juce::String prefix = "Slot " + juce::String (s + 1) + " ";
        const int defType = defaultTypes[s];
        const auto& td = Fx::typeDef (defType);

        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "type"), 1 },
                                                                  prefix + "Effect", names, defType));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotId (s, "on"), 1 },
                                                                prefix + "On", true));

        for (int i = 0; i < Fx::numParams; ++i)
        {
            const float defNorm = td.p[i].name != nullptr ? Fx::toNorm (td.p[i], td.p[i].def) : 0.5f;

            // The displayed text depends on which effect currently sits in the slot
            auto toText = [this, s, i] (float v, int)
            {
                const int t = sp[(size_t) s].type != nullptr ? (int) std::lround (sp[(size_t) s].type->load()) : 0;
                return Fx::format (Fx::typeDef (t).p[i], v);
            };
            auto fromText = [this, s, i] (const juce::String& txt)
            {
                const int t = sp[(size_t) s].type != nullptr ? (int) std::lround (sp[(size_t) s].type->load()) : 0;
                return Fx::parse (Fx::typeDef (t).p[i], txt);
            };

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { slotId (s, "p" + juce::String (i + 1)), 1 },
                prefix + "Param " + juce::String (i + 1),
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), defNorm,
                Attr().withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
        }

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { slotId (s, "mix"), 1 }, prefix + "Mix",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), td.defMix,
            Attr().withStringFromValueFunction (fmtPercent).withValueFromStringFunction (parsePercent)));
    }

    return layout;
}

//==============================================================================
void MultiRubyAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    for (int s = 0; s < Fx::numSlots; ++s)
    {
        for (int t = 1; t < Fx::NumTypes; ++t)
        {
            auto& e = fx[(size_t) s][(size_t) t];
            e = Fx::create (t);
            if (e != nullptr)
            {
                e->prepare (sampleRate);
                e->reset();
            }
        }
        lastType[(size_t) s] = -1;
    }

    lastInGain  = juce::Decibels::decibelsToGain (pIn->load());
    lastOutGain = juce::Decibels::decibelsToGain (pOut->load());
}

bool MultiRubyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void MultiRubyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();
    const int inCh  = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();

    if (inCh < 1 || outCh < 1 || total <= 0)
    {
        buffer.clear();
        return;
    }

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 0.0)
                    bpm = *b;

    const float inGain  = juce::Decibels::decibelsToGain (pIn->load());
    const float outGain = juce::Decibels::decibelsToGain (pOut->load());

    float inPeak = 0.0f, outPeak = 0.0f;

    for (int start = 0; start < total; start += kChunk)
    {
        const int n = juce::jmin (kChunk, total - start);

        // ---- input (mono is fed to both channels) ----
        const float* in0 = buffer.getReadPointer (0) + start;
        const float* in1 = inCh > 1 ? buffer.getReadPointer (1) + start : in0;
        for (int i = 0; i < n; ++i)
        {
            const float g = lastInGain + (inGain - lastInGain) * (float) (i + 1) / (float) n;
            workL[i] = in0[i] * g;
            workR[i] = in1[i] * g;
            inPeak = juce::jmax (inPeak, std::abs (workL[i]), std::abs (workR[i]));
        }
        lastInGain = inGain;

        // ---- the effect chain ----
        for (int s = 0; s < Fx::numSlots; ++s)
        {
            const auto& slot = sp[(size_t) s];
            const int type = juce::jlimit (0, (int) Fx::NumTypes - 1, (int) std::lround (slot.type->load()));

            if (type != lastType[(size_t) s])
            {
                lastType[(size_t) s] = type;
                if (fx[(size_t) s][(size_t) type] != nullptr)
                    fx[(size_t) s][(size_t) type]->reset();
            }

            auto* effect = fx[(size_t) s][(size_t) type].get();
            if (type == Fx::Empty || effect == nullptr || slot.on->load() < 0.5f)
                continue;

            const auto& td = Fx::typeDef (type);
            float r[Fx::numParams];
            for (int i = 0; i < Fx::numParams; ++i)
                r[i] = Fx::toReal (td.p[i], slot.p[i]->load());

            const float mix = slot.mix->load();
            const bool blend = mix < 0.999f;
            if (blend)
            {
                std::copy (workL, workL + n, dryL);
                std::copy (workR, workR + n, dryR);
            }

            effect->process (workL, workR, n, r, bpm);

            if (blend)
                for (int i = 0; i < n; ++i)
                {
                    workL[i] = dryL[i] + (workL[i] - dryL[i]) * mix;
                    workR[i] = dryR[i] + (workR[i] - dryR[i]) * mix;
                }
        }

        // ---- output ----
        float* out0 = buffer.getWritePointer (0) + start;
        float* out1 = outCh > 1 ? buffer.getWritePointer (1) + start : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float g = lastOutGain + (outGain - lastOutGain) * (float) (i + 1) / (float) n;
            float l = workL[i] * g, r = workR[i] * g;
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            outPeak = juce::jmax (outPeak, std::abs (l), std::abs (r));

            if (out1 != nullptr) { out0[i] = l; out1[i] = r; }
            else                  out0[i] = 0.5f * (l + r);
        }
        lastOutGain = outGain;
    }

    inLevel.store (juce::jmax (inLevel.load(), inPeak));
    outLevel.store (juce::jmax (outLevel.load(), outPeak));
}

//==============================================================================
void MultiRubyAudioProcessor::swapSlots (int a, int b)
{
    if (a < 0 || b < 0 || a >= Fx::numSlots || b >= Fx::numSlots || a == b)
        return;

    juce::StringArray suffixes { "type", "on", "p1", "p2", "p3", "p4", "p5", "mix" };
    for (const auto& suf : suffixes)
    {
        auto* pa = apvts.getParameter (slotId (a, suf));
        auto* pb = apvts.getParameter (slotId (b, suf));
        if (pa == nullptr || pb == nullptr) continue;

        const float va = pa->getValue(), vb = pb->getValue();
        pa->beginChangeGesture(); pa->setValueNotifyingHost (vb); pa->endChangeGesture();
        pb->beginChangeGesture(); pb->setValueNotifyingHost (va); pb->endChangeGesture();
    }
}

void MultiRubyAudioProcessor::applyTypeDefaults (int slot)
{
    if (slot < 0 || slot >= Fx::numSlots) return;

    const int type = juce::jlimit (0, (int) Fx::NumTypes - 1, (int) std::lround (sp[(size_t) slot].type->load()));
    const auto& td = Fx::typeDef (type);

    for (int i = 0; i < Fx::numParams; ++i)
    {
        if (td.p[i].name == nullptr) continue;
        if (auto* prm = apvts.getParameter (slotId (slot, "p" + juce::String (i + 1))))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (Fx::toNorm (td.p[i], td.p[i].def));
            prm->endChangeGesture();
        }
    }
    if (auto* prm = apvts.getParameter (slotId (slot, "mix")))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (td.defMix);
        prm->endChangeGesture();
    }
}

//==============================================================================
juce::AudioProcessorEditor* MultiRubyAudioProcessor::createEditor()
{
    return new MultiRubyAudioProcessorEditor (*this);
}

void MultiRubyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MultiRubyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiRubyAudioProcessor();
}
