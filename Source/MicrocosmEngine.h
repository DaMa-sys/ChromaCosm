#pragma once
#include "ChromaEngine.h"
#include <atomic>

namespace ChromaCosm
{

//==============================================================================
// High-End Dattorro Figure-of-Eight Modulated Reverb Tank
// The secret behind legendary boutique studio reverbs & Microcosm's lush Space
//==============================================================================
class DattorroReverbTank
{
public:
    void prepare (double sampleRate)
    {
        sr = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        double scale = sr / 29760.0; // Dattorro reference clock scaling

        int ap1Size = juce::jmax (16, static_cast<int> (142 * scale));
        int ap2Size = juce::jmax (16, static_cast<int> (107 * scale));
        int ap3Size = juce::jmax (16, static_cast<int> (379 * scale));
        int ap4Size = juce::jmax (16, static_cast<int> (277 * scale));

        inAP1.assign (ap1Size, 0.0f); inAP1Idx = 0;
        inAP2.assign (ap2Size, 0.0f); inAP2Idx = 0;
        inAP3.assign (ap3Size, 0.0f); inAP3Idx = 0;
        inAP4.assign (ap4Size, 0.0f); inAP4Idx = 0;

        int del1Size = juce::jmax (64, static_cast<int> (4453 * scale));
        int del2Size = juce::jmax (64, static_cast<int> (3720 * scale));
        int del3Size = juce::jmax (64, static_cast<int> (4217 * scale));
        int del4Size = juce::jmax (64, static_cast<int> (3163 * scale));

        tankDelay1.assign (del1Size, 0.0f); tankDel1Idx = 0;
        tankDelay2.assign (del2Size, 0.0f); tankDel2Idx = 0;
        tankDelay3.assign (del3Size, 0.0f); tankDel3Idx = 0;
        tankDelay4.assign (del4Size, 0.0f); tankDel4Idx = 0;

        int tAP1Size = juce::jmax (16, static_cast<int> (672 * scale));
        int tAP2Size = juce::jmax (16, static_cast<int> (908 * scale));
        tankAP1.assign (tAP1Size, 0.0f); tankAP1Idx = 0;
        tankAP2.assign (tAP2Size, 0.0f); tankAP2Idx = 0;

        dampL = dampR = 0.0f;
        tankL = tankR = 0.0f;
        lfoPhase1 = 0.0;
        lfoPhase2 = 0.35;
    }

    void process (float inL, float inR, float decay, float damping, float sizeNorm, int spaceMode, float& outL, float& outR)
    {
        if (inAP1.empty()) return;

        float monoIn = 0.5f * (inL + inR);

        // Input 4-stage allpass diffusion cascade (smeared transients, zero metal ringing)
        float d1 = processAllpass (inAP1, inAP1Idx, monoIn, 0.75f);
        float d2 = processAllpass (inAP2, inAP2Idx, d1, 0.75f);
        float d3 = processAllpass (inAP3, inAP3Idx, d2, 0.625f);
        float diffIn = processAllpass (inAP4, inAP4Idx, d3, 0.625f);

        // LFO for modulated delay lines (creates rich choral movement in the tail)
        lfoPhase1 += 0.72 / sr;
        if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.95 / sr;
        if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        float modL = std::sin (static_cast<float> (lfoPhase1 * juce::MathConstants<double>::twoPi)) * (12.0f * sizeNorm);
        float modR = std::cos (static_cast<float> (lfoPhase2 * juce::MathConstants<double>::twoPi)) * (14.0f * sizeNorm);

        // Shimmer pitch injection for Space IV
        float shimmerSig = 0.0f;
        if (spaceMode == 3)
            shimmerSig = (tankL + tankR) * 0.22f;

        // Loop Left (Tank 1)
        float t1In = diffIn + tankR * decay + shimmerSig;
        float apTank1 = processAllpass (tankAP1, tankAP1Idx, t1In, 0.5f);
        writeDelay (tankDelay1, tankDel1Idx, apTank1);
        float d1Out = readDelayInterpolated (tankDelay1, tankDel1Idx, static_cast<float> (tankDelay1.size() - 8) + modL);

        // Single-pole damping
        dampL = (1.0f - damping) * d1Out + damping * dampL;
        writeDelay (tankDelay2, tankDel2Idx, dampL);
        tankL = readDelay (tankDelay2, tankDel2Idx, static_cast<int> (tankDelay2.size() - 4));

        // Loop Right (Tank 2)
        float t2In = diffIn + tankL * decay + shimmerSig;
        float apTank2 = processAllpass (tankAP2, tankAP2Idx, t2In, 0.5f);
        writeDelay (tankDelay3, tankDel3Idx, apTank2);
        float d3Out = readDelayInterpolated (tankDelay3, tankDel3Idx, static_cast<float> (tankDelay3.size() - 8) + modR);

        dampR = (1.0f - damping) * d3Out + damping * dampR;
        writeDelay (tankDelay4, tankDel4Idx, dampR);
        tankR = readDelay (tankDelay4, tankDel4Idx, static_cast<int> (tankDelay4.size() - 4));

        // Multi-tap decorrelated stereo extraction
        outL = (d1Out * 0.42f + tankR * 0.38f - apTank2 * 0.25f);
        outR = (d3Out * 0.42f + tankL * 0.38f - apTank1 * 0.25f);
    }

private:
    static inline float processAllpass (std::vector<float>& buf, int& idx, float input, float gain)
    {
        int sz = static_cast<int> (buf.size());
        idx = (idx + 1) % sz;
        float bufOut = buf[idx];
        float out = -input * gain + bufOut;
        buf[idx] = input + out * gain;
        return out;
    }

