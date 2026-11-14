// Offline sanity test: every effect, many parameter settings, odd block sizes.
#include "PluginProcessor.h"
#include <cstdio>
#include <random>

static void setNorm (MultiRubyAudioProcessor& p, const juce::String& id, float v)
{
    p.apvts.getParameter (id)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

static void setType (MultiRubyAudioProcessor& p, int slot, int type)
{
    auto* prm = p.apvts.getParameter (MultiRubyAudioProcessor::slotId (slot, "type"));
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) type));
}

struct Stats { float peak = 0, rms = 0; bool bad = false; };

static Stats run (MultiRubyAudioProcessor& p, int blockSize, int totalSamples, double sr, float& phase, int& pos)
{
    Stats st; double sum = 0; long cnt = 0;
    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;
    int done = 0;
    while (done < totalSamples)
    {
        const int n = juce::jmin (blockSize, totalSamples - done);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i, ++pos)
        {
            // plucked 110 Hz "guitar": harmonics, re-plucked every 0.5 s, plus a little noise
            const double t = std::fmod ((double) pos / sr, 0.5);
            const float env = (float) std::exp (-t * 4.0);
            phase += 110.0f / (float) sr; if (phase > 1.0f) phase -= 1.0f;
            float s = 0;
            for (int h = 1; h <= 6; ++h) s += std::sin (6.2831853f * phase * (float) h) / (float) h;
            s = 0.4f * env * s + 0.0005f * (float) ((pos * 7919) % 200 - 100) / 100.0f;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v) || std::abs (v) > 50.0f) st.bad = true;
                st.peak = juce::jmax (st.peak, std::abs (v));
                sum += (double) v * v; ++cnt;
            }
        done += n;
    }
    st.rms = (float) std::sqrt (sum / (double) juce::jmax (1L, cnt));
    return st;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;

    MultiRubyAudioProcessor p;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);

    for (int s = 0; s < Fx::numSlots; ++s) setType (p, s, 0);

    int failures = 0;
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    const int blocks[] = { 512, 1, 7, 64, 1000 };

    std::printf ("%-16s %10s %10s %10s\n", "effect", "rms(def)", "peak(def)", "peak(max)");
    for (int t = 1; t < Fx::NumTypes; ++t)
    {
        setType (p, 0, t);
        float defPeak = 0, defRms = 0, worstPeak = 0;
        bool bad = false;

        for (int variant = 0; variant < 12; ++variant)
        {
            const auto& td = Fx::typeDef (t);
            for (int i = 0; i < Fx::numParams; ++i)
            {
                float v;
                if (variant == 0)       v = td.p[i].name ? Fx::toNorm (td.p[i], td.p[i].def) : 0.5f;
                else if (variant == 1)  v = 0.0f;
                else if (variant == 2)  v = 1.0f;
                else                    v = uni (rng);
                setNorm (p, MultiRubyAudioProcessor::slotId (0, "p" + juce::String (i + 1)), v);
            }
            setNorm (p, MultiRubyAudioProcessor::slotId (0, "mix"), variant == 0 ? td.defMix : 1.0f);

            float phase = 0; int pos = 0;
            auto st = run (p, blocks[variant % 5], (int) (sr * 1.5), sr, phase, pos);
            if (variant == 0) { defPeak = st.peak; defRms = st.rms; }
            worstPeak = juce::jmax (worstPeak, st.peak);
            bad = bad || st.bad;
        }
        std::printf ("%-16s %10.4f %10.4f %10.4f %s\n", Fx::typeDef (t).name, defRms, defPeak, worstPeak, bad ? "  <-- BAD" : "");
        failures += bad ? 1 : 0;
    }
    setType (p, 0, 0);

    // default chain, plus swapping and re-defaulting
    {
        MultiRubyAudioProcessor q;
        q.setPlayConfigDetails (2, 2, sr, 512);
        q.prepareToPlay (sr, 512);
        float phase = 0; int pos = 0;
        auto st = run (q, 480, (int) (sr * 2), sr, phase, pos);
        q.swapSlots (0, 1); q.swapSlots (6, 7); q.applyTypeDefaults (3);
        auto st2 = run (q, 333, (int) (sr * 1), sr, phase, pos);
        std::printf ("default chain: rms %.4f peak %.4f | after swap: rms %.4f peak %.4f %s\n",
                     st.rms, st.peak, st2.rms, st2.peak, (st.bad || st2.bad) ? "<-- BAD" : "");
        failures += (st.bad || st2.bad) ? 1 : 0;

        // state round trip
        juce::MemoryBlock mb; q.getStateInformation (mb);
        MultiRubyAudioProcessor q2; q2.setStateInformation (mb.getData(), (int) mb.getSize());
        const float t0 = q.apvts.getRawParameterValue ("s1_type")->load();
        const float t1 = q2.apvts.getRawParameterValue ("s1_type")->load();
        std::printf ("state round trip slot1 type: %.0f -> %.0f\n", t0, t1);
        failures += (t0 != t1);
    }

    // every effect in every slot at once (stress)
    {
        MultiRubyAudioProcessor q;
        q.setPlayConfigDetails (2, 2, sr, 512);
        q.prepareToPlay (sr, 512);
        for (int s = 0; s < Fx::numSlots; ++s) { setType (q, s, 1 + (s * 2) % (Fx::NumTypes - 1)); q.applyTypeDefaults (s); }
        float phase = 0; int pos = 0;
        auto st = run (q, 256, (int) (sr * 3), sr, phase, pos);
        std::printf ("10-slot stress: rms %.4f peak %.4f %s\n", st.rms, st.peak, st.bad ? "<-- BAD" : "");
        failures += st.bad ? 1 : 0;
    }

    std::printf (failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
