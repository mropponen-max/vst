#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Effect catalogue: names, categories and the five generic parameters of every effect.
// Slot parameters are stored normalised (0..1) and mapped to real units here, so the
// DSP, the host automation display and the editor all use one source of truth.
namespace Fx
{
    enum Type
    {
        Empty = 0, NoiseGate, Compressor, Booster, Overdrive, Distortion, Fuzz, EQ3, Wah,
        Chorus, Flanger, Phaser, Tremolo, AutoPan, Delay, Reverb, PitchShift, RingMod,
        Bitcrusher, CabSim, ToneStack, Limiter, Utility, NumTypes
    };

    constexpr int numSlots  = 10;
    constexpr int numParams = 5;

    // categories (used for colouring the slots in the editor)
    enum Category { CatNone = 0, CatDynamics, CatDrive, CatTone, CatModulation, CatSpace, CatSpecial };

    struct ParamDef
    {
        const char* name;       // nullptr = parameter not used by this effect
        float minV, maxV, def;  // real units
        bool  logS;             // logarithmic mapping
        float curve;            // power curve for linear mapping (1 = linear)
        const char* unit;
        int   decimals;
        const char* choices;    // "A|B|C" for stepped parameters, else nullptr
    };

    struct TypeDef
    {
        const char* name;
        int category;
        ParamDef p[numParams];
        float defMix;
    };

    #define FX_P(n, mn, mx, df, lg, cv, un, dc) { n, mn, mx, df, lg, cv, un, dc, nullptr }
    #define FX_C(n, ch, df)                     { n, 0.0f, 1.0f, (float) (df), false, 1.0f, "", 0, ch }