    static inline void writeDelay (std::vector<float>& buf, int& idx, float input)
    {
        int sz = static_cast<int> (buf.size());
        idx = (idx + 1) % sz;
        buf[idx] = input;
    }

    static inline float readDelay (const std::vector<float>& buf, int idx, int offset)
    {
        int sz = static_cast<int> (buf.size());
        int r = (idx - offset) % sz;
        if (r < 0) r += sz;
        return buf[r];
    }

    static inline float readDelayInterpolated (const std::vector<float>& buf, int idx, float offset)
    {
        int sz = static_cast<int> (buf.size());
        float rPos = static_cast<float> (idx) - offset;
        while (rPos < 0.0f) rPos += static_cast<float> (sz);
        int i0 = static_cast<int> (rPos) % sz;
        int i1 = (i0 + 1) % sz;
        float frac = rPos - std::floor (rPos);
        return buf[i0] * (1.0f - frac) + buf[i1] * frac;
    }

    double sr = 44100.0;
    std::vector<float> inAP1, inAP2, inAP3, inAP4;
    int inAP1Idx = 0, inAP2Idx = 0, inAP3Idx = 0, inAP4Idx = 0;

    std::vector<float> tankDelay1, tankDelay2, tankDelay3, tankDelay4;
    int tankDel1Idx = 0, tankDel2Idx = 0, tankDel3Idx = 0, tankDel4Idx = 0;

    std::vector<float> tankAP1, tankAP2;
    int tankAP1Idx = 0, tankAP2Idx = 0;

    float dampL = 0.0f, dampR = 0.0f;
    float tankL = 0.0f, tankR = 0.0f;
    double lfoPhase1 = 0.0, lfoPhase2 = 0.35;
};

//==============================================================================
// AUTHENTIC MICROCOSM DSP ENGINE
//==============================================================================
class MicrocosmEngine
{
public:
    enum EffectType
    {
        FX_Mosaic = 0,
        FX_Seq,
        FX_Glide,
        FX_Haze,
        FX_Tunnel,
        FX_Strum,
        FX_Blocks,
        FX_Interrupt,
        FX_Arp,
        FX_Pattern,
        FX_Warp,
        FX_Count
    };

    enum LooperState
    {
        Looper_Empty = 0,
        Looper_Recording,
        Looper_Playing,
        Looper_Overdubbing,
        Looper_Stopped
    };

    struct GrainVisual
    {
        float posNorm = 0.0f;
        float pitch = 1.0f;
        float env = 0.0f;
        float pan = 0.5f;
        bool active = false;
    };

    struct Parameters
    {
        int effectType = FX_Mosaic;
        int variation = 0;
        float time = 0.5f;
        int subdiv = 2; // 0=1/4, 1=1/2, 2=1x, 3=2x, 4=4x, 5=TRIP
        bool tempoSync = true;
        float activity = 0.55f;
        float repeats = 0.5f;
        float shape = 0.5f; // 0..0.25 Square, 0.25..0.50 Ramp, 0.50..0.75 Triangle, 0.75..1.0 Sine
        float filter = 0.65f;
        float filterRes = 0.25f;
        float mix = 0.5f;
        float space = 0.45f;
        int spaceMode = 1; // 0 Room, 1 Hall, 2 Cathedral, 3 Shimmer
        float pitchModRate = 0.35f;
        float pitchModDepth = 0.25f;
        bool reverse = false;
        bool holdSampler = false;
        bool bypass = false;

