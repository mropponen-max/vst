#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxInfo.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// All effects work on a stereo pair of float buffers, in place.
// `r` holds the five REAL parameter values (see FxInfo.h), constant for the block.
namespace Fx
{
    constexpr float kPi    = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717959f;

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float msCoef (double sr, float ms)  { return 1.0f - std::exp (-1.0f / (0.001f * ms * (float) sr)); }
    inline float hzCoef (double sr, float hz)  { return 1.0f - std::exp (-kTwoPi * hz / (float) sr); }
    inline float clampFreq (float f, double sr) { return juce::jlimit (10.0f, (float) (0.49 * sr), f); }

    //==========================================================================
    struct Smooth
    {
        float v = 0.0f; bool init = false;
        float next (float target, float coef)
        {
            if (! init) { v = target; init = true; }
            v += coef * (target - v);
            return v;
        }
        void reset() { init = false; }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

        inline float process (int ch, float x)
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }

        void set (float B0, float B1, float B2, float A0, float A1, float A2)
        {
            const float i = 1.0f / A0;
            b0 = B0 * i; b1 = B1 * i; b2 = B2 * i; a1 = A1 * i; a2 = A2 * i;
        }

        void lowpass (double sr, float f, float q)
        {
            f = clampFreq (f, sr);
            const float w = kTwoPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
            set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
        }
        void highpass (double sr, float f, float q)
        {
            f = clampFreq (f, sr);
            const float w = kTwoPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
            set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
        }
        void peak (double sr, float f, float q, float gainDb)
        {
            f = clampFreq (f, sr);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w = kTwoPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void lowShelf (double sr, float f, float gainDb)
        {
            f = clampFreq (f, sr);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w = kTwoPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
            const float beta = 2.0f * std::sqrt (A) * (s / 2.0f * 1.41421356f);
            set (A * ((A + 1) - (A - 1) * c + beta),
                 2 * A * ((A - 1) - (A + 1) * c),
                 A * ((A + 1) - (A - 1) * c - beta),
                 (A + 1) + (A - 1) * c + beta,
                 -2 * ((A - 1) + (A + 1) * c),
                 (A + 1) + (A - 1) * c - beta);
        }
        void highShelf (double sr, float f, float gainDb)
        {
            f = clampFreq (f, sr);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w = kTwoPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
            const float beta = 2.0f * std::sqrt (A) * (s / 2.0f * 1.41421356f);
            set (A * ((A + 1) + (A - 1) * c + beta),
                 -2 * A * ((A - 1) + (A + 1) * c),
                 A * ((A + 1) + (A - 1) * c - beta),
                 (A + 1) - (A - 1) * c + beta,
                 2 * ((A - 1) - (A + 1) * c),
                 (A + 1) - (A - 1) * c - beta);
        }
    };

    // Zero-delay-feedback state variable filter (bandpass tap only, that is all we need)
    struct Svf
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
        float ic1[2] = { 0, 0 }, ic2[2] = { 0, 0 };