    inline const TypeDef& typeDef (int t)
    {
        static const TypeDef defs[NumTypes] =
        {
            { "Empty", CatNone, {}, 1.0f },

            { "Noise Gate", CatDynamics,
              { FX_P ("Threshold", -80.0f, -10.0f, -50.0f, false, 1.0f, "dB", 1),
                FX_P ("Attack",    0.1f, 50.0f, 2.0f,  true,  1.0f, "ms", 1),
                FX_P ("Release",   10.0f, 500.0f, 100.0f, true, 1.0f, "ms", 0),
                FX_P ("Hold",      0.0f, 300.0f, 20.0f, false, 1.0f, "ms", 0),
                FX_P ("Range",     -80.0f, 0.0f, -80.0f, false, 1.0f, "dB", 0) }, 1.0f },

            { "Compressor", CatDynamics,
              { FX_P ("Threshold", -60.0f, 0.0f, -20.0f, false, 1.0f, "dB", 1),
                FX_P ("Ratio",     1.0f, 20.0f, 4.0f, true, 1.0f, ":1", 1),
                FX_P ("Attack",    0.1f, 100.0f, 10.0f, true, 1.0f, "ms", 1),
                FX_P ("Release",   10.0f, 1000.0f, 150.0f, true, 1.0f, "ms", 0),
                FX_P ("Makeup",    0.0f, 24.0f, 6.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Booster", CatDrive,
              { FX_P ("Gain", -12.0f, 30.0f, 10.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Overdrive", CatDrive,
              { FX_P ("Drive",   0.0f, 40.0f, 18.0f, false, 1.0f, "dB", 1),
                FX_P ("Tone",    500.0f, 8000.0f, 3500.0f, true, 1.0f, "Hz", 0),
                FX_P ("Level",   -30.0f, 6.0f, -8.0f, false, 1.0f, "dB", 1),
                FX_P ("Bias",    0.0f, 1.0f, 0.2f, false, 1.0f, "", 2),
                FX_P ("Low Cut", 20.0f, 500.0f, 120.0f, true, 1.0f, "Hz", 0) }, 1.0f },

            { "Distortion", CatDrive,
              { FX_P ("Drive",    10.0f, 50.0f, 32.0f, false, 1.0f, "dB", 1),
                FX_P ("Tone",     500.0f, 10000.0f, 4000.0f, true, 1.0f, "Hz", 0),
                FX_P ("Level",    -30.0f, 6.0f, -10.0f, false, 1.0f, "dB", 1),
                FX_P ("Low Cut",  20.0f, 500.0f, 100.0f, true, 1.0f, "Hz", 0),
                FX_P ("Hardness", 0.0f, 1.0f, 0.5f, false, 1.0f, "", 2) }, 1.0f },

            { "Fuzz", CatDrive,
              { FX_P ("Fuzz",   0.0f, 1.0f, 0.7f, false, 1.0f, "", 2),
                FX_P ("Tone",   500.0f, 10000.0f, 3000.0f, true, 1.0f, "Hz", 0),
                FX_P ("Level",  -30.0f, 6.0f, -12.0f, false, 1.0f, "dB", 1),
                FX_P ("Sag",    0.0f, 1.0f, 0.3f, false, 1.0f, "", 2),
                FX_P ("Octave", 0.0f, 1.0f, 0.0f, false, 1.0f, "", 2) }, 1.0f },

            { "EQ 3-Band", CatTone,
              { FX_P ("Low",      -15.0f, 15.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Mid",      -15.0f, 15.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Mid Freq", 200.0f, 5000.0f, 800.0f, true, 1.0f, "Hz", 0),
                FX_P ("High",     -15.0f, 15.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Level",    -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Wah / Auto Wah", CatTone,
              { FX_P ("Pedal",    0.0f, 1.0f, 0.3f, false, 1.0f, "", 2),
                FX_P ("Q",        1.0f, 12.0f, 5.0f, true, 1.0f, "", 1),
                FX_P ("LFO",      0.0f, 1.0f, 0.0f, false, 1.0f, "", 2),
                FX_P ("LFO Rate", 0.1f, 10.0f, 1.5f, true, 1.0f, "Hz", 2),
                FX_P ("Envelope", 0.0f, 1.0f, 0.6f, false, 1.0f, "", 2) }, 1.0f },

            { "Chorus", CatModulation,
              { FX_P ("Rate",     0.1f, 5.0f, 0.8f, true, 1.0f, "Hz", 2),
                FX_P ("Depth",    0.0f, 10.0f, 3.0f, false, 1.0f, "ms", 1),
                FX_P ("Delay",    5.0f, 30.0f, 15.0f, false, 1.0f, "ms", 1),
                FX_P ("Spread",   0.0f, 1.0f, 0.5f, false, 1.0f, "", 2),
                FX_P ("Feedback", 0.0f, 0.5f, 0.0f, false, 1.0f, "", 2) }, 0.5f },

            { "Flanger", CatModulation,
              { FX_P ("Rate",     0.05f, 5.0f, 0.3f, true, 1.0f, "Hz", 2),
                FX_P ("Depth",    0.0f, 5.0f, 2.0f, false, 1.0f, "ms", 2),
                FX_P ("Delay",    0.5f, 8.0f, 2.0f, false, 1.0f, "ms", 2),
                FX_P ("Spread",   0.0f, 1.0f, 0.5f, false, 1.0f, "", 2),
                FX_P ("Feedback", -0.95f, 0.95f, 0.5f, false, 1.0f, "", 2) }, 0.5f },

            { "Phaser", CatModulation,
              { FX_P ("Rate",     0.05f, 8.0f, 0.5f, true, 1.0f, "Hz", 2),
                FX_P ("Depth",    0.0f, 1.0f, 0.8f, false, 1.0f, "", 2),
                FX_P ("Center",   200.0f, 3000.0f, 700.0f, true, 1.0f, "Hz", 0),
                FX_P ("Feedback", 0.0f, 0.95f, 0.4f, false, 1.0f, "", 2),
                FX_P ("Stages",   2.0f, 12.0f, 6.0f, false, 1.0f, "", 0) }, 1.0f },

            { "Tremolo", CatModulation,
              { FX_P ("Rate",   0.5f, 15.0f, 5.0f, true, 1.0f, "Hz", 2),
                FX_P ("Depth",  0.0f, 1.0f, 0.6f, false, 1.0f, "", 2),
                FX_P ("Shape",  0.0f, 1.0f, 0.2f, false, 1.0f, "", 2),
                FX_P ("Stereo", 0.0f, 1.0f, 0.0f, false, 1.0f, "", 2) }, 1.0f },

            { "Auto Pan", CatModulation,
              { FX_P ("Rate",  0.1f, 10.0f, 1.0f, true, 1.0f, "Hz", 2),
                FX_P ("Depth", 0.0f, 1.0f, 1.0f, false, 1.0f, "", 2),
                FX_P ("Shape", 0.0f, 1.0f, 0.2f, false, 1.0f, "", 2) }, 1.0f },

            { "Delay", CatSpace,
              { FX_P ("Time",     20.0f, 1500.0f, 380.0f, true, 1.0f, "ms", 0),
                FX_P ("Feedback", 0.0f, 0.95f, 0.4f, false, 1.0f, "", 2),
                FX_P ("Tone",     800.0f, 12000.0f, 5000.0f, true, 1.0f, "Hz", 0),
                FX_C ("Sync",     "Off|1/4|1/8|1/8.|1/16|1/8T|1/2|1/4.", 0),
                FX_P ("PingPong", 0.0f, 1.0f, 0.0f, false, 1.0f, "", 2) }, 0.3f },

            { "Reverb", CatSpace,
              { FX_P ("Size",      0.0f, 1.0f, 0.5f, false, 1.0f, "", 2),
                FX_P ("Damping",   0.0f, 1.0f, 0.5f, false, 1.0f, "", 2),
                FX_P ("Width",     0.0f, 1.0f, 1.0f, false, 1.0f, "", 2),
                FX_P ("Pre-delay", 0.0f, 200.0f, 20.0f, false, 1.0f, "ms", 0),
                FX_P ("Tone",      1500.0f, 16000.0f, 8000.0f, true, 1.0f, "Hz", 0) }, 0.3f },

            { "Pitch Shifter", CatSpecial,
              { FX_P ("Pitch",  -24.0f, 24.0f, 12.0f, false, 1.0f, "st", 1),
                FX_P ("Fine",   -50.0f, 50.0f, 0.0f, false, 1.0f, "cents", 0),
                FX_P ("Window", 20.0f, 120.0f, 60.0f, false, 1.0f, "ms", 0) }, 0.5f },

            { "Ring Mod", CatSpecial,
              { FX_P ("Freq",      20.0f, 3000.0f, 300.0f, true, 1.0f, "Hz", 0),
                FX_P ("Shape",     0.0f, 1.0f, 0.0f, false, 1.0f, "", 2),
                FX_P ("LFO Depth", 0.0f, 1.0f, 0.0f, false, 1.0f, "", 2),
                FX_P ("LFO Rate",  0.1f, 20.0f, 2.0f, true, 1.0f, "Hz", 2) }, 0.6f },

            { "Bitcrusher", CatSpecial,
              { FX_P ("Bits",       2.0f, 16.0f, 8.0f, false, 1.0f, "bit", 1),
                FX_P ("Downsample", 1.0f, 40.0f, 4.0f, true, 1.0f, "x", 0),
                FX_P ("Tone",       1000.0f, 16000.0f, 8000.0f, true, 1.0f, "Hz", 0) }, 1.0f },

            { "Cab Sim", CatTone,
              { FX_C ("Cabinet",   "1x12 Vintage|2x12 Open|4x12 Modern|Bright Combo", 2),
                FX_P ("Low Cut",   50.0f, 300.0f, 80.0f, true, 1.0f, "Hz", 0),
                FX_P ("High Cut",  2000.0f, 9000.0f, 5000.0f, true, 1.0f, "Hz", 0),
                FX_P ("Presence",  -6.0f, 6.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Resonance", 0.0f, 8.0f, 3.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Amp Tone Stack", CatTone,
              { FX_P ("Bass",     -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Mid",      -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Treble",   -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Presence", -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Level",    -12.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Limiter", CatDynamics,
              { FX_P ("Threshold", -30.0f, 0.0f, -3.0f, false, 1.0f, "dB", 1),
                FX_P ("Release",   10.0f, 500.0f, 80.0f, true, 1.0f, "ms", 0),
                FX_P ("Makeup",    0.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1) }, 1.0f },

            { "Utility", CatNone,
              { FX_P ("Gain",   -60.0f, 12.0f, 0.0f, false, 1.0f, "dB", 1),
                FX_P ("Pan",    -1.0f, 1.0f, 0.0f, false, 1.0f, "", 2),
                FX_P ("Width",  0.0f, 2.0f, 1.0f, false, 1.0f, "", 2),
                FX_C ("Mono",   "Off|On", 0),
                FX_C ("Invert", "Off|On", 0) }, 1.0f },
        };

        return defs[juce::jlimit (0, (int) NumTypes - 1, t)];
    }

    inline int numChoices (const char* c)
    {
        int n = 1;
        for (; *c != 0; ++c)
            if (*c == '|') ++n;
        return n;
    }

    inline juce::String choiceName (const char* c, int idx)
    {
        juce::StringArray a;
        a.addTokens (juce::String (c), "|", "");
        return a[juce::jlimit (0, a.size() - 1, idx)];
    }

    inline float toReal (const ParamDef& d, float norm)
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        if (d.choices != nullptr)
            return (float) juce::roundToInt (norm * (float) (numChoices (d.choices) - 1));
        if (d.logS)
            return d.minV * std::pow (d.maxV / d.minV, norm);
        return d.minV + (d.maxV - d.minV) * std::pow (norm, d.curve);
    }

    inline float toNorm (const ParamDef& d, float real)
    {
        if (d.choices != nullptr)
        {
            const int n = numChoices (d.choices);
            return n > 1 ? juce::jlimit (0.0f, 1.0f, real / (float) (n - 1)) : 0.0f;
        }
        real = juce::jlimit (juce::jmin (d.minV, d.maxV), juce::jmax (d.minV, d.maxV), real);
        if (d.logS)
            return std::log (real / d.minV) / std::log (d.maxV / d.minV);
        const float t = (real - d.minV) / (d.maxV - d.minV);
        return std::pow (juce::jlimit (0.0f, 1.0f, t), 1.0f / d.curve);
    }

    inline juce::String format (const ParamDef& d, float norm)
    {
        if (d.name == nullptr) return {};
        if (d.choices != nullptr) return choiceName (d.choices, (int) toReal (d, norm));
        const float v = toReal (d, norm);
        // note: String(x, 0) would print the default precision, so round explicitly
        auto s = d.decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, d.decimals);
        return *d.unit != 0 ? s + " " + d.unit : s;
    }

    inline float parse (const ParamDef& d, const juce::String& text)
    {
        if (d.name == nullptr) return 0.0f;
        if (d.choices != nullptr)
        {
            juce::StringArray a;
            a.addTokens (juce::String (d.choices), "|", "");
            for (int i = 0; i < a.size(); ++i)
                if (a[i].equalsIgnoreCase (text.trim()))
                    return toNorm (d, (float) i);
            return toNorm (d, text.getFloatValue());
        }
        return toNorm (d, text.retainCharacters ("0123456789.-").getFloatValue());
    }

    inline juce::StringArray typeNames()
    {
        juce::StringArray a;
        for (int t = 0; t < NumTypes; ++t)
            a.add (typeDef (t).name);
        return a;
    }
}