        // Phrase Looper
        float loopLevel = 0.8f;
        int loopSpeed = 1;
        bool loopReverse = false;
        bool loopPreFX = false;
        double bpm = 120.0;
    };

    static constexpr int kMaxGrains = 24;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = (newSampleRate > 1000.0) ? newSampleRate : 44100.0;

        historySize = static_cast<int> (sampleRate * 8.0) + 128;
        historyL.assign (historySize, 0.0f);
        historyR.assign (historySize, 0.0f);
        historyWritePos = 0;

        maxLooperSamples = static_cast<int> (sampleRate * 40.0);
        looperBufL.assign (maxLooperSamples, 0.0f);
        looperBufR.assign (maxLooperSamples, 0.0f);
        looperLength = 0;
        looperWritePos = 0;
        looperReadPos = 0.0;
        looperState.store (Looper_Empty);

        for (auto& g : grains)
            g.active = false;

        spaceReverbTank.prepare (sampleRate);
        microFilterL.reset();
        microFilterR.reset();

        stepCounter = 0;
        currentStep = 0;
        pitchModPhase = 0.0;
        onsetEnvFast = onsetEnvSlow = 0.0f;
        onsetCooldown = 0;
        duckEnv = 0.0f;
    }

    void triggerLooperRecordOrPlay()
    {
        int st = looperState.load();
        if (st == Looper_Empty || st == Looper_Stopped)
        {
            if (looperLength == 0)
            {
                looperWritePos = 0;
                looperReadPos = 0.0;
                looperState.store (Looper_Recording);
            }
            else
            {
                looperReadPos = 0.0;
                looperState.store (Looper_Playing);
            }
        }
        else if (st == Looper_Recording)
        {
            looperLength = juce::jmax (1024, looperWritePos);
            looperReadPos = 0.0;
            looperState.store (Looper_Playing);
        }
        else if (st == Looper_Playing)
        {
            looperState.store (Looper_Overdubbing);
        }
        else if (st == Looper_Overdubbing)
        {
            looperState.store (Looper_Playing);
        }
    }

    void triggerLooperStop()
    {
        int st = looperState.load();
        if (st == Looper_Recording)
            looperLength = juce::jmax (1024, looperWritePos);
        if (looperLength > 0)
            looperState.store (Looper_Stopped);
        else
            looperState.store (Looper_Empty);
    }

    void triggerLooperClear()
    {
        looperState.store (Looper_Empty);
        looperLength = 0;
        looperWritePos = 0;
        looperReadPos = 0.0;
        std::fill (looperBufL.begin(), looperBufL.end(), 0.0f);
        std::fill (looperBufR.begin(), looperBufR.end(), 0.0f);
    }