        void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }
        void set (double sr, float f, float q)
        {
            f = juce::jlimit (20.0f, (float) (0.45 * sr), f);
            g = std::tan (kPi * f / (float) sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        inline float bandpass (int ch, float x)
        {
            const float v3 = x - ic2[ch];
            const float v1 = a1 * ic1[ch] + a2 * v3;
            const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
            ic1[ch] = 2 * v1 - ic1[ch];
            ic2[ch] = 2 * v2 - ic2[ch];
            return v1;
        }
    };

    struct DcBlock
    {
        float x[2] = { 0, 0 }, y[2] = { 0, 0 };
        void reset() { x[0] = x[1] = y[0] = y[1] = 0.0f; }
        inline float process (int ch, float in)
        {
            const float out = in - x[ch] + 0.995f * y[ch];
            x[ch] = in; y[ch] = out;
            return out;
        }
    };

    struct DelayLine
    {
        std::vector<float> buf;
        int size = 0, w = 0;

        void init (int n) { size = juce::jmax (4, n); buf.assign ((size_t) size, 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        inline void push (float x) { buf[(size_t) w] = x; if (++w >= size) w = 0; }

        // d = number of samples back (1 = the sample pushed last)
        inline float read (float d) const
        {
            d = juce::jlimit (1.0f, (float) (size - 2), d);
            float pos = (float) w - d;
            if (pos < 0.0f) pos += (float) size;
            const int fl = (int) pos;
            const float f = pos - (float) fl;           // fraction must come from the unwrapped index
            int i0 = fl >= size ? fl - size : fl;
            int i1 = i0 + 1;
            if (i1 >= size) i1 = 0;
            return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
        }
    };

    //==========================================================================
    struct Base
    {
        virtual ~Base() = default;
        virtual void prepare (double sampleRate) { sr = sampleRate; }
        virtual void reset() {}
        virtual void process (float* L, float* R, int n, const float* r, double bpm) = 0;
        double sr = 44100.0;
    };

    //==========================================================================
    struct NoiseGateFx : Base
    {
        float env = 0, g = 1; int hold = 0;
        void reset() override { env = 0; g = 1; hold = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float thr = dbToGain (r[0]), att = msCoef (sr, r[1]), rel = msCoef (sr, r[2]);
            const int holdS = (int) (r[3] * 0.001 * sr);
            const float floorG = dbToGain (r[4]), envRel = msCoef (sr, 30.0f);
            for (int i = 0; i < n; ++i)
            {
                const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
                env = lvl > env ? lvl : env + envRel * (lvl - env);
                float target;
                if (env > thr)       { hold = holdS; target = 1.0f; }
                else if (hold > 0)   { --hold; target = 1.0f; }
                else                 target = floorG;
                g += (target > g ? att : rel) * (target - g);
                L[i] *= g; R[i] *= g;
            }
        }
    };

    struct CompressorFx : Base
    {
        float grDb = 0;
        void reset() override { grDb = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float thr = r[0], slope = 1.0f - 1.0f / r[1];
            const float att = msCoef (sr, r[2]), rel = msCoef (sr, r[3]), makeup = r[4];
            for (int i = 0; i < n; ++i)
            {
                const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
                const float over = 20.0f * std::log10 (lvl + 1.0e-6f) - thr;
                const float tgt = over > 0.0f ? over * slope : 0.0f;
                grDb += (tgt > grDb ? att : rel) * (tgt - grDb);
                const float g = dbToGain (makeup - grDb);
                L[i] *= g; R[i] *= g;
            }
        }
    };

    struct BoosterFx : Base
    {
        Smooth g;
        void reset() override { g.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float target = dbToGain (r[0]), c = msCoef (sr, 10.0f);
            for (int i = 0; i < n; ++i)
            {
                const float gg = g.next (target, c);
                L[i] *= gg; R[i] *= gg;
            }
        }
    };

    // ---- overdrive: high-pass -> asymmetric tanh -> dc block -> tone -> level
    struct OverdriveFx : Base
    {
        Biquad hp, lp; DcBlock dc;
        void reset() override { hp.reset(); lp.reset(); dc.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            hp.highpass (sr, r[4], 0.707f);
            lp.lowpass  (sr, r[1], 0.707f);
            const float drive = dbToGain (r[0]), bias = r[3] * 0.6f, level = dbToGain (r[2]), bt = std::tanh (bias);
            float* buf[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    float x = hp.process (ch, buf[ch][i]);
                    float y = std::tanh (x * drive + bias) - bt;
                    y = dc.process (ch, y);
                    buf[ch][i] = lp.process (ch, y) * level;
                }
        }
    };

    // ---- distortion: tighter, harder clipping
    struct DistortionFx : Base
    {
        Biquad hp, lp; DcBlock dc;
        void reset() override { hp.reset(); lp.reset(); dc.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            hp.highpass (sr, r[3], 0.707f);
            lp.lowpass  (sr, r[1], 0.707f);
            const float drive = dbToGain (r[0]), level = dbToGain (r[2]), hard = r[4];
            float* buf[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    const float x = hp.process (ch, buf[ch][i]) * drive;
                    const float soft = std::tanh (x);
                    const float clipped = juce::jlimit (-1.0f, 1.0f, x);
                    float y = soft * (1.0f - hard) + clipped * hard;
                    y = dc.process (ch, y);
                    buf[ch][i] = lp.process (ch, y) * level;
                }
        }
    };

    // ---- fuzz: asymmetric hard clipping with optional octave-up (rectifier) blend
    struct FuzzFx : Base
    {
        Biquad lp; DcBlock dc, dc2;
        void reset() override { lp.reset(); dc.reset(); dc2.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            lp.lowpass (sr, r[1], 0.707f);
            const float gain = dbToGain (10.0f + r[0] * 50.0f), level = dbToGain (r[2]);
            const float sag = r[3] * 0.4f, oct = r[4];
            float* buf[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    float x = std::tanh (buf[ch][i] * gain + sag);
                    x = x > 0.0f ? std::min (x, 0.6f) : std::max (x, -1.0f);       // asymmetric clip
                    x = dc.process (ch, x);
                    const float rect = dc2.process (ch, std::abs (x) * 2.0f);       // octave-up
                    float y = x * (1.0f - 0.5f * oct) + rect * oct * 0.6f;
                    buf[ch][i] = lp.process (ch, y) * level;
                }
        }
    };

    struct EQ3Fx : Base
    {
        Biquad lo, mid, hi; Smooth lv;
        void reset() override { lo.reset(); mid.reset(); hi.reset(); lv.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            lo.lowShelf (sr, 100.0f, r[0]);
            mid.peak (sr, r[2], 0.8f, r[1]);
            hi.highShelf (sr, 4000.0f, r[3]);
            const float target = dbToGain (r[4]), c = msCoef (sr, 10.0f);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                const float g = lv.next (target, c);
                for (int ch = 0; ch < 2; ++ch)
                    buf[ch][i] = hi.process (ch, mid.process (ch, lo.process (ch, buf[ch][i]))) * g;
            }
        }
    };

    struct WahFx : Base
    {
        Svf svf; float env = 0, ph = 0;
        void reset() override { svf.reset(); env = 0; ph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float pedal = r[0], q = r[1], lfoAmt = r[2], rate = r[3], envAmt = r[4];
            const float atk = msCoef (sr, 4.0f), rel = msCoef (sr, 90.0f), outG = (1.0f / q) * (1.0f + 0.35f * q);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                const float lvl = std::abs (0.5f * (L[i] + R[i]));
                env += (lvl > env ? atk : rel) * (lvl - env);
                if ((i & 7) == 0)
                {
                    float pos = pedal + lfoAmt * 0.5f * std::sin (kTwoPi * ph) + envAmt * std::min (1.0f, env * 6.0f);
                    pos = juce::jlimit (0.0f, 1.0f, pos);
                    svf.set (sr, 350.0f * std::pow (2200.0f / 350.0f, pos), q);
                }
                ph += rate / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
                for (int ch = 0; ch < 2; ++ch)
                    buf[ch][i] = svf.bandpass (ch, buf[ch][i]) * outG;
            }
        }
    };

    // chorus & flanger share the same engine (only the parameter ranges differ)
    struct ModDelayFx : Base
    {
        DelayLine d[2]; float ph = 0;
        void prepare (double s) override { Base::prepare (s); d[0].init ((int) (0.12 * s) + 8); d[1].init ((int) (0.12 * s) + 8); }
        void reset() override { d[0].clear(); d[1].clear(); ph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float rate = r[0], depth = r[1], delayMs = r[2], spread = r[3], fb = r[4];
            const float inc = rate / (float) sr, msToS = 0.001f * (float) sr;
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                const float m[2] = { std::sin (kTwoPi * ph), std::sin (kTwoPi * (ph + spread * 0.25f)) };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float dl = std::max (1.0f, (delayMs + depth * m[ch]) * msToS);
                    const float wet = d[ch].read (dl);
                    d[ch].push (buf[ch][i] + wet * fb);
                    buf[ch][i] = wet;
                }
                ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            }
        }
    };

    struct PhaserFx : Base
    {
        float ph = 0, z[12][2] = {}, fbState[2] = { 0, 0 };
        void reset() override { ph = 0; for (auto& s : z) s[0] = s[1] = 0.0f; fbState[0] = fbState[1] = 0.0f; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float rate = r[0], depth = r[1], centre = r[2], fb = r[3];
            const int stages = juce::jlimit (2, 12, 2 * juce::roundToInt (r[4] * 0.5f));
            float a[2] = { 0, 0 };
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                if ((i & 7) == 0)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float lfo = std::sin (kTwoPi * (ph + 0.25f * (float) ch));
                        const float f = juce::jlimit (30.0f, (float) (0.45 * sr), centre * std::pow (2.0f, depth * 2.0f * lfo));
                        const float t = std::tan (kPi * f / (float) sr);
                        a[ch] = (t - 1.0f) / (t + 1.0f);
                    }
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = buf[ch][i];
                    float y = x + fb * fbState[ch];
                    for (int s = 0; s < stages; ++s)
                    {
                        const float o = a[ch] * y + z[s][ch];
                        z[s][ch] = y - a[ch] * o;
                        y = o;
                    }
                    fbState[ch] = juce::jlimit (-4.0f, 4.0f, y);
                    buf[ch][i] = 0.5f * (x + y);
                }
                ph += rate / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
            }
        }
    };

    struct TremoloFx : Base
    {
        float ph = 0;
        void reset() override { ph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float rate = r[0], depth = r[1], shape = r[2], stereo = r[3];
            const float k = 1.0f + shape * 10.0f, norm = 1.0f / std::tanh (k);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float s = std::sin (kTwoPi * (ph + (ch == 1 ? stereo * 0.5f : 0.0f)));
                    const float u = 0.5f + 0.5f * std::tanh (s * k) * norm;
                    buf[ch][i] *= 1.0f - depth * (1.0f - u);
                }
                ph += rate / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
            }
        }
    };

    struct AutoPanFx : Base
    {
        float ph = 0;
        void reset() override { ph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float rate = r[0], depth = r[1], shape = r[2];
            const float k = 1.0f + shape * 10.0f, norm = 1.0f / std::tanh (k);
            for (int i = 0; i < n; ++i)
            {
                const float pan = depth * std::tanh (std::sin (kTwoPi * ph) * k) * norm;
                const float ang = (pan + 1.0f) * 0.25f * kPi;
                L[i] *= std::cos (ang) * 1.41421356f;
                R[i] *= std::sin (ang) * 1.41421356f;
                ph += rate / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
            }
        }
    };

    struct DelayFx : Base
    {
        DelayLine d[2]; float dt = -1.0f, lpS[2] = { 0, 0 };
        void prepare (double s) override { Base::prepare (s); d[0].init ((int) (2.0 * s) + 8); d[1].init ((int) (2.0 * s) + 8); }
        void reset() override { d[0].clear(); d[1].clear(); dt = -1.0f; lpS[0] = lpS[1] = 0.0f; }
        void process (float* L, float* R, int n, const float* r, double bpm) override
        {
            static const float mult[8] = { 0.0f, 1.0f, 0.5f, 0.75f, 0.25f, 1.0f / 3.0f, 2.0f, 1.5f };
            const int sync = juce::jlimit (0, 7, (int) r[3]);
            float ms = sync > 0 ? (float) (60000.0 / juce::jmax (20.0, bpm)) * mult[sync] : r[0];
            ms = juce::jlimit (5.0f, 1900.0f, ms);
            const float target = ms * 0.001f * (float) sr;
            if (dt < 0.0f) dt = target;
            const float smooth = msCoef (sr, 60.0f), fb = r[1], lpC = hzCoef (sr, r[2]), pp = r[4];
            for (int i = 0; i < n; ++i)
            {
                dt += smooth * (target - dt);
                const float dl = d[0].read (dt), dr = d[1].read (dt);
                lpS[0] += lpC * (dl - lpS[0]);
                lpS[1] += lpC * (dr - lpS[1]);
                const float fl = lpS[0], fr = lpS[1];
                const float wL = fl * (1.0f - pp) + fr * pp;
                const float wR = fr * (1.0f - pp) + fl * pp;
                const float m = 0.5f * (L[i] + R[i]);
                const float inL = L[i] * (1.0f - pp) + m * pp, inR = R[i] * (1.0f - pp);
                d[0].push (inL + wL * fb);
                d[1].push (inR + wR * fb);
                L[i] = dl; R[i] = dr;
            }
        }
    };

    struct ReverbFx : Base
    {
        juce::Reverb rev; DelayLine pre[2]; Biquad lp;
        void prepare (double s) override
        {
            Base::prepare (s);
            rev.setSampleRate (s);
            pre[0].init ((int) (0.21 * s) + 8); pre[1].init ((int) (0.21 * s) + 8);
        }
        void reset() override { rev.reset(); pre[0].clear(); pre[1].clear(); lp.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float preS = std::max (1.0f, r[3] * 0.001f * (float) sr);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float y = pre[ch].read (preS);
                    pre[ch].push (buf[ch][i]);
                    buf[ch][i] = y;
                }

            juce::Reverb::Parameters p;
            p.roomSize = r[0]; p.damping = r[1]; p.width = r[2];
            p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.freezeMode = 0.0f;
            rev.setParameters (p);
            rev.processStereo (L, R, n);

            lp.lowpass (sr, r[4], 0.707f);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buf[ch][i] = lp.process (ch, buf[ch][i]);
        }
    };

    // classic two-tap crossfaded delay-line pitch shifter
    struct PitchShiftFx : Base
    {
        DelayLine d[2]; float ph = 0;
        void prepare (double s) override { Base::prepare (s); d[0].init ((int) (0.3 * s) + 8); d[1].init ((int) (0.3 * s) + 8); }
        void reset() override { d[0].clear(); d[1].clear(); ph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float st = r[0] + r[1] * 0.01f, ratio = std::pow (2.0f, st / 12.0f);
            const float W = std::max (64.0f, r[2] * 0.001f * (float) sr);
            const float inc = (1.0f - ratio) / W;
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                ph += inc; ph -= std::floor (ph);
                float ph2 = ph + 0.5f; ph2 -= std::floor (ph2);
                const float g1 = std::sin (kPi * ph), g2 = std::sin (kPi * ph2);
                const float d1 = 1.0f + ph * W, d2 = 1.0f + ph2 * W;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float wet = d[ch].read (d1) * g1 + d[ch].read (d2) * g2;
                    d[ch].push (buf[ch][i]);
                    buf[ch][i] = wet;
                }
            }
        }
    };

    struct RingModFx : Base
    {
        float ph = 0, lph = 0;
        void reset() override { ph = 0; lph = 0; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float freq = r[0], shape = r[1], depth = r[2], lrate = r[3];
            const float k = 1.0f + shape * 12.0f, norm = 1.0f / std::tanh (k);
            for (int i = 0; i < n; ++i)
            {
                const float f = freq * (1.0f + depth * 0.9f * std::sin (kTwoPi * lph));
                const float c = std::tanh (std::sin (kTwoPi * ph) * k) * norm;
                L[i] *= c; R[i] *= c;
                ph += f / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
                lph += lrate / (float) sr; if (lph >= 1.0f) lph -= 1.0f;
            }
        }
    };

    struct BitcrusherFx : Base
    {
        int cnt = 0; float held[2] = { 0, 0 }; Biquad lp;
        void reset() override { cnt = 0; held[0] = held[1] = 0.0f; lp.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float levels = std::pow (2.0f, r[0] - 1.0f);
            const int down = std::max (1, juce::roundToInt (r[1]));
            lp.lowpass (sr, r[2], 0.707f);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                if (++cnt >= down)
                {
                    cnt = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        held[ch] = std::round (buf[ch][i] * levels) / levels;
                }
                for (int ch = 0; ch < 2; ++ch)
                    buf[ch][i] = lp.process (ch, held[ch]);
            }
        }
    };

    struct CabSimFx : Base
    {
        Biquad hp, lp1, lp2, res, scoop, bump, pres;
        void reset() override { hp.reset(); lp1.reset(); lp2.reset(); res.reset(); scoop.reset(); bump.reset(); pres.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            static const float resF[4]   = { 95.0f, 110.0f, 80.0f, 120.0f };
            static const float scoopF[4] = { 600.0f, 500.0f, 450.0f, 700.0f };
            static const float scoopD[4] = { -3.0f, -2.0f, -5.0f, -1.0f };
            static const float bumpF[4]  = { 2200.0f, 2800.0f, 3000.0f, 3500.0f };
            static const float bumpD[4]  = { 3.0f, 4.0f, 5.0f, 6.0f };
            const int c = juce::jlimit (0, 3, (int) r[0]);
            hp.highpass (sr, r[1], 0.707f);
            lp1.lowpass (sr, r[2], 0.9f);
            lp2.lowpass (sr, r[2], 0.55f);
            res.peak (sr, resF[c], 1.2f, r[4]);
            scoop.peak (sr, scoopF[c], 0.9f, scoopD[c]);
            bump.peak (sr, bumpF[c], 1.2f, bumpD[c]);
            pres.peak (sr, 4000.0f, 0.8f, r[3]);
            float* buf[2] = { L, R };
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                {
                    float x = hp.process (ch, buf[ch][i]);
                    x = lp2.process (ch, lp1.process (ch, x));
                    x = pres.process (ch, bump.process (ch, scoop.process (ch, res.process (ch, x))));
                    buf[ch][i] = x;
                }
        }
    };

    struct ToneStackFx : Base
    {
        Biquad bass, mid, treble, pres; Smooth lv;
        void reset() override { bass.reset(); mid.reset(); treble.reset(); pres.reset(); lv.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            bass.lowShelf (sr, 120.0f, r[0]);
            mid.peak (sr, 650.0f, 0.7f, r[1]);
            treble.highShelf (sr, 3000.0f, r[2]);
            pres.highShelf (sr, 6500.0f, r[3]);
            const float target = dbToGain (r[4]), c = msCoef (sr, 10.0f);
            float* buf[2] = { L, R };
            for (int i = 0; i < n; ++i)
            {
                const float g = lv.next (target, c);
                for (int ch = 0; ch < 2; ++ch)
                    buf[ch][i] = pres.process (ch, treble.process (ch, mid.process (ch, bass.process (ch, buf[ch][i])))) * g;
            }
        }
    };

    struct LimiterFx : Base
    {
        float g = 1;
        void reset() override { g = 1; }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float thr = dbToGain (r[0]), rel = msCoef (sr, r[1]), makeup = dbToGain (r[2]);
            for (int i = 0; i < n; ++i)
            {
                const float peak = std::max (std::abs (L[i]), std::abs (R[i]));
                const float req = peak > thr ? thr / peak : 1.0f;
                if (req < g) g = req; else g += rel * (1.0f - g);
                L[i] *= g * makeup; R[i] *= g * makeup;
            }
        }
    };

    struct UtilityFx : Base
    {
        Smooth gs;
        void reset() override { gs.reset(); }
        void process (float* L, float* R, int n, const float* r, double) override
        {
            const float target = dbToGain (r[0]), c = msCoef (sr, 10.0f);
            const float ang = (r[1] + 1.0f) * 0.25f * kPi;
            const float pl = std::cos (ang) * 1.41421356f, pr = std::sin (ang) * 1.41421356f;
            const float width = r[2], inv = r[4] > 0.5f ? -1.0f : 1.0f;
            const bool mono = r[3] > 0.5f;
            for (int i = 0; i < n; ++i)
            {
                const float g = gs.next (target, c) * inv;
                float l = L[i], rr = R[i];
                const float m = 0.5f * (l + rr), s = 0.5f * (l - rr) * (mono ? 0.0f : width);
                l = m + s; rr = m - s;
                L[i] = l * g * pl; R[i] = rr * g * pr;
            }
        }
    };

    //==========================================================================
    inline std::unique_ptr<Base> create (int type)
    {
        switch (type)
        {
            case NoiseGate:  return std::make_unique<NoiseGateFx>();
            case Compressor: return std::make_unique<CompressorFx>();
            case Booster:    return std::make_unique<BoosterFx>();
            case Overdrive:  return std::make_unique<OverdriveFx>();
            case Distortion: return std::make_unique<DistortionFx>();
            case Fuzz:       return std::make_unique<FuzzFx>();
            case EQ3:        return std::make_unique<EQ3Fx>();
            case Wah:        return std::make_unique<WahFx>();
            case Chorus:
            case Flanger:    return std::make_unique<ModDelayFx>();
            case Phaser:     return std::make_unique<PhaserFx>();
            case Tremolo:    return std::make_unique<TremoloFx>();
            case AutoPan:    return std::make_unique<AutoPanFx>();
            case Delay:      return std::make_unique<DelayFx>();
            case Reverb:     return std::make_unique<ReverbFx>();
            case PitchShift: return std::make_unique<PitchShiftFx>();
            case RingMod:    return std::make_unique<RingModFx>();
            case Bitcrusher: return std::make_unique<BitcrusherFx>();
            case CabSim:     return std::make_unique<CabSimFx>();
            case ToneStack:  return std::make_unique<ToneStackFx>();
            case Limiter:    return std::make_unique<LimiterFx>();
            case Utility:    return std::make_unique<UtilityFx>();
            default:         return nullptr;
        }
    }
}