    int getLooperState() const { return looperState.load(); }
    float getLooperProgress() const
    {
        if (looperLength <= 0) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (looperReadPos / static_cast<double> (looperLength)));
    }

    std::array<GrainVisual, 16> getGrainVisuals() const
    {
        std::array<GrainVisual, 16> out;
        for (int i = 0; i < 16; ++i)
        {
            out[i].active = grains[i].active;
            out[i].posNorm = grains[i].posNorm;
            out[i].pitch = grains[i].rate;
            out[i].env = grains[i].lastEnv;
            out[i].pan = grains[i].pan;
        }
        return out;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, const Parameters& p)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0)
            return;

        if (historyL.empty() || looperBufL.empty())
            prepare (sampleRate, numSamples);

        float* left  = buffer.getWritePointer (0);
        float* right = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        const float subdivMults[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 0.6667f };
        const float subdivMult = subdivMults[juce::jlimit (0, 5, p.subdiv)];
        double beatSec = p.tempoSync
                       ? (60.0 / juce::jlimit (40.0, 260.0, p.bpm)) * subdivMult
                       : (0.08 + juce::jlimit (0.0f, 1.0f, p.time) * 1.8) * subdivMult;
        beatSec = juce::jlimit (0.05, 3.0, beatSec);

        // Grid interval
        int stepInterval = juce::jmax (128, static_cast<int> (sampleRate * beatSec * 0.25));

        // Space Reverb parameters (decay up to 0.985 in Cathedral/Shimmer for endless ambient wash!)
        const float modeDecays[4]  = { 0.62f, 0.85f, 0.982f, 0.988f };
        const float modeDampings[4]= { 0.48f, 0.32f, 0.18f,  0.10f };
        int sm = juce::jlimit (0, 3, p.spaceMode);
        float revDecay   = juce::jlimit (0.3f, 0.988f, modeDecays[sm] * (0.65f + p.space * 0.35f));
        float revDamping = modeDampings[sm];

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = sanitizeAudio (left[i]);
            float inR = (right != nullptr) ? sanitizeAudio (right[i]) : inL;

            if (p.loopPreFX)
                processLooperSample (inL, inR, p);

            if (p.bypass)
            {
                if (! p.loopPreFX)
                    processLooperSample (inL, inR, p);
                left[i] = inL;
                if (right != nullptr) right[i] = inR;
                continue;
            }

            // Transient onset detector
            float absIn = 0.5f * (std::fabs (inL) + std::fabs (inR));
            onsetEnvFast = sanitizeAudio (0.85f * onsetEnvFast + 0.15f * absIn);
            onsetEnvSlow = sanitizeAudio (0.999f * onsetEnvSlow + 0.001f * absIn);
            bool onsetTriggered = (onsetEnvFast > onsetEnvSlow * 2.5f + 0.016f) && (onsetCooldown == 0);
            if (onsetCooldown > 0) --onsetCooldown;
            if (onsetTriggered)
            {
                onsetCooldown = static_cast<int> (sampleRate * 0.08);
                if (p.effectType == FX_Strum)
                    triggerStrumCascade (p, beatSec);
            }

            // Clean continuous sampling into buffer
            if (! p.holdSampler)
            {
                historyWritePos = safeWrap (historyWritePos, historySize);
                historyL[historyWritePos] = inL;
                historyR[historyWritePos] = inR;
                historyWritePos = safeWrap (historyWritePos + 1, historySize);
            }

            // Step clock spawner
            if (++stepCounter >= stepInterval)
            {
                stepCounter = 0;
                currentStep = (currentStep + 1) & 15;
                spawnGrainsOnGrid (p, beatSec);
            }

            // Pitch modulation LFO
            float pmHz = 0.15f + juce::jlimit (0.0f, 1.0f, p.pitchModRate) * 4.0f;
            pitchModPhase += pmHz / sampleRate;
            pitchModPhase -= std::floor (pitchModPhase);
            float pitchModRatio = 1.0f + std::sin (static_cast<float> (pitchModPhase * juce::MathConstants<double>::twoPi))
                                       * (juce::jlimit (0.0f, 1.0f, p.pitchModDepth) * 0.022f);

            // Render grains with 4-zone Shape envelope
            float wetL = 0.0f, wetR = 0.0f;
            renderGrains (wetL, wetR, p, pitchModRatio);

            if (p.effectType == FX_Pattern || p.effectType == FX_Warp)
                renderMultiDelay (wetL, wetR, p, beatSec, pitchModRatio);

            wetL = sanitizeAudio (wetL);
            wetR = sanitizeAudio (wetR);

            // Shape Resonant Low-Pass / High-Pass Filter
            float filtNorm = juce::jlimit (0.02f, 0.98f, p.filter);
            float cutoffHz = 160.0f * std::pow (68.0f, filtNorm);
            float res = juce::jlimit (0.0f, 0.85f, p.filterRes * 0.80f);

            float lowL, bandL, highL, lowR, bandR, highR;
            microFilterL.process (wetL, cutoffHz, res, static_cast<float> (sampleRate), lowL, bandL, highL);
            microFilterR.process (wetR, cutoffHz, res, static_cast<float> (sampleRate), lowR, bandR, highR);

            if (filtNorm < 0.70f)
            {
                wetL = lowL + bandL * 0.15f;
                wetR = lowR + bandR * 0.15f;
            }
            else
            {
                float t = (filtNorm - 0.70f) / 0.30f;
                wetL = (lowL + bandL * 0.15f) * (1.0f - t) + highL * t;
                wetR = (lowR + bandR * 0.15f) * (1.0f - t) + highR * t;
            }

            // High-End Dattorro Reverb Tank (Lush, endless ambient space)
            const float spaceAmt = juce::jlimit (0.0f, 1.0f, p.space);
            if (spaceAmt > 0.005f)
            {
                float revInL = wetL + inL * 0.20f;
                float revInR = wetR + inR * 0.20f;
                float tankOutL = 0.0f, tankOutR = 0.0f;

                spaceReverbTank.process (revInL, revInR, revDecay, revDamping, spaceAmt, sm, tankOutL, tankOutR);

                wetL = wetL * (1.0f - spaceAmt * 0.35f) + tankOutL * (spaceAmt * 1.15f);
                wetR = wetR * (1.0f - spaceAmt * 0.35f) + tankOutR * (spaceAmt * 1.15f);
            }

            // Dynamic Blooming & Auto-Ducking
            float dryEnv = 0.5f * (std::fabs (inL) + std::fabs (inR));
            duckEnv = sanitizeAudio (0.994f * duckEnv + 0.006f * dryEnv);
            float bloomGain = juce::jlimit (0.55f, 1.0f, 1.0f - duckEnv * 0.45f);
            wetL *= bloomGain;
            wetR *= bloomGain;

            float dryDuck = 1.0f;
            if (p.effectType == FX_Interrupt)
            {
                float wetMag = 0.5f * (std::fabs (wetL) + std::fabs (wetR));
                dryDuck = juce::jlimit (0.25f, 1.0f, 1.0f - wetMag * 0.85f);
            }

            const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
            float outL = inL * (1.0f - mix) * dryDuck + wetL * mix;
            float outR = inR * (1.0f - mix) * dryDuck + wetR * mix;

            if (! p.loopPreFX)
                processLooperSample (outL, outR, p);

            left[i]  = sanitizeAudio (outL, 2.0f);
            if (right != nullptr)
                right[i] = sanitizeAudio (outR, 2.0f);
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;
        double phase = 0.0;
        double phaseInc = 0.001;
        float rate = 1.0f;
        float glideTargetRate = 1.0f;
        float pan = 0.5f;
        float gain = 0.7f;
        float posNorm = 0.5f;
        float lastEnv = 0.0f;
        int remainingRepeats = 0;
        double sliceDurationSec = 0.25;
        double sliceDelaySec = 0.25;
    };

    int findFreeGrainSlot()
    {
        double maxPhase = -1.0;
        int oldestSlot = 0;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (! grains[i].active)
                return i;
            if (grains[i].phase > maxPhase)
            {
                maxPhase = grains[i].phase;
                oldestSlot = i;
            }
        }
        return oldestSlot;
    }

    void spawnGrainsOnGrid (const Parameters& p, double beatSec)
    {
        if (p.activity <= 0.01f || p.effectType == FX_Strum)
            return;

        float act = juce::jlimit (0.0f, 1.0f, p.activity);
        int repeatCount = static_cast<int> (1.0f + p.repeats * 8.0f);
        int var = juce::jlimit (0, 3, p.variation);

        bool shouldSpawn = false;
        if (act > 0.85f)      shouldSpawn = true;
        else if (act > 0.55f) shouldSpawn = (currentStep & 1) == 0;
        else if (act > 0.28f) shouldSpawn = (currentStep & 3) == 0;
        else                  shouldSpawn = (currentStep & 7) == 0;

        if (! shouldSpawn && nextRandom01() > act * 0.4f)
            return;

        static const float panTable[8] = { 0.15f, 0.85f, 0.28f, 0.72f, 0.10f, 0.90f, 0.35f, 0.65f };

        switch (p.effectType)
        {
            case FX_Mosaic:
            {
                const float speedBanks[4][3] = {
                    { 1.0f, 2.0f, 0.5f },
                    { 1.0f, 2.0f, 1.5f },
                    { 0.5f, 1.0f, 0.5f },
                    { 1.0f, 1.5f, 2.0f }
                };

                for (int h = 0; h < 3; ++h)
                {
                    if (h > 0 && act < 0.35f) continue;
                    int slot = findFreeGrainSlot();
                    auto& g = grains[slot];
                    g.active = true;
                    g.phase = 0.0;
                    g.rate = (p.reverse ? -1.0f : 1.0f) * speedBanks[var][h];
                    g.glideTargetRate = g.rate;
                    g.pan = panTable[(currentStep + h * 2) & 7];
                    g.gain = (h == 0) ? 0.68f : 0.48f;
                    g.remainingRepeats = repeatCount;

                    g.sliceDurationSec = beatSec * (h == 1 ? 0.25 : 0.50);
                    g.sliceDelaySec    = beatSec * (0.25 * (1 + (currentStep & 3)));

                    double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                    g.phaseInc = 1.0 / durSamples;

                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64),
                                                         g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                break;
            }

            case FX_Seq:
            {
                const float seqPitches[4][8] = {
                    { 1.0f, 1.0f, 2.0f, 1.0f, 0.5f, 1.0f, 1.5f, 1.0f },
                    { 1.0f, 1.5f, 2.0f, 1.5f, 1.0f, 0.5f, 1.0f, 2.0f },
                    { 2.0f, 1.0f, 0.5f, 1.0f, 2.0f, 1.5f, 1.0f, 0.5f },
                    { 1.0f, 1.25f, 1.5f, 2.0f, 1.5f, 1.25f, 1.0f, 0.5f }
                };
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                float pRatio = seqPitches[var][currentStep & 7];
                g.rate = (p.reverse ? -1.0f : 1.0f) * pRatio;
                g.glideTargetRate = g.rate;
                g.pan = (currentStep & 1) ? 0.2f : 0.8f;
                g.gain = 0.68f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * 0.25;
                g.sliceDelaySec    = beatSec * (0.125 * (1 + ((currentStep + var) & 7)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }

            case FX_Glide:
            {
                const float startRates[4] = { 1.0f, 0.5f, 1.0f, 2.0f };
                const float endRates[4]   = { 1.5f, 1.0f, 0.5f, 1.0f };
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                double dir = p.reverse ? -1.0 : 1.0;
                g.rate = static_cast<float> (dir * startRates[var]);
                g.glideTargetRate = static_cast<float> (dir * endRates[var]);
                g.pan = panTable[currentStep & 7];
                g.gain = 0.62f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * (0.50 + 0.25 * var);
                g.sliceDelaySec    = beatSec * (0.25 * (1 + (currentStep & 3)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }

            case FX_Haze:
            {
                const float hazeDetunes[4] = { 0.005f, 0.015f, 0.5f, 1.0f };
                int clusterCount = (act > 0.6f) ? 3 : 2;
                for (int c = 0; c < clusterCount; ++c)
                {
                    int slot = findFreeGrainSlot();
                    auto& g = grains[slot];
                    g.active = true;
                    g.phase = 0.0;
                    float detune = (var == 2 && c == 1) ? 0.5f : (var == 3 && c == 1) ? 2.0f : (1.0f + (nextRandom01() - 0.5f) * hazeDetunes[var]);
                    g.rate = (p.reverse ? -1.0f : 1.0f) * detune;
                    g.glideTargetRate = g.rate;
                    g.pan = panTable[(currentStep + c * 3) & 7];
                    g.gain = 0.48f;
                    g.remainingRepeats = repeatCount + 2;
                    g.sliceDurationSec = beatSec * (0.50 + 0.25 * c);
                    g.sliceDelaySec    = beatSec * (0.125 * (1 + ((currentStep + c) & 7)));

                    double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                    g.phaseInc = 1.0 / durSamples;
                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                break;
            }

            case FX_Blocks:
            case FX_Interrupt:
            case FX_Arp:
            default:
            {
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                float pRate = 1.0f;
                if (p.effectType == FX_Arp)
                {
                    const float arpChords[4][4] = {
                        { 1.0f, 1.25f, 1.5f, 2.0f },
                        { 1.0f, 1.1892f, 1.4983f, 1.7818f },
                        { 0.5f, 1.0f, 1.5f, 2.0f },
                        { 1.0f, 1.1225f, 1.4983f, 2.0f }
                    };
                    pRate = arpChords[var][currentStep & 3];
                }
                else if (p.effectType == FX_Interrupt)
                {
                    const float intRates[4] = { 2.0f, 0.5f, 1.5f, 1.0f };
                    pRate = intRates[(currentStep + var) & 3];
                }

                g.rate = (p.reverse ? -1.0f : 1.0f) * pRate;
                g.glideTargetRate = g.rate;
                g.pan = panTable[currentStep & 7];
                g.gain = 0.68f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * 0.25;
                g.sliceDelaySec    = beatSec * (0.125 * (1 + (currentStep & 7)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }
        }
    }

    void triggerStrumCascade (const Parameters& p, double beatSec)
    {
        const float strumPitches[4][4] = {
            { 1.0f, 1.25f, 1.5f, 2.0f },
            { 1.0f, 1.5f, 2.0f, 2.5f },
            { 0.5f, 1.0f, 1.5f, 2.0f },
            { 2.0f, 1.5f, 1.25f, 1.0f }
        };
        int var = juce::jlimit (0, 3, p.variation);
        int repeatCount = static_cast<int> (1.0f + p.repeats * 6.0f);

        for (int note = 0; note < 4; ++note)
        {
            int slot = findFreeGrainSlot();
            auto& g = grains[slot];
            g.active = true;
            g.phase = 0.0;
            g.rate = (p.reverse ? -1.0f : 1.0f) * strumPitches[var][note];
            g.glideTargetRate = g.rate;
            g.pan = 0.15f + 0.70f * (static_cast<float> (note) / 3.0f);
            g.gain = 0.68f;
            g.remainingRepeats = repeatCount;
            g.sliceDurationSec = beatSec * 0.35;
            g.sliceDelaySec = beatSec * (0.04 * (note + 1));

            double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
            g.phaseInc = 1.0 / durSamples;
            double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
            g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
            while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
        }
    }

    float evaluateShapeEnvelope (float phaseNorm, float shapeNorm)
    {
        float ph = juce::jlimit (0.0f, 1.0f, phaseNorm);
        float s = juce::jlimit (0.0f, 1.0f, shapeNorm);

        if (s <= 0.25f)
        {
            float edge = 0.04f;
            if (ph < edge) return ph / edge;
            if (ph > (1.0f - edge)) return (1.0f - ph) / edge;
            return 1.0f;
        }
        else if (s <= 0.50f)
        {
            float t = (s - 0.25f) / 0.25f;
            float att = 0.03f;
            float rawDecay = (ph < att) ? (ph / att) : std::exp (-3.5f * (ph - att));
            float squareVal = (ph < 0.04f) ? (ph / 0.04f) : (ph > 0.96f) ? (1.0f - ph) / 0.04f : 1.0f;
            return squareVal * (1.0f - t) + rawDecay * t;
        }
        else if (s <= 0.75f)
        {
            float t = (s - 0.50f) / 0.25f;
            float tri = (ph < 0.5f) ? (ph * 2.0f) : (2.0f - ph * 2.0f);
            float rampVal = (ph < 0.03f) ? (ph / 0.03f) : std::exp (-3.5f * (ph - 0.03f));
            return rampVal * (1.0f - t) + tri * t;
        }
        else
        {
            float t = (s - 0.75f) / 0.25f;
            float tri = (ph < 0.5f) ? (ph * 2.0f) : (2.0f - ph * 2.0f);
            float sineVal = std::sin (ph * juce::MathConstants<float>::pi);
            sineVal = sineVal * sineVal;
            return tri * (1.0f - t) + sineVal * t;
        }
    }

    void renderGrains (float& outL, float& outR, const Parameters& p, float pitchModRatio)
    {
        for (int i = 0; i < kMaxGrains; ++i)
        {
            auto& g = grains[i];
            if (! g.active)
                continue;

            float ph = static_cast<float> (g.phase);
            float env = evaluateShapeEnvelope (ph, p.shape);
            g.lastEnv = env;

            int idx0 = safeWrap (static_cast<int> (g.readPos), historySize);
            int idx1 = safeWrap (idx0 + 1, historySize);
            float frac = static_cast<float> (g.readPos - std::floor (g.readPos));

            float sL = historyL[idx0] * (1.0f - frac) + historyL[idx1] * frac;
            float sR = historyR[idx0] * (1.0f - frac) + historyR[idx1] * frac;

            float amp = env * g.gain * 0.45f;
            float panL = std::cos (g.pan * juce::MathConstants<float>::halfPi);
            float panR = std::sin (g.pan * juce::MathConstants<float>::halfPi);

            outL += sL * amp * panL;
            outR += sR * amp * panR;

            float curRate = g.rate + (g.glideTargetRate - g.rate) * ph;
            g.readPos += static_cast<double> (curRate * pitchModRatio);
            while (g.readPos >= historySize) g.readPos -= historySize;
            while (g.readPos < 0.0)          g.readPos += historySize;

            g.posNorm = static_cast<float> (g.readPos / static_cast<double> (historySize));

            g.phase += g.phaseInc;
            if (g.phase >= 1.0)
            {
                if (--g.remainingRepeats > 0 && ! p.holdSampler)
                {
                    g.phase = 0.0;
                    g.gain *= 0.82f;
                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64),
                                                         g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                else
                {
                    g.active = false;
                    g.lastEnv = 0.0f;
                }
            }
        }
    }

    void renderMultiDelay (float& outL, float& outR, const Parameters& p, double beatSec, float pitchModRatio)
    {
        const float tapRatios[4][4] = {
            { 0.25f, 0.50f, 0.75f, 1.00f },
            { 0.333f, 0.666f, 1.00f, 1.333f },
            { 0.382f, 0.618f, 1.00f, 1.618f },
            { 0.1875f, 0.375f, 0.75f, 1.125f }
        };
        int var = juce::jlimit (0, 3, p.variation);
        float safeRatio = juce::jmax (0.5f, pitchModRatio);

        for (int t = 0; t < 4; ++t)
        {
            double tapDelaySamples = beatSec * tapRatios[var][t] * sampleRate / safeRatio;
            tapDelaySamples = juce::jlimit (16.0, static_cast<double> (historySize - 32), tapDelaySamples);

            double rPos = static_cast<double> (historyWritePos) - tapDelaySamples;
            while (rPos < 0.0) rPos += historySize;
            int i0 = safeWrap (static_cast<int> (rPos), historySize);
            int i1 = safeWrap (i0 + 1, historySize);
            float frac = static_cast<float> (rPos - std::floor (rPos));

            float tL = historyL[i0] * (1.0f - frac) + historyL[i1] * frac;
            float tR = historyR[i0] * (1.0f - frac) + historyR[i1] * frac;

            float tapGain = (0.35f - t * 0.05f) * (0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, p.activity));
            if (t & 1)
            {
                outL += tR * tapGain * 0.7f;
                outR += tL * tapGain;
            }
            else
            {
                outL += tL * tapGain;
                outR += tR * tapGain * 0.7f;
            }
        }
    }

    void processLooperSample (float& l, float& r, const Parameters& p)
    {
        int st = looperState.load();
        if (st == Looper_Empty || st == Looper_Stopped)
            return;

        if (st == Looper_Recording)
        {
            if (looperWritePos < maxLooperSamples)
            {
                looperBufL[looperWritePos] = sanitizeAudio (l);
                looperBufR[looperWritePos] = sanitizeAudio (r);
                ++looperWritePos;
                looperLength = looperWritePos;
                looperReadPos = static_cast<double> (looperWritePos);
            }
            else
            {
                looperLength = maxLooperSamples;
                looperReadPos = 0.0;
                looperState.store (Looper_Playing);
            }
            return;
        }

        if (looperLength <= 0)
            return;

        int i0 = safeWrap (static_cast<int> (looperReadPos), looperLength);
        int i1 = safeWrap (i0 + 1, looperLength);
        float frac = juce::jlimit (0.0f, 1.0f, static_cast<float> (looperReadPos - std::floor (looperReadPos)));

        float loopL = looperBufL[i0] * (1.0f - frac) + looperBufL[i1] * frac;
        float loopR = looperBufR[i0] * (1.0f - frac) + looperBufR[i1] * frac;

        if (st == Looper_Overdubbing)
        {
            looperBufL[i0] = std::tanh (looperBufL[i0] * 0.92f + sanitizeAudio (l));
            looperBufR[i0] = std::tanh (looperBufR[i0] * 0.92f + sanitizeAudio (r));
        }

        float lvl = juce::jlimit (0.0f, 1.2f, p.loopLevel);
        l += loopL * lvl;
        r += loopR * lvl;

        const double speeds[3] = { 0.5, 1.0, 2.0 };
        double inc = speeds[juce::jlimit (0, 2, p.loopSpeed)] * (p.loopReverse ? -1.0 : 1.0);
        looperReadPos += inc;
        while (looperReadPos >= looperLength) looperReadPos -= looperLength;
        while (looperReadPos < 0.0)           looperReadPos += looperLength;
    }

    float nextRandom01()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return static_cast<float> (rngState & 0x00FFFFFFu) / static_cast<float> (0x01000000u);
    }

    double sampleRate = 44100.0;
    uint32_t rngState = 987654321u;

    std::vector<float> historyL, historyR;
    int historySize = 352800;
    int historyWritePos = 0;

    std::array<Grain, kMaxGrains> grains;
    DattorroReverbTank spaceReverbTank;
    StableSVF microFilterL, microFilterR;

    int stepCounter = 0;
    int currentStep = 0;
    double pitchModPhase = 0.0;
    float onsetEnvFast = 0.0f, onsetEnvSlow = 0.0f;
    int onsetCooldown = 0;
    float duckEnv = 0.0f;

    std::vector<float> looperBufL, looperBufR;
    int maxLooperSamples = 1764000;
    int looperLength = 0;
    int looperWritePos = 0;
    double looperReadPos = 0.0;
    std::atomic<int> looperState { Looper_Empty };
};

} // namespace ChromaCosm
